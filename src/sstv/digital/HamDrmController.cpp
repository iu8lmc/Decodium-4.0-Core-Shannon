// SPDX-License-Identifier: GPL-3.0-or-later

#include "HamDrmController.h"

#include "HamDrmBsrCodec.h"
#include "src/sstv/diagnostics/SstvDiagnosticLogging.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QMetaObject>
#include <QStringList>
#include <QThread>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace decodium::sstv::hamdrm {

struct HamDrmController::InboxRecord final
{
    explicit InboxRecord(std::uint16_t transportId,
                         const HamDrmLimits& limits)
        : assembler(std::make_unique<HamDrmObjectAssembler>(transportId,
                                                             limits))
    {
        progress.transportId = transportId;
    }

    std::unique_ptr<HamDrmObjectAssembler> assembler;
    HamDrmAssemblyProgress progress;
    std::optional<HamDrmMotObjectMetadata> metadata;
    std::map<std::uint16_t, std::vector<std::uint8_t>> headerSegments;
    std::optional<std::size_t> totalHeaderSegments;
    std::size_t headerBytes {0U};
    std::size_t bodySegmentSize {0U};
    std::optional<HamDrmAssembledObject> completedObject;
    std::optional<HamDrmImageInfo> imageInfo;
    QString error;
    bool persisted {false};
    bool rejected {false};
    bool jpeg2000Decoded {false};
};

namespace {

constexpr std::size_t kMaximumControllerInboxObjects = 4'096U;
constexpr std::size_t kMaximumControllerErrorCharacters = 65'536U;
constexpr std::size_t kMaximumMissingTextItems = 256U;
// Match the default Gallery decoded-image allowance.  The archive snapshot
// must not turn a structurally valid HAMDRM object into a larger, unbounded
// Qt decoder allocation.
constexpr std::uint32_t kMaximumGallerySnapshotDimension = 8'192U;
constexpr std::uint64_t kMaximumGallerySnapshotPixels =
    32ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t kMaximumGallerySnapshotBytes =
    128ULL * 1024ULL * 1024ULL;

bool gallerySnapshotWithinLimits(std::uint32_t width,
                                 std::uint32_t height,
                                 const HamDrmLimits& limits) noexcept
{
    if (width == 0U || height == 0U
        || width > limits.maximumImageDimension
        || height > limits.maximumImageDimension
        || width > kMaximumGallerySnapshotDimension
        || height > kMaximumGallerySnapshotDimension) {
        return false;
    }
    const std::uint64_t pixels = static_cast<std::uint64_t>(width)
        * static_cast<std::uint64_t>(height);
    return pixels <= limits.maximumImagePixels
        && pixels <= kMaximumGallerySnapshotPixels
        && pixels <= std::numeric_limits<std::uint64_t>::max() / 4U
        && pixels * 4U <= kMaximumGallerySnapshotBytes
        && pixels * 4U
            <= static_cast<std::uint64_t>(
                std::numeric_limits<qsizetype>::max());
}

QImage decodeGalleryImage(const QByteArray& bytes,
                          const HamDrmImageInfo& info,
                          const HamDrmLimits& limits)
{
    if (bytes.isEmpty()
        || info.width == 0U || info.height == 0U
        || info.width > static_cast<std::uint32_t>(
            std::numeric_limits<int>::max())
        || info.height > static_cast<std::uint32_t>(
            std::numeric_limits<int>::max())
        // This is deliberately checked before QImageReader::read(): the
        // native allocation-free validator and this Gallery copy share the
        // same dimension/pixel policy, plus the Gallery decoded-byte cap.
        || !gallerySnapshotWithinLimits(info.width, info.height, limits)) {
        return {};
    }
    QBuffer input;
    input.setData(bytes);
    if (!input.open(QIODevice::ReadOnly)) {
        return {};
    }
    QImageReader reader(&input);
    reader.setAutoTransform(false);
    const QSize expected(static_cast<int>(info.width),
                         static_cast<int>(info.height));
    if (reader.size() != expected) {
        return {};
    }
    const QImage decoded = reader.read();
    if (decoded.isNull() || decoded.size() != expected) {
        return {};
    }
    const QImage normalized = decoded.convertToFormat(QImage::Format_RGBA8888);
    if (normalized.isNull() || normalized.sizeInBytes() <= 0
        || static_cast<std::uint64_t>(normalized.sizeInBytes())
            > kMaximumGallerySnapshotBytes) {
        return {};
    }
    return normalized;
}

QImage galleryImageFromRgba(const HamDrmRgbaImage& decoded,
                            const HamDrmLimits& limits)
{
    if (decoded.width == 0U || decoded.height == 0U
        || decoded.width > static_cast<std::uint32_t>(
            std::numeric_limits<int>::max())
        || decoded.height > static_cast<std::uint32_t>(
            std::numeric_limits<int>::max())
        || !gallerySnapshotWithinLimits(decoded.width, decoded.height,
                                        limits)) {
        return {};
    }
    const std::uint64_t pixels = static_cast<std::uint64_t>(decoded.width)
        * static_cast<std::uint64_t>(decoded.height);
    if (pixels > std::numeric_limits<std::size_t>::max() / 4U
        || decoded.rgba.size() != static_cast<std::size_t>(pixels * 4U)) {
        return {};
    }
    QImage image(static_cast<int>(decoded.width),
                 static_cast<int>(decoded.height),
                 QImage::Format_RGBA8888);
    if (image.isNull()
        || image.sizeInBytes() != static_cast<qsizetype>(pixels * 4U)) {
        return {};
    }
    std::memcpy(image.bits(), decoded.rgba.data(), decoded.rgba.size());
    return image;
}

class NativeJpeg2000Backend final : public HamDrmJpeg2000Backend
{
public:
    HamDrmJpeg2000Capability capability() const override
    {
        return {true,
                true,
                QStringLiteral("OpenJPEG / Decodium native"),
                HamDrmController::tr(
                    "Native bounded JPEG2000 decode and lossless encode are linked")};
    }

    HamDrmValueResult<HamDrmRgbaImage> decode(
        const std::uint8_t* data,
        std::size_t size,
        const HamDrmLimits& limits) const override
    {
        return decodeHamDrmJpeg2000(data, size, limits);
    }

    HamDrmValueResult<std::vector<std::uint8_t>> encodeLossless(
        const HamDrmRgbaImage& image,
        const HamDrmLimits& limits) const override
    {
        return encodeHamDrmJpeg2000Lossless(image, limits);
    }
};

QString statusDetail(const HamDrmStatus& status, const char* fallback)
{
    return status.detail.empty()
        ? QString::fromLatin1(fallback)
        : QString::fromUtf8(status.detail.data(),
                            static_cast<qsizetype>(status.detail.size()));
}

std::vector<std::uint8_t> byteVector(const QByteArray& bytes)
{
    const auto* begin = reinterpret_cast<const std::uint8_t*>(
        bytes.constData());
    return std::vector<std::uint8_t>(begin, begin + bytes.size());
}

} // namespace

std::shared_ptr<HamDrmJpeg2000Backend>
makeNativeHamDrmJpeg2000Backend()
{
    return std::make_shared<NativeJpeg2000Backend>();
}

HamDrmController::HamDrmController(QObject* parent)
    : HamDrmController(HamDrmControllerConfig {},
                       HamDrmControllerBackends {
                           {}, {}, makeNativeHamDrmJpeg2000Backend()},
                       parent)
{
}

HamDrmController::HamDrmController(HamDrmControllerConfig config,
                                   HamDrmControllerBackends backends,
                                   QObject* parent)
    : QObject(parent)
    , config_(std::move(config))
    , backends_(std::move(backends))
{
    validateConfig();
    if (!config_.partialStoreRoot.isEmpty()) {
        partialStore_ = std::make_unique<HamDrmPartialStore>(
            config_.partialStoreRoot, config_.limits);
    }

    const auto& registered = HamDrmProfileRegistry::all();
    if (registered.size() != 72U) {
        throw std::logic_error(
            "HAMDRM controller requires the canonical 72-profile registry");
    }
    profiles_.reserve(static_cast<qsizetype>(registered.size()));
    for (const HamDrmProfile& profile : registered) {
        if (!HamDrmProfileRegistry::validate(profile).ok()) {
            throw std::logic_error("HAMDRM registry contains an invalid profile");
        }
        profiles_.push_back(profileMap(profile));
    }
    selectedProfileId_ = QString::fromStdString(registered.front().id);
    rxState_ = waveformRxAvailable()
        ? OperationState::Idle : OperationState::Unavailable;
    txState_ = waveformTxAvailable()
        ? OperationState::Idle : OperationState::Unavailable;
}

