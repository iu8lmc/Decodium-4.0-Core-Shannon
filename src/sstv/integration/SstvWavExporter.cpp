// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvWavExporter.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>

#include <algorithm>
#include <array>
#include <exception>
#include <limits>
#include <utility>
#include <vector>

namespace decodium::sstv {
namespace {

class QtSaveFileSink final : public SstvSeekableByteSink
{
public:
    explicit QtSaveFileSink(QSaveFile& file) noexcept
        : file_(file)
    {
    }

    bool resize(std::uint64_t size) noexcept override
    {
        if (size > static_cast<std::uint64_t>(
                       std::numeric_limits<qint64>::max())) {
            return false;
        }
        return file_.resize(static_cast<qint64>(size));
    }

    bool seek(std::uint64_t absolutePosition) noexcept override
    {
        if (absolutePosition > static_cast<std::uint64_t>(
                                   std::numeric_limits<qint64>::max())) {
            return false;
        }
        return file_.seek(static_cast<qint64>(absolutePosition));
    }

    std::size_t write(const std::uint8_t* bytes,
                      std::size_t byteCount) noexcept override
    {
        if ((bytes == nullptr && byteCount != 0U)
            || byteCount > static_cast<std::size_t>(
                               std::numeric_limits<qint64>::max())) {
            return 0U;
        }
        const qint64 written = file_.write(
            reinterpret_cast<const char*>(bytes),
            static_cast<qint64>(byteCount));
        return written < 0 ? 0U : static_cast<std::size_t>(written);
    }