HamDrmController::~HamDrmController()
{
    if (rxSessionId_ != 0U && backends_.waveformRx) {
        backends_.waveformRx->cancel(rxSessionId_);
    }
    if (txSessionId_ != 0U && backends_.waveformTx) {
        backends_.waveformTx->cancel(txSessionId_);
    }
    rxSessionId_ = 0U;
    txSessionId_ = 0U;
}

QVariantMap HamDrmController::profileMap(const HamDrmProfile& profile)
{
    QVariantMap result;
    result.insert(QStringLiteral("id"), QString::fromStdString(profile.id));
    result.insert(QStringLiteral("displayName"),
                  QString::fromStdString(profile.displayName));
    result.insert(QStringLiteral("robustness"),
                  QString::fromStdString(
                      hamDrmRobustnessName(profile.robustness)));
    result.insert(QStringLiteral("bandwidth"),
                  QString::fromStdString(
                      hamDrmBandwidthName(profile.occupiedBandwidth)));
    result.insert(QStringLiteral("bandwidthHz"),
                  static_cast<qulonglong>(profile.occupiedBandwidthHz));
    result.insert(QStringLiteral("constellation"),
                  QString::fromStdString(
                      hamDrmConstellationName(profile.constellation)));
    result.insert(QStringLiteral("protection"),
                  QString::fromStdString(
                      hamDrmProtectionName(profile.protection)));
    result.insert(QStringLiteral("interleaver"),
                  QString::fromStdString(
                      hamDrmInterleaverName(profile.interleaver)));
    result.insert(QStringLiteral("payloadBytesPer400msFrame"),
                  static_cast<qulonglong>(
                      profile.payloadBytesPer400msFrame));
    result.insert(QStringLiteral("expectedPayloadBitrate"),
                  static_cast<qulonglong>(profile.expectedPayloadBitrate));
    return result;
}

QVariantList HamDrmController::profiles() const
{
    return profiles_;
}

QString HamDrmController::selectedProfileId() const
{
    return selectedProfileId_;
}

QVariantMap HamDrmController::selectedProfile() const
{
    const HamDrmProfile* profile = currentProfile();
    return profile == nullptr ? QVariantMap {} : profileMap(*profile);
}

void HamDrmController::setSelectedProfileId(const QString& profileId)
{
    if (!ownerThread()) {
        return;
    }
    const QString requested = profileId.trimmed();
    const HamDrmProfile* selected = HamDrmProfileRegistry::findById(
        requested.toStdString());
    if (selected == nullptr) {
        for (const HamDrmProfile& candidate : HamDrmProfileRegistry::all()) {
            if (QString::compare(QString::fromStdString(candidate.id),
                                 requested,
                                 Qt::CaseInsensitive) == 0) {
                selected = &candidate;
                break;
            }
        }
    }
    if (selected == nullptr) {
        static_cast<void>(reject(
            QStringLiteral("profile"),
            tr("Unknown HAMDRM profile ID; profile selection was unchanged")));
        return;
    }
    const QString canonical = QString::fromStdString(selected->id);
    if (canonical == selectedProfileId_) {
        return;
    }
    if (busy()) {
        static_cast<void>(reject(
            QStringLiteral("profile"),
            tr("Cancel the active HAMDRM operation before changing profile")));
        return;
    }
    selectedProfileId_ = canonical;
    clearError();
    emit selectedProfileChanged();
}

HamDrmWaveformCapability HamDrmController::rxCapability() const
{
    if (!backends_.waveformRx) {
        return {false, {}, tr("Complete HAMDRM waveform RX backend is not connected")};
    }
    try {
        HamDrmWaveformCapability capability =
            backends_.waveformRx->capability();
        if (!capability.completeBackend && capability.detail.isEmpty()) {
            capability.detail = tr(
                "Injected HAMDRM RX component is not a complete waveform backend");
        }
        return capability;
    } catch (...) {
        return {false, {}, tr("HAMDRM waveform RX capability probe failed")};
    }
}

HamDrmWaveformCapability HamDrmController::txCapability() const
{
    if (!backends_.waveformTx) {
        return {false, {}, tr("Complete HAMDRM waveform TX backend is not connected")};
    }
    try {
        HamDrmWaveformCapability capability =
            backends_.waveformTx->capability();
        if (!capability.completeBackend && capability.detail.isEmpty()) {
            capability.detail = tr(
                "Injected HAMDRM TX component is not a complete waveform backend");
        }
        return capability;
    } catch (...) {
        return {false, {}, tr("HAMDRM waveform TX capability probe failed")};
    }
}

HamDrmJpeg2000Capability HamDrmController::jpegCapability() const
{
    if (!backends_.jpeg2000) {
        return {false, false, {},
                tr("JPEG2000 codec adapter is not connected")};
    }
    try {
        HamDrmJpeg2000Capability capability =
            backends_.jpeg2000->capability();
        if (!capability.decodeAvailable && !capability.encodeAvailable
            && capability.detail.isEmpty()) {
            capability.detail = tr(
                "Injected JPEG2000 component exposes no operational direction");
        }
        return capability;
    } catch (...) {
        return {false, false, {},
                tr("JPEG2000 capability probe failed")};
    }
}

QVariantMap HamDrmController::capabilities() const
{
    const HamDrmWaveformCapability rx = rxCapability();
    const HamDrmWaveformCapability tx = txCapability();
    const HamDrmJpeg2000Capability jpeg = jpegCapability();
    QVariantMap result;
    result.insert(QStringLiteral("profileRegistry"), true);
    result.insert(QStringLiteral("profileCount"), profiles_.size());
    result.insert(QStringLiteral("motObjectCodec"), true);
    result.insert(QStringLiteral("bsrCodec"), true);
    result.insert(QStringLiteral("boundedImageStructuralValidation"), true);
    result.insert(QStringLiteral("partialResume"), partialResumeAvailable());
    result.insert(QStringLiteral("waveformRx"), rx.completeBackend);
    result.insert(QStringLiteral("waveformTx"), tx.completeBackend);
    result.insert(QStringLiteral("jpeg2000Decode"), jpeg.decodeAvailable);
    result.insert(QStringLiteral("jpeg2000Encode"), jpeg.encodeAvailable);
    result.insert(QStringLiteral("rxBackendName"), rx.backendName);
    result.insert(QStringLiteral("txBackendName"), tx.backendName);
    result.insert(QStringLiteral("jpeg2000BackendName"), jpeg.backendName);
    return result;
}

QString HamDrmController::capabilityMessage() const
{
    QStringList unavailable;
    if (!waveformRxAvailable()) {
        unavailable.push_back(tr("complete waveform RX"));
    }
    if (!waveformTxAvailable()) {
        unavailable.push_back(tr("complete waveform TX"));
    }
    if (!jpeg2000DecodeAvailable()) {
        unavailable.push_back(tr("JPEG2000 decode adapter"));
    }
    if (!jpeg2000EncodeAvailable()) {
        unavailable.push_back(tr("JPEG2000 encode adapter"));
    }
    if (!partialResumeAvailable()) {
        unavailable.push_back(tr("configured partial-object store"));
    }

    const QString base = tr(
        "HAMDRM application core provides 72 named profiles, bounded MOT/BSR handling and structural image validation.");
    if (unavailable.isEmpty()) {
        return base + QLatin1Char(' ')
            + tr("All injected adapters report operational; this is not an on-air interoperability claim.");
    }
    return base + QLatin1Char(' ')
        + tr("Not connected: %1. RF start remains blocked for each missing waveform direction.")
              .arg(unavailable.join(QStringLiteral(", ")));
}

bool HamDrmController::waveformRxAvailable() const
{
    return rxCapability().completeBackend;
}

bool HamDrmController::waveformTxAvailable() const
{
    return txCapability().completeBackend;
}

bool HamDrmController::jpeg2000DecodeAvailable() const
{
    return jpegCapability().decodeAvailable;
}

bool HamDrmController::jpeg2000EncodeAvailable() const
{
    return jpegCapability().encodeAvailable;
}

bool HamDrmController::partialResumeAvailable() const noexcept
{
    return static_cast<bool>(partialStore_);
}

HamDrmController::OperationState HamDrmController::rxState() const noexcept
{
    return rxState_;
}

HamDrmController::OperationState HamDrmController::txState() const noexcept
{
    return txState_;
}

QString HamDrmController::operationStateName(OperationState state,
                                              bool receive)
{
    switch (state) {
    case OperationState::Unavailable: return tr("Unavailable");
    case OperationState::Idle: return tr("Idle");
    case OperationState::Starting: return tr("Starting");
    case OperationState::Active:
        return receive ? tr("Receiving") : tr("Transmitting");
    case OperationState::Cancelling: return tr("Cancelling");
    case OperationState::Completed: return tr("Completed");
    case OperationState::Cancelled: return tr("Cancelled");
    case OperationState::Error: return tr("Error");
    }
    return tr("Unknown");
}

QString HamDrmController::rxStateName() const
{
    return operationStateName(rxState_, true);
}

QString HamDrmController::txStateName() const
{
    return operationStateName(txState_, false);
}

double HamDrmController::rxProgress() const noexcept
{
    return rxProgress_;
}

double HamDrmController::txProgress() const noexcept
{
    return txProgress_;
}

bool HamDrmController::activeState(OperationState state) noexcept
{
    return state == OperationState::Starting
        || state == OperationState::Active
        || state == OperationState::Cancelling;
}

bool HamDrmController::busy() const noexcept
{
    return activeState(rxState_) || activeState(txState_);
}

QString HamDrmController::error() const
{
    return error_;
}

QString HamDrmController::imageFormatName(HamDrmImageFormat format)
{
    switch (format) {
    case HamDrmImageFormat::Jpeg: return QStringLiteral("JPEG");
    case HamDrmImageFormat::Jpeg2000: return QStringLiteral("JPEG2000/JP2");
    case HamDrmImageFormat::Png: return QStringLiteral("PNG");
    case HamDrmImageFormat::Gif: return QStringLiteral("GIF");
    case HamDrmImageFormat::Bmp: return QStringLiteral("BMP");
    }
    return QStringLiteral("unknown");
}

QVariantList HamDrmController::inbox() const
{
    QVariantList result;
    result.reserve(static_cast<qsizetype>(inbox_.size()));
    for (const auto& entry : inbox_) {
        const InboxRecord& record = *entry.second;
        QVariantMap row;
        row.insert(QStringLiteral("transportId"), entry.first);
        row.insert(QStringLiteral("transportKey"),
                   QStringLiteral("0x%1").arg(entry.first, 4, 16,
                                              QLatin1Char('0')));
        QString state = QStringLiteral("partial");
        if (record.rejected) {
            state = QStringLiteral("rejected");
        } else if (record.completedObject.has_value()) {
            state = record.imageInfo.has_value()
                    && record.imageInfo->format == HamDrmImageFormat::Jpeg2000
                    && !record.jpeg2000Decoded
                ? QStringLiteral("complete-structural")
                : QStringLiteral("complete");
        }
        row.insert(QStringLiteral("state"), state);
        row.insert(QStringLiteral("headerComplete"),
                   record.progress.headerComplete);
        row.insert(QStringLiteral("objectComplete"),
                   record.progress.objectComplete);
        row.insert(QStringLiteral("bodySegmentsReceived"),
                   static_cast<qulonglong>(
                       record.progress.bodySegmentsReceived));
        row.insert(QStringLiteral("totalBodySegments"),
                   static_cast<qulonglong>(record.progress.totalBodySegments));
        row.insert(QStringLiteral("bodyBytesReceived"),
                   static_cast<qulonglong>(record.progress.bodyBytesReceived));
        row.insert(QStringLiteral("expectedBodyBytes"),
                   static_cast<qulonglong>(record.progress.expectedBodyBytes));
        double progress = 0.0;
        if (record.progress.expectedBodyBytes != 0U) {
            progress = std::min(
                1.0,
                static_cast<double>(record.progress.bodyBytesReceived)
                    / static_cast<double>(record.progress.expectedBodyBytes));
        } else if (record.progress.totalBodySegments != 0U) {
            progress = std::min(
                1.0,
                static_cast<double>(record.progress.bodySegmentsReceived)
                    / static_cast<double>(record.progress.totalBodySegments));
        }
        row.insert(QStringLiteral("progress"), progress);
        const std::size_t missing = record.assembler
            ? record.assembler->missingBodySegments().size() : 0U;
        row.insert(QStringLiteral("missingCount"),
                   static_cast<qulonglong>(missing));
        row.insert(QStringLiteral("persisted"), record.persisted);
        row.insert(QStringLiteral("error"), record.error);
        row.insert(QStringLiteral("filename"),
                   record.metadata.has_value()
                       ? QString::fromStdString(record.metadata->filename)
                       : QString {});
        row.insert(QStringLiteral("mimeType"),
                   record.metadata.has_value()
                       ? QString::fromStdString(record.metadata->mimeType)
                       : QString {});
        row.insert(QStringLiteral("imageStructurallyValidated"),
                   record.imageInfo.has_value());
        row.insert(QStringLiteral("jpeg2000Decoded"),
                   record.jpeg2000Decoded);
        if (record.imageInfo.has_value()) {
            row.insert(QStringLiteral("imageFormat"),
                       imageFormatName(record.imageInfo->format));
            row.insert(QStringLiteral("width"), record.imageInfo->width);
            row.insert(QStringLiteral("height"), record.imageInfo->height);
        } else {
            row.insert(QStringLiteral("imageFormat"), QString {});
            row.insert(QStringLiteral("width"), 0U);
            row.insert(QStringLiteral("height"), 0U);
        }
        result.push_back(row);
    }
    return result;
}

int HamDrmController::selectedTransportId() const noexcept
{
    return selectedTransportId_;
}

HamDrmController::InboxRecord* HamDrmController::recordFor(
    int transportId) noexcept
{
    if (!validTransportId(transportId)) {
        return nullptr;
    }
    const auto found = inbox_.find(static_cast<std::uint16_t>(transportId));
    return found == inbox_.end() ? nullptr : found->second.get();
}

const HamDrmController::InboxRecord* HamDrmController::recordFor(
    int transportId) const noexcept
{
    if (!validTransportId(transportId)) {
        return nullptr;
    }
    const auto found = inbox_.find(static_cast<std::uint16_t>(transportId));
    return found == inbox_.end() ? nullptr : found->second.get();
}

std::vector<std::uint16_t> HamDrmController::selectedMissingSegments() const
{
    const InboxRecord* record = recordFor(selectedTransportId_);
    return record != nullptr && record->assembler
        ? record->assembler->missingBodySegments()
        : std::vector<std::uint16_t> {};
}

QVariantList HamDrmController::missingSegments() const
{
    const auto missing = selectedMissingSegments();
    QVariantList result;
    result.reserve(static_cast<qsizetype>(missing.size()));
    for (const std::uint16_t segment : missing) {
        result.push_back(segment);
    }
    return result;
}

QString HamDrmController::missingSegmentsText() const
{
    const auto missing = selectedMissingSegments();
    QStringList text;
    const std::size_t shown = std::min(missing.size(),
                                       kMaximumMissingTextItems);
    text.reserve(static_cast<qsizetype>(shown));
    for (std::size_t index = 0U; index < shown; ++index) {
        text.push_back(QString::number(missing[index]));
    }
    QString result = text.join(QStringLiteral(", "));
    if (shown < missing.size()) {
        result += tr(" … and %1 more").arg(missing.size() - shown);
    }
    return result;
}

bool HamDrmController::canBuildBsr() const
{
    const InboxRecord* record = recordFor(selectedTransportId_);
    return record != nullptr && record->assembler
        && record->progress.headerComplete
        && record->metadata.has_value()
        && record->bodySegmentSize > 0U
        && !record->assembler->missingBodySegments().empty();
}

QString HamDrmController::bsrText() const
{
    return bsrText_;
}

QVariantMap HamDrmController::lastImageValidation() const
{
    return lastImageValidation_;
}