    bool flush() noexcept override
    {
        return file_.flush();
    }

private:
    QSaveFile& file_;
};

QString boundedText(QString text)
{
    if (text.size() > SstvWavExporter::MaximumErrorCharacters) {
        text.truncate(SstvWavExporter::MaximumErrorCharacters);
    }
    return text;
}

QString boundedExceptionText(const char* text)
{
    if (text == nullptr) {
        return SstvWavExporter::tr("unknown exception");
    }
    int length = 0;
    while (length < SstvWavExporter::MaximumErrorCharacters
           && text[length] != '\0') {
        ++length;
    }
    return QString::fromUtf8(text, length);
}

SstvWavExportResult failure(SstvWavExportError code,
                            QString error,
                            QString wavPath = {})
{
    SstvWavExportResult result;
    result.code = code;
    result.error = boundedText(std::move(error));
    result.wavPath = std::move(wavPath);
    return result;
}

bool addJsonEstimate(std::uint64_t amount,
                     std::uint64_t limit,
                     std::uint64_t& total) noexcept
{
    if (total > limit || amount > limit - total) {
        return false;
    }
    total += amount;
    return true;
}

bool addJsonStringEstimate(const QString& text,
                           std::uint64_t limit,
                           std::uint64_t& total) noexcept
{
    if (!addJsonEstimate(2U, limit, total)) {
        return false;
    }
    for (qsizetype index = 0; index < text.size(); ++index) {
        const char16_t code = text.at(index).unicode();
        std::uint64_t bytes = 3U;
        if (code < 0x20U) {
            bytes = 6U;
        } else if (code == u'"' || code == u'\\') {
            bytes = 2U;
        } else if (code <= 0x7fU) {
            bytes = 1U;
        } else if (code <= 0x7ffU) {
            bytes = 2U;
        } else if (code >= 0xd800U && code <= 0xdbffU
                   && index + 1 < text.size()) {
            const char16_t low = text.at(index + 1).unicode();
            if (low >= 0xdc00U && low <= 0xdfffU) {
                bytes = 4U;
                ++index;
            } else {
                bytes = 6U;
            }
        } else if ((code >= 0xdc00U && code <= 0xdfffU)
                   || code == 0x2028U || code == 0x2029U) {
            bytes = 6U;
        }
        if (!addJsonEstimate(bytes, limit, total)) {
            return false;
        }
    }
    return true;
}

bool metadataFitsBound(const QJsonObject& metadata)
{
    constexpr std::uint64_t fixedSidecarReserve = 4'096U;
    const std::uint64_t maximumBytes = static_cast<std::uint64_t>(
        SstvWavExporter::MaximumMetadataBytes);
    if (maximumBytes <= fixedSidecarReserve) {
        return false;
    }
    const std::uint64_t limit = maximumBytes - fixedSidecarReserve;

    struct PendingValue final
    {
        QJsonValue value;
        int depth {0};
    };

    std::vector<PendingValue> pending;
    pending.reserve(64U);
    pending.push_back({QJsonValue(metadata), 0});
    std::size_t nodes = 1U;
    std::uint64_t estimated = 0U;

    while (!pending.empty()) {
        PendingValue current = std::move(pending.back());
        pending.pop_back();
        if (current.depth > SstvWavExporter::MaximumMetadataDepth) {
            return false;
        }

        switch (current.value.type()) {
        case QJsonValue::Null:
        case QJsonValue::Undefined:
            if (!addJsonEstimate(4U, limit, estimated)) {
                return false;
            }
            break;
        case QJsonValue::Bool:
            if (!addJsonEstimate(5U, limit, estimated)) {
                return false;
            }
            break;
        case QJsonValue::Double:
            if (!addJsonEstimate(32U, limit, estimated)) {
                return false;
            }
            break;
        case QJsonValue::String:
            if (!addJsonStringEstimate(current.value.toString(),
                                       limit, estimated)) {
                return false;
            }
            break;
        case QJsonValue::Array: {
            const QJsonArray array = current.value.toArray();
            if (!addJsonEstimate(2U, limit, estimated)) {
                return false;
            }
            for (qsizetype index = 0; index < array.size(); ++index) {
                if (nodes >= SstvWavExporter::MaximumMetadataNodes
                    || !addJsonEstimate(index == 0 ? 0U : 1U,
                                        limit,
                                        estimated)) {
                    return false;
                }
                pending.push_back({array.at(index), current.depth + 1});
                ++nodes;
            }
            break;
        }
        case QJsonValue::Object: {
            const QJsonObject object = current.value.toObject();
            if (!addJsonEstimate(2U, limit, estimated)) {
                return false;
            }
            bool first = true;
            for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
                if (nodes >= SstvWavExporter::MaximumMetadataNodes
                    || !addJsonEstimate(first ? 1U : 2U,
                                        limit,
                                        estimated)
                    || !addJsonStringEstimate(it.key(), limit, estimated)) {
                    return false;
                }
                first = false;
                pending.push_back({it.value(), current.depth + 1});
                ++nodes;
            }
            break;
        }
        }
    }
    return true;
}

QString writerErrorText(SstvWavStreamWriter::Error error)
{
    using Error = SstvWavStreamWriter::Error;
    switch (error) {
    case Error::None:
        return SstvWavExporter::tr("No WAV writer error");
    case Error::InvalidState:
        return SstvWavExporter::tr("invalid WAV writer state");
    case Error::InvalidSampleRate:
        return SstvWavExporter::tr("unsupported WAV sample rate");
    case Error::InvalidArgument:
        return SstvWavExporter::tr("invalid WAV sample buffer");
    case Error::NonFiniteSample:
        return SstvWavExporter::tr("non-finite WAV sample");
    case Error::ChunkTooLarge:
        return SstvWavExporter::tr("WAV sample chunk exceeds its bound");
    case Error::DeclaredSampleLimit:
        return SstvWavExporter::tr("WAV source exceeded its declared length");
    case Error::RiffSizeLimit:
        return SstvWavExporter::tr("WAV source exceeds the classic RIFF limit");
    case Error::SinkResizeFailed:
        return SstvWavExporter::tr("WAV temporary file resize failed");
    case Error::SinkSeekFailed:
        return SstvWavExporter::tr("WAV temporary file seek failed");
    case Error::SinkWriteFailed:
        return SstvWavExporter::tr("WAV temporary file write failed");
    case Error::SinkFlushFailed:
        return SstvWavExporter::tr("WAV temporary file flush failed");
    case Error::Cancelled:
        return SstvWavExporter::tr("WAV export was cancelled");
    }
    return SstvWavExporter::tr("unknown WAV writer error");
}

bool cancelled(const std::shared_ptr<std::atomic_bool>& requested) noexcept
{
    return requested
        && requested->load(std::memory_order_acquire);
}

QByteArray hashCommittedFile(const QString& path,
                             qint64 expectedBytes,
                             QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = SstvWavExporter::tr(
                "Cannot verify the committed WAV file: %1")
                         .arg(file.errorString());
        }
        return {};
    }
    if (expectedBytes < 0 || file.size() != expectedBytes) {
        if (error) {
            *error = SstvWavExporter::tr(
                "Committed WAV size does not match the encoded source");
        }
        return {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    std::array<char, 64 * 1'024U> bytes {};
    qint64 total = 0;
    while (!file.atEnd()) {
        const qint64 count = file.read(bytes.data(),
                                       static_cast<qint64>(bytes.size()));
        if (count <= 0) {
            if (error) {
                *error = count < 0
                    ? SstvWavExporter::tr(
                          "Cannot read the committed WAV file: %1")
                              .arg(file.errorString())
                    : SstvWavExporter::tr(
                          "Committed WAV file ended unexpectedly");
            }
            return {};
        }
        if (total > expectedBytes - count) {
            if (error) {
                *error = SstvWavExporter::tr(
                    "Committed WAV file exceeds its expected size");
            }
            return {};
        }
        total += count;
        hash.addData(QByteArrayView(bytes.data(), count));
    }
    if (total != expectedBytes) {
        if (error) {
            *error = SstvWavExporter::tr(
                "Committed WAV file is incomplete");
        }
        return {};
    }
    return hash.result();
}

bool writeSidecar(const SstvWavExportRequest& request,
                  const SstvWavExportResult& wav,
                  std::uint32_t sampleRate,
                  std::uint64_t sampleCount,
                  QString* metadataPath,
                  QString* error)
{
    if (!metadataFitsBound(request.metadata)) {
        if (error) {
            *error = SstvWavExporter::tr(
                "WAV metadata exceeds its structural or size bound");
        }
        return false;
    }
    const QString path = SstvWavExporter::metadataPathForWav(wav.wavPath);
    if (path.isEmpty()) {
        if (error) {
            *error = SstvWavExporter::tr(
                "Cannot derive the WAV metadata path");
        }
        return false;
    }
    const QFileInfo existing(path);
    if (existing.isSymLink()) {
        if (error) {
            *error = SstvWavExporter::tr(
                "WAV metadata destination must not be a symbolic link");
        }
        return false;
    }
    if (existing.exists() && !request.replaceExisting) {
        if (error) {
            *error = SstvWavExporter::tr(
                "WAV metadata destination already exists");
        }
        return false;
    }

    QJsonObject object;
    object.insert(QStringLiteral("schema"),
                  QStringLiteral("decodium-sstv-wav-metadata"));
    object.insert(QStringLiteral("schemaVersion"), 1);
    object.insert(QStringLiteral("createdAtUtc"),
                  QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    object.insert(QStringLiteral("wavFile"), QFileInfo(wav.wavPath).fileName());
    object.insert(QStringLiteral("mode"), request.mode.trimmed());
    object.insert(QStringLiteral("sampleRate"),
                  static_cast<qint64>(sampleRate));
    object.insert(QStringLiteral("sampleCount"),
                  static_cast<qint64>(sampleCount));
    object.insert(QStringLiteral("durationMilliseconds"),
                  static_cast<qint64>((sampleCount * 1'000U
                                       + sampleRate - 1U) / sampleRate));
    object.insert(QStringLiteral("channels"), 1);
    object.insert(QStringLiteral("bitsPerSample"), 16);
    object.insert(QStringLiteral("encoding"),
                  QStringLiteral("PCM signed little-endian"));
    object.insert(QStringLiteral("fileSizeBytes"), wav.fileSizeBytes);
    object.insert(QStringLiteral("sha256"),
                  QString::fromLatin1(wav.sha256.toHex()));
    if (!request.metadata.isEmpty()) {
        object.insert(QStringLiteral("metadata"), request.metadata);
    }

    const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    if (bytes.isEmpty()
        || bytes.size() > SstvWavExporter::MaximumMetadataBytes) {
        if (error) {
            *error = SstvWavExporter::tr(
                "WAV metadata exceeds its size bound");
        }
        return false;
    }

    QSaveFile output(path);
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly)
        || output.write(bytes) != bytes.size()
        || !output.commit()) {
        const QString detail = output.errorString();
        output.cancelWriting();
        if (error) {
            *error = SstvWavExporter::tr(
                "Cannot commit WAV metadata: %1").arg(detail);
        }
        return false;
    }
    if (metadataPath) {
        *metadataPath = path;
    }
    return true;
}

} // namespace