void HamDrmController::validateConfig()
{
    const HamDrmLimits& limits = config_.limits;
    if (limits.maximumObjectBytes == 0U
        || limits.maximumObjectBytes
            > std::numeric_limits<std::uint32_t>::max()
        || limits.maximumHeaderBytes == 0U
        || limits.maximumSegmentBytes == 0U
        || limits.maximumSegmentBytes
            > std::numeric_limits<std::uint16_t>::max()
        || limits.maximumSegments == 0U
        || limits.maximumSegments
            > static_cast<std::size_t>(
                std::numeric_limits<std::uint16_t>::max()) + 1U
        || limits.maximumFilenameBytes == 0U
        || limits.maximumImageDimension == 0U
        || limits.maximumImagePixels == 0U
        || config_.maximumInboxObjects == 0U
        || config_.maximumInboxObjects > kMaximumControllerInboxObjects
        || config_.maximumCompletedObjects == 0U
        || config_.maximumCompletedObjects > config_.maximumInboxObjects
        || config_.maximumCompletedBytes == 0U
        || config_.txBodySegmentBytes == 0U
        || config_.txBodySegmentBytes > limits.maximumSegmentBytes
        || config_.maximumErrorCharacters == 0U
        || config_.maximumErrorCharacters
            > kMaximumControllerErrorCharacters) {
        throw std::invalid_argument("invalid HAMDRM controller bounds");
    }
    if (!config_.partialStoreRoot.isEmpty()) {
        const QString cleaned = QDir::cleanPath(config_.partialStoreRoot);
        if (!QFileInfo(cleaned).isAbsolute()) {
            throw std::invalid_argument(
                "HAMDRM partial-store root must be absolute");
        }
        config_.partialStoreRoot = cleaned;
    }
}

bool HamDrmController::ownerThread() const noexcept
{
    return QThread::currentThread() == thread();
}

const HamDrmProfile* HamDrmController::currentProfile() const noexcept
{
    return HamDrmProfileRegistry::findById(
        selectedProfileId_.toStdString());
}

std::uint64_t HamDrmController::nextSessionId() noexcept
{
    ++sessionSequence_;
    if (sessionSequence_ == 0U) {
        ++sessionSequence_;
    }
    return sessionSequence_;
}

QString HamDrmController::bounded(QString detail) const
{
    return detail.left(static_cast<qsizetype>(
        config_.maximumErrorCharacters));
}

void HamDrmController::setError(QString detail)
{
    detail = bounded(std::move(detail));
    if (detail == error_) {
        return;
    }
    error_ = std::move(detail);
    recordSstvDiagnosticEvent(
        sstvHamDrmLog(), QtWarningMsg, QStringLiteral("hamdrm.error-set"),
        {{QStringLiteral("component"), QStringLiteral("controller")},
         {QStringLiteral("success"), false}});
    emit errorChanged();
}

void HamDrmController::clearError()
{
    if (!ownerThread() || error_.isEmpty()) {
        return;
    }
    error_.clear();
    emit errorChanged();
}

bool HamDrmController::reject(const QString& operation,
                              const QString& detail)
{
    const QString safe = bounded(detail);
    setError(safe);
    recordSstvDiagnosticEvent(
        sstvHamDrmLog(), QtWarningMsg,
        QStringLiteral("hamdrm.operation-rejected"),
        {{QStringLiteral("operation"), operation},
         {QStringLiteral("success"), false}});
    emit operationRejected(operation, safe);
    return false;
}

void HamDrmController::setRxState(OperationState state, double progress)
{
    const double boundedProgress = std::clamp(progress, 0.0, 1.0);
    if (state == rxState_ && boundedProgress == rxProgress_) {
        return;
    }
    const OperationState previous = rxState_;
    rxState_ = state;
    rxProgress_ = boundedProgress;
    recordSstvDiagnosticEvent(
        sstvHamDrmLog(), state == OperationState::Error
                         ? QtWarningMsg : QtInfoMsg,
        QStringLiteral("hamdrm.rx-state-changed"),
        {{QStringLiteral("component"), QStringLiteral("rx")},
         {QStringLiteral("previousState"), static_cast<int>(previous)},
         {QStringLiteral("state"), static_cast<int>(state)}});
    emit operationStateChanged();
}

void HamDrmController::setTxState(OperationState state, double progress)
{
    const double boundedProgress = std::clamp(progress, 0.0, 1.0);
    if (state == txState_ && boundedProgress == txProgress_) {
        return;
    }
    const OperationState previous = txState_;
    txState_ = state;
    txProgress_ = boundedProgress;
    recordSstvDiagnosticEvent(
        sstvHamDrmLog(), state == OperationState::Error
                         ? QtWarningMsg : QtInfoMsg,
        QStringLiteral("hamdrm.tx-state-changed"),
        {{QStringLiteral("component"), QStringLiteral("tx")},
         {QStringLiteral("previousState"), static_cast<int>(previous)},
         {QStringLiteral("state"), static_cast<int>(state)}});
    emit operationStateChanged();
}

bool HamDrmController::startRx()
{
    if (!ownerThread()) {
        return false;
    }
    if (busy()) {
        return reject(QStringLiteral("rx"),
                      tr("Another HAMDRM RX/TX operation is active"));
    }
    const HamDrmWaveformCapability capability = rxCapability();
    if (!capability.completeBackend || !backends_.waveformRx) {
        return reject(QStringLiteral("rx"),
                      capability.detail.isEmpty()
                          ? tr("Complete HAMDRM waveform RX backend is not connected")
                          : capability.detail);
    }
    const HamDrmProfile* profile = currentProfile();
    if (profile == nullptr) {
        setRxState(OperationState::Error, 0.0);
        return reject(QStringLiteral("rx"),
                      tr("Selected HAMDRM profile is invalid"));
    }

    clearError();
    rxSessionId_ = nextSessionId();
    const std::uint64_t session = rxSessionId_;
    setRxState(OperationState::Starting, 0.0);
    HamDrmStatus status;
    try {
        status = backends_.waveformRx->start(*profile, session, *this);
    } catch (const std::exception& exception) {
        status = HamDrmStatus::failure(HamDrmErrorCode::IoFailure,
                                       exception.what());
    } catch (...) {
        status = HamDrmStatus::failure(
            HamDrmErrorCode::IoFailure,
            "HAMDRM waveform RX backend threw an unknown exception");
    }
    if (!status.ok()) {
        if (rxSessionId_ == session) {
            rxSessionId_ = 0U;
            setRxState(OperationState::Error, 0.0);
        }
        return reject(QStringLiteral("rx"),
                      statusDetail(status,
                                   "HAMDRM waveform RX start failed"));
    }
    if (rxSessionId_ == session
        && rxState_ == OperationState::Starting) {
        setRxState(OperationState::Active, 0.0);
    }
    return true;
}

HamDrmValueResult<HamDrmController::TxImageCandidate>
HamDrmController::readTxImage(const QUrl& localImage,
                              bool requireJpeg2000Decode) const
{
    if (!localImage.isValid() || !localImage.isLocalFile()) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::InvalidArgument,
                                      "HAMDRM TX image must be a local file")};
    }
    const QString requested = QDir::cleanPath(
        localImage.toLocalFile().trimmed());
    const QFileInfo info(requested);
    const QString canonicalPath = info.canonicalFilePath();
    if (requested.isEmpty() || !info.isAbsolute() || canonicalPath.isEmpty()
        || !info.exists() || !info.isFile() || info.isSymLink()
        || !info.isReadable() || info.size() <= 0
        || static_cast<quint64>(info.size()) > config_.limits.maximumObjectBytes
        || static_cast<quint64>(info.size())
            > std::numeric_limits<std::uint32_t>::max()) {
        return {std::nullopt,
                HamDrmStatus::failure(
                    HamDrmErrorCode::InvalidArgument,
                    "HAMDRM TX image is unavailable, unsafe or exceeds policy")};
    }

    const QByteArray filename = info.fileName().toUtf8();
    const std::string safeFilename(filename.constData(),
                                   static_cast<std::size_t>(filename.size()));
    if (const HamDrmStatus status = validateHamDrmFilename(
            safeFilename, config_.limits);
        !status.ok()) {
        return {std::nullopt, status};
    }

    QFile file(canonicalPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::IoFailure,
                                      "cannot open HAMDRM TX image")};
    }
    const qint64 expectedSize = info.size();
    QByteArray bytes = file.readAll();
    if (bytes.size() != expectedSize || file.size() != expectedSize) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::IoFailure,
                                      "HAMDRM TX image changed while reading")};
    }

    HamDrmMotObjectMetadata metadata;
    metadata.transportId = qsstvCompatibleTransportId(safeFilename);
    metadata.bodySize = static_cast<std::uint32_t>(bytes.size());
    metadata.filename = safeFilename;
    const auto encodedHeader = encodeHamDrmMotHeader(metadata, config_.limits);
    if (!encodedHeader.ok()) {
        return {std::nullopt, encodedHeader.status};
    }
    const auto canonicalMetadata = parseHamDrmMotHeader(
        encodedHeader.value->data(), encodedHeader.value->size(),
        metadata.transportId, config_.limits);
    if (!canonicalMetadata.ok()) {
        return {std::nullopt, canonicalMetadata.status};
    }
    const auto image = validateHamDrmImage(
        *canonicalMetadata.value,
        reinterpret_cast<const std::uint8_t*>(bytes.constData()),
        static_cast<std::size_t>(bytes.size()),
        config_.limits);
    if (!image.ok()) {
        return {std::nullopt, image.status};
    }

    TxImageCandidate candidate;
    candidate.bytes = std::move(bytes);
    candidate.metadata = *canonicalMetadata.value;
    candidate.imageInfo = *image.value;
    candidate.canonicalPath = canonicalPath;
    // This guard is intentionally before either OpenJPEG or QImageReader.
    // The persisted Gallery uses the same 8192 px / 32 MiB-pixel / 128 MiB
    // decoded-image envelope, so an otherwise valid HAMDRM object cannot
    // make the decoder allocate an image that the archive must later reject.
    if (!gallerySnapshotWithinLimits(candidate.imageInfo.width,
                                     candidate.imageInfo.height,
                                     config_.limits)) {
        return {std::nullopt,
                HamDrmStatus::failure(
                    HamDrmErrorCode::LimitExceeded,
                    "HAMDRM TX image exceeds bounded Gallery snapshot limits")};
    }
    if (candidate.imageInfo.format == HamDrmImageFormat::Jpeg2000) {
        const HamDrmJpeg2000Capability capability = jpegCapability();
        if (!capability.decodeAvailable || !backends_.jpeg2000) {
            if (requireJpeg2000Decode) {
                return {std::nullopt,
                        HamDrmStatus::failure(
                            HamDrmErrorCode::UnsupportedFeature,
                            "JPEG2000 is structurally valid but a complete decode adapter is not connected")};
            }
        } else {
            HamDrmValueResult<HamDrmRgbaImage> decoded;
            try {
                decoded = backends_.jpeg2000->decode(
                    reinterpret_cast<const std::uint8_t*>(
                        candidate.bytes.constData()),
                    static_cast<std::size_t>(candidate.bytes.size()),
                    config_.limits);
            } catch (const std::exception& exception) {
                decoded.status = HamDrmStatus::failure(
                    HamDrmErrorCode::Malformed, exception.what());
            } catch (...) {
                decoded.status = HamDrmStatus::failure(
                    HamDrmErrorCode::Malformed,
                    "JPEG2000 decode adapter threw an unknown exception");
            }
            if (!decoded.ok()) {
                return {std::nullopt, decoded.status};
            }
            if (decoded.value->width != candidate.imageInfo.width
                || decoded.value->height != candidate.imageInfo.height) {
                return {std::nullopt,
                        HamDrmStatus::failure(
                            HamDrmErrorCode::InconsistentObject,
                            "JPEG2000 decoded dimensions differ from JP2 metadata")};
            }
            candidate.jpeg2000Decoded = true;
            candidate.galleryImage = galleryImageFromRgba(*decoded.value,
                                                          config_.limits);
        }
    } else {
        candidate.galleryImage = decodeGalleryImage(candidate.bytes,
                                                    candidate.imageInfo,
                                                    config_.limits);
    }
    return {std::move(candidate), HamDrmStatus::success()};
}

QVariantMap HamDrmController::imageValidationMap(
    const TxImageCandidate& candidate) const
{
    QVariantMap result;
    result.insert(QStringLiteral("valid"), true);
    result.insert(QStringLiteral("path"), candidate.canonicalPath);
    result.insert(QStringLiteral("filename"),
                  QString::fromStdString(candidate.metadata.filename));
    result.insert(QStringLiteral("mimeType"),
                  QString::fromStdString(candidate.metadata.mimeType));
    result.insert(QStringLiteral("format"),
                  imageFormatName(candidate.imageInfo.format));
    result.insert(QStringLiteral("width"), candidate.imageInfo.width);
    result.insert(QStringLiteral("height"), candidate.imageInfo.height);
    result.insert(QStringLiteral("bytes"),
                  static_cast<qulonglong>(candidate.bytes.size()));
    result.insert(QStringLiteral("structuralValidation"), true);
    result.insert(QStringLiteral("jpeg2000"),
                  candidate.imageInfo.format == HamDrmImageFormat::Jpeg2000);
    result.insert(QStringLiteral("jpeg2000Decoded"),
                  candidate.jpeg2000Decoded);
    return result;
}

bool HamDrmController::validateTxImage(const QUrl& localImage)
{
    if (!ownerThread()) {
        return false;
    }
    const auto candidate = readTxImage(localImage, false);
    if (!candidate.ok()) {
        lastImageValidation_ = {
            {QStringLiteral("valid"), false},
            {QStringLiteral("error"),
             statusDetail(candidate.status,
                          "HAMDRM image validation failed")},
        };
        emit imageValidationChanged();
        return reject(QStringLiteral("image"),
                      lastImageValidation_.value(
                          QStringLiteral("error")).toString());
    }
    lastImageValidation_ = imageValidationMap(*candidate.value);
    emit imageValidationChanged();
    clearError();
    return true;
}

bool HamDrmController::startTx(const QUrl& localImage)
{
    if (!ownerThread()) {
        return false;
    }
    if (busy()) {
        return reject(QStringLiteral("tx"),
                      tr("Another HAMDRM RX/TX operation is active"));
    }
    const HamDrmWaveformCapability capability = txCapability();
    if (!capability.completeBackend || !backends_.waveformTx) {
        return reject(QStringLiteral("tx"),
                      capability.detail.isEmpty()
                          ? tr("Complete HAMDRM waveform TX backend is not connected")
                          : capability.detail);
    }
    const HamDrmProfile* profile = currentProfile();
    if (profile == nullptr) {
        setTxState(OperationState::Error, 0.0);
        return reject(QStringLiteral("tx"),
                      tr("Selected HAMDRM profile is invalid"));
    }

    const auto candidate = readTxImage(localImage, true);
    if (!candidate.ok()) {
        setTxState(OperationState::Error, 0.0);
        return reject(QStringLiteral("tx"),
                      statusDetail(candidate.status,
                                   "HAMDRM TX image validation failed"));
    }
    lastImageValidation_ = imageValidationMap(*candidate.value);
    emit imageValidationChanged();
    // The accepted TX must have an exact, bounded Gallery snapshot.  Do this
    // before the waveform backend sees any object: a structurally valid image
    // that Qt cannot decode must not become an unarchived on-air TX.
    if (candidate.value->galleryImage.isNull()) {
        setTxState(OperationState::Error, 0.0);
        return reject(QStringLiteral("tx"),
                      tr("HAMDRM TX image cannot be archived as a bounded Gallery snapshot"));
    }

    const auto encoded = encodeHamDrmObject(
        candidate.value->metadata,
        byteVector(candidate.value->bytes),
        config_.txBodySegmentBytes,
        config_.limits);
    if (!encoded.ok()) {
        setTxState(OperationState::Error, 0.0);
        return reject(QStringLiteral("tx"),
                      statusDetail(encoded.status,
                                   "HAMDRM MOT object encoding failed"));
    }

    clearError();
    txSessionId_ = nextSessionId();
    const std::uint64_t session = txSessionId_;
    setTxState(OperationState::Starting, 0.0);
    HamDrmStatus status;
    try {
        status = backends_.waveformTx->start(
            *profile, *encoded.value, session, *this);
    } catch (const std::exception& exception) {
        status = HamDrmStatus::failure(HamDrmErrorCode::IoFailure,
                                       exception.what());
    } catch (...) {
        status = HamDrmStatus::failure(
            HamDrmErrorCode::IoFailure,
            "HAMDRM waveform TX backend threw an unknown exception");
    }
    if (!status.ok()) {
        if (txSessionId_ == session) {
            txSessionId_ = 0U;
            setTxState(OperationState::Error, 0.0);
        }
        return reject(QStringLiteral("tx"),
                      statusDetail(status,
                                   "HAMDRM waveform TX start failed"));
    }
    if (txSessionId_ == session
        && txState_ == OperationState::Starting) {
        setTxState(OperationState::Active, 0.0);
    }
    emit txImageAccepted(
        candidate.value->galleryImage,
        QString::fromStdString(profile->id),
        static_cast<int>(profile->occupiedBandwidthHz));
    return true;
}