SstvWavExportResult SstvWavExporter::exportAtomic(
    std::unique_ptr<SstvPcm16Source> source,
    const SstvWavExportRequest& request,
    const std::shared_ptr<std::atomic_bool>& cancelRequested)
{
    if (request.outputPath.size() > MaximumPathCharacters
        || request.outputPath.contains(QChar::Null)
        || request.mode.size() > MaximumModeCharacters
        || request.mode.contains(QChar::Null)
        || request.mode.trimmed().isEmpty()) {
        return failure(SstvWavExportError::InvalidRequest,
                       tr("Invalid atomic WAV export request"));
    }
    const QString cleanPath = QDir::cleanPath(request.outputPath.trimmed());
    if (!source || cleanPath.isEmpty()
        || cleanPath.size() > MaximumPathCharacters
        || !QFileInfo(cleanPath).isAbsolute()
        || QFileInfo(cleanPath).fileName().isEmpty()
        || QFileInfo(cleanPath).fileName().size() > 255
        || QFileInfo(cleanPath).suffix().compare(
               QStringLiteral("wav"), Qt::CaseInsensitive) != 0
        || request.pullSamples < MinimumPullSamples
        || request.pullSamples > MaximumPullSamples) {
        return failure(SstvWavExportError::InvalidRequest,
                       tr("Invalid atomic WAV export request"), cleanPath);
    }
    const QFileInfo destination(cleanPath);
    const QFileInfo parent(destination.absolutePath());
    if (!parent.exists() || !parent.isDir() || !parent.isWritable()
        || destination.isSymLink()) {
        return failure(SstvWavExportError::InvalidRequest,
                       tr("WAV destination directory is unavailable or unsafe"),
                       cleanPath);
    }

    const std::uint32_t sampleRate = source->sampleRate();
    const std::uint64_t totalSamples = source->totalSamples();
    if (source->cancelled() || source->complete()
        || source->producedSamples() != 0U
        || sampleRate < SstvWavStreamWriter::kMinimumSampleRate
        || sampleRate > SstvWavStreamWriter::kMaximumSampleRate
        || totalSamples == 0U
        || !SstvWavStreamWriter::canRepresentPcmSamples(totalSamples)) {
        return failure(SstvWavExportError::InvalidRequest,
                       tr("WAV source is not a fresh bounded PCM16 stream"),
                       cleanPath);
    }
    if (cancelled(cancelRequested)) {
        source->cancel();
        return failure(SstvWavExportError::Cancelled,
                       tr("WAV export was cancelled"), cleanPath);
    }

    QLockFile lock(cleanPath + QStringLiteral(".lock"));
    lock.setStaleLockTime(30'000);
    if (!lock.tryLock(0)) {
        return failure(SstvWavExportError::Locked,
                       tr("Another process is exporting this WAV file"),
                       cleanPath);
    }
    if (QFileInfo::exists(cleanPath) && !request.replaceExisting) {
        return failure(SstvWavExportError::Collision,
                       tr("WAV destination already exists"), cleanPath);
    }

    QSaveFile output(cleanPath);
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly)) {
        return failure(SstvWavExportError::CommitFailure,
                       tr("Cannot open the atomic WAV destination: %1")
                           .arg(output.errorString()),
                       cleanPath);
    }

    QtSaveFileSink sink(output);
    SstvWavStreamWriter writer(sink);
    if (!writer.begin(sampleRate, totalSamples)) {
        output.cancelWriting();
        return failure(SstvWavExportError::WriterFailure,
                       tr("Cannot start WAV encoding: %1")
                           .arg(writerErrorText(writer.lastError())),
                       cleanPath);
    }

    std::vector<std::int16_t> samples(request.pullSamples);
    std::uint64_t pulled = 0U;
    try {
        while (!source->complete()) {
            if (cancelled(cancelRequested)) {
                source->cancel();
                static_cast<void>(writer.cancel());
                output.cancelWriting();
                return failure(SstvWavExportError::Cancelled,
                               tr("WAV export was cancelled"), cleanPath);
            }
            if (source->sampleRate() != sampleRate
                || source->totalSamples() != totalSamples
                || source->producedSamples() != pulled
                || source->cancelled()) {
                source->cancel();
                static_cast<void>(writer.cancel());
                output.cancelWriting();
                return failure(
                    SstvWavExportError::SourceFailure,
                    tr("WAV source changed its declared streaming state"),
                    cleanPath);
            }
            const std::uint64_t remaining = totalSamples - pulled;
            const std::size_t capacity = static_cast<std::size_t>(
                std::min<std::uint64_t>(remaining, samples.size()));
            if (capacity == 0U) {
                static_cast<void>(writer.cancel());
                output.cancelWriting();
                return failure(
                    SstvWavExportError::SourceFailure,
                    tr("WAV source produced more samples than declared"),
                    cleanPath);
            }
            const std::uint64_t before = source->producedSamples();
            const std::size_t count = source->pullPcm16(samples.data(), capacity);
            if (cancelled(cancelRequested)) {
                source->cancel();
                static_cast<void>(writer.cancel());
                output.cancelWriting();
                return failure(SstvWavExportError::Cancelled,
                               tr("WAV export was cancelled"), cleanPath);
            }
            const std::uint64_t after = source->producedSamples();
            if (count == 0U || count > capacity
                || static_cast<std::uint64_t>(count) > remaining
                || after < before
                || after - before != static_cast<std::uint64_t>(count)
                || after > totalSamples
                || source->sampleRate() != sampleRate
                || source->totalSamples() != totalSamples
                || source->cancelled()) {
                source->cancel();
                static_cast<void>(writer.cancel());
                output.cancelWriting();
                return failure(
                    SstvWavExportError::SourceFailure,
                    tr("WAV source stalled or violated its pull contract"),
                    cleanPath);
            }
            if (!writer.appendPcm16(samples.data(), count)) {
                const QString detail = writerErrorText(writer.lastError());
                static_cast<void>(writer.cancel());
                output.cancelWriting();
                return failure(SstvWavExportError::WriterFailure,
                               tr("WAV encoding failed: %1").arg(detail),
                               cleanPath);
            }
            pulled += static_cast<std::uint64_t>(count);
        }
    } catch (const std::exception& exception) {
        source->cancel();
        static_cast<void>(writer.cancel());
        output.cancelWriting();
        return failure(SstvWavExportError::SourceFailure,
                       tr("WAV source failed: %1")
                           .arg(boundedExceptionText(exception.what())),
                       cleanPath);
    } catch (...) {
        source->cancel();
        static_cast<void>(writer.cancel());
        output.cancelWriting();
        return failure(SstvWavExportError::SourceFailure,
                       tr("WAV source failed unexpectedly"), cleanPath);
    }

    if (cancelled(cancelRequested)) {
        source->cancel();
        static_cast<void>(writer.cancel());
        output.cancelWriting();
        return failure(SstvWavExportError::Cancelled,
                       tr("WAV export was cancelled"), cleanPath);
    }
    if (pulled != totalSamples || source->producedSamples() != totalSamples
        || source->sampleRate() != sampleRate
        || source->totalSamples() != totalSamples
        || !source->complete() || source->cancelled()) {
        static_cast<void>(writer.cancel());
        output.cancelWriting();
        return failure(SstvWavExportError::SourceFailure,
                       tr("WAV source ended at an unexpected sample boundary"),
                       cleanPath);
    }
    if (!writer.finalize()) {
        const QString detail = writerErrorText(writer.lastError());
        output.cancelWriting();
        return failure(SstvWavExportError::WriterFailure,
                       tr("Cannot finalize WAV encoding: %1").arg(detail),
                       cleanPath);
    }
    const SstvWavStreamWriter::Metrics metrics = writer.metrics();
    if (cancelled(cancelRequested)) {
        source->cancel();
        output.cancelWriting();
        return failure(SstvWavExportError::Cancelled,
                       tr("WAV export was cancelled"), cleanPath);
    }
    // QLockFile serializes cooperating exporters.  Re-check immediately before
    // QSaveFile's atomic rename as an additional no-clobber guard against a
    // destination created while a long source was being rendered.
    const QFileInfo lateDestination(cleanPath);
    if (lateDestination.isSymLink()) {
        output.cancelWriting();
        return failure(SstvWavExportError::InvalidRequest,
                       tr("WAV destination became a symbolic link"),
                       cleanPath);
    }
    if (!request.replaceExisting && lateDestination.exists()) {
        output.cancelWriting();
        return failure(SstvWavExportError::Collision,
                       tr("WAV destination appeared during export"),
                       cleanPath);
    }
    if (!output.commit()) {
        const QString detail = output.errorString();
        output.cancelWriting();
        return failure(SstvWavExportError::CommitFailure,
                       tr("Cannot atomically commit the WAV file: %1")
                           .arg(detail),
                       cleanPath);
    }

    const std::uint64_t expectedUnsigned = SstvWavStreamWriter::kHeaderBytes
        + totalSamples * 2U;
    if (expectedUnsigned > static_cast<std::uint64_t>(
                               std::numeric_limits<qint64>::max())) {
        return failure(SstvWavExportError::IntegrityFailure,
                       tr("Committed WAV size exceeds the platform limit"),
                       cleanPath);
    }
    const qint64 expectedBytes = static_cast<qint64>(expectedUnsigned);
    QString integrityError;
    const QByteArray digest = hashCommittedFile(cleanPath, expectedBytes,
                                                &integrityError);
    if (digest.size() != 32) {
        SstvWavExportResult result = failure(
            SstvWavExportError::IntegrityFailure,
            integrityError.isEmpty()
                ? tr("Cannot verify the committed WAV file")
                : integrityError,
            cleanPath);
        result.fileSizeBytes = QFileInfo(cleanPath).size();
        result.metrics = metrics;
        return result;
    }

    SstvWavExportResult result;
    result.ok = true;
    result.code = SstvWavExportError::None;
    result.wavPath = cleanPath;
    result.sha256 = digest;
    result.fileSizeBytes = expectedBytes;
    result.metrics = metrics;

    if (request.writeMetadataSidecar) {
        QString sidecarError;
        try {
            if (writeSidecar(request, result, sampleRate, totalSamples,
                             &result.metadataPath, &sidecarError)) {
                result.metadataCommitted = true;
            } else {
                result.warning = boundedText(sidecarError.isEmpty()
                    ? tr("WAV was committed but its metadata sidecar was not")
                    : sidecarError);
            }
        } catch (const std::exception& exception) {
            result.warning = boundedText(
                tr("WAV metadata failed: %1")
                    .arg(boundedExceptionText(exception.what())));
        } catch (...) {
            result.warning = tr("WAV metadata failed unexpectedly");
        }
    }
    return result;
}

QString SstvWavExporter::metadataPathForWav(const QString& wavPath)
{
    const QFileInfo info(wavPath);
    if (wavPath.trimmed().isEmpty() || info.fileName().isEmpty()) {
        return {};
    }
    QString base = info.completeBaseName();
    if (base.isEmpty()) {
        base = info.fileName();
    }
    return QDir(info.absolutePath()).absoluteFilePath(
        base + QStringLiteral(".json"));
}

} // namespace decodium::sstv