bool HamDrmController::cancelRx()
{
    if (!ownerThread()) {
        return false;
    }
    if (!activeState(rxState_) || rxSessionId_ == 0U
        || !backends_.waveformRx) {
        return reject(QStringLiteral("rx"),
                      tr("No active HAMDRM RX operation to cancel"));
    }
    const std::uint64_t session = rxSessionId_;
    setRxState(OperationState::Cancelling, rxProgress_);
    backends_.waveformRx->cancel(session);
    if (rxSessionId_ == session) {
        rxSessionId_ = 0U;
        setRxState(OperationState::Cancelled, rxProgress_);
    }
    return true;
}

bool HamDrmController::cancelTx()
{
    if (!ownerThread()) {
        return false;
    }
    if (!activeState(txState_) || txSessionId_ == 0U
        || !backends_.waveformTx) {
        return reject(QStringLiteral("tx"),
                      tr("No active HAMDRM TX operation to cancel"));
    }
    const std::uint64_t session = txSessionId_;
    setTxState(OperationState::Cancelling, txProgress_);
    backends_.waveformTx->cancel(session);
    if (txSessionId_ == session) {
        txSessionId_ = 0U;
        setTxState(OperationState::Cancelled, txProgress_);
    }
    return true;
}

bool HamDrmController::cancelAll()
{
    if (!ownerThread()) {
        return false;
    }
    bool cancelled = false;
    if (activeState(rxState_) && rxSessionId_ != 0U) {
        cancelled = cancelRx() || cancelled;
    }
    if (activeState(txState_) && txSessionId_ != 0U) {
        cancelled = cancelTx() || cancelled;
    }
    return cancelled
        ? true
        : reject(QStringLiteral("cancel"),
                 tr("No active HAMDRM operation to cancel"));
}

HamDrmStatus HamDrmController::updateHeaderMetadata(
    InboxRecord& record,
    const HamDrmMotDataGroup& group)
{
    if (group.kind == HamDrmMotGroupKind::Body) {
        record.bodySegmentSize = std::max(record.bodySegmentSize,
                                          group.payload.size());
        return HamDrmStatus::success();
    }

    const auto existing = record.headerSegments.find(group.segmentNumber);
    if (existing == record.headerSegments.end()) {
        if (group.payload.size()
            > config_.limits.maximumHeaderBytes - record.headerBytes) {
            return HamDrmStatus::failure(HamDrmErrorCode::LimitExceeded,
                                         "MOT header cache exceeds policy");
        }
        record.headerBytes += group.payload.size();
        record.headerSegments.emplace(group.segmentNumber, group.payload);
    } else if (existing->second != group.payload) {
        return HamDrmStatus::failure(
            HamDrmErrorCode::ConflictingDuplicate,
            "conflicting duplicate MOT header segment");
    }
    if (group.lastSegment) {
        const std::size_t total = static_cast<std::size_t>(
            group.segmentNumber) + 1U;
        if (record.totalHeaderSegments.has_value()
            && *record.totalHeaderSegments != total) {
            return HamDrmStatus::failure(
                HamDrmErrorCode::InconsistentObject,
                "MOT header extent changed");
        }
        record.totalHeaderSegments = total;
    }
    if (!record.totalHeaderSegments.has_value()
        || record.headerSegments.size() != *record.totalHeaderSegments) {
        return HamDrmStatus::success();
    }

    std::vector<std::uint8_t> header;
    header.reserve(record.headerBytes);
    for (std::size_t index = 0U;
         index < *record.totalHeaderSegments; ++index) {
        const auto found = record.headerSegments.find(
            static_cast<std::uint16_t>(index));
        if (found == record.headerSegments.end()) {
            return HamDrmStatus::success();
        }
        header.insert(header.end(), found->second.begin(), found->second.end());
    }
    const auto metadata = parseHamDrmMotHeader(
        header.data(), header.size(), record.progress.transportId,
        config_.limits);
    if (!metadata.ok()) {
        return metadata.status;
    }
    record.metadata = *metadata.value;
    return HamDrmStatus::success();
}

HamDrmStatus HamDrmController::rebuildRecordMetadata(InboxRecord& record)
{
    if (!record.assembler) {
        return HamDrmStatus::failure(HamDrmErrorCode::Incomplete,
                                     "partial assembler is unavailable");
    }
    record.headerSegments.clear();
    record.totalHeaderSegments.reset();
    record.headerBytes = 0U;
    record.bodySegmentSize = 0U;
    record.metadata.reset();
    const auto snapshot = record.assembler->snapshotGroups();
    if (!snapshot.ok()) {
        return snapshot.status;
    }
    for (const auto& encoded : *snapshot.value) {
        const auto group = parseHamDrmMotDataGroup(
            encoded.data(), encoded.size(), config_.limits);
        if (!group.ok()) {
            return group.status;
        }
        const HamDrmStatus status = updateHeaderMetadata(record, *group.value);
        if (!status.ok()) {
            return status;
        }
    }
    record.progress = record.assembler->progress();
    return HamDrmStatus::success();
}

HamDrmStatus HamDrmController::persistRecord(InboxRecord& record)
{
    if (!partialStore_ || !record.assembler) {
        record.persisted = false;
        return HamDrmStatus::success();
    }
    const HamDrmStatus status = partialStore_->save(*record.assembler);
    record.persisted = status.ok();
    return status;
}

HamDrmStatus HamDrmController::finishRecord(InboxRecord& record)
{
    if (!record.assembler) {
        return HamDrmStatus::failure(HamDrmErrorCode::Incomplete,
                                     "HAMDRM assembler is unavailable");
    }
    auto assembled = record.assembler->assembledObject();
    if (!assembled.ok()) {
        return assembled.status;
    }
    const auto image = validateHamDrmImage(
        assembled.value->metadata,
        assembled.value->originalBytes.data(),
        assembled.value->originalBytes.size(),
        config_.limits);
    if (!image.ok()) {
        return image.status;
    }

    bool jpegDecoded = false;
    if (image.value->format == HamDrmImageFormat::Jpeg2000
        && jpeg2000DecodeAvailable() && backends_.jpeg2000) {
        HamDrmValueResult<HamDrmRgbaImage> decoded;
        try {
            decoded = backends_.jpeg2000->decode(
                assembled.value->originalBytes.data(),
                assembled.value->originalBytes.size(), config_.limits);
        } catch (const std::exception& exception) {
            decoded.status = HamDrmStatus::failure(
                HamDrmErrorCode::Malformed, exception.what());
        } catch (...) {
            decoded.status = HamDrmStatus::failure(
                HamDrmErrorCode::Malformed,
                "JPEG2000 decode adapter threw an unknown exception");
        }
        if (!decoded.ok()) {
            return decoded.status;
        }
        if (decoded.value->width != image.value->width
            || decoded.value->height != image.value->height) {
            return HamDrmStatus::failure(
                HamDrmErrorCode::InconsistentObject,
                "decoded JPEG2000 dimensions differ from MOT image metadata");
        }
        jpegDecoded = true;
    }

    const std::size_t objectBytes = assembled.value->originalBytes.size();
    if (completedObjects_ >= config_.maximumCompletedObjects
        || objectBytes > config_.maximumCompletedBytes - completedBytes_) {
        return HamDrmStatus::failure(
            HamDrmErrorCode::LimitExceeded,
            "completed HAMDRM inbox capacity is exhausted");
    }
    if (partialStore_) {
        const HamDrmStatus removed = partialStore_->remove(
            record.progress.transportId);
        if (!removed.ok()) {
            return removed;
        }
    }

    record.progress = record.assembler->progress();
    record.metadata = assembled.value->metadata;
    record.imageInfo = *image.value;
    record.jpeg2000Decoded = jpegDecoded;
    record.persisted = false;
    record.completedObject = std::move(*assembled.value);
    record.assembler.reset();
    record.headerSegments.clear();
    record.headerBytes = 0U;
    record.totalHeaderSegments.reset();
    ++completedObjects_;
    completedBytes_ += objectBytes;
    return HamDrmStatus::success();
}

void HamDrmController::publishInbox(std::uint16_t preferredTransportId)
{
    if (inbox_.find(preferredTransportId) != inbox_.end()) {
        selectedTransportId_ = preferredTransportId;
    } else if (recordFor(selectedTransportId_) == nullptr) {
        selectedTransportId_ = inbox_.empty()
            ? -1 : static_cast<int>(inbox_.begin()->first);
    }
    emit inboxChanged();
    emit selectedObjectChanged();
}

HamDrmStatus HamDrmController::ingestMotGroup(
    const QByteArray& encodedGroup)
{
    if (!ownerThread()) {
        return HamDrmStatus::failure(
            HamDrmErrorCode::InvalidArgument,
            "HAMDRM MOT ingestion must run on the controller owner thread");
    }
    if (encodedGroup.isEmpty()) {
        return HamDrmStatus::failure(HamDrmErrorCode::InvalidArgument,
                                     "empty HAMDRM MOT data group");
    }
    const auto parsed = parseHamDrmMotDataGroup(
        reinterpret_cast<const std::uint8_t*>(encodedGroup.constData()),
        static_cast<std::size_t>(encodedGroup.size()), config_.limits);
    if (!parsed.ok()) {
        setError(statusDetail(parsed.status, "HAMDRM MOT parse failed"));
        return parsed.status;
    }
    const std::uint16_t transportId = parsed.value->transportId;
    auto found = inbox_.find(transportId);
    if (found == inbox_.end()) {
        if (inbox_.size() >= config_.maximumInboxObjects) {
            const HamDrmStatus status = HamDrmStatus::failure(
                HamDrmErrorCode::LimitExceeded,
                "HAMDRM inbox object bound is exhausted");
            setError(statusDetail(status, "HAMDRM inbox is full"));
            return status;
        }
        found = inbox_.emplace(
            transportId,
            std::make_unique<InboxRecord>(transportId, config_.limits)).first;
    }
    InboxRecord& record = *found->second;
    if (!record.assembler || record.rejected
        || record.completedObject.has_value()) {
        const HamDrmStatus status = HamDrmStatus::failure(
            HamDrmErrorCode::InconsistentObject,
            "HAMDRM transport ID already has a terminal inbox object");
        setError(statusDetail(status, "HAMDRM object is terminal"));
        return status;
    }

    const auto ingested = record.assembler->ingest(*parsed.value);
    if (!ingested.ok()) {
        record.error = bounded(statusDetail(
            ingested.status, "HAMDRM MOT segment rejected"));
        setError(record.error);
        publishInbox(transportId);
        return ingested.status;
    }
    const HamDrmStatus metadataStatus = updateHeaderMetadata(
        record, *parsed.value);
    if (!metadataStatus.ok()) {
        record.rejected = true;
        record.error = bounded(statusDetail(
            metadataStatus, "HAMDRM MOT header rejected"));
        record.assembler.reset();
        setError(record.error);
        publishInbox(transportId);
        return metadataStatus;
    }
    record.progress = record.assembler->progress();
    record.error.clear();

    if (record.progress.objectComplete) {
        const HamDrmStatus completed = finishRecord(record);
        if (!completed.ok()) {
            record.rejected = true;
            record.error = bounded(statusDetail(
                completed, "HAMDRM object validation failed"));
            record.assembler.reset();
            if (partialStore_) {
                static_cast<void>(partialStore_->remove(transportId));
            }
            setError(record.error);
            publishInbox(transportId);
            return completed;
        }
        clearError();
        publishInbox(transportId);
        emit objectCompleted(
            transportId,
            QString::fromStdString(record.metadata->filename));
        return HamDrmStatus::success();
    }

    if (*ingested.value != HamDrmIngestOutcome::DuplicateIgnored) {
        const HamDrmStatus persisted = persistRecord(record);
        if (!persisted.ok()) {
            record.error = bounded(statusDetail(
                persisted, "HAMDRM partial persistence failed"));
            setError(record.error);
            publishInbox(transportId);
            return persisted;
        }
    }
    clearError();
    publishInbox(transportId);
    return HamDrmStatus::success();
}

bool HamDrmController::validTransportId(int transportId) noexcept
{
    return transportId >= 0
        && transportId <= std::numeric_limits<std::uint16_t>::max();
}

bool HamDrmController::selectObject(int transportId)
{
    if (!ownerThread()) {
        return false;
    }
    if (!validTransportId(transportId) || recordFor(transportId) == nullptr) {
        return reject(QStringLiteral("inbox"),
                      tr("Unknown HAMDRM transport ID"));
    }
    if (selectedTransportId_ != transportId) {
        selectedTransportId_ = transportId;
        bsrText_.clear();
        emit selectedObjectChanged();
        emit bsrChanged();
    }
    clearError();
    return true;
}

QString HamDrmController::buildBsr(int transportId, bool qsstvExtended)
{
    if (!ownerThread()) {
        return {};
    }
    InboxRecord* record = recordFor(transportId);
    if (record == nullptr || !record->assembler
        || !record->progress.headerComplete
        || !record->metadata.has_value()
        || record->bodySegmentSize == 0U) {
        static_cast<void>(reject(
            QStringLiteral("bsr"),
            tr("Selected HAMDRM object has insufficient partial metadata for BSR")));
        return {};
    }
    const auto missing = record->assembler->missingBodySegments();
    if (missing.empty()) {
        static_cast<void>(reject(
            QStringLiteral("bsr"),
            tr("Selected HAMDRM object has no missing body segments")));
        return {};
    }
    const HamDrmProfile* profile = currentProfile();
    if (profile == nullptr
        || record->bodySegmentSize
            > std::numeric_limits<std::uint16_t>::max()) {
        static_cast<void>(reject(QStringLiteral("bsr"),
                                 tr("HAMDRM BSR profile or segment size is invalid")));
        return {};
    }

    HamDrmBsrRequest request;
    request.transportId = static_cast<std::uint16_t>(transportId);
    request.headerReceived = true;
    request.segmentSize = static_cast<std::uint16_t>(record->bodySegmentSize);
    request.missingSegments = missing;
    if (qsstvExtended) {
        request.filename = record->metadata->filename;
        request.qsstvCompatibilityCode = profile->qsstvCompatibilityCode;
    }
    const auto encoded = encodeHamDrmBsr(
        request,
        qsstvExtended ? HamDrmBsrDialect::QsstvExtended
                      : HamDrmBsrDialect::EasyPalCompatible,
        config_.limits);
    if (!encoded.ok()) {
        static_cast<void>(reject(
            QStringLiteral("bsr"),
            statusDetail(encoded.status, "HAMDRM BSR encoding failed")));
        return {};
    }
    bsrText_ = QString::fromLatin1(
        reinterpret_cast<const char*>(encoded.value->data()),
        static_cast<qsizetype>(encoded.value->size()));
    selectedTransportId_ = transportId;
    clearError();
    emit selectedObjectChanged();
    emit bsrChanged();
    return bsrText_;
}

bool HamDrmController::resumePartial(int transportId)
{
    if (!ownerThread()) {
        return false;
    }
    if (!partialStore_) {
        return reject(QStringLiteral("resume"),
                      tr("HAMDRM partial-object store is not configured"));
    }
    if (!validTransportId(transportId)) {
        return reject(QStringLiteral("resume"),
                      tr("HAMDRM transport ID is outside 0…65535"));
    }
    if (recordFor(transportId) != nullptr) {
        return reject(QStringLiteral("resume"),
                      tr("HAMDRM transport ID is already present in the inbox"));
    }
    if (inbox_.size() >= config_.maximumInboxObjects) {
        return reject(QStringLiteral("resume"),
                      tr("HAMDRM inbox object bound is exhausted"));
    }
    const auto loaded = partialStore_->load(
        static_cast<std::uint16_t>(transportId));
    if (!loaded.ok()) {
        return reject(QStringLiteral("resume"),
                      statusDetail(loaded.status,
                                   "HAMDRM partial resume failed"));
    }

    auto record = std::make_unique<InboxRecord>(
        static_cast<std::uint16_t>(transportId), config_.limits);
    record->assembler = std::make_unique<HamDrmObjectAssembler>(
        *loaded.value);
    record->persisted = true;
    const HamDrmStatus rebuilt = rebuildRecordMetadata(*record);
    if (!rebuilt.ok()) {
        return reject(QStringLiteral("resume"),
                      statusDetail(rebuilt,
                                   "HAMDRM partial metadata reconstruction failed"));
    }
    const std::uint16_t id = static_cast<std::uint16_t>(transportId);
    inbox_.emplace(id, std::move(record));
    clearError();
    publishInbox(id);
    return true;
}

bool HamDrmController::discardObject(int transportId)
{
    if (!ownerThread()) {
        return false;
    }
    if (!validTransportId(transportId)) {
        return reject(QStringLiteral("discard"),
                      tr("HAMDRM transport ID is outside 0…65535"));
    }
    const std::uint16_t id = static_cast<std::uint16_t>(transportId);
    if (partialStore_) {
        const HamDrmStatus removed = partialStore_->remove(id);
        if (!removed.ok()) {
            return reject(QStringLiteral("discard"),
                          statusDetail(removed,
                                       "HAMDRM partial removal failed"));
        }
    }
    const auto found = inbox_.find(id);
    if (found != inbox_.end()) {
        if (found->second->completedObject.has_value()) {
            const std::size_t bytes =
                found->second->completedObject->originalBytes.size();
            completedBytes_ = bytes > completedBytes_
                ? 0U : completedBytes_ - bytes;
            if (completedObjects_ != 0U) {
                --completedObjects_;
            }
        }
        inbox_.erase(found);
    }
    if (selectedTransportId_ == transportId) {
        bsrText_.clear();
        emit bsrChanged();
    }
    clearError();
    publishInbox(id);
    return true;
}

HamDrmValueResult<HamDrmAssembledObject>
HamDrmController::takeCompletedObject(std::uint16_t transportId)
{
    if (!ownerThread()) {
        return {std::nullopt,
                HamDrmStatus::failure(
                    HamDrmErrorCode::InvalidArgument,
                    "completed object must be taken on the owner thread")};
    }
    const auto found = inbox_.find(transportId);
    if (found == inbox_.end()
        || !found->second->completedObject.has_value()) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::Incomplete,
                                      "HAMDRM object is not complete")};
    }
    HamDrmAssembledObject result = std::move(
        *found->second->completedObject);
    const std::size_t bytes = result.originalBytes.size();
    completedBytes_ = bytes > completedBytes_ ? 0U : completedBytes_ - bytes;
    if (completedObjects_ != 0U) {
        --completedObjects_;
    }
    inbox_.erase(found);
    publishInbox(transportId);
    return {std::move(result), HamDrmStatus::success()};
}

void HamDrmController::handleRxProgress(std::uint64_t sessionId,
                                        double progress)
{
    if (sessionId != rxSessionId_ || !activeState(rxState_)) {
        return;
    }
    if (!std::isfinite(progress) || progress < rxProgress_
        || progress < 0.0 || progress > 1.0) {
        const std::uint64_t active = rxSessionId_;
        rxSessionId_ = 0U;
        backends_.waveformRx->cancel(active);
        setRxState(OperationState::Error, rxProgress_);
        static_cast<void>(reject(
            QStringLiteral("rx"),
            tr("HAMDRM waveform RX backend reported invalid progress")));
        return;
    }
    setRxState(OperationState::Active, progress);
}

void HamDrmController::handleRxMotGroup(std::uint64_t sessionId,
                                        QByteArray encodedGroup)
{
    if (sessionId != rxSessionId_ || !activeState(rxState_)) {
        return;
    }
    const HamDrmStatus status = ingestMotGroup(encodedGroup);
    if (!status.ok()) {
        emit operationRejected(
            QStringLiteral("mot"),
            statusDetail(status, "HAMDRM MOT group rejected"));
    }
}

void HamDrmController::handleRxFinished(std::uint64_t sessionId,
                                        HamDrmStatus status)
{
    if (sessionId != rxSessionId_) {
        return;
    }
    rxSessionId_ = 0U;
    if (status.ok()) {
        clearError();
        setRxState(OperationState::Completed, 1.0);
    } else {
        setRxState(OperationState::Error, rxProgress_);
        static_cast<void>(reject(
            QStringLiteral("rx"),
            statusDetail(status, "HAMDRM waveform RX failed")));
    }
}

void HamDrmController::handleTxProgress(std::uint64_t sessionId,
                                        double progress)
{
    if (sessionId != txSessionId_ || !activeState(txState_)) {
        return;
    }
    if (!std::isfinite(progress) || progress < txProgress_
        || progress < 0.0 || progress > 1.0) {
        const std::uint64_t active = txSessionId_;
        txSessionId_ = 0U;
        backends_.waveformTx->cancel(active);
        setTxState(OperationState::Error, txProgress_);
        static_cast<void>(reject(
            QStringLiteral("tx"),
            tr("HAMDRM waveform TX backend reported invalid progress")));
        return;
    }
    setTxState(OperationState::Active, progress);
}

void HamDrmController::handleTxFinished(std::uint64_t sessionId,
                                        HamDrmStatus status)
{
    if (sessionId != txSessionId_) {
        return;
    }
    txSessionId_ = 0U;
    if (status.ok()) {
        clearError();
        setTxState(OperationState::Completed, 1.0);
    } else {
        setTxState(OperationState::Error, txProgress_);
        static_cast<void>(reject(
            QStringLiteral("tx"),
            statusDetail(status, "HAMDRM waveform TX failed")));
    }
}

void HamDrmController::hamDrmRxProgress(std::uint64_t sessionId,
                                        double progress)
{
    if (ownerThread()) {
        handleRxProgress(sessionId, progress);
        return;
    }
    QMetaObject::invokeMethod(
        this,
        [this, sessionId, progress] {
            handleRxProgress(sessionId, progress);
        },
        Qt::QueuedConnection);
}

void HamDrmController::hamDrmRxMotGroup(std::uint64_t sessionId,
                                        QByteArray encodedGroup)
{
    if (ownerThread()) {
        handleRxMotGroup(sessionId, std::move(encodedGroup));
        return;
    }
    QMetaObject::invokeMethod(
        this,
        [this, sessionId, encodedGroup = std::move(encodedGroup)]() mutable {
            handleRxMotGroup(sessionId, std::move(encodedGroup));
        },
        Qt::QueuedConnection);
}

void HamDrmController::hamDrmRxFinished(std::uint64_t sessionId,
                                        HamDrmStatus status)
{
    if (ownerThread()) {
        handleRxFinished(sessionId, std::move(status));
        return;
    }
    QMetaObject::invokeMethod(
        this,
        [this, sessionId, status = std::move(status)]() mutable {
            handleRxFinished(sessionId, std::move(status));
        },
        Qt::QueuedConnection);
}

void HamDrmController::hamDrmTxProgress(std::uint64_t sessionId,
                                        double progress)
{
    if (ownerThread()) {
        handleTxProgress(sessionId, progress);
        return;
    }
    QMetaObject::invokeMethod(
        this,
        [this, sessionId, progress] {
            handleTxProgress(sessionId, progress);
        },
        Qt::QueuedConnection);
}

void HamDrmController::hamDrmTxFinished(std::uint64_t sessionId,
                                        HamDrmStatus status)
{
    if (ownerThread()) {
        handleTxFinished(sessionId, std::move(status));
        return;
    }
    QMetaObject::invokeMethod(
        this,
        [this, sessionId, status = std::move(status)]() mutable {
            handleTxFinished(sessionId, std::move(status));
        },
        Qt::QueuedConnection);
}

} // namespace decodium::sstv::hamdrm
