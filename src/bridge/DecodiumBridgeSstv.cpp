// SPDX-License-Identifier: GPL-3.0-or-later

#include "DecodiumBridge.h"

#if DECODIUM_HAS_SSTV
#include "Audio/soundout.h"
#include "Modulator/Modulator.hpp"
#include "DecodiumAudioSink.h"
#include "DecodiumLegacyBackend.h"
#include "DecoPortLink.h"
#include "src/rtl/RtlSdrInput.h"
#include "src/sstv/core/SstvModeRegistry.h"
#include "src/sstv/diagnostics/SstvDiagnosticLogging.h"
#include "src/sstv/diagnostics/SstvDiagnosticsController.h"
#include "src/sstv/integration/SstvRxAudioJobController.h"
#include "src/sstv/integration/SstvRxRuntime.h"
#include "src/sstv/integration/SstvQsoLog.h"
#include "src/sstv/integration/SstvStudioController.h"
#include "src/sstv/integration/SstvTxCoordinator.h"
#include "src/sstv/integration/SstvTxSources.h"
#include "src/sstv/integration/SstvWavReplayController.h"
#include "src/sstv/image/SstvImageFrame.h"
#include "src/sstv/models/SstvGalleryModel.h"
#include "src/sstv/models/SstvShareController.h"
#include "src/sstv/models/SstvThumbnailProvider.h"
#include "src/sstv/storage/SstvImageStorage.h"
#include "src/sstv/storage/SstvStorageWorker.h"
#include "src/sstv/storage/SstvTxGalleryArchive.h"
#include "SecureSettings.hpp"
#include "DecodiumProfileSettings.h"
#if DECODIUM_HAS_HAMDRM
#include "src/sstv/digital/HamDrmController.h"
#include "src/sstv/digital/waveform/HamDrmWaveformAdapters.h"
#endif
#endif

#include <QDir>
#include <QCoreApplication>
#include <QFileInfo>
#include <QImage>
#include <QMetaObject>
#include <QMutexLocker>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QUuid>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>

#if DECODIUM_HAS_SSTV
// The relay is deliberately not a QObject.  Direct producer callbacks only
// enter this small gate and the runtime's bounded ingress; no callback needs
// the bridge object or its affinity thread.  disableAndDrain() closes the gate
// under the same mutex used to count entrants, so a callback cannot slip
// between the final in-flight check and SstvRxRuntime::shutdown().
class DecodiumSstvAudioRelay final
{
public:
#if DECODIUM_HAS_HAMDRM
    void enable(
        decodium::sstv::SstvRxRuntime* runtime,
        std::shared_ptr<decodium::sstv::hamdrm::waveform::HamDrmNativeRxBackend>
            hamDrm)
#else
    void enable(decodium::sstv::SstvRxRuntime* runtime)
#endif
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_runtime = runtime;
#if DECODIUM_HAS_HAMDRM
        m_hamDrm = std::move(hamDrm);
        m_enabled = runtime != nullptr || m_hamDrm != nullptr;
#else
        m_enabled = runtime != nullptr;
#endif
    }

    void disableAndDrain()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_enabled = false;
        m_drained.wait(lock, [this]() { return m_inFlight == 0U; });
        m_runtime = nullptr;
#if DECODIUM_HAS_HAMDRM
        m_hamDrm.reset();
#endif
    }

    bool submit(QVector<short> samples,
                int sampleRate,
                decodium::sstv::SstvRxRouteToken token)
    {
        decodium::sstv::SstvRxRuntime* runtime = nullptr;
#if DECODIUM_HAS_HAMDRM
        std::shared_ptr<
            decodium::sstv::hamdrm::waveform::HamDrmNativeRxBackend> hamDrm;
#endif
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_enabled || samples.isEmpty()) {
                return false;
            }
            runtime = m_runtime;
#if DECODIUM_HAS_HAMDRM
            hamDrm = m_hamDrm;
            if (!runtime && !hamDrm) {
                return false;
            }
#else
            if (!runtime) {
                return false;
            }
#endif
            ++m_inFlight;
        }

        bool accepted = false;
#if DECODIUM_HAS_HAMDRM
        static_assert(sizeof(short) == sizeof(std::int16_t),
                      "Decodium PCM16 bridge requires 16-bit short");
        if (hamDrm) {
            accepted = hamDrm->submitPcm16(
                reinterpret_cast<const std::int16_t*>(samples.constData()),
                static_cast<std::size_t>(samples.size()),
                static_cast<std::uint32_t>(sampleRate));
        }
#endif
        if (runtime && token.valid()) {
            accepted = runtime->enqueuePcm16(
                std::move(samples), sampleRate, token) || accepted;
        }

        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            --m_inFlight;
            if (m_inFlight == 0U) {
                m_drained.notify_all();
            }
        }
        return accepted;
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_drained;
    decodium::sstv::SstvRxRuntime* m_runtime {nullptr};
#if DECODIUM_HAS_HAMDRM
    std::shared_ptr<
        decodium::sstv::hamdrm::waveform::HamDrmNativeRxBackend> m_hamDrm;
#endif
    std::size_t m_inFlight {0U};
    bool m_enabled {false};
};
#endif

namespace
{

#if DECODIUM_HAS_SSTV
using decodium::sstv::SstvAudioSourceKind;
using decodium::sstv::SstvRxAfcMode;
using decodium::sstv::SstvRxControlSettings;
using decodium::sstv::SstvRxModeControl;
using decodium::sstv::SstvRxRedecodeParameters;
using decodium::sstv::SstvRxRuntime;
using decodium::sstv::SstvRxSlantMode;

QString sstvRxModeControlName(SstvRxModeControl mode)
{
    return mode == SstvRxModeControl::Manual
        ? QStringLiteral("manual") : QStringLiteral("auto");
}

QString sstvRxAfcModeName(SstvRxAfcMode mode)
{
    switch (mode) {
    case SstvRxAfcMode::Off: return QStringLiteral("off");
    case SstvRxAfcMode::Automatic: return QStringLiteral("auto");
    case SstvRxAfcMode::Manual: return QStringLiteral("manual");
    }
    return QStringLiteral("auto");
}

QString sstvRxSlantModeName(SstvRxSlantMode mode)
{
    switch (mode) {
    case SstvRxSlantMode::Off: return QStringLiteral("off");
    case SstvRxSlantMode::Automatic: return QStringLiteral("auto");
    case SstvRxSlantMode::Manual: return QStringLiteral("manual");
    }
    return QStringLiteral("auto");
}

bool parseSstvRxModeControl(const QVariant& value,
                            SstvRxModeControl* parsed)
{
    const QString text = value.toString().trimmed().toLower();
    if (text == QLatin1String("auto")
        || text == QLatin1String("automatic")) {
        *parsed = SstvRxModeControl::Automatic;
        return true;
    }
    if (text == QLatin1String("manual")) {
        *parsed = SstvRxModeControl::Manual;
        return true;
    }
    return false;
}

bool parseSstvRxAfcMode(const QVariant& value, SstvRxAfcMode* parsed)
{
    const QString text = value.toString().trimmed().toLower();
    if (text == QLatin1String("off")) {
        *parsed = SstvRxAfcMode::Off;
        return true;
    }
    if (text == QLatin1String("auto")
        || text == QLatin1String("automatic")) {
        *parsed = SstvRxAfcMode::Automatic;
        return true;
    }
    if (text == QLatin1String("manual")) {
        *parsed = SstvRxAfcMode::Manual;
        return true;
    }
    return false;
}

bool parseSstvRxSlantMode(const QVariant& value,
                          SstvRxSlantMode* parsed)
{
    const QString text = value.toString().trimmed().toLower();
    if (text == QLatin1String("off")) {
        *parsed = SstvRxSlantMode::Off;
        return true;
    }
    if (text == QLatin1String("auto")
        || text == QLatin1String("automatic")) {
        *parsed = SstvRxSlantMode::Automatic;
        return true;
    }
    if (text == QLatin1String("manual")) {
        *parsed = SstvRxSlantMode::Manual;
        return true;
    }
    return false;
}

bool selectableSstvRxMode(const std::string& id)
{
    if (id.empty()) {
        return false;
    }
    const decodium::sstv::SstvModeRegistry registry =
        decodium::sstv::SstvModeRegistry::canonical();
    const decodium::sstv::SstvModeSpec* mode = registry.findById(id);
    return mode && mode->classification
            == decodium::sstv::ModeClassification::AnalogSstv
        && mode->claimsRxSupport();
}

bool sstvRxSettingsFromMap(const QVariantMap& values,
                           SstvRxControlSettings base,
                           SstvRxControlSettings* parsed,
                           QString* error)
{
    auto fail = [error](const QString& detail) {
        if (error) {
            *error = detail;
        }
        return false;
    };
    if (values.contains(QStringLiteral("modeControl"))
        && !parseSstvRxModeControl(
            values.value(QStringLiteral("modeControl")),
            &base.modeControl)) {
        return fail(QObject::tr("Invalid SSTV RX mode control"));
    }
    if (values.contains(QStringLiteral("manualMode"))) {
        base.manualMode = values.value(QStringLiteral("manualMode"))
                              .toString().trimmed().toStdString();
    }
    if (values.contains(QStringLiteral("modeLockEnabled"))) {
        base.modeLockEnabled = values.value(
            QStringLiteral("modeLockEnabled")).toBool();
    }
    if (values.contains(QStringLiteral("lockedMode"))) {
        base.lockedMode = values.value(QStringLiteral("lockedMode"))
                              .toString().trimmed().toStdString();
    }
    if (values.contains(QStringLiteral("receiveWithoutVis"))) {
        base.receiveWithoutVis = values.value(
            QStringLiteral("receiveWithoutVis")).toBool();
    }
    if (values.contains(QStringLiteral("timingFallbackEnabled"))) {
        base.timingFallbackEnabled = values.value(
            QStringLiteral("timingFallbackEnabled")).toBool();
    }
    if (values.contains(QStringLiteral("afcMode"))
        && !parseSstvRxAfcMode(values.value(QStringLiteral("afcMode")),
                               &base.afcMode)) {
        return fail(QObject::tr("Invalid SSTV AFC mode"));
    }
    if (values.contains(QStringLiteral("manualFrequencyCorrectionHz"))) {
        bool ok = false;
        const double value = values.value(
            QStringLiteral("manualFrequencyCorrectionHz")).toDouble(&ok);
        if (!ok || !std::isfinite(value)) {
            return fail(QObject::tr("Invalid manual SSTV AFC correction"));
        }
        base.manualFrequencyCorrectionHz = value;
    }
    if (values.contains(QStringLiteral("slantMode"))
        && !parseSstvRxSlantMode(
            values.value(QStringLiteral("slantMode")), &base.slantMode)) {
        return fail(QObject::tr("Invalid SSTV slant mode"));
    }
    if (values.contains(QStringLiteral("manualClockErrorPpm"))) {
        bool ok = false;
        const double value = values.value(
            QStringLiteral("manualClockErrorPpm")).toDouble(&ok);
        if (!ok || !std::isfinite(value)) {
            return fail(QObject::tr("Invalid manual SSTV slant correction"));
        }
        base.manualClockErrorPpm = value;
    }
    if (values.contains(QStringLiteral("replayRetentionSeconds"))) {
        bool ok = false;
        const uint value = values.value(
            QStringLiteral("replayRetentionSeconds")).toUInt(&ok);
        if (!ok) {
            return fail(QObject::tr("Invalid SSTV replay retention"));
        }
        base.replayRetentionSeconds = value;
    }
    if (values.contains(QStringLiteral("retainRawAudio"))) {
        base.retainRawAudio = values.value(
            QStringLiteral("retainRawAudio")).toBool();
    }
    if (values.contains(QStringLiteral("diagnosticScopeEnabled"))) {
        base.diagnosticScopeEnabled = values.value(
            QStringLiteral("diagnosticScopeEnabled")).toBool();
    }
    if ((base.modeControl == SstvRxModeControl::Manual
         && !selectableSstvRxMode(base.manualMode))
        || (base.modeLockEnabled && !selectableSstvRxMode(base.lockedMode))
        || !decodium::sstv::SstvRxControlPolicy::settingsAreValid(base)) {
        return fail(QObject::tr("SSTV RX controls are outside their safe bounds"));
    }
    *parsed = std::move(base);
    return true;
}

void persistSstvRxControls(const SstvRxControlSettings& settings)
{
    // Keep the whole SSTV RX control snapshot in one QSettings transaction.
    // Calling DecodiumBridge::setSetting() once per field forces a synchronous
    // sync() for every field, which can pause the GUI while FT8/panadapter are
    // active (Lock used to trigger that path twice in succession).
    QSettings store(QSettings::IniFormat,
                    QSettings::UserScope,
                    QStringLiteral("Decodium"),
                    QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(store);
    store.setValue(QStringLiteral("SSTV/RxModeControl"),
                   sstvRxModeControlName(settings.modeControl));
    store.setValue(QStringLiteral("SSTV/RxManualMode"),
                   QString::fromStdString(settings.manualMode));
    store.setValue(QStringLiteral("SSTV/RxModeLockEnabled"),
                   settings.modeLockEnabled);
    store.setValue(QStringLiteral("SSTV/RxLockedMode"),
                   QString::fromStdString(settings.lockedMode));
    store.setValue(QStringLiteral("SSTV/RxWithoutVis"),
                   settings.receiveWithoutVis);
    store.setValue(QStringLiteral("SSTV/RxTimingFallbackEnabled"),
                   settings.timingFallbackEnabled);
    store.setValue(QStringLiteral("SSTV/RxAfcMode"),
                   sstvRxAfcModeName(settings.afcMode));
    store.setValue(QStringLiteral("SSTV/RxManualFrequencyCorrectionHz"),
                   settings.manualFrequencyCorrectionHz);
    store.setValue(QStringLiteral("SSTV/RxSlantMode"),
                   sstvRxSlantModeName(settings.slantMode));
    store.setValue(QStringLiteral("SSTV/RxManualClockErrorPpm"),
                   settings.manualClockErrorPpm);
    store.setValue(QStringLiteral("SSTV/RxReplayRetentionSeconds"),
                   settings.replayRetentionSeconds);
    store.setValue(QStringLiteral("SSTV/RxRetainRawAudio"),
                   settings.retainRawAudio);
    store.setValue(QStringLiteral("SSTV/RxDiagnosticScopeEnabled"),
                   settings.diagnosticScopeEnabled);
    store.sync();
}

QString sstvSourceName(SstvAudioSourceKind kind)
{
    switch (kind) {
    case SstvAudioSourceKind::LocalSoundCard:
        return QObject::tr("Local sound card");
    case SstvAudioSourceKind::DecoPort:
        return QStringLiteral("DecoPort");
    case SstvAudioSourceKind::Tci:
        return QStringLiteral("TCI");
    case SstvAudioSourceKind::WebSdr:
        return QObject::tr("WebSDR/KiwiSDR monitor");
    case SstvAudioSourceKind::RtlSdr:
        return QStringLiteral("RTL-SDR");
    case SstvAudioSourceKind::LegacyBackend:
        return QObject::tr("Decodium legacy backend");
    case SstvAudioSourceKind::Replay:
        return QObject::tr("Replay/WAV");
    case SstvAudioSourceKind::Unknown:
        break;
    }
    return QObject::tr("None");
}

QString sstvDiagnosticSourceToken(SstvAudioSourceKind kind)
{
    // Diagnostics must remain locale-independent and pass the central scalar
    // allowlist.  Do not reuse the translated UI labels (some contain '/').
    switch (kind) {
    case SstvAudioSourceKind::LocalSoundCard:
        return QStringLiteral("local-sound-card");
    case SstvAudioSourceKind::DecoPort:
        return QStringLiteral("decoport");
    case SstvAudioSourceKind::Tci:
        return QStringLiteral("tci");
    case SstvAudioSourceKind::WebSdr:
        return QStringLiteral("websdr-kiwisdr");
    case SstvAudioSourceKind::RtlSdr:
        return QStringLiteral("rtl-sdr");
    case SstvAudioSourceKind::LegacyBackend:
        return QStringLiteral("legacy-backend");
    case SstvAudioSourceKind::Replay:
        return QStringLiteral("replay-wav");
    case SstvAudioSourceKind::Unknown:
        break;
    }
    return QStringLiteral("unknown");
}

QString sstvRuntimeStateName(SstvRxRuntime::State state)
{
    switch (state) {
    case SstvRxRuntime::State::Inactive:
        return QStringLiteral("Inactive");
    case SstvRxRuntime::State::Running:
        return QStringLiteral("Running");
    case SstvRxRuntime::State::Cancelled:
        return QStringLiteral("Cancelled");
    case SstvRxRuntime::State::Stopping:
        return QStringLiteral("Stopping");
    case SstvRxRuntime::State::Error:
        return QStringLiteral("Error");
    case SstvRxRuntime::State::Shutdown:
        return QStringLiteral("Shutdown");
    }
    return QStringLiteral("Unknown");
}

quint32 stableSstvStreamId(QStringView identity)
{
    // Stable FNV-1a over UTF-16 code units. This runs only on owner-thread
    // source transitions; the audio callback receives the resulting scalar.
    quint32 value = 2'166'136'261U;
    for (QChar character : identity) {
        const quint16 codeUnit = character.unicode();
        value ^= static_cast<quint8>(codeUnit & 0xffU);
        value *= 16'777'619U;
        value ^= static_cast<quint8>((codeUnit >> 8U) & 0xffU);
        value *= 16'777'619U;
    }
    return value == 0U ? 1U : value;
}

qulonglong boundedUnsigned(std::uint64_t value)
{
    return static_cast<qulonglong>(value);
}

bool terminalSstvTxState(decodium::sstv::SstvTxState state) noexcept
{
    return decodium::sstv::SstvTxStateMachine::isTerminal(state)
        || state == decodium::sstv::SstvTxState::Disabled;
}

#if defined(Q_OS_MAC)
constexpr int kSstvOutputBufferFrames = 16'384;
#else
constexpr int kSstvOutputBufferFrames = 49'152;
#endif

bool sstvOutputMatchesConfiguredSelection(
    const QString& configuredName,
    const QString& configuredId,
    bool requestedDeviceFound,
    const QAudioDevice& output)
{
    if (output.isNull()) {
        return false;
    }
    const QString name = configuredName.trimmed();
    const QString id = configuredId.trimmed();
    if (name.isEmpty() && id.isEmpty()) {
        return true;
    }
    if (!requestedDeviceFound) {
        return false;
    }

    // findOutputDevice() historically reports "found" for an ID-only
    // selection when its name is empty, even if it returned the default
    // fallback. Close that hole locally without changing legacy routing.
    if (name.isEmpty()) {
        const QString selectedId = QString::fromLatin1(output.id().toHex());
        return !selectedId.isEmpty()
            && selectedId.compare(id, Qt::CaseInsensitive) == 0;
    }
    return true;
}

QImage sstvSnapshotToImage(
    const decodium::sstv::SstvImageSnapshot& snapshot)
{
    const std::size_t width = snapshot.width;
    const std::size_t height = snapshot.height;
    if (width == 0U || height == 0U
        || width > decodium::sstv::SstvImageFrame::kMaximumDimension
        || height > decodium::sstv::SstvImageFrame::kMaximumDimension
        || width > decodium::sstv::SstvImageFrame::kMaximumPixels / height
        || snapshot.pixels.size() != width * height
        || width > static_cast<std::size_t>(
               std::numeric_limits<int>::max())
        || height > static_cast<std::size_t>(
               std::numeric_limits<int>::max())) {
        return {};
    }

    QImage image(static_cast<int>(width), static_cast<int>(height),
                 QImage::Format_RGB888);
    if (image.isNull()) {
        return {};
    }
    image.fill(Qt::black);
    for (std::size_t y = 0U; y < height; ++y) {
        uchar* const destination = image.scanLine(static_cast<int>(y));
        if (!destination) {
            return {};
        }
        const decodium::sstv::SstvRgbPixel* const source =
            snapshot.pixels.data() + y * width;
        for (std::size_t x = 0U; x < width; ++x) {
            destination[x * 3U] = source[x].red;
            destination[x * 3U + 1U] = source[x].green;
            destination[x * 3U + 2U] = source[x].blue;
        }
    }
    return image;
}

bool sstvQsoRequestFromMap(
    const QVariantMap& values,
    decodium::sstv::SstvQsoLogRequest* request,
    QString* error)
{
    return decodium::sstv::SstvQsoLog::requestFromVariantMap(
        values, request, error);
}
#endif

} // namespace

bool DecodiumBridge::sstvAvailable() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvRxRuntime != nullptr;
#else
    return false;
#endif
}

bool DecodiumBridge::sstvRxRequested() const
{
    return m_sstvRxRequested;
}

bool DecodiumBridge::sstvRxActive() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvRxRequested
        && m_sstvRxRuntime
        && m_sstvRxRuntime->state() == SstvRxRuntime::State::Running;
#else
    return false;
#endif
}

QString DecodiumBridge::sstvRxSource() const
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvRxRuntime) {
        return QStringLiteral("Unavailable");
    }
    return sstvSourceName(m_sstvRxRuntime->routeToken().source.kind);
#else
    return QStringLiteral("Unavailable");
#endif
}

QString DecodiumBridge::sstvRxSourceDevice() const
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvRxRuntime) {
        return {};
    }

    const auto kind = m_sstvRxRuntime->routeToken().source.kind;
    switch (kind) {
    case SstvAudioSourceKind::LocalSoundCard:
    case SstvAudioSourceKind::WebSdr: {
        // While a native session is live, report the QAudioDevice opened by
        // SoundInput. Before capture starts, report the configured selection.
        if (m_sstvRxRequested && !m_activeRxInputDeviceName.trimmed().isEmpty()) {
            return m_activeRxInputDeviceName.trimmed();
        }
        const QString configured = m_audioInputDevice.trimmed();
        return configured.isEmpty()
            ? tr("System default input") : configured;
    }
    case SstvAudioSourceKind::RtlSdr:
        return m_activeRxInputDeviceName.trimmed();
    case SstvAudioSourceKind::DecoPort:
    case SstvAudioSourceKind::Tci:
    case SstvAudioSourceKind::LegacyBackend:
    case SstvAudioSourceKind::Replay:
    case SstvAudioSourceKind::Unknown:
        break;
    }
#endif
    return {};
}

QString DecodiumBridge::sstvRxState() const
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvRxRuntime) {
        return QStringLiteral("Unavailable");
    }
    return sstvRuntimeStateName(m_sstvRxRuntime->state());
#else
    return QStringLiteral("Unavailable");
#endif
}

QString DecodiumBridge::sstvDetectedMode() const
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvRxRuntime) {
        return {};
    }
    const SstvRxRuntime::Snapshot snapshot = m_sstvRxRuntime->snapshot();
    return snapshot.vis.modeMapped ? snapshot.vis.mappedMode : QString();
#else
    return {};
#endif
}

QString DecodiumBridge::sstvRxImageSource() const
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvRxRuntime) {
        return {};
    }
    const SstvRxRuntime::Snapshot snapshot = m_sstvRxRuntime->snapshot();
    if (!snapshot.image.available || snapshot.image.generation == 0U
        || snapshot.image.revision == 0U) {
        return {};
    }
    // A new URL only when the immutable pixel revision changes prevents
    // redundant provider work for telemetry-only snapshot notifications.
    return QStringLiteral("image://decodium-sstv/live/%1/%2")
        .arg(boundedUnsigned(snapshot.image.generation))
        .arg(boundedUnsigned(snapshot.image.revision));
#else
    return {};
#endif
}

std::shared_ptr<const decodium::sstv::SstvImageSnapshot>
DecodiumBridge::sstvRxImageSnapshot() const noexcept
{
#if DECODIUM_HAS_SSTV
    return m_sstvRxRuntime
        ? m_sstvRxRuntime->latestImageSnapshot()
        : std::shared_ptr<const decodium::sstv::SstvImageSnapshot> {};
#else
    return {};
#endif
}

QObject* DecodiumBridge::sstvStudio() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvStudioController;
#else
    return nullptr;
#endif
}

QObject* DecodiumBridge::sstvGallery() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvGalleryModel;
#else
    return nullptr;
#endif
}

QObject* DecodiumBridge::sstvShare() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvShareController;
#else
    return nullptr;
#endif
}

QObject* DecodiumBridge::sstvDigital() const
{
#if DECODIUM_HAS_SSTV && DECODIUM_HAS_HAMDRM
    return m_hamDrmController;
#else
    return nullptr;
#endif
}

QObject* DecodiumBridge::sstvDiagnostics() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvDiagnosticsController;
#else
    return nullptr;
#endif
}

void DecodiumBridge::refreshSstvDiagnosticsSnapshot()
{
#if DECODIUM_HAS_SSTV
    Q_ASSERT(QThread::currentThread() == thread());
    if (!m_sstvDiagnosticsController
        || !m_sstvDiagnosticsController->ready()) {
        return;
    }

    constexpr quint64 maximumExactJsonInteger
        = (quint64 {1} << 53U) - 1U;
    const auto boundedCounter = [maximumExactJsonInteger](quint64 value) {
        return QVariant::fromValue<qulonglong>(
            std::min(value, maximumExactJsonInteger));
    };
    const auto saturatedSum = [maximumExactJsonInteger](
                                  quint64 left, quint64 right) {
        const quint64 boundedLeft = std::min(left, maximumExactJsonInteger);
        const quint64 boundedRight = std::min(right, maximumExactJsonInteger);
        return boundedLeft > maximumExactJsonInteger - boundedRight
            ? maximumExactJsonInteger
            : boundedLeft + boundedRight;
    };

    QVariantMap capabilities {
        {QStringLiteral("analogRx"), m_sstvRxRuntime != nullptr},
        {QStringLiteral("analogTx"), m_sstvTxCoordinator != nullptr},
        {QStringLiteral("wavReplay"), m_sstvWavReplayController != nullptr},
        {QStringLiteral("wavExport"), m_sstvStudioController != nullptr},
        {QStringLiteral("gallery"), m_sstvGalleryModel != nullptr},
        {QStringLiteral("remoteSharing"), m_sstvShareController != nullptr},
        {QStringLiteral("secureCredentials"),
         m_sstvShareController
             && m_sstvShareController->secureStorageAvailable()},
        {QStringLiteral("liveAudio"), m_sstvRxRuntime != nullptr},
        {QStringLiteral("realRadioTx"),
         m_sstvTxCoordinator
             && (sstvTxUsesVoxPtt() || sstvTxCanControlPtt())},
    };
#if DECODIUM_HAS_HAMDRM
    capabilities.insert(QStringLiteral("hamdrm"),
                        m_hamDrmController != nullptr);
#else
    capabilities.insert(QStringLiteral("hamdrm"), false);
#endif

    const QVariantMap controls = sstvRxControls();
    QVariantMap settings {
        {QStringLiteral("autoDetect"),
         controls.value(QStringLiteral("modeControl")).toString()
             == QStringLiteral("auto")},
        {QStringLiteral("autoSave"), m_sstvRxAutoSaveEnabled},
        {QStringLiteral("retainRawAudio"),
         controls.value(QStringLiteral("retainRawAudio")).toBool()},
        {QStringLiteral("backgroundDetector"), false},
        {QStringLiteral("remoteSharingEnabled"),
         m_sstvShareController && m_sstvShareController->enabled()},
        {QStringLiteral("rxSampleRateHz"), 12'000},
    };
#if DECODIUM_HAS_HAMDRM
    settings.insert(QStringLiteral("hamdrmEnabled"),
                    m_hamDrmController != nullptr);
#else
    settings.insert(QStringLiteral("hamdrmEnabled"), false);
#endif

    QVariantMap rx {
        {QStringLiteral("state"), sstvRxState()},
        {QStringLiteral("sourceKind"), QStringLiteral("unavailable")},
        {QStringLiteral("modeId"), sstvDetectedMode()},
    };
    if (m_sstvRxRuntime) {
        const SstvRxRuntime::Snapshot snapshot
            = m_sstvRxRuntime->snapshot();
        rx.insert(QStringLiteral("sourceKind"),
                  sstvDiagnosticSourceToken(snapshot.route.source.kind));
        QString modeId = snapshot.image.mode;
        if (modeId.isEmpty() && snapshot.vis.modeMapped) {
            modeId = snapshot.vis.mappedMode;
        }
        rx.insert(QStringLiteral("modeId"), modeId);
        rx.insert(QStringLiteral("generation"),
                  boundedCounter(snapshot.route.generation));
        rx.insert(QStringLiteral("queuedChunks"),
                  boundedCounter(snapshot.ingress.queue.queuedChunks));
        rx.insert(QStringLiteral("queuedSamples"),
                  boundedCounter(snapshot.ingress.queue.queuedSamples));
        rx.insert(QStringLiteral("droppedChunks"),
                  boundedCounter(snapshot.ingress.queue.droppedChunks));
        rx.insert(QStringLiteral("droppedSamples"),
                  boundedCounter(snapshot.ingress.queue.droppedSamples));
        rx.insert(QStringLiteral("samplesConverted"),
                  boundedCounter(snapshot.samplesConverted));
        rx.insert(QStringLiteral("samplesResampled"),
                  boundedCounter(snapshot.samplesResampled));
        rx.insert(QStringLiteral("frequencyObservations"),
                  boundedCounter(snapshot.frequencyObservations));
        rx.insert(QStringLiteral("staleChunksDiscarded"),
                  boundedCounter(snapshot.staleChunksDiscarded));
        rx.insert(QStringLiteral("processingFailures"),
                  boundedCounter(snapshot.processingFailures));
        rx.insert(QStringLiteral("producerRejectedCalls"),
                  boundedCounter(snapshot.producerRejectedCalls));
        rx.insert(QStringLiteral("coveragePermille"),
                  qRound(std::clamp(snapshot.image.coverage,
                                    0.0, 1.0) * 1'000.0));
        rx.insert(QStringLiteral("offsetHz"), snapshot.afc.correctionHz);
        rx.insert(QStringLiteral("slantPpm"),
                  snapshot.slant.appliedClockErrorPpm);
    }

    QVariantMap tx {
        {QStringLiteral("state"), sstvTxState()},
        {QStringLiteral("active"), sstvTxActive()},
        {QStringLiteral("pttActive"), sstvTxPttActive()},
        {QStringLiteral("progressPermille"),
         qRound(std::clamp(m_sstvTxProgress, 0.0, 1.0) * 1'000.0)},
        {QStringLiteral("underruns"), boundedCounter(m_sstvTxUnderruns)},
    };

    QVariantMap calibration {
        {QStringLiteral("available"), m_sstvTxCoordinator != nullptr},
        {QStringLiteral("completed"), false},
        {QStringLiteral("success"), false},
    };
    // Preserve the last real terminal tone result when a later non-calibration
    // TX replaces the coordinator's current mode. Availability and running are
    // always recomputed from the live bridge state.
    QVariantMap testTone = m_sstvDiagnosticsController->testToneResults();
    testTone.insert(QStringLiteral("available"),
                    m_sstvTxCoordinator
                        && sstvTxGlobalPreflightReady());
    testTone.insert(QStringLiteral("running"), false);
    if (!testTone.contains(QStringLiteral("success"))) {
        testTone.insert(QStringLiteral("success"), false);
    }
    testTone.insert(QStringLiteral("frequencyHz"), 1'500);
    testTone.insert(QStringLiteral("durationMs"), 2'000);
    if (m_sstvTxCoordinator) {
        const decodium::sstv::SstvTxCoordinatorSnapshot snapshot
            = m_sstvTxCoordinator->snapshot();
        const QString modeId = QString::fromStdString(
            snapshot.stateMachine.mode);
        tx.insert(QStringLiteral("modeId"), modeId);
        tx.insert(QStringLiteral("samplesProduced"),
                  boundedCounter(snapshot.stateMachine.encodedSamples));
        tx.insert(QStringLiteral("failures"),
                  boundedCounter(snapshot.stateMachine.sessionsFailed));

        constexpr auto calibrationPrefix = "calibration-";
        const bool isCalibration = modeId.startsWith(
            QString::fromLatin1(calibrationPrefix));
        const bool isTestTone = modeId
            == QStringLiteral("calibration-black-1500");
        const bool running = isCalibration
            && m_sstvTxCoordinator->stateMachine().active();
        const bool completed = isCalibration
            && snapshot.stateMachine.state
                == decodium::sstv::SstvTxState::Completed;
        int frequencyHz = 0;
        if (isCalibration) {
            const QString toneId = modeId.mid(
                static_cast<qsizetype>(
                    std::char_traits<char>::length(calibrationPrefix)));
            const auto toneKind
                = decodium::sstv::calibrationToneKindFromId(
                    toneId.toStdString());
            if (toneKind.has_value()) {
                frequencyHz = qRound(
                    decodium::sstv::calibrationToneSpec(
                        *toneKind).frequencyHz);
            }
        }
        calibration.insert(QStringLiteral("completed"),
                           isCalibration && !running);
        calibration.insert(QStringLiteral("success"), completed);
        calibration.insert(QStringLiteral("frequencyHz"), frequencyHz);
        calibration.insert(QStringLiteral("levelMilli"),
                           qRound(snapshot.headroom * 1'000.0));
        calibration.insert(QStringLiteral("durationMs"),
                           boundedCounter(
                               snapshot.stateMachine.encodedDurationMs));
        calibration.insert(QStringLiteral("errorCode"),
                           static_cast<int>(
                               snapshot.stateMachine.lastErrorCode));
        if (isTestTone) {
            testTone.insert(QStringLiteral("running"), running);
            testTone.insert(QStringLiteral("success"), completed);
            testTone.insert(QStringLiteral("levelMilli"),
                            qRound(snapshot.headroom * 1'000.0));
            testTone.insert(QStringLiteral("errorCode"),
                            static_cast<int>(
                                snapshot.stateMachine.lastErrorCode));
        }
    }

    QVariantMap storage {
        {QStringLiteral("ready"), m_sstvStorageReady},
    };
    if (m_sstvGalleryModel) {
        const QVariantMap quota = m_sstvGalleryModel->quotaSummary();
        for (const QString& key : {
                 QStringLiteral("recordCount"),
                 QStringLiteral("imageBytes"),
                 QStringLiteral("thumbnailBytes"),
                 QStringLiteral("rawAudioBytes")}) {
            if (quota.contains(key)) {
                storage.insert(key, quota.value(key));
            }
        }
        const QVariantMap retention
            = m_sstvGalleryModel->retentionSettings();
        quint64 quotaBytes = 0U;
        for (const QString& key : {
                 QStringLiteral("imageQuotaBytes"),
                 QStringLiteral("thumbnailQuotaBytes"),
                 QStringLiteral("rawAudioQuotaBytes")}) {
            quotaBytes = saturatedSum(
                quotaBytes, retention.value(key).toULongLong());
        }
        storage.insert(QStringLiteral("quotaBytes"),
                       boundedCounter(quotaBytes));
    }
    if (m_sstvStorageWorker) {
        const auto performance
            = m_sstvStorageWorker->performanceSnapshot();
        storage.insert(QStringLiteral("operations"),
                       boundedCounter(
                           performance.databaseOperationsCompleted));
        storage.insert(QStringLiteral("failures"),
                       boundedCounter(saturatedSum(
                           performance.databaseQueueFailures,
                           performance.imageSaveFailures)));
    }

    QVariantMap share {
        {QStringLiteral("enabled"),
         m_sstvShareController && m_sstvShareController->enabled()},
        {QStringLiteral("configured"),
         m_sstvShareController && m_sstvShareController->configured()},
    };
    if (m_sstvShareController) {
        const QVariantMap shareDiagnostics
            = m_sstvShareController->diagnostics();
        for (auto it = shareDiagnostics.cbegin();
             it != shareDiagnostics.cend(); ++it) {
            share.insert(it.key(), it.value());
        }
        share.insert(QStringLiteral("failures"),
                     m_sstvShareController->errorString().isEmpty() ? 0 : 1);
    }

    QVariantMap hamdrm {
        {QStringLiteral("enabled"), false},
        {QStringLiteral("available"), false},
        {QStringLiteral("state"), QStringLiteral("unavailable")},
    };
#if DECODIUM_HAS_HAMDRM
    if (m_hamDrmController) {
        hamdrm.insert(QStringLiteral("enabled"), true);
        hamdrm.insert(
            QStringLiteral("available"),
            m_hamDrmController->waveformRxAvailable()
                || m_hamDrmController->waveformTxAvailable());
        hamdrm.insert(
            QStringLiteral("state"),
            QStringLiteral("rx:%1 tx:%2")
                .arg(static_cast<int>(m_hamDrmController->rxState()))
                .arg(static_cast<int>(m_hamDrmController->txState())));
        hamdrm.insert(QStringLiteral("activeSessions"),
                      m_hamDrmController->busy() ? 1 : 0);
        hamdrm.insert(QStringLiteral("failures"),
                      m_hamDrmController->error().isEmpty() ? 0 : 1);
    }
#endif

    QString error;
    if (!m_sstvDiagnosticsController->setInputSnapshot({
            {QStringLiteral("capabilities"), capabilities},
            {QStringLiteral("settings"), settings},
            {QStringLiteral("rx"), rx},
            {QStringLiteral("tx"), tx},
            {QStringLiteral("storage"), storage},
            {QStringLiteral("share"), share},
            {QStringLiteral("hamdrm"), hamdrm},
            {QStringLiteral("calibration"), calibration},
            {QStringLiteral("testTone"), testTone},
        }, &error)) {
        decodium::sstv::recordSstvDiagnosticEvent(
            sstvSecurityLog(), QtWarningMsg,
            QStringLiteral("diagnostics.bridge-snapshot-rejected"),
            {{QStringLiteral("reasonCode"),
              QStringLiteral("invalid-scalar-snapshot")}});
    }
#endif
}

bool DecodiumBridge::sstvTxCanStart() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvStudioController
        && m_sstvStudioController->preparedReady()
        && !m_sstvStudioController->busy()
        && sstvTxGlobalPreflightReady();
#else
    return false;
#endif
}

QString DecodiumBridge::sstvTxPreflightRejection() const
{
    QStringList const blockers = sstvTxPreflightBlockers();
    if (blockers.isEmpty()) {
        // Il preflight e' passato fra il controllo e questo messaggio: raro,
        // ma dire "non pronto: nulla" sarebbe peggio che dirlo genericamente.
        return tr("SSTV TX preflight is not ready; stop any other TX and verify audio/CAT or VOX");
    }
    return tr("SSTV TX preflight is not ready: %1")
        .arg(blockers.join(QStringLiteral("; ")));
}

// 1.0.588: il preflight ha venticinque condizioni e ne riportava una sola,
// sempre la stessa: "non pronto, verifica audio/CAT o VOX". Chi la leggeva non
// poteva sapere quale delle venticinque fosse, e le piu' comuni - il TX di un
// altro modo ancora armato, il sink audio non ancora rilasciato - con audio,
// CAT e VOX non c'entrano nulla. Ora le condizioni stanno scritte una volta
// sola, ognuna col suo nome, e di qui vengono sia la risposta booleana sia il
// messaggio all'operatore.
QStringList DecodiumBridge::sstvTxPreflightBlockers() const
{
    QStringList blockers;
#if DECODIUM_HAS_SSTV
    auto const blocks = [&blockers](bool condition, char const* label) {
        if (condition) {
            blockers << QString::fromLatin1(label);
        }
    };

    blocks(!m_sstvTxCoordinator, "SSTV TX coordinator missing");
    blocks(!m_soundOutput, "audio output not initialised");
    blocks(m_sstvTxShuttingDown || m_shuttingDown
           || QCoreApplication::closingDown(), "application is shutting down");
    blocks(m_sstvOwnsBridgeTx, "an SSTV transmission already owns the TX path");
    blocks(m_transmitting, "another mode is transmitting");
    blocks(m_tuning, "the tune tone is active");
    blocks(m_txEnabled, "Enable TX is armed for another mode");
    blocks(m_autoCqRepeat, "Auto CQ is running");
    blocks(m_deferredManualSyncTx || m_txRequested,
           "a transmission is already requested");
    blocks(m_pttPending || m_pttConfirmed, "PTT is already engaged");
    blocks(m_bridgeAudioLegacyTxActive, "the legacy TX audio path is active");
    blocks(m_ft2LinkTxActive, "FT2-Link is transmitting");
    blocks(m_cwTxActive, "CW transmission is active");
    blocks(m_manualTxHold, "manual TX hold is engaged");
    blocks(m_bridgeAudioTuneActive || m_legacyBridgeAudioTxStartPending,
           "the TX audio path is starting up");
    blocks(m_txPlaybackReleasePending || m_txAudioRestartPending
           || m_txPlaybackHoldUntilMs != 0 || m_txPlaybackHardDeadlineMs != 0,
           "the previous transmission has not released the audio device yet");
    blocks(m_txAudioSink || m_txPcmBuffer,
           "the TX audio sink is still allocated");
    blocks(m_modulator && m_modulator->isActive(), "the modulator is running");
    blocks(!sstvTxUsesVoxPtt() && sstvTxPttActive(),
           "the radio is already keyed");
    blocks(m_decoPortUseRemote, "DecoPort is driving a remote radio");
    blocks(usingTciAudioInput(), "audio is coming from TCI");

    blocks(m_sstvTxOutputPinned
           && (m_sstvTxOutputDevice.isNull()
               || (m_sstvTxOutputChannels != 1U
                   && m_sstvTxOutputChannels != 2U)),
           "the pinned SSTV output device is no longer valid");

    if (m_sstvTxCoordinator) {
        const decodium::sstv::SstvTxCoordinatorSnapshot snapshot
            = m_sstvTxCoordinator->snapshot();
        blocks(snapshot.stateMachine.state
                   == decodium::sstv::SstvTxState::Disabled,
               "the SSTV TX state machine is disabled");
        blocks(snapshot.stateMachine.releaseRequired,
               "the previous SSTV transmission still needs releasing");
        blocks(snapshot.audioLeaseRetained,
               "the SSTV audio lease is still held");
    }

    blocks(!sstvTxUsesVoxPtt() && !sstvTxCanControlPtt()
               && !sstvTxAudioOnlyAllowed(),
           "no way to key the radio: no CAT PTT, no VOX");
#endif
    return blockers;
}

bool DecodiumBridge::sstvTxGlobalPreflightReady() const
{
#if DECODIUM_HAS_SSTV
    if (!sstvTxPreflightBlockers().isEmpty()) {
        return false;
    }
    // La cache dei dispositivi audio non entra nel preflight della UI: un
    // cambio di dispositivo la invalida di proposito, e il percorso di avvio
    // vero risolve l'uscita subito prima di fissarla. Pretenderla fresca qui
    // faceva dire "non pronto" a un instradamento VOX perfettamente valido
    // finche' non capitava un aggiornamento audio per altri motivi.
    return true;
#else
    return false;
#endif
}

bool DecodiumBridge::sstvTxAudioOnlyAllowed() const
{
#if DECODIUM_HAS_SSTV
    // A disconnected CAT/PTT path is safe for a local BlackHole/WAV test:
    // audio can still be rendered, but no radio can be keyed. Remote and TCI
    // routes remain excluded because they have their own ownership model.
    return !m_decoPortUseRemote
        && !usingTciAudioInput()
        && !sstvTxUsesVoxPtt()
        && !sstvTxCanControlPtt()
        && !sstvTxPttActive();
#else
    return false;
#endif
}

QVariantMap DecodiumBridge::sstvTxDiagnostics() const
{
    QVariantMap result;
#if DECODIUM_HAS_SSTV
    if (!m_sstvTxCoordinator) {
        return result;
    }
    const decodium::sstv::SstvTxCoordinatorSnapshot snapshot
        = m_sstvTxCoordinator->snapshot();
    const double headroomDb = snapshot.headroom > 0.0
        ? 20.0 * std::log10(snapshot.headroom) : 0.0;
    result.insert(QStringLiteral("peak"), snapshot.pcmPeak);
    result.insert(QStringLiteral("pcmPeak"), snapshot.pcmPeak);
    result.insert(QStringLiteral("headroom"), snapshot.headroom);
    result.insert(QStringLiteral("headroomDb"), headroomDb);
    result.insert(QStringLiteral("clippedFrames"),
                  static_cast<qulonglong>(snapshot.clippedFrames));
    result.insert(QStringLiteral("clipping"),
                  snapshot.clippedFrames != 0U);
    QStringList const blockers = sstvTxPreflightBlockers();
    result.insert(QStringLiteral("preflightReady"), blockers.isEmpty());
    result.insert(QStringLiteral("preflightBlockers"), blockers);
#endif
    return result;
}

bool DecodiumBridge::sstvTxActive() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvOwnsBridgeTx
        || (m_sstvTxCoordinator
            && m_sstvTxCoordinator->stateMachine().active());
#else
    return false;
#endif
}

QString DecodiumBridge::sstvTxState() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvTxState;
#else
    return QStringLiteral("Disabled");
#endif
}

double DecodiumBridge::sstvTxProgress() const noexcept
{
#if DECODIUM_HAS_SSTV
    return m_sstvTxProgress;
#else
    return 0.0;
#endif
}

QString DecodiumBridge::sstvTxError() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvTxError;
#else
    return {};
#endif
}

int DecodiumBridge::sstvPttLeadMs() const
{
    const double legacySeconds = getSetting(
        QStringLiteral("TxDelay"), 0.2).toDouble();
    const double safeLegacy = std::isfinite(legacySeconds)
        ? std::clamp(legacySeconds, 0.0, 5.0) : 0.2;
    const int fallback = static_cast<int>(std::llround(safeLegacy * 1'000.0));
    return std::clamp(getSetting(
        QStringLiteral("SSTV/PttLeadMs"), fallback).toInt(), 0, 5'000);
}

void DecodiumBridge::setSstvPttLeadMs(int value)
{
    const int bounded = std::clamp(value, 0, 5'000);
    if (bounded == sstvPttLeadMs()) {
        return;
    }
    if (sstvTxActive()) {
        emit statusMessage(tr("Stop SSTV TX before changing its timing"));
        return;
    }
    const int previous = sstvPttLeadMs();
    setSetting(QStringLiteral("SSTV/PttLeadMs"), bounded);
    if (!applySstvTxTimingSettings()) {
        setSetting(QStringLiteral("SSTV/PttLeadMs"), previous);
        emit statusMessage(tr("The SSTV TX timing was rejected"));
        return;
    }
    emit sstvTxTimingChanged();
}

int DecodiumBridge::sstvPttTailMs() const
{
    return std::clamp(getSetting(
        QStringLiteral("SSTV/PttTailMs"), 500).toInt(), 0, 5'000);
}

void DecodiumBridge::setSstvPttTailMs(int value)
{
    const int bounded = std::clamp(value, 0, 5'000);
    if (bounded == sstvPttTailMs()) {
        return;
    }
    if (sstvTxActive()) {
        emit statusMessage(tr("Stop SSTV TX before changing its timing"));
        return;
    }
    const int previous = sstvPttTailMs();
    setSetting(QStringLiteral("SSTV/PttTailMs"), bounded);
    if (!applySstvTxTimingSettings()) {
        setSetting(QStringLiteral("SSTV/PttTailMs"), previous);
        emit statusMessage(tr("The SSTV TX timing was rejected"));
        return;
    }
    emit sstvTxTimingChanged();
}

int DecodiumBridge::sstvPttReleaseRetryMs() const
{
    return std::clamp(getSetting(
        QStringLiteral("SSTV/PttReleaseRetryMs"), 500).toInt(),
        100, 2'000);
}

void DecodiumBridge::setSstvPttReleaseRetryMs(int value)
{
    const int bounded = std::clamp(value, 100, 2'000);
    if (bounded == sstvPttReleaseRetryMs()) {
        return;
    }
    if (sstvTxActive()) {
        emit statusMessage(tr("Stop SSTV TX before changing its timing"));
        return;
    }
    const int previous = sstvPttReleaseRetryMs();
    setSetting(QStringLiteral("SSTV/PttReleaseRetryMs"), bounded);
    if (!applySstvTxTimingSettings()) {
        setSetting(QStringLiteral("SSTV/PttReleaseRetryMs"), previous);
        emit statusMessage(tr("The SSTV TX timing was rejected"));
        return;
    }
    emit sstvTxTimingChanged();
}

int DecodiumBridge::sstvVoxPreKeyMs() const
{
    return std::clamp(getSetting(
        QStringLiteral("SSTV/VoxPreKeyMs"), 750).toInt(), 100, 4'000);
}

void DecodiumBridge::setSstvVoxPreKeyMs(int value)
{
    const int bounded = std::clamp(value, 100, 4'000);
    if (bounded == sstvVoxPreKeyMs()) {
        return;
    }
    if (sstvTxActive()) {
        emit statusMessage(tr("Stop SSTV TX before changing its timing"));
        return;
    }
    const int previous = sstvVoxPreKeyMs();
    setSetting(QStringLiteral("SSTV/VoxPreKeyMs"), bounded);
    if (!applySstvTxTimingSettings()) {
        setSetting(QStringLiteral("SSTV/VoxPreKeyMs"), previous);
        emit statusMessage(tr("The SSTV TX timing was rejected"));
        return;
    }
    emit sstvTxTimingChanged();
}

int DecodiumBridge::sstvVoxHangMs() const
{
    return std::clamp(getSetting(
        QStringLiteral("SSTV/VoxHangMs"), 500).toInt(), 100, 5'000);
}

void DecodiumBridge::setSstvVoxHangMs(int value)
{
    const int bounded = std::clamp(value, 100, 5'000);
    if (bounded == sstvVoxHangMs()) {
        return;
    }
    if (sstvTxActive()) {
        emit statusMessage(tr("Stop SSTV TX before changing its timing"));
        return;
    }
    const int previous = sstvVoxHangMs();
    setSetting(QStringLiteral("SSTV/VoxHangMs"), bounded);
    if (!applySstvTxTimingSettings()) {
        setSetting(QStringLiteral("SSTV/VoxHangMs"), previous);
        emit statusMessage(tr("The SSTV TX timing was rejected"));
        return;
    }
    emit sstvTxTimingChanged();
}

double DecodiumBridge::sstvVoxToneFrequencyHz() const
{
    const double value = getSetting(
        QStringLiteral("SSTV/VoxToneFrequencyHz"), 1'900.0).toDouble();
    return std::isfinite(value)
        ? std::clamp(value, 300.0, 3'000.0) : 1'900.0;
}

void DecodiumBridge::setSstvVoxToneFrequencyHz(double value)
{
    const double bounded = std::isfinite(value)
        ? std::clamp(value, 300.0, 3'000.0) : 1'900.0;
    if (qFuzzyCompare(bounded + 1.0, sstvVoxToneFrequencyHz() + 1.0)) {
        return;
    }
    if (sstvTxActive()) {
        emit statusMessage(tr("Stop SSTV TX before changing its timing"));
        return;
    }
    const double previous = sstvVoxToneFrequencyHz();
    setSetting(QStringLiteral("SSTV/VoxToneFrequencyHz"), bounded);
    if (!applySstvTxTimingSettings()) {
        setSetting(QStringLiteral("SSTV/VoxToneFrequencyHz"), previous);
        emit statusMessage(tr("The SSTV TX timing was rejected"));
        return;
    }
    emit sstvTxTimingChanged();
}

double DecodiumBridge::sstvVoxToneLevel() const
{
    const double value = getSetting(
        QStringLiteral("SSTV/VoxToneLevel"), 0.5).toDouble();
    return std::isfinite(value)
        ? std::clamp(value, 0.05, 1.0) : 0.5;
}

void DecodiumBridge::setSstvVoxToneLevel(double value)
{
    const double bounded = std::isfinite(value)
        ? std::clamp(value, 0.05, 1.0) : 0.5;
    if (qFuzzyCompare(bounded + 1.0, sstvVoxToneLevel() + 1.0)) {
        return;
    }
    if (sstvTxActive()) {
        emit statusMessage(tr("Stop SSTV TX before changing its timing"));
        return;
    }
    const double previous = sstvVoxToneLevel();
    setSetting(QStringLiteral("SSTV/VoxToneLevel"), bounded);
    if (!applySstvTxTimingSettings()) {
        setSetting(QStringLiteral("SSTV/VoxToneLevel"), previous);
        emit statusMessage(tr("The SSTV TX timing was rejected"));
        return;
    }
    emit sstvTxTimingChanged();
}

bool DecodiumBridge::applySstvTxTimingSettings()
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvTxCoordinator) {
        return true;
    }
    decodium::sstv::SstvTxTimingConfig timing;
    timing.pttLeadDelayMs = static_cast<std::uint64_t>(sstvPttLeadMs());
    timing.pttTailDelayMs = static_cast<std::uint64_t>(sstvPttTailMs());
    timing.pttReleaseRetryMs = static_cast<std::uint64_t>(
        sstvPttReleaseRetryMs());
    timing.voxPreKeyMs = static_cast<std::uint64_t>(sstvVoxPreKeyMs());
    timing.voxHangMs = static_cast<std::uint64_t>(sstvVoxHangMs());
    timing.voxToneFrequencyHz = sstvVoxToneFrequencyHz();
    timing.voxToneLevel = sstvVoxToneLevel();
    return m_sstvTxCoordinator->updateTimingConfig(timing);
#else
    return true;
#endif
}

#if DECODIUM_HAS_SSTV
void DecodiumBridge::setSstvThumbnailProvider(
    decodium::sstv::SstvThumbnailProvider* provider)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_sstvThumbnailProvider == provider) {
        return;
    }
    if (m_sstvGalleryModel) {
        m_sstvGalleryModel->setThumbnailProvider(nullptr);
    }
    m_sstvThumbnailProvider = provider;
    if (m_sstvGalleryModel) {
        m_sstvGalleryModel->setThumbnailProvider(provider);
    }
}
#endif

std::shared_ptr<const QImage>
DecodiumBridge::sstvTxSourceImageSnapshot() const noexcept
{
#if DECODIUM_HAS_SSTV
    return m_sstvStudioController
        ? m_sstvStudioController->sourceSnapshot()
        : std::shared_ptr<const QImage> {};
#else
    return {};
#endif
}

std::shared_ptr<const QImage>
DecodiumBridge::sstvTxPreparedImageSnapshot() const noexcept
{
#if DECODIUM_HAS_SSTV
    return m_sstvStudioController
        ? m_sstvStudioController->preparedSnapshot()
        : std::shared_ptr<const QImage> {};
#else
    return {};
#endif
}

std::uint64_t DecodiumBridge::sstvTxNowMs() const noexcept
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvTxClock.isValid()) {
        return 0U;
    }
    const qint64 elapsed = m_sstvTxClock.elapsed();
    return elapsed <= 0 ? 0U : static_cast<std::uint64_t>(elapsed);
#else
    return 0U;
#endif
}

void DecodiumBridge::initialiseSstvTx()
{
#if DECODIUM_HAS_SSTV
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_sstvTxCoordinator || m_sstvTxShuttingDown) {
        return;
    }
    if (!m_soundOutput) {
        m_sstvTxState = tr("Unavailable");
        m_sstvTxError = tr("Decodium TX audio output is unavailable");
        emit sstvTxStateChanged();
        refreshSstvDiagnosticsSnapshot();
        return;
    }

    decodium::sstv::SstvTxCoordinatorConfig config;
    config.sampleRate = 48'000U;
    config.pttLeadDelayMs = static_cast<std::uint64_t>(sstvPttLeadMs());
    config.pttTailDelayMs = static_cast<std::uint64_t>(sstvPttTailMs());
    config.pttReleaseRetryMs = static_cast<std::uint64_t>(
        sstvPttReleaseRetryMs());
    config.voxPreKeyMs = static_cast<std::uint64_t>(sstvVoxPreKeyMs());
    config.voxHangMs = static_cast<std::uint64_t>(sstvVoxHangMs());
    config.voxToneFrequencyHz = sstvVoxToneFrequencyHz();
    config.voxToneLevel = sstvVoxToneLevel();
    config.voxEnvelopeEnabled = !qEnvironmentVariableIsSet(
        "DECODIUM_LAB_SSTV_SILENT_VOX");

    decodium::sstv::SstvTxCoordinatorHooks hooks;
    hooks.queryPreflight = [this]() {
        decodium::sstv::SstvTxCoordinatorPreflight result;
        const bool vox = sstvTxUsesVoxPtt();
        const bool localAudioRoute = !m_decoPortUseRemote
            && !usingTciAudioInput();
        const bool audioOnly = sstvTxAudioOnlyAllowed();
        result.audioOutputReady = m_soundOutput && localAudioRoute
            && m_sstvTxOutputPinned
            && !m_sstvTxOutputDevice.isNull()
            && (m_sstvTxOutputChannels == 1U
                || m_sstvTxOutputChannels == 2U);
        result.pttPathReady = vox || sstvTxCanControlPtt() || audioOnly;
        result.weakSignalSequencerActive = m_txEnabled || m_autoCqRepeat
            || m_deferredManualSyncTx || m_txRequested || m_pttPending
            || m_pttConfirmed;
        result.transmitAlreadyActive = m_sstvOwnsBridgeTx
            || m_transmitting || m_tuning || m_bridgeAudioLegacyTxActive
            || m_ft2LinkTxActive || m_cwTxActive || m_manualTxHold
            || m_bridgeAudioTuneActive
            || m_legacyBridgeAudioTxStartPending
            || m_txPlaybackReleasePending || m_txAudioRestartPending
            || m_txPlaybackHoldUntilMs != 0
            || m_txPlaybackHardDeadlineMs != 0
            || m_txAudioSink || m_txPcmBuffer
            || (m_modulator && m_modulator->isActive())
            || (!vox && sstvTxPttActive())
            || m_shuttingDown || QCoreApplication::closingDown();
        result.pttReleaseRequired = !vox && !audioOnly;
        if (!localAudioRoute) {
            result.detail =
                "SSTV TX currently requires Decodium's local SoundOutput route";
        } else if (!result.audioOutputReady) {
            // 1.0.588: "audio output is unavailable" e' la congiunzione di
            // quattro cose diverse, e l'operatore che la legge non puo' sapere
            // quale manchi: il dispositivo scollegato si riconnette, un
            // dispositivo mai fissato e' un difetto di sequenza nostro, e un
            // numero di canali fuori da 1/2 e' un driver che ha risposto male.
            // Dirlo qui costa una riga e risparmia un'indagine.
            if (!m_soundOutput) {
                result.detail =
                    "Decodium TX audio output is unavailable: no SoundOutput";
            } else if (!m_sstvTxOutputPinned) {
                result.detail =
                    "Decodium TX audio output is unavailable: the output device was not pinned for this session";
            } else if (m_sstvTxOutputDevice.isNull()) {
                result.detail =
                    "Decodium TX audio output is unavailable: the pinned output device is gone";
            } else {
                result.detail =
                    "Decodium TX audio output is unavailable: unsupported channel count "
                    + std::to_string(m_sstvTxOutputChannels);
            }
        } else if (!result.pttPathReady) {
            result.detail = "Decodium CAT/PTT or VOX is not ready";
        } else if (result.weakSignalSequencerActive) {
            result.detail = "the weak-signal sequencer is armed or pending";
        } else if (result.transmitAlreadyActive) {
            result.detail = "another Decodium transmission is active";
        }
        return result;
    };
    hooks.requestPttOn = [this](std::uint64_t sessionId) {
        if (m_sstvTxShuttingDown || m_shuttingDown
            || QCoreApplication::closingDown() || m_sstvOwnsBridgeTx
            || m_transmitting || m_tuning || m_txEnabled
            || m_autoCqRepeat || m_txRequested || m_pttPending
            || !m_sstvTxOutputPinned
            || m_sstvTxOutputDevice.isNull()) {
            return false;
        }
        const bool vox = sstvTxUsesVoxPtt();
        const bool audioOnly = sstvTxAudioOnlyAllowed();
        if (!vox && !sstvTxCanControlPtt() && !audioOnly) {
            return false;
        }
        if (!vox && sstvTxPttActive()) {
            return false;
        }

        // Invalidate every callback token owned by the weak-signal audio
        // sequencer before claiming the shared SoundOutput/m_transmitting
        // resources.  Entry guards in its cleanup paths then make any
        // already-queued callback a no-op for the SSTV session.
        ++m_txPlaybackSerial;
        ++m_txAudioTelemetrySerial;
        m_txPlaybackReleasePending = false;
        m_txPlaybackHoldUntilMs = 0;
        m_txPlaybackHardDeadlineMs = 0;
        m_txAudioRestartPending = false;
        clearLegacyPttTransition(QStringLiteral("sstv-tx-start"));

        m_sstvOwnsBridgeTx = true;
        m_sstvTxSessionId = static_cast<quint64>(sessionId);
        m_transmitting = true;
        emit transmittingChanged();
        applyTxAudioSchedulingBoost(QStringLiteral("sstv-tx"));
        suspendNonAudioTxWork(QStringLiteral("sstv-tx"));
        if (m_soundInput) {
            pauseRxAudioForTx(QStringLiteral("SSTV TX"));
        }
        emit sstvTxStateChanged();

        if (vox) {
            QTimer::singleShot(0, this, [this, sessionId]() {
                if (m_sstvTxCoordinator
                    && m_sstvTxSessionId == sessionId) {
                    m_sstvTxCoordinator->notifyPttConfirmed(
                        sstvTxNowMs(), sessionId);
                }
            });
        } else if (sstvTxCanControlPtt()) {
            setSstvTxPtt(true);
        } else {
            QTimer::singleShot(0, this, [this, sessionId]() {
                if (m_sstvTxCoordinator
                    && m_sstvTxSessionId == sessionId) {
                    m_sstvTxCoordinator->notifyPttConfirmed(
                        sstvTxNowMs(), sessionId);
                }
            });
        }
        return true;
    };
    hooks.requestPttOff = [this](std::uint64_t sessionId) {
        if (!sstvTxUsesVoxPtt() && sstvTxCanControlPtt()) {
            setSstvTxPtt(false);
        }
        if (!sstvTxPttActive()) {
            QTimer::singleShot(0, this, [this, sessionId]() {
                if (m_sstvTxCoordinator
                    && m_sstvTxSessionId == sessionId) {
                    m_sstvTxCoordinator->notifyPttReleased(
                        sstvTxNowMs(), sessionId);
                }
            });
        }
        return true;
    };
    hooks.startAudio = [this](
        const std::shared_ptr<decodium::sstv::SstvTxAudioDevice>& device,
        const decodium::sstv::SstvTxAudioPlan& plan) {
        if (!device || !m_soundOutput || !m_sstvOwnsBridgeTx
            || plan.sessionId != m_sstvTxSessionId
            || plan.sampleRate != 48'000U
            || !m_sstvTxOutputPinned
            || m_sstvTxOutputDevice.isNull()) {
            return false;
        }
        const unsigned channels = m_sstvTxOutputChannels;
        if (channels != plan.channelCount
            || channels != device->channelCount()) {
            return false;
        }
        m_soundOutput->setFormat(m_sstvTxOutputDevice, channels,
                                 kSstvOutputBufferFrames);
        m_sstvTxAudioDevice = device.get();
        const bool started = m_soundOutput->restartTrackedPlayback(
            device.get(), static_cast<quint64>(plan.sessionId),
            static_cast<quint64>(plan.totalFrames));
        if (!started) {
            return false;
        }
        emit statusMessage(tr("SSTV TX audio started: %1")
                               .arg(QString::fromStdString(plan.mode)));
        return true;
    };
    hooks.detachAudio = [this](
        const std::shared_ptr<decodium::sstv::SstvTxAudioDevice>& device,
        decodium::sstv::SstvTxAudioDetachReason reason) {
        if (!m_soundOutput || !device
            || (m_sstvTxAudioDevice
                && m_sstvTxAudioDevice.data() != device.get())) {
            return false;
        }
        if (reason == decodium::sstv::SstvTxAudioDetachReason::Completed) {
            if (!m_soundOutput->finishTrackedPlayback(m_sstvTxSessionId)) {
                return false;
            }
        } else {
            if (!m_soundOutput->stopTrackedPlayback(m_sstvTxSessionId)) {
                // startAudio is conservative: a hook can fail before tracked
                // playback is armed but after the coordinator marks the raw
                // device as possibly attached. Stop the owner path explicitly
                // so the shared lease can still be released synchronously.
                m_soundOutput->stop();
            }
        }
        m_sstvTxAudioDevice = nullptr;
        return true;
    };
    hooks.stateChanged = [this](
        const decodium::sstv::SstvTxCoordinatorSnapshot& snapshot) {
#if DECODIUM_HAS_HAMDRM
        if (m_hamDrmTxBackend) {
            m_hamDrmTxBackend->coordinatorStateChanged(snapshot);
        }
#endif
        const QString state = QString::fromLatin1(
            decodium::sstv::SstvTxStateMachine::stateName(
                snapshot.stateMachine.state));
        QString error;
        if (!snapshot.lastOperationDetail.empty()) {
            error = QString::fromStdString(snapshot.lastOperationDetail);
        } else if (!snapshot.stateMachine.lastErrorDetail.empty()) {
            error = QString::fromStdString(
                snapshot.stateMachine.lastErrorDetail);
        }
        const double progress = std::clamp(snapshot.progress, 0.0, 1.0);
        const bool diagnosticLifecycleChanged
            = state != m_sstvTxState || error != m_sstvTxError;
        const bool changed = state != m_sstvTxState
            || error != m_sstvTxError
            || !qFuzzyCompare(progress + 1.0, m_sstvTxProgress + 1.0);
        m_sstvTxState = state;
        m_sstvTxError = error;
        m_sstvTxProgress = progress;
        m_sstvTxSessionId = static_cast<quint64>(
            snapshot.stateMachine.currentSessionId);
        // 1.0.588: non smontare la rotta audio mentre ne stiamo montando una
        // nuova. publishState() notifica a ogni passaggio, anche quando lo
        // stato e' ancora quello TERMINALE della sessione precedente, e start()
        // /startPrepared() pubblicano prima di arrivare al proprio preflight:
        // il rilascio cancellava il dispositivo appena fissato e il preflight
        // rifiutava con "the output device was not pinned for this session".
        // Effetto per l'operatore: la prima trasmissione parte, tutte le
        // successive no, finche' non si riavvia il programma.
        if (!m_sstvTxStartInProgress
            && terminalSstvTxState(snapshot.stateMachine.state)
            && !snapshot.audioLeaseRetained
            && (!snapshot.stateMachine.releaseRequired
                || snapshot.pttReleased)) {
            releaseSstvTxBridgeOwnership();
        }
        if (changed) {
            emit sstvTxStateChanged();
        }
        if (diagnosticLifecycleChanged && m_sstvDiagnosticsController
            && m_sstvDiagnosticsController->ready()) {
            // Do not rebuild the complete diagnostic snapshot for every
            // 20 ms playback-progress update. Lifecycle transitions remain
            // immediate on the next event-loop turn and terminal state cannot
            // leave the test-tone UI stuck in its running state.
            QTimer::singleShot(
                0, this,
                [this]() { refreshSstvDiagnosticsSnapshot(); });
        }
    };

    try {
        m_sstvTxClock.start();
        m_sstvTxCoordinator
            = std::make_unique<decodium::sstv::SstvTxCoordinator>(
                config, std::move(hooks));
        const decodium::sstv::SstvTxCoordinatorResult enabled
            = m_sstvTxCoordinator->enable(sstvTxNowMs());
        if (!enabled.accepted) {
            throw std::runtime_error(enabled.detail.empty()
                                         ? "SSTV TX enable failed"
                                         : enabled.detail);
        }
    } catch (const std::exception& exception) {
        m_sstvTxCoordinator.reset();
        m_sstvTxState = tr("Unavailable");
        m_sstvTxError = tr("Cannot initialise SSTV TX: %1")
                            .arg(QString::fromUtf8(exception.what()));
        emit sstvTxStateChanged();
        refreshSstvDiagnosticsSnapshot();
        return;
    }

    m_sstvTxTimer = new QTimer(this);
    m_sstvTxTimer->setObjectName(QStringLiteral("SSTV TX coordinator"));
    m_sstvTxTimer->setTimerType(Qt::PreciseTimer);
    m_sstvTxTimer->setInterval(20);
    connect(m_sstvTxTimer, &QTimer::timeout,
            this, &DecodiumBridge::tickSstvTx);
    connect(m_soundOutput, &SoundOutput::playbackError, this,
            [this](quint64 sessionId, const QString& detail) {
        if (!m_sstvTxCoordinator
            || m_sstvTxSessionId != sessionId) {
            return;
        }
        m_sstvTxCoordinator->notifyAudioError(
            sstvTxNowMs(), sessionId, detail.toStdString());
    }, Qt::QueuedConnection);
    connect(m_soundOutput, &SoundOutput::playbackUnderrun, this,
            [this](quint64 sessionId, const QString& detail) {
        if (!m_sstvTxCoordinator
            || m_sstvTxSessionId != sessionId) {
            return;
        }
        if (m_sstvTxUnderruns
                < std::numeric_limits<quint64>::max()) {
            ++m_sstvTxUnderruns;
        }
        m_sstvTxCoordinator->notifyAudioUnderrun(
            sstvTxNowMs(), sessionId, detail.toStdString());
    }, Qt::QueuedConnection);
    m_sstvTxTimer->start();
    emit sstvTxStateChanged();
    refreshSstvDiagnosticsSnapshot();
#endif
}

#if DECODIUM_HAS_SSTV && DECODIUM_HAS_HAMDRM
void DecodiumBridge::queueHamDrmTransmittedImage(
    QImage image,
    QString profileId,
    int occupiedBandwidthHz)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const QPointer<decodium::sstv::SstvStorageWorker> worker
        = m_sstvStorageWorker;
    if (!m_sstvStorageReady || !worker || !worker->isInitialized()
        || image.isNull() || profileId.trimmed().isEmpty()) {
        emit statusMessage(tr(
            "HAMDRM TX was accepted, but Gallery storage is not ready; "
            "the transmitted image was not archived"));
        return;
    }

    decodium::sstv::SstvTxGalleryArchiveContext context;
    context.eventAtUtc = QDateTime::currentDateTimeUtc();
    context.mode = profileId.trimmed().left(64);
    context.localCallsign = m_callsign.trimmed().left(64);
    context.localGrid = m_grid.trimmed().left(16);
    context.source = QStringLiteral("hamdrm");
    context.digital = true;
    context.note = QStringLiteral("HAMDRM transmission accepted");
    context.frequencyHz = std::isfinite(m_frequency)
            && m_frequency > 0.0
            && m_frequency
                <= static_cast<double>(std::numeric_limits<qint64>::max())
        ? static_cast<qint64>(std::llround(m_frequency)) : 0;
    // The HAMDRM profile identifies occupied bandwidth but not one truthful
    // single-tone audio offset, so retain zero rather than Decodium's unrelated
    // weak-signal TX offset.
    context.audioFrequencyHz = 0;
    context.qualityMetrics = {
        {QStringLiteral("occupiedBandwidthHz"),
         static_cast<double>(std::max(0, occupiedBandwidthHz))},
        {QStringLiteral("txAccepted"), 1.0},
    };
    context.fileNameTemplate = getSetting(
        QStringLiteral("SSTV/ImageNamingTemplate"),
        QStringLiteral("{date}_{time}_{mode}_{remoteCall}_{id}"))
                                   .toString()
                                   .trimmed();
    const auto archive = decodium::sstv::makeSstvTxGalleryArchiveRequest(
        image, decodium::sstv::SstvImageCategory::Transmitted, context);
    if (!archive.has_value()) {
        emit errorMessage(tr("The accepted HAMDRM TX image could not be archived"));
        return;
    }

    if (m_sstvStorageRequestId == 0U) {
        m_sstvStorageRequestId = 1U;
    }
    const quint64 requestId = m_sstvStorageRequestId++;
    const auto archiveImage = std::make_shared<const QImage>(
        std::move(image));
    decodium::sstv::SstvImageSaveRequest request = *archive;
    if (!worker->enqueueDatabaseOperation(
            [archiveImage, request = std::move(request), requestId](
                decodium::sstv::SstvStorageWorker& storage) mutable {
                request.image = *archiveImage;
                storage.storeAndInsertImage(std::move(request), requestId);
            })) {
        emit errorMessage(tr("Could not queue the accepted HAMDRM TX image for Gallery storage"));
        return;
    }
    m_sstvTxGallerySaveRequests.insert(requestId,
                                       QStringLiteral("transmitted"));
}

decodium::sstv::SstvTxCoordinatorResult
DecodiumBridge::startHamDrmPreparedAudio(
    std::unique_ptr<decodium::sstv::SstvPcm16Source> source,
    std::string mode)
{
    Q_ASSERT(QThread::currentThread() == thread());
    using decodium::sstv::SstvTxCoordinatorResult;
    using decodium::sstv::SstvTxErrorCode;
    auto reject = [](SstvTxErrorCode error, std::string detail) {
        return SstvTxCoordinatorResult {false, error, std::move(detail), 0U};
    };
    if (!source || source->sampleRate() != 48'000U
        || source->totalSamples() == 0U) {
        return reject(SstvTxErrorCode::EncodingFailure,
                      "HAMDRM prepared PCM source is invalid");
    }
    if (!m_sstvTxCoordinator) {
        initialiseSstvTx();
    }
    if (!m_sstvTxCoordinator || m_sstvTxShuttingDown
        || m_shuttingDown || QCoreApplication::closingDown()) {
        return reject(SstvTxErrorCode::Shutdown,
                      "Decodium SSTV TX authority is unavailable");
    }
    if (!checkSwrAllowsTransmission(QStringLiteral("hamdrm-tx"))) {
        return reject(SstvTxErrorCode::TxNotPermitted,
                      "HAMDRM TX was blocked by the Decodium SWR safety check");
    }

    bool requestedDeviceFound = false;
    const QAudioDevice output = resolveTxOutputDevice(
        &requestedDeviceFound);
    if (!sstvOutputMatchesConfiguredSelection(
            m_audioOutputDevice, m_audioOutputDeviceId,
            requestedDeviceFound, output)) {
        return reject(
            SstvTxErrorCode::AudioDeviceLoss,
            "the configured Decodium TX audio output is unavailable");
    }
    const unsigned channels = output.preferredFormat().channelCount() <= 1
        ? 1U : 2U;

    decodium::sstv::SstvTxPreparedAudioRequest request;
    request.headerEndFrame = 0U;
    request.imageEndFrame = source->totalSamples();
    request.mode = std::move(mode);
    request.source = std::move(source);
    request.channelCount = channels;
    if (channels == 1U || m_audioOutputChannel == 0
        || m_audioOutputChannel == 3) {
        request.channelRoute = decodium::sstv::SstvTxChannelRoute::Both;
    } else if (m_audioOutputChannel == 1) {
        request.channelRoute = decodium::sstv::SstvTxChannelRoute::Left;
    } else {
        request.channelRoute = decodium::sstv::SstvTxChannelRoute::Right;
    }

    // 1.0.588: come per SSTV analogico, la sessione precedente va fatta
    // scadere prima del pin: il tick interno di startPrepared() rilascerebbe
    // il route appena fissato.
    if (m_sstvTxCoordinator) {
        static_cast<void>(m_sstvTxCoordinator->tick(sstvTxNowMs()));
    }

    // Pin the exact route before the coordinator runs its shared preflight.
    // The same release barrier used by analog SSTV clears this lease.
    m_sstvTxOutputDevice = output;
    m_sstvTxOutputChannels = channels;
    m_sstvTxOutputPinned = true;
    m_sstvTxError.clear();
    m_sstvTxProgress = 0.0;
    m_sstvTxStartInProgress = true;
    const SstvTxCoordinatorResult result =
        m_sstvTxCoordinator->startPrepared(
            sstvTxNowMs(), std::move(request));
    m_sstvTxStartInProgress = false;
    if (!result.accepted) {
        m_sstvTxOutputDevice = QAudioDevice {};
        m_sstvTxOutputChannels = 0U;
        m_sstvTxOutputPinned = false;
        return result;
    }
    m_sstvTxSessionId = static_cast<quint64>(result.sessionId);
    emit sstvTxStateChanged();
    emit statusMessage(tr("HAMDRM TX accepted by Decodium audio/PTT authority"));
    return result;
}

bool DecodiumBridge::cancelHamDrmPreparedAudio(
    std::uint64_t coordinatorSessionId)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (!m_sstvTxCoordinator || coordinatorSessionId == 0U) {
        return false;
    }
    const auto snapshot = m_sstvTxCoordinator->snapshot();
    if (snapshot.stateMachine.currentSessionId != coordinatorSessionId
        || !m_sstvTxCoordinator->stateMachine().active()) {
        return false;
    }
    return m_sstvTxCoordinator->cancel(sstvTxNowMs());
}
#endif

bool DecodiumBridge::startSstvCalibrationTone(const QString& toneId)
{
#if DECODIUM_HAS_SSTV
    Q_ASSERT(QThread::currentThread() == thread());
    if (!m_sstvTxCoordinator) {
        initialiseSstvTx();
    }
    auto reject = [this](const QString& detail) {
        m_sstvTxError = detail;
        emit sstvTxStateChanged();
        emit errorMessage(detail);
        return false;
    };

    const std::string requestedId = toneId.trimmed().toStdString();
    const std::optional<decodium::sstv::SstvCalibrationToneKind> kind
        = decodium::sstv::calibrationToneKindFromId(requestedId);
    if (!kind.has_value()) {
        return reject(tr("The selected SSTV calibration reference is invalid"));
    }
    if (!m_sstvTxCoordinator) {
        return reject(tr("Native SSTV TX is unavailable"));
    }
    if (!sstvTxGlobalPreflightReady()) {
        return reject(sstvTxPreflightRejection());
    }
    if (!checkSwrAllowsTransmission(QStringLiteral("sstv-tx"))) {
        return reject(tr("SSTV TX was blocked by the Decodium SWR safety check"));
    }

    std::unique_ptr<decodium::sstv::SstvPcm16Source> source;
    try {
        source = decodium::sstv::makeCalibrationTonePcm16Source(
            *kind, 48'000U, 2'000U);
    } catch (const std::exception& exception) {
        return reject(tr("Cannot prepare the SSTV calibration reference: %1")
                          .arg(QString::fromUtf8(exception.what())));
    }
    if (!source || source->sampleRate() != 48'000U
        || source->totalSamples() == 0U) {
        return reject(tr("The SSTV calibration audio source is invalid"));
    }

    bool requestedDeviceFound = false;
    const QAudioDevice output = resolveTxOutputDevice(
        &requestedDeviceFound);
    if (!sstvOutputMatchesConfiguredSelection(
            m_audioOutputDevice, m_audioOutputDeviceId,
            requestedDeviceFound, output)) {
        return reject(tr("The configured Decodium TX audio output is unavailable; reconnect it or select an available output before SSTV TX"));
    }
    const unsigned channels
        = output.preferredFormat().channelCount() <= 1 ? 1U : 2U;
    const decodium::sstv::SstvCalibrationToneSpec& spec
        = decodium::sstv::calibrationToneSpec(*kind);

    decodium::sstv::SstvTxPreparedAudioRequest request;
    request.mode = std::string("calibration-") + spec.id;
    request.width = 1U;
    request.height = 1U;
    request.channelCount = channels;
    if (channels == 1U || m_audioOutputChannel == 0
        || m_audioOutputChannel == 3) {
        request.channelRoute = decodium::sstv::SstvTxChannelRoute::Both;
    } else if (m_audioOutputChannel == 1) {
        request.channelRoute = decodium::sstv::SstvTxChannelRoute::Left;
    } else {
        request.channelRoute = decodium::sstv::SstvTxChannelRoute::Right;
    }
    request.headerEndFrame = 0U;
    request.imageEndFrame = source->totalSamples();
    request.headroom = decodium::sstv::kDefaultSstvTxHeadroom;
    request.source = std::move(source);

    // 1.0.588: far scadere QUI la sessione precedente, prima di fissare il
    // dispositivo. start()/startPrepared() cominciano con un tick del
    // coordinatore: se la sessione di prima e' in uno stato terminale, quel
    // tick emette stateChanged, il nostro gancio chiama
    // releaseSstvTxBridgeOwnership() e cancella il pin appena messo. Il
    // preflight che segue trovava il dispositivo non fissato e rifiutava, cosi'
    // la prima trasmissione riusciva e ogni successiva no.
    if (m_sstvTxCoordinator) {
        static_cast<void>(m_sstvTxCoordinator->tick(sstvTxNowMs()));
    }

    // Resolve and pin exactly once before the coordinator's shared preflight,
    // just like an image TX.  The normal release barrier clears this route.
    m_sstvTxOutputDevice = output;
    m_sstvTxOutputChannels = channels;
    m_sstvTxOutputPinned = true;
    m_sstvTxError.clear();
    m_sstvTxProgress = 0.0;
    const bool audioOnly = sstvTxAudioOnlyAllowed();
    m_sstvTxStartInProgress = true;
    const decodium::sstv::SstvTxCoordinatorResult result
        = m_sstvTxCoordinator->startPrepared(
            sstvTxNowMs(), std::move(request));
    m_sstvTxStartInProgress = false;
    if (!result.accepted) {
        m_sstvTxOutputDevice = QAudioDevice {};
        m_sstvTxOutputChannels = 0U;
        m_sstvTxOutputPinned = false;
        const QString detail = result.detail.empty()
            ? tr("SSTV TX preflight failed")
            : QString::fromStdString(result.detail);
        return reject(detail);
    }
    m_sstvTxSessionId = static_cast<quint64>(result.sessionId);
    emit sstvTxStateChanged();
    emit statusMessage(
        audioOnly
            ? tr("SSTV audio-only calibration accepted: %1")
                  .arg(QString::fromLatin1(spec.id))
            : tr("SSTV calibration reference accepted: %1")
                  .arg(QString::fromLatin1(spec.id)));
    return true;
#else
    Q_UNUSED(toneId)
    emit errorMessage(tr("This Decodium build has no native SSTV support"));
    return false;
#endif
}

bool DecodiumBridge::startSstvTx(const QString& fskId)
{
#if DECODIUM_HAS_SSTV
    Q_ASSERT(QThread::currentThread() == thread());
    if (!m_sstvTxCoordinator) {
        initialiseSstvTx();
    }
    auto reject = [this](const QString& detail) {
        m_sstvTxError = detail;
        emit sstvTxStateChanged();
        emit errorMessage(detail);
        return false;
    };
    if (!m_sstvTxCoordinator || !m_sstvStudioController) {
        return reject(tr("Native SSTV TX is unavailable"));
    }
    const std::shared_ptr<const QImage> prepared
        = m_sstvStudioController->preparedSnapshot();
    if (!prepared || prepared->isNull()) {
        return reject(tr("Prepare an SSTV image before transmitting"));
    }
    if (!sstvTxCanStart()) {
        return reject(sstvTxPreflightRejection());
    }
    if (!checkSwrAllowsTransmission(QStringLiteral("sstv-tx"))) {
        return reject(tr("SSTV TX was blocked by the Decodium SWR safety check"));
    }

    const std::string modeId
        = m_sstvStudioController->modeId().toStdString();
    const std::optional<decodium::sstv::SstvTxCoordinatorMode> mode
        = decodium::sstv::SstvTxSourceBuilder::modeFromId(modeId);
    if (!mode.has_value()) {
        return reject(tr("The selected SSTV mode is not executable"));
    }

    bool requestedDeviceFound = false;
    const QAudioDevice output = resolveTxOutputDevice(
        &requestedDeviceFound);
    if (!sstvOutputMatchesConfiguredSelection(
            m_audioOutputDevice, m_audioOutputDeviceId,
            requestedDeviceFound, output)) {
        return reject(tr("The configured Decodium TX audio output is unavailable; reconnect it or select an available output before SSTV TX"));
    }
    const unsigned channels
        = output.preferredFormat().channelCount() <= 1 ? 1U : 2U;

    decodium::sstv::SstvTxCoordinatorRequest request;
    request.mode = *mode;
    request.channelCount = channels;
    if (channels == 1U || m_audioOutputChannel == 0
        || m_audioOutputChannel == 3) {
        request.channelRoute = decodium::sstv::SstvTxChannelRoute::Both;
    } else if (m_audioOutputChannel == 1) {
        request.channelRoute = decodium::sstv::SstvTxChannelRoute::Left;
    } else {
        request.channelRoute = decodium::sstv::SstvTxChannelRoute::Right;
    }
    try {
        request.pixels
            = decodium::sstv::SstvTxSourceBuilder::pixelsFromImage(
                *prepared, *mode);
    } catch (const std::exception& exception) {
        return reject(tr("Cannot prepare SSTV TX pixels: %1")
                          .arg(QString::fromUtf8(exception.what())));
    }

    const QString identifier = fskId.trimmed().toUpper();
    if (!identifier.isEmpty()) {
        decodium::sstv::SstvTxFskIdPlan plan;
        plan.text = identifier.toStdString();
        request.fskId = std::move(plan);
    }

    // 1.0.588: far scadere QUI la sessione precedente, prima di fissare il
    // dispositivo. start()/startPrepared() cominciano con un tick del
    // coordinatore: se la sessione di prima e' in uno stato terminale, quel
    // tick emette stateChanged, il nostro gancio chiama
    // releaseSstvTxBridgeOwnership() e cancella il pin appena messo. Il
    // preflight che segue trovava il dispositivo non fissato e rifiutava, cosi'
    // la prima trasmissione riusciva e ogni successiva no.
    if (m_sstvTxCoordinator) {
        static_cast<void>(m_sstvTxCoordinator->tick(sstvTxNowMs()));
    }

    // Resolve exactly once, before any PTT ownership is acquired. The value
    // object and channel plan stay pinned until the coordinator releases this
    // session; later settings changes cannot redirect an in-flight TX.
    m_sstvTxOutputDevice = output;
    m_sstvTxOutputChannels = channels;
    m_sstvTxOutputPinned = true;
    m_sstvTxError.clear();
    m_sstvTxProgress = 0.0;
    const bool audioOnly = sstvTxAudioOnlyAllowed();
    m_sstvTxStartInProgress = true;
    const decodium::sstv::SstvTxCoordinatorResult result
        = m_sstvTxCoordinator->start(sstvTxNowMs(), request);
    m_sstvTxStartInProgress = false;
    if (!result.accepted) {
        m_sstvTxOutputDevice = QAudioDevice {};
        m_sstvTxOutputChannels = 0U;
        m_sstvTxOutputPinned = false;
        const QString detail = result.detail.empty()
            ? tr("SSTV TX preflight failed")
            : QString::fromStdString(result.detail);
        return reject(detail);
    }
    m_sstvTxSessionId = static_cast<quint64>(result.sessionId);
    queueSstvStudioTransmittedImage(prepared, identifier, m_sstvTxSessionId);
    emit sstvTxStateChanged();
    emit statusMessage(
        audioOnly
            ? tr("SSTV audio-only TX accepted: %1")
                  .arg(m_sstvStudioController->modeName())
            : tr("SSTV TX accepted: %1")
                  .arg(m_sstvStudioController->modeName()));
    return true;
#else
    Q_UNUSED(fskId)
    emit errorMessage(tr("This Decodium build has no native SSTV support"));
    return false;
#endif
}

void DecodiumBridge::cancelSstvTx()
{
#if DECODIUM_HAS_SSTV
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_sstvTxCoordinator && m_sstvTxCoordinator->stateMachine().active()) {
        m_sstvTxCoordinator->cancel(sstvTxNowMs());
        emit statusMessage(tr("SSTV TX cancelled"));
    }
#endif
}

void DecodiumBridge::tickSstvTx()
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvTxCoordinator || m_sstvTxShuttingDown) {
        return;
    }
    const std::uint64_t now = sstvTxNowMs();
    decodium::sstv::SstvTxCoordinatorSnapshot snapshot
        = m_sstvTxCoordinator->snapshot();
    const std::uint64_t sessionId
        = snapshot.stateMachine.currentSessionId;

    if (snapshot.stateMachine.state
            == decodium::sstv::SstvTxState::WaitingForPtt
        && (sstvTxUsesVoxPtt() || sstvTxPttActive())) {
        m_sstvTxCoordinator->notifyPttConfirmed(now, sessionId);
    }
    snapshot = m_sstvTxCoordinator->snapshot();
    if (snapshot.stateMachine.state
            == decodium::sstv::SstvTxState::ReleasingPtt
        && !sstvTxPttActive()) {
        m_sstvTxCoordinator->notifyPttReleased(now, sessionId);
    }
    snapshot = m_sstvTxCoordinator->snapshot();
    if (m_sstvTxCoordinator->stateMachine().active()
        && snapshot.stateMachine.pttRequestDispatched
        && snapshot.stateMachine.releaseRequired
        && !sstvTxCanControlPtt()
        && snapshot.stateMachine.state
            != decodium::sstv::SstvTxState::ReleasingPtt) {
        m_sstvTxCoordinator->notifyAudioError(
            now, sessionId,
            "Decodium CAT/PTT path disconnected during SSTV TX");
    }

    m_sstvTxCoordinator->tick(now);
    snapshot = m_sstvTxCoordinator->snapshot();
    if (snapshot.audioAttached && m_sstvTxAudioDevice && m_soundOutput) {
        const SoundOutputPlaybackStatus playback
            = m_soundOutput->trackedPlaybackStatus(
                static_cast<quint64>(sessionId));
        const std::uint64_t delivered
            = m_sstvTxAudioDevice->framesReadFromDevice();
        decodium::sstv::SstvTxPlaybackProgress progress;
        progress.playedFrames = playback.drained
            ? snapshot.audioPlan.totalFrames
            : std::min<std::uint64_t>(snapshot.audioPlan.totalFrames,
                                      playback.processedFrames);
        progress.playbackComplete = playback.drained
            && delivered >= snapshot.audioPlan.totalFrames;
        progress.failed = m_sstvTxAudioDevice->failed()
            || !playback.valid || playback.failed
            || playback.expectedFrames != snapshot.audioPlan.totalFrames;
        if (m_sstvTxAudioDevice->failed()) {
            progress.detail = "native SSTV audio source failed";
        } else if (!playback.valid) {
            progress.detail = "SSTV SoundOutput session tracking was lost";
        } else if (playback.failed) {
            progress.detail = playback.detail.toStdString();
        } else if (playback.expectedFrames
                   != snapshot.audioPlan.totalFrames) {
            progress.detail = "SSTV SoundOutput frame plan changed during TX";
        }
        m_sstvTxCoordinator->notifyPlayback(
            now, sessionId, progress);
    }
#endif
}

void DecodiumBridge::releaseSstvTxBridgeOwnership()
{
#if DECODIUM_HAS_SSTV
    m_sstvTxAudioDevice = nullptr;
    m_sstvTxOutputDevice = QAudioDevice {};
    m_sstvTxOutputChannels = 0U;
    m_sstvTxOutputPinned = false;
    m_sstvTxSessionId = 0U;
    if (!m_sstvOwnsBridgeTx) {
        return;
    }
    m_sstvOwnsBridgeTx = false;
    if (m_transmitting) {
        m_transmitting = false;
        emit transmittingChanged();
    }
    restoreTxAudioSchedulingBoost(QStringLiteral("sstv-tx"));
    if (!m_sstvTxShuttingDown && !m_shuttingDown
        && !QCoreApplication::closingDown()) {
        resumeRxAudioAfterTx(QStringLiteral("sstv-tx"));
        resumeNonAudioTxWork(QStringLiteral("sstv-tx"));
    }
    emit sstvTxStateChanged();
#endif
}

void DecodiumBridge::shutdownSstvTx()
{
#if DECODIUM_HAS_SSTV
    Q_ASSERT(QThread::currentThread() == thread());
    m_sstvTxShuttingDown = true;
    if (m_sstvTxTimer) {
        m_sstvTxTimer->stop();
    }
    if (m_sstvTxCoordinator) {
        m_sstvTxCoordinator->shutdown(sstvTxNowMs());
        const auto snapshot = m_sstvTxCoordinator->snapshot();
        if (snapshot.stateMachine.state
                == decodium::sstv::SstvTxState::ReleasingPtt
            && !sstvTxPttActive()) {
            m_sstvTxCoordinator->notifyPttReleased(
                sstvTxNowMs(), snapshot.stateMachine.currentSessionId);
        }
    }
    if (m_sstvOwnsBridgeTx && !sstvTxUsesVoxPtt()) {
        setSstvTxPtt(false);
    }
    if (m_soundOutput && m_sstvTxAudioDevice) {
        if (!m_soundOutput->stopTrackedPlayback(m_sstvTxSessionId)) {
            m_soundOutput->stop();
        }
    }
    m_sstvTxAudioDevice = nullptr;
    m_sstvTxCoordinator.reset();
    releaseSstvTxBridgeOwnership();
    m_sstvTxState = QStringLiteral("Disabled");
    m_sstvTxError.clear();
    m_sstvTxProgress = 0.0;
#endif
}

QVariantMap DecodiumBridge::sstvRxControls() const
{
    QVariantMap result;
#if DECODIUM_HAS_SSTV
    if (!m_sstvRxRuntime) {
        return result;
    }
    const auto controls = m_sstvRxRuntime->rxControlSnapshot();
    result.insert(QStringLiteral("modeControl"),
                  sstvRxModeControlName(controls.settings.modeControl));
    result.insert(QStringLiteral("manualMode"),
                  QString::fromStdString(controls.settings.manualMode));
    result.insert(QStringLiteral("modeLockEnabled"),
                  controls.settings.modeLockEnabled);
    result.insert(QStringLiteral("lockedMode"),
                  QString::fromStdString(controls.settings.lockedMode));
    result.insert(QStringLiteral("receiveWithoutVis"),
                  controls.settings.receiveWithoutVis);
    result.insert(QStringLiteral("timingFallbackEnabled"),
                  controls.settings.timingFallbackEnabled);
    result.insert(QStringLiteral("afcMode"),
                  sstvRxAfcModeName(controls.settings.afcMode));
    result.insert(QStringLiteral("manualFrequencyCorrectionHz"),
                  controls.settings.manualFrequencyCorrectionHz);
    result.insert(QStringLiteral("slantMode"),
                  sstvRxSlantModeName(controls.settings.slantMode));
    result.insert(QStringLiteral("manualClockErrorPpm"),
                  controls.settings.manualClockErrorPpm);
    result.insert(QStringLiteral("replayRetentionSeconds"),
                  controls.settings.replayRetentionSeconds);
    result.insert(QStringLiteral("retainRawAudio"),
                  controls.settings.retainRawAudio);
    result.insert(QStringLiteral("diagnosticScopeEnabled"),
                  controls.settings.diagnosticScopeEnabled);
    result.insert(QStringLiteral("revision"),
                  boundedUnsigned(controls.revision));
#endif
    return result;
}

QVariantList DecodiumBridge::sstvRxModeChoices() const
{
    QVariantList result;
#if DECODIUM_HAS_SSTV
    const decodium::sstv::SstvModeRegistry registry =
        decodium::sstv::SstvModeRegistry::canonical();
    for (const decodium::sstv::SstvModeSpec& mode : registry.modes()) {
        if (mode.classification
                != decodium::sstv::ModeClassification::AnalogSstv
            || !mode.claimsRxSupport()) {
            continue;
        }
        result.push_back(QVariantMap {
            {QStringLiteral("id"), QString::fromStdString(mode.id)},
            {QStringLiteral("name"), QString::fromStdString(mode.longName)},
            {QStringLiteral("family"), QString::fromStdString(mode.family)},
        });
    }
#endif
    return result;
}

bool DecodiumBridge::sstvRxAudioJobBusy() const noexcept
{
#if DECODIUM_HAS_SSTV
    return m_sstvRxAudioJobController
        && m_sstvRxAudioJobController->busy();
#else
    return false;
#endif
}

QString DecodiumBridge::sstvRxAudioJobState() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvRxAudioJobController
        ? m_sstvRxAudioJobController->stateName()
        : QStringLiteral("Unavailable");
#else
    return QStringLiteral("Unavailable");
#endif
}

QString DecodiumBridge::sstvRxAudioJobError() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvRxAudioJobController
        ? m_sstvRxAudioJobController->lastError() : QString {};
#else
    return {};
#endif
}

QString DecodiumBridge::sstvRxRawAudioPath() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvRxRawAudioPath;
#else
    return {};
#endif
}

QVariantMap DecodiumBridge::sstvRxDiagnostics() const
{
    QVariantMap result;
    result.insert(QStringLiteral("available"), sstvAvailable());
    result.insert(QStringLiteral("requested"), sstvRxRequested());
    result.insert(QStringLiteral("active"), sstvRxActive());
    result.insert(QStringLiteral("state"), sstvRxState());
    result.insert(QStringLiteral("source"), sstvRxSource());
    result.insert(QStringLiteral("sourceDevice"), sstvRxSourceDevice());

#if DECODIUM_HAS_SSTV
    if (!m_sstvRxRuntime) {
        return result;
    }

    const SstvRxRuntime::Snapshot snapshot = m_sstvRxRuntime->snapshot();
    result.insert(QStringLiteral("revision"), boundedUnsigned(snapshot.revision));
    result.insert(QStringLiteral("generation"), boundedUnsigned(snapshot.route.generation));
    result.insert(QStringLiteral("streamId"), snapshot.route.source.streamId);
    result.insert(QStringLiteral("workerRunning"), snapshot.workerRunning);
    result.insert(QStringLiteral("workerStarts"),
                  boundedUnsigned(snapshot.workerStarts));
    result.insert(QStringLiteral("workerStops"),
                  boundedUnsigned(snapshot.workerStops));
    result.insert(QStringLiteral("pipelineResets"),
                  boundedUnsigned(snapshot.pipelineResets));
    result.insert(QStringLiteral("chunksProcessed"), boundedUnsigned(snapshot.chunksProcessed));
    result.insert(QStringLiteral("samplesConverted"), boundedUnsigned(snapshot.samplesConverted));
    result.insert(QStringLiteral("samplesResampled"), boundedUnsigned(snapshot.samplesResampled));
    result.insert(QStringLiteral("frequencyObservations"), boundedUnsigned(snapshot.frequencyObservations));
    result.insert(QStringLiteral("lastFrequencyHz"), snapshot.lastFrequencyHz);
    result.insert(QStringLiteral("lastFrequencyConfidence"), snapshot.lastFrequencyConfidence);
    result.insert(QStringLiteral("discontinuities"), boundedUnsigned(snapshot.discontinuities));
    result.insert(QStringLiteral("staleChunksDiscarded"), boundedUnsigned(snapshot.staleChunksDiscarded));
    result.insert(QStringLiteral("processingFailures"), boundedUnsigned(snapshot.processingFailures));
    result.insert(QStringLiteral("producerRejectedCalls"), boundedUnsigned(snapshot.producerRejectedCalls));
    result.insert(QStringLiteral("wrongThreadLifecycleCalls"),
                  boundedUnsigned(snapshot.wrongThreadLifecycleCalls));
    result.insert(QStringLiteral("rxState"), static_cast<int>(snapshot.rxState));
    result.insert(QStringLiteral("rxCause"), static_cast<int>(snapshot.rxCause));
    result.insert(QStringLiteral("visAvailable"), snapshot.vis.available);
    result.insert(QStringLiteral("visValid"), snapshot.vis.valid);
    result.insert(QStringLiteral("visFormat"), static_cast<int>(snapshot.vis.format));
    result.insert(QStringLiteral("visPrimary"), snapshot.vis.primaryPayload);
    result.insert(QStringLiteral("visExtension"), snapshot.vis.extensionPayload);
    result.insert(QStringLiteral("visConfidence"), snapshot.vis.confidence);
    result.insert(QStringLiteral("visRawBits"), snapshot.vis.rawBits);
    result.insert(QStringLiteral("visMappedMode"), snapshot.vis.mappedMode);
    result.insert(QStringLiteral("detectedMode"),
                  snapshot.vis.modeMapped ? snapshot.vis.mappedMode : QString());
    result.insert(QStringLiteral("imageAvailable"), snapshot.image.available);
    result.insert(QStringLiteral("imageComplete"), snapshot.image.complete);
    result.insert(QStringLiteral("imagePartial"), snapshot.image.partial);
    result.insert(QStringLiteral("imageCancelled"), snapshot.image.cancelled);
    result.insert(QStringLiteral("imageGeneration"),
                  boundedUnsigned(snapshot.image.generation));
    result.insert(QStringLiteral("imageAcquisitionId"),
                  boundedUnsigned(snapshot.image.acquisitionId));
    result.insert(QStringLiteral("imageRevision"),
                  boundedUnsigned(snapshot.image.revision));
    result.insert(QStringLiteral("imageLinesPublished"),
                  boundedUnsigned(snapshot.image.linesPublished));
    result.insert(QStringLiteral("imageWidth"), snapshot.image.width);
    result.insert(QStringLiteral("imageHeight"), snapshot.image.height);
    result.insert(QStringLiteral("imageCoveredComponents"),
                  static_cast<qulonglong>(snapshot.image.coveredComponents));
    result.insert(QStringLiteral("imageCompletedPixels"),
                  static_cast<qulonglong>(snapshot.image.completedPixels));
    result.insert(QStringLiteral("imageCoverage"), snapshot.image.coverage);
    result.insert(QStringLiteral("imageMode"), snapshot.image.mode);
    result.insert(QStringLiteral("startUtc"),
                  m_sstvRxStartUtc.isValid()
                      ? m_sstvRxStartUtc.toString(Qt::ISODateWithMs)
                      : QString {});
    result.insert(QStringLiteral("rfFrequencyHz"), m_frequency);

    result.insert(QStringLiteral("afcMode"),
                  sstvRxAfcModeName(snapshot.afc.mode));
    result.insert(QStringLiteral("afcMeasuredOffsetHz"),
                  snapshot.afc.measuredOffsetHz);
    result.insert(QStringLiteral("afcCorrectionHz"),
                  snapshot.afc.correctionHz);
    result.insert(QStringLiteral("afcConfidence"),
                  snapshot.afc.confidence);
    result.insert(QStringLiteral("afcAcceptedReferences"),
                  boundedUnsigned(snapshot.afc.acceptedReferences));
    result.insert(QStringLiteral("afcRejectedReferences"),
                  boundedUnsigned(snapshot.afc.rejectedReferences));
    result.insert(QStringLiteral("afcRejectedImageObservations"),
                  boundedUnsigned(snapshot.afc.rejectedImageObservations));

    result.insert(QStringLiteral("slantMode"),
                  sstvRxSlantModeName(snapshot.slant.mode));
    result.insert(QStringLiteral("slantEstimateValid"),
                  snapshot.slant.estimateValid);
    result.insert(QStringLiteral("slantMeasuredPpm"),
                  snapshot.slant.measuredClockErrorPpm);
    result.insert(QStringLiteral("slantAppliedPpm"),
                  snapshot.slant.appliedClockErrorPpm);
    result.insert(QStringLiteral("slantConfidence"),
                  snapshot.slant.confidence);
    result.insert(QStringLiteral("slantObservedSyncs"),
                  boundedUnsigned(snapshot.slant.observedSyncs));
    result.insert(QStringLiteral("slantRejectedSyncs"),
                  boundedUnsigned(snapshot.slant.rejectedSyncs));

    result.insert(QStringLiteral("syncObserved"), snapshot.sync.observed);
    result.insert(QStringLiteral("syncLocked"), snapshot.sync.locked);
    result.insert(QStringLiteral("syncConfidence"),
                  snapshot.sync.confidence);
    result.insert(QStringLiteral("syncPulseCount"),
                  boundedUnsigned(snapshot.sync.pulseCount));
    result.insert(QStringLiteral("currentLine"),
                  boundedUnsigned(snapshot.sync.currentLine));
    result.insert(QStringLiteral("syncMissedLines"),
                  boundedUnsigned(snapshot.sync.missedLines));

    result.insert(QStringLiteral("fallbackStatus"),
                  static_cast<int>(snapshot.fallback.status));
    result.insert(QStringLiteral("fallbackMode"),
                  snapshot.fallback.selectedMode);
    result.insert(QStringLiteral("fallbackCandidateCount"),
                  static_cast<qulonglong>(snapshot.fallback.candidateCount));
    result.insert(QStringLiteral("fallbackConfidence"),
                  snapshot.fallback.confidence);
    result.insert(QStringLiteral("fallbackLinePeriodSamples"),
                  snapshot.fallback.observedLinePeriodSamples);
    result.insert(QStringLiteral("fallbackSyncDurationSamples"),
                  snapshot.fallback.observedSyncDurationSamples);
    result.insert(QStringLiteral("fallbackAmbiguityCount"),
                  boundedUnsigned(snapshot.fallback.ambiguityCount));

    result.insert(QStringLiteral("fskIdAvailable"),
                  snapshot.fskId.available);
    result.insert(QStringLiteral("fskIdValid"), snapshot.fskId.valid);
    result.insert(QStringLiteral("fskId"), snapshot.fskId.identifier);
    result.insert(QStringLiteral("fskIdConfidence"),
                  snapshot.fskId.confidence);
    result.insert(QStringLiteral("fskIdRawSymbolCount"),
                  static_cast<qulonglong>(snapshot.fskId.rawSymbolCount));

    result.insert(QStringLiteral("signalRms"), snapshot.signal.rms);
    result.insert(QStringLiteral("signalSnrDb"), snapshot.signal.snrDb);
    result.insert(QStringLiteral("signalConfidence"),
                  snapshot.signal.confidence);
    result.insert(QStringLiteral("signalClippingFraction"),
                  snapshot.signal.clippingFraction);

    result.insert(QStringLiteral("replaySampleRate"),
                  snapshot.replay.sampleRate);
    result.insert(QStringLiteral("replayRetentionSeconds"),
                  snapshot.replay.retentionSeconds);
    result.insert(QStringLiteral("replayRetainedSamples"),
                  static_cast<qulonglong>(snapshot.replay.retainedSamples));
    result.insert(QStringLiteral("replayCapacitySamples"),
                  static_cast<qulonglong>(snapshot.replay.capacitySamples));
    result.insert(QStringLiteral("replayAcquisitionDescriptors"),
                  static_cast<qulonglong>(
                      snapshot.replay.acquisitionDescriptors));
    result.insert(QStringLiteral("replayMostRecentAcquisitionId"),
                  boundedUnsigned(snapshot.replay.mostRecentAcquisitionId));

    result.insert(QStringLiteral("dspMeasuredBlocks"),
                  boundedUnsigned(snapshot.performance.measuredDspBlocks));
    result.insert(QStringLiteral("dspAverageBlockNs"), boundedUnsigned(
                      snapshot.performance.averageDspBlockNanoseconds));
    result.insert(QStringLiteral("dspMaximumBlockNs"), boundedUnsigned(
                      snapshot.performance.maximumDspBlockNanoseconds));
    result.insert(QStringLiteral("progressiveUpdates"), boundedUnsigned(
                      snapshot.performance.progressiveUpdates));
    result.insert(QStringLiteral("progressiveUpdatesPerSecond"),
                  snapshot.performance.progressiveUpdatesPerSecond);

    QVariantList scope;
    scope.reserve(snapshot.scope.size());
    for (const SstvRxRuntime::ScopePoint& point : snapshot.scope) {
        scope.push_back(QVariantMap {
            {QStringLiteral("sample"), boundedUnsigned(point.sample)},
            {QStringLiteral("frequencyHz"), point.frequencyHz},
            {QStringLiteral("confidence"), point.confidence},
            {QStringLiteral("rms"), point.rms},
            {QStringLiteral("snrDb"), point.snrDb},
        });
    }
    result.insert(QStringLiteral("scope"), scope);
    result.insert(QStringLiteral("queuedChunks"),
                  static_cast<qulonglong>(snapshot.ingress.queue.queuedChunks));
    result.insert(QStringLiteral("queuedSamples"),
                  static_cast<qulonglong>(snapshot.ingress.queue.queuedSamples));
    result.insert(QStringLiteral("droppedChunks"),
                  boundedUnsigned(snapshot.ingress.queue.droppedChunks));
    result.insert(QStringLiteral("droppedSamples"),
                  boundedUnsigned(snapshot.ingress.queue.droppedSamples));
    result.insert(QStringLiteral("lastError"), snapshot.lastError);
#endif
    return result;
}

bool DecodiumBridge::sstvStorageReady() const noexcept
{
#if DECODIUM_HAS_SSTV
    return m_sstvStorageReady;
#else
    return false;
#endif
}

bool DecodiumBridge::sstvRxAutoSaveEnabled() const noexcept
{
#if DECODIUM_HAS_SSTV
    return m_sstvRxAutoSaveEnabled;
#else
    return false;
#endif
}

void DecodiumBridge::setSstvRxAutoSaveEnabled(bool enabled)
{
#if DECODIUM_HAS_SSTV
    Q_ASSERT(QThread::currentThread() == thread());
    if (enabled && !m_sstvStorageReady) {
        setSstvRxSaveStatus(
            QStringLiteral("error"),
            tr("SSTV storage is not ready; automatic save remains disabled"));
        return;
    }
    if (m_sstvRxAutoSaveEnabled == enabled) {
        return;
    }
    m_sstvRxAutoSaveEnabled = enabled;
    setSetting(QStringLiteral("SSTV/RxAutoSaveEnabled"), enabled);
    emit sstvRxAutoSaveChanged();
    if (enabled) {
        maybeAutoSaveSstvRxImage();
    }
#else
    Q_UNUSED(enabled)
#endif
}

QString DecodiumBridge::sstvRxSaveState() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvRxSaveState;
#else
    return QStringLiteral("idle");
#endif
}

QString DecodiumBridge::sstvRxSaveError() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvRxSaveError;
#else
    return {};
#endif
}

bool DecodiumBridge::sstvWavReplayActive() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvWavReplayController
        && m_sstvWavReplayController->active();
#else
    return false;
#endif
}

QString DecodiumBridge::sstvWavReplayState() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvWavReplayController
        ? m_sstvWavReplayController->stateName()
        : QStringLiteral("Unavailable");
#else
    return QStringLiteral("Unavailable");
#endif
}

double DecodiumBridge::sstvWavReplayProgress() const noexcept
{
#if DECODIUM_HAS_SSTV
    return m_sstvWavReplayController
        ? m_sstvWavReplayController->progress() : 0.0;
#else
    return 0.0;
#endif
}

QString DecodiumBridge::sstvWavReplayFileName() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvWavReplayController
        ? m_sstvWavReplayController->fileName() : QString {};
#else
    return {};
#endif
}

QString DecodiumBridge::sstvWavReplayError() const
{
#if DECODIUM_HAS_SSTV
    return m_sstvWavReplayController
        ? m_sstvWavReplayController->lastError() : QString {};
#else
    return {};
#endif
}

void DecodiumBridge::initialiseSstvRuntime()
{
#if DECODIUM_HAS_SSTV
    Q_ASSERT(QThread::currentThread() == thread());
    if (!m_sstvAudioRelay) {
        m_sstvAudioRelay = std::make_shared<DecodiumSstvAudioRelay>();
    }
    m_sstvRxAutoSaveEnabled = getSetting(
        QStringLiteral("SSTV/RxAutoSaveEnabled"), false).toBool();
    if (!m_sstvStudioController) {
        m_sstvStudioController
            = new decodium::sstv::SstvStudioController(this);
        connect(m_sstvStudioController,
                &decodium::sstv::SstvStudioController::preparedChanged,
                this, &DecodiumBridge::handleSstvStudioPreparedChanged);
    }
    if (!m_sstvShareController) {
        m_sstvShareController = new decodium::sstv::SstvShareController(
            &secure_settings::default_backend(), this);
        m_sstvShareController->setObjectName(QStringLiteral("sstvShare"));
    }
#if DECODIUM_HAS_HAMDRM
    if (!m_hamDrmController) {
        decodium::sstv::hamdrm::HamDrmControllerConfig config;
        const QString appData = QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation);
        if (!appData.isEmpty()) {
            config.partialStoreRoot = QDir(appData).absoluteFilePath(
                QStringLiteral("sstv/hamdrm/partial"));
        }
        decodium::sstv::hamdrm::HamDrmControllerBackends backends;
        decodium::sstv::hamdrm::waveform::HamDrmNativeRxHooks rxHooks;
        rxHooks.activateSharedAudioTap = [this]() {
            return activateHamDrmRxTap();
        };
        rxHooks.deactivateSharedAudioTap = [this]() {
            if (QThread::currentThread() == thread()) {
                deactivateHamDrmRxTap();
                return;
            }
            static_cast<void>(QMetaObject::invokeMethod(
                this, [this]() { deactivateHamDrmRxTap(); },
                Qt::QueuedConnection));
        };
        m_hamDrmRxBackend = std::make_shared<
            decodium::sstv::hamdrm::waveform::HamDrmNativeRxBackend>(
                std::move(rxHooks));

        decodium::sstv::hamdrm::waveform::HamDrmNativeTxHooks txHooks;
        txHooks.configuredCallsign = [this]() {
            return m_callsign.trimmed().toUpper().toStdString();
        };
        txHooks.startPreparedAudio = [this](
            std::unique_ptr<decodium::sstv::SstvPcm16Source> source,
            std::string mode) {
                return startHamDrmPreparedAudio(
                    std::move(source), std::move(mode));
            };
        txHooks.cancelAudio = [this](std::uint64_t sessionId) {
            return cancelHamDrmPreparedAudio(sessionId);
        };
        m_hamDrmTxBackend = std::make_shared<
            decodium::sstv::hamdrm::waveform::HamDrmNativeTxBackend>(
                std::move(txHooks));
        backends.waveformRx = m_hamDrmRxBackend;
        backends.waveformTx = m_hamDrmTxBackend;
        backends.jpeg2000 =
            decodium::sstv::hamdrm::makeNativeHamDrmJpeg2000Backend();
        m_hamDrmController =
            new decodium::sstv::hamdrm::HamDrmController(
                std::move(config), std::move(backends), this);
        m_hamDrmController->setObjectName(QStringLiteral("sstvDigital"));
        connect(m_hamDrmController,
                &decodium::sstv::hamdrm::HamDrmController::operationRejected,
                this, [this](const QString& operation,
                             const QString& detail) {
                    emit statusMessage(tr("HAMDRM %1: %2")
                                           .arg(operation, detail));
                });
        connect(m_hamDrmController,
                &decodium::sstv::hamdrm::HamDrmController::txImageAccepted,
                this,
                [this](QImage image, const QString& profileId,
                       int occupiedBandwidthHz) {
                    queueHamDrmTransmittedImage(
                        std::move(image), profileId, occupiedBandwidthHz);
                });
    }
#endif
    initialiseSstvStorage();
    if (!m_sstvDiagnosticsController) {
        m_sstvDiagnosticsController
            = new decodium::sstv::SstvDiagnosticsController(this);
        m_sstvDiagnosticsController->setObjectName(
            QStringLiteral("sstvDiagnostics"));
        auto* const txDiagnosticsRefreshTimer
            = new QTimer(m_sstvDiagnosticsController);
        txDiagnosticsRefreshTimer->setObjectName(
            QStringLiteral("SSTV diagnostics TX refresh"));
        txDiagnosticsRefreshTimer->setSingleShot(true);
        txDiagnosticsRefreshTimer->setInterval(250);
        connect(txDiagnosticsRefreshTimer, &QTimer::timeout,
                this, &DecodiumBridge::refreshSstvDiagnosticsSnapshot);
        const auto scheduleTxDiagnosticsRefresh =
            [txDiagnosticsRefreshTimer]() {
                if (!txDiagnosticsRefreshTimer->isActive()) {
                    txDiagnosticsRefreshTimer->start();
                }
            };
        connect(
            this, &DecodiumBridge::sstvTxStateChanged,
            txDiagnosticsRefreshTimer,
            [this, txDiagnosticsRefreshTimer,
             scheduleTxDiagnosticsRefresh]() {
                const bool terminal = m_sstvTxCoordinator
                    && terminalSstvTxState(
                        m_sstvTxCoordinator->stateMachine().state());
                if (terminal) {
                    txDiagnosticsRefreshTimer->stop();
                    refreshSstvDiagnosticsSnapshot();
                    return;
                }
                scheduleTxDiagnosticsRefresh();
            });
        connect(this, &DecodiumBridge::transmittingChanged,
                txDiagnosticsRefreshTimer, scheduleTxDiagnosticsRefresh);
        connect(this, &DecodiumBridge::pttPendingChanged,
                txDiagnosticsRefreshTimer, scheduleTxDiagnosticsRefresh);
        connect(this, &DecodiumBridge::pttConfirmedChanged,
                txDiagnosticsRefreshTimer, scheduleTxDiagnosticsRefresh);
        connect(this, &DecodiumBridge::tuningChanged,
                txDiagnosticsRefreshTimer, scheduleTxDiagnosticsRefresh);
        connect(this, &DecodiumBridge::txEnabledChanged,
                txDiagnosticsRefreshTimer, scheduleTxDiagnosticsRefresh);
        connect(this, &DecodiumBridge::autoCqRepeatChanged,
                txDiagnosticsRefreshTimer, scheduleTxDiagnosticsRefresh);
        connect(this, &DecodiumBridge::audioOutputDeviceChanged,
                txDiagnosticsRefreshTimer, scheduleTxDiagnosticsRefresh);
        connect(this, &DecodiumBridge::audioOutputChannelChanged,
                txDiagnosticsRefreshTimer, scheduleTxDiagnosticsRefresh);
        connect(this, &DecodiumBridge::catConnectedChanged,
                txDiagnosticsRefreshTimer, scheduleTxDiagnosticsRefresh);
        connect(this, &DecodiumBridge::catManagerChanged,
                txDiagnosticsRefreshTimer, scheduleTxDiagnosticsRefresh);
        connect(this, &DecodiumBridge::catBackendChanged,
                txDiagnosticsRefreshTimer, scheduleTxDiagnosticsRefresh);
        connect(this, &DecodiumBridge::decoPortUseRemoteChanged,
                txDiagnosticsRefreshTimer, scheduleTxDiagnosticsRefresh);
        connect(
            m_sstvDiagnosticsController,
            &decodium::sstv::SstvDiagnosticsController::inputSnapshotRequested,
            this, &DecodiumBridge::refreshSstvDiagnosticsSnapshot,
            Qt::DirectConnection);
        connect(
            m_sstvDiagnosticsController,
            &decodium::sstv::SstvDiagnosticsController::testToneRequested,
            this,
            [this]() {
                static_cast<void>(startSstvCalibrationTone(
                    QStringLiteral("black-1500")));
                refreshSstvDiagnosticsSnapshot();
            },
            Qt::DirectConnection);
        connect(this, &DecodiumBridge::sstvRxStateChanged,
                this, &DecodiumBridge::refreshSstvDiagnosticsSnapshot);
        connect(this, &DecodiumBridge::sstvStorageStateChanged,
                this, &DecodiumBridge::refreshSstvDiagnosticsSnapshot);
        if (m_sstvGalleryModel) {
            connect(m_sstvGalleryModel,
                    &decodium::sstv::SstvGalleryModel::quotaSummaryChanged,
                    this, &DecodiumBridge::refreshSstvDiagnosticsSnapshot);
            connect(m_sstvGalleryModel,
                    &decodium::sstv::SstvGalleryModel::retentionSettingsChanged,
                    this, &DecodiumBridge::refreshSstvDiagnosticsSnapshot);
        }
        if (m_sstvShareController) {
            connect(m_sstvShareController,
                    &decodium::sstv::SstvShareController::stateChanged,
                    this, &DecodiumBridge::refreshSstvDiagnosticsSnapshot);
        }
#if DECODIUM_HAS_HAMDRM
        if (m_hamDrmController) {
            connect(
                m_hamDrmController,
                &decodium::sstv::hamdrm::HamDrmController::operationStateChanged,
                this, &DecodiumBridge::refreshSstvDiagnosticsSnapshot);
            connect(m_hamDrmController,
                    &decodium::sstv::hamdrm::HamDrmController::errorChanged,
                    this, &DecodiumBridge::refreshSstvDiagnosticsSnapshot);
        }
#endif
    }
    if (m_sstvRxRuntime) {
        refreshSstvDiagnosticsSnapshot();
        return;
    }

    m_sstvRxRuntime = std::make_unique<SstvRxRuntime>();
    {
        QVariantMap savedControls {
            {QStringLiteral("modeControl"), getSetting(
                 QStringLiteral("SSTV/RxModeControl"),
                 QStringLiteral("auto"))},
            {QStringLiteral("manualMode"), getSetting(
                 QStringLiteral("SSTV/RxManualMode"), QString())},
            {QStringLiteral("modeLockEnabled"), getSetting(
                 QStringLiteral("SSTV/RxModeLockEnabled"), false)},
            {QStringLiteral("lockedMode"), getSetting(
                 QStringLiteral("SSTV/RxLockedMode"), QString())},
            {QStringLiteral("receiveWithoutVis"), getSetting(
                 QStringLiteral("SSTV/RxWithoutVis"), false)},
            {QStringLiteral("timingFallbackEnabled"), getSetting(
                 QStringLiteral("SSTV/RxTimingFallbackEnabled"), true)},
            {QStringLiteral("afcMode"), getSetting(
                 QStringLiteral("SSTV/RxAfcMode"),
                 QStringLiteral("auto"))},
            {QStringLiteral("manualFrequencyCorrectionHz"), getSetting(
                 QStringLiteral("SSTV/RxManualFrequencyCorrectionHz"), 0.0)},
            {QStringLiteral("slantMode"), getSetting(
                 QStringLiteral("SSTV/RxSlantMode"),
                 QStringLiteral("auto"))},
            {QStringLiteral("manualClockErrorPpm"), getSetting(
                 QStringLiteral("SSTV/RxManualClockErrorPpm"), 0.0)},
            {QStringLiteral("replayRetentionSeconds"), getSetting(
                 QStringLiteral("SSTV/RxReplayRetentionSeconds"), 180)},
            {QStringLiteral("retainRawAudio"), getSetting(
                 QStringLiteral("SSTV/RxRetainRawAudio"), false)},
            {QStringLiteral("diagnosticScopeEnabled"), getSetting(
                 QStringLiteral("SSTV/RxDiagnosticScopeEnabled"), false)},
        };
        SstvRxControlSettings settings;
        QString error;
        if (sstvRxSettingsFromMap(
                savedControls, settings, &settings, &error)) {
            static_cast<void>(m_sstvRxRuntime->replaceRxControlSettings(
                std::move(settings)));
        } else {
            const SstvRxControlSettings defaults;
            setSetting(QStringLiteral("SSTV/RxModeControl"),
                       sstvRxModeControlName(defaults.modeControl));
            setSetting(QStringLiteral("SSTV/RxManualMode"), QString {});
            setSetting(QStringLiteral("SSTV/RxModeLockEnabled"), false);
            setSetting(QStringLiteral("SSTV/RxLockedMode"), QString {});
            setSetting(QStringLiteral("SSTV/RxWithoutVis"),
                       defaults.receiveWithoutVis);
            setSetting(QStringLiteral("SSTV/RxTimingFallbackEnabled"),
                       defaults.timingFallbackEnabled);
            setSetting(QStringLiteral("SSTV/RxAfcMode"),
                       sstvRxAfcModeName(defaults.afcMode));
            setSetting(QStringLiteral(
                           "SSTV/RxManualFrequencyCorrectionHz"),
                       defaults.manualFrequencyCorrectionHz);
            setSetting(QStringLiteral("SSTV/RxSlantMode"),
                       sstvRxSlantModeName(defaults.slantMode));
            setSetting(QStringLiteral("SSTV/RxManualClockErrorPpm"),
                       defaults.manualClockErrorPpm);
            setSetting(QStringLiteral("SSTV/RxReplayRetentionSeconds"),
                       defaults.replayRetentionSeconds);
            setSetting(QStringLiteral("SSTV/RxRetainRawAudio"),
                       defaults.retainRawAudio);
            setSetting(QStringLiteral("SSTV/RxDiagnosticScopeEnabled"),
                       defaults.diagnosticScopeEnabled);
            emit errorMessage(tr(
                "Invalid saved SSTV RX controls were reset: %1").arg(error));
        }
    }
    connect(m_sstvRxRuntime.get(), &SstvRxRuntime::runtimeStateChanged,
            this, [this](SstvRxRuntime::State, quint64) {
        emit sstvRxStateChanged();
        emit sstvRxSnapshotChanged();
    });
    connect(m_sstvRxRuntime.get(), &SstvRxRuntime::snapshotAvailable,
            this, [this](quint64) {
        const SstvRxRuntime::Snapshot snapshot =
            m_sstvRxRuntime->snapshot();
        if (snapshot.image.acquisitionId != 0U
            && snapshot.image.acquisitionId
                != m_sstvRxStartAcquisitionId) {
            m_sstvRxStartAcquisitionId = snapshot.image.acquisitionId;
            m_sstvRxStartUtc = QDateTime::currentDateTimeUtc();
            if (m_sstvRxRawAudioAcquisitionId != 0U
                && m_sstvRxRawAudioAcquisitionId
                    != snapshot.image.acquisitionId) {
                m_sstvRxRawAudioPath.clear();
                m_sstvRxRawAudioAcquisitionId = 0U;
                emit sstvRxAudioJobChanged();
            }
        }
        emit sstvRxSnapshotChanged();
        maybeAutoSaveSstvRxImage();
    });
    connect(m_sstvRxRuntime.get(), &SstvRxRuntime::visDetectionAvailable,
            this, [this](quint64, int, int, int, int primaryPayload,
                         int, double confidence, const QString& mappedMode) {
        const SstvRxRuntime::Snapshot snapshot = m_sstvRxRuntime->snapshot();
        emit sstvVisDetected(mappedMode, primaryPayload,
                             snapshot.vis.valid, confidence);
        emit sstvRxSnapshotChanged();
    });
    connect(m_sstvRxRuntime.get(), &SstvRxRuntime::workerError,
            this, [this](const QString& detail) {
        emit errorMessage(tr("SSTV RX: %1").arg(detail));
        emit sstvRxStateChanged();
        emit sstvRxSnapshotChanged();
    });

    m_sstvRxAudioJobController =
        new decodium::sstv::SstvRxAudioJobController(
            m_sstvRxRuntime.get(), this);
    m_sstvRxAudioJobController->setObjectName(
        QStringLiteral("sstvRxAudioJobs"));
    connect(m_sstvRxAudioJobController,
            &decodium::sstv::SstvRxAudioJobController::stateChanged,
            this, &DecodiumBridge::sstvRxAudioJobChanged);
    connect(m_sstvRxAudioJobController,
            &decodium::sstv::SstvRxAudioJobController::rawAudioExportFinished,
            this,
            [this](bool ok, const QString& path, quint64 acquisitionId,
                   const QString& error) {
                if (ok) {
                    m_sstvRxRawAudioPath = path;
                    m_sstvRxRawAudioAcquisitionId = acquisitionId;
                    emit statusMessage(tr(
                        "Raw SSTV diagnostic audio saved: %1").arg(path));
                } else if (!error.isEmpty()) {
                    emit errorMessage(tr(
                        "Raw SSTV audio save failed: %1").arg(error));
                }
                emit sstvRxAudioJobChanged();
            });
    connect(m_sstvRxAudioJobController,
            &decodium::sstv::SstvRxAudioJobController::redecodePrepared,
            this,
            [this](bool ok, const QUrl& privateWav,
                   const QString& error) {
                if (!ok || !m_sstvRxRuntime) {
                    if (!error.isEmpty()) {
                        emit errorMessage(tr(
                            "SSTV re-decode preparation failed: %1")
                                              .arg(error));
                    }
                    emit sstvRxAudioJobChanged();
                    return;
                }
                const SstvRxRedecodeParameters parameters =
                    m_sstvRxAudioJobController
                        ->preparedRedecodeParameters();
                m_sstvRedecodeRestoreControls = sstvRxControls();
                SstvRxControlSettings temporary =
                    m_sstvRxRuntime->rxControlSnapshot().settings;
                temporary.modeControl = parameters.mode.empty()
                    ? SstvRxModeControl::Automatic
                    : SstvRxModeControl::Manual;
                temporary.manualMode = parameters.mode;
                temporary.modeLockEnabled = false;
                temporary.lockedMode.clear();
                temporary.receiveWithoutVis = !parameters.mode.empty();
                temporary.timingFallbackEnabled = true;
                temporary.afcMode = parameters.afcMode;
                temporary.manualFrequencyCorrectionHz =
                    parameters.frequencyCorrectionHz;
                temporary.slantMode = parameters.slantMode;
                temporary.manualClockErrorPpm = parameters.clockErrorPpm;
                if (!m_sstvRxRuntime->requestRxRedecode(parameters)
                    || !m_sstvRxRuntime->replaceRxControlSettings(
                        std::move(temporary))) {
                    emit errorMessage(tr(
                        "The requested SSTV re-decode controls were rejected"));
                    m_sstvRxAudioJobController->discardPreparedRedecode();
                    m_sstvRedecodeRestoreControls.clear();
                    emit sstvRxAudioJobChanged();
                    return;
                }
                m_sstvRedecodeActive = true;
                if (!startSstvWavReplay(privateWav)) {
                    SstvRxControlSettings restore =
                        m_sstvRxRuntime->rxControlSnapshot().settings;
                    QString restoreError;
                    if (sstvRxSettingsFromMap(
                            m_sstvRedecodeRestoreControls,
                            restore, &restore, &restoreError)) {
                        static_cast<void>(
                            m_sstvRxRuntime->replaceRxControlSettings(
                                std::move(restore)));
                    }
                    m_sstvRedecodeActive = false;
                    m_sstvRedecodeRestoreControls.clear();
                    m_sstvRxAudioJobController->discardPreparedRedecode();
                    emit errorMessage(tr(
                        "The prepared SSTV re-decode could not start"));
                }
                emit sstvRxAudioJobChanged();
            });

    m_sstvWavReplayController
        = new decodium::sstv::SstvWavReplayController(
            m_sstvRxRuntime.get(), this);
    m_sstvWavReplayController->setObjectName(
        QStringLiteral("sstvWavReplay"));
    connect(m_sstvWavReplayController,
            &decodium::sstv::SstvWavReplayController::stateChanged,
            this, &DecodiumBridge::sstvWavReplayChanged);
    connect(m_sstvWavReplayController,
            &decodium::sstv::SstvWavReplayController::progressChanged,
            this, &DecodiumBridge::sstvWavReplayChanged);
    connect(m_sstvWavReplayController,
            &decodium::sstv::SstvWavReplayController::replayChanged,
            this, &DecodiumBridge::sstvWavReplayChanged);
    connect(m_sstvWavReplayController,
            &decodium::sstv::SstvWavReplayController::errorOccurred,
            this, [this](const QString& detail) {
                emit errorMessage(tr("SSTV WAV replay: %1").arg(detail));
            });
    connect(m_sstvWavReplayController,
            &decodium::sstv::SstvWavReplayController::replayFinished,
            this, &DecodiumBridge::finishSstvWavReplay);
    refreshSstvDiagnosticsSnapshot();
#endif
}

void DecodiumBridge::initialiseSstvStorage()
{
#if DECODIUM_HAS_SSTV
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_sstvGalleryModel) {
        return;
    }

    m_sstvGalleryModel = new decodium::sstv::SstvGalleryModel(this);
    m_sstvGalleryModel->setObjectName(QStringLiteral("sstvGallery"));

    const QString appData = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    if (appData.isEmpty()) {
        const QString error = QStringLiteral(
            "Qt did not provide an application data directory for SSTV storage");
        m_sstvGalleryModel->setUnavailableError(error);
        setSstvRxSaveStatus(QStringLiteral("unavailable"), error);
        emit sstvStorageStateChanged();
        return;
    }
    const decodium::sstv::SstvStorageLayout layout(
        QDir(appData).absoluteFilePath(QStringLiteral("sstv")));
    if (layout.rootPath().isEmpty() || layout.databasePath().isEmpty()) {
        const QString error = QStringLiteral(
            "could not resolve the SSTV storage layout");
        m_sstvGalleryModel->setUnavailableError(error);
        setSstvRxSaveStatus(QStringLiteral("unavailable"), error);
        emit sstvStorageStateChanged();
        return;
    }

    m_sstvStorageThread = new QThread(this);
    m_sstvStorageThread->setObjectName(QStringLiteral("SSTV storage worker"));
    auto* worker = new decodium::sstv::SstvStorageWorker(
        layout.databasePath(), layout.rootPath());
    m_sstvStorageWorker = worker;
    worker->moveToThread(m_sstvStorageThread);
    if (m_sstvShareController) {
        connect(m_sstvShareController,
                &decodium::sstv::SstvShareController::incomingHandoffReady,
                this,
                [this, worker](QVariantMap handoff) {
                    if (m_sstvStorageWorker != worker) {
                        return;
                    }
                    const QString transferId = handoff.value(
                        QStringLiteral("transferId")).toString();
                    if (transferId.isEmpty()) {
                        emit errorMessage(tr(
                            "Incoming SSTV Gallery import rejected: "
                            "validated handoff has no transfer identifier"));
                        return;
                    }
                    m_sstvIncomingImportHandoffs.insert(
                        transferId, std::move(handoff));
                    m_sstvIncomingImportRetryCounts.insert(transferId, 0);
                    if (!queueSstvIncomingImport(transferId)) {
                        m_sstvIncomingImportHandoffs.remove(transferId);
                        m_sstvIncomingImportRetryCounts.remove(transferId);
                        emit errorMessage(tr(
                            "Incoming SSTV Gallery import could not be queued; "
                            "the validated staging file was retained"));
                    }
                },
                Qt::QueuedConnection);
    }
    connect(m_sstvStorageThread, &QThread::finished,
            worker, &QObject::deleteLater);
    connect(worker, &decodium::sstv::SstvStorageWorker::initialized,
            this,
            [this, worker](bool ok, const QString& error, int, quintptr) {
                if (m_sstvStorageWorker != worker || !m_sstvGalleryModel) {
                    return;
                }
                const bool readyChanged = m_sstvStorageReady != ok;
                m_sstvStorageReady = ok;
                if (ok) {
                    m_sstvGalleryModel->setStorageWorker(worker);
                    // The QSO dialog must never cold-parse the active ADIF
                    // logbook on the GUI thread. Start Decodium's existing
                    // asynchronous native cache as soon as Gallery is ready.
                    warmLogCacheAsync();
                    if (m_sstvStudioController) {
                        const decodium::sstv::SstvStorageLayout readyLayout(
                            worker->storageRoot());
                        m_sstvStudioController->setWavExportRoot(
                            readyLayout.wavExportRoot());
                        // A Studio image may have been prepared while the
                        // asynchronous worker was opening its database.
                        // Archive that immutable revision now instead of
                        // requiring the operator to prepare it again.
                        queueSstvStudioDraftImage();
                    }
                    if (m_sstvShareController) {
                        m_sstvShareController->setStorageRoot(
                            worker->storageRoot(), m_callsign);
                    }
                    if (m_sstvRxSaveRequests.isEmpty()) {
                        setSstvRxSaveStatus(QStringLiteral("idle"));
                    }
                    maybeAutoSaveSstvRxImage();
                } else {
                    m_sstvGalleryModel->setUnavailableError(
                        QStringLiteral("SSTV gallery storage: %1").arg(error));
                    setSstvRxSaveStatus(QStringLiteral("unavailable"), error);
                }
                if (readyChanged) {
                    emit sstvStorageStateChanged();
                }
            }, Qt::QueuedConnection);
    connect(worker, &decodium::sstv::SstvStorageWorker::imageStoreFinished,
            this,
            [this, worker](quint64 requestId, bool ok,
                           const decodium::sstv::SstvImageRecord& record,
                           const QString& error) {
                if (m_sstvStorageWorker != worker) {
                    return;
                }
                const auto archive = m_sstvTxGallerySaveRequests.find(
                    requestId);
                if (archive != m_sstvTxGallerySaveRequests.end()) {
                    const QString category = archive.value();
                    m_sstvTxGallerySaveRequests.erase(archive);
                    if (ok) {
                        emit statusMessage(
                            category == QLatin1String("draft")
                                ? tr("Prepared SSTV Studio image saved in Gallery: %1")
                                      .arg(record.mode)
                                : tr("Transmitted SSTV image saved in Gallery: %1")
                                      .arg(record.mode));
                    } else {
                        emit errorMessage(
                            category == QLatin1String("draft")
                                ? tr("Prepared SSTV Studio image save failed: %1")
                                      .arg(error.isEmpty()
                                           ? tr("The SSTV image could not be saved")
                                           : error)
                                : tr("Transmitted SSTV image save failed: %1")
                                      .arg(error.isEmpty()
                                           ? tr("The SSTV image could not be saved")
                                           : error));
                    }
                    return;
                }
                if (!m_sstvRxSaveRequests.contains(requestId)) {
                    return;
                }
                const QString key = m_sstvRxSaveRequests.take(requestId);
                m_sstvRxPendingSaveKeys.remove(key);
                if (ok) {
                    if (!m_sstvRxSavedKeys.contains(key)) {
                        m_sstvRxSavedKeys.insert(key);
                        m_sstvRxSavedKeyOrder.enqueue(key);
                        constexpr qsizetype kMaximumRememberedSaves = 256;
                        while (m_sstvRxSavedKeyOrder.size()
                               > kMaximumRememberedSaves) {
                            m_sstvRxSavedKeys.remove(
                                m_sstvRxSavedKeyOrder.dequeue());
                        }
                    }
                    setSstvRxSaveStatus(
                        m_sstvRxSaveRequests.isEmpty()
                            ? QStringLiteral("saved")
                            : QStringLiteral("saving"));
                    emit statusMessage(tr("SSTV image saved in Gallery: %1")
                                           .arg(record.mode));
                } else {
                    setSstvRxSaveStatus(
                        QStringLiteral("error"),
                        error.isEmpty()
                            ? tr("The SSTV image could not be saved") : error);
                    emit errorMessage(tr("SSTV image save failed: %1")
                                          .arg(m_sstvRxSaveError));
                }
            }, Qt::QueuedConnection);
    connect(worker, &decodium::sstv::SstvStorageWorker::recordFetched,
            this,
            [this, worker](quint64 requestId, bool found,
                           const decodium::sstv::SstvImageRecord& record,
                           const QString& error) {
                if (m_sstvStorageWorker != worker) {
                    return;
                }
                auto pending = m_sstvQsoLogRequests.find(requestId);
                if (pending == m_sstvQsoLogRequests.end()
                    || pending->value(QStringLiteral("phase")).toString()
                           != QStringLiteral("fetch")) {
                    return;
                }
                if (!found) {
                    finishSstvQsoLog(
                        requestId, false,
                        error.isEmpty()
                            ? tr("The selected SSTV Gallery image no longer exists")
                            : error);
                    return;
                }

                decodium::sstv::SstvQsoLogRequest request;
                QString validationError;
                if (!sstvQsoRequestFromMap(
                        pending->value(QStringLiteral("request")).toMap(),
                        &request, &validationError)) {
                    finishSstvQsoLog(requestId, false, validationError);
                    return;
                }
                if (record.id != request.imageRecordId) {
                    finishSstvQsoLog(
                        requestId, false,
                        tr("The Gallery returned a different SSTV image record"));
                    return;
                }
                if (record.mode.trimmed().compare(
                        request.imageMode.trimmed(),
                        Qt::CaseInsensitive) != 0) {
                    finishSstvQsoLog(
                        requestId, false,
                        tr("The SSTV image mode changed before the QSO was logged"));
                    return;
                }

                QString qsoId = request.existingQsoId.trimmed();
                bool qsoCreated = false;
                if (request.createNewQso) {
                    QString commitError;
                    if (!commitSstvNewQso(
                            request, &qsoId, &commitError)) {
                        finishSstvQsoLog(requestId, false, commitError);
                        return;
                    }
                    qsoCreated = true;
                } else if (!isSstvExistingQsoChoiceCurrent(qsoId)) {
                    m_sstvIssuedExistingQsoIds.clear();
                    invalidateQsoSearchCache();
                    warmLogCacheAsync();
                    finishSstvQsoLog(
                        requestId, false,
                        tr("Select an existing QSO from the current Decodium logbook list"));
                    return;
                }

                pending = m_sstvQsoLogRequests.find(requestId);
                if (pending == m_sstvQsoLogRequests.end()) {
                    return;
                }
                pending->insert(QStringLiteral("phase"),
                                QStringLiteral("associate"));
                pending->insert(QStringLiteral("qsoId"), qsoId);
                pending->insert(QStringLiteral("qsoCreated"), qsoCreated);
                if (!queueSstvQsoAssociation(
                        requestId, request.imageRecordId, qsoId)) {
                    finishSstvQsoLog(
                        requestId, false,
                        qsoCreated
                            ? tr("The SSTV QSO was logged, but its local image association could not be queued; associate the image with the existing QSO to retry")
                            : tr("The SSTV image association could not be queued"));
                }
            }, Qt::QueuedConnection);
    connect(worker, &decodium::sstv::SstvStorageWorker::operationFinished,
            this,
            [this, worker](quint64 requestId,
                           decodium::sstv::SstvStorageOperation operation,
                           bool ok, const QString& error) {
                if (m_sstvStorageWorker != worker
                    || operation
                           != decodium::sstv::SstvStorageOperation::AssociateQso) {
                    return;
                }
                const auto pending = m_sstvQsoLogRequests.constFind(requestId);
                if (pending == m_sstvQsoLogRequests.cend()
                    || pending->value(QStringLiteral("phase")).toString()
                           != QStringLiteral("associate")) {
                    return;
                }
                finishSstvQsoLog(
                    requestId, ok,
                    ok ? QString {}
                       : (error.isEmpty()
                              ? tr("The local SSTV image association was not stored")
                              : error));
            }, Qt::QueuedConnection);
    connect(worker,
            &decodium::sstv::SstvStorageWorker::incomingImportFinished,
            this,
            [this, worker](
                const decodium::sstv::SstvIncomingImportResult& result) {
                if (m_sstvStorageWorker != worker) {
                    return;
                }
                if (result.ok) {
                    m_sstvIncomingImportHandoffs.remove(result.transferId);
                    m_sstvIncomingImportRetryCounts.remove(result.transferId);
                    emit statusMessage(
                        result.idempotent
                            ? tr("Received SSTV image is already in Gallery: %1")
                                  .arg(result.record.mode)
                            : tr("Received SSTV image imported into Gallery: %1")
                                  .arg(result.record.mode));
                    return;
                }
                const QString detail = result.error.isEmpty()
                    ? tr("The validated incoming SSTV image could not be imported")
                    : result.error;
                constexpr int kMaximumAutomaticRetries = 2;
                const auto handoff = m_sstvIncomingImportHandoffs.constFind(
                    result.transferId);
                const int completedRetries
                    = m_sstvIncomingImportRetryCounts.value(
                        result.transferId, 0);
                if (result.retryable
                    && handoff != m_sstvIncomingImportHandoffs.cend()
                    && completedRetries < kMaximumAutomaticRetries) {
                    const int retryNumber = completedRetries + 1;
                    m_sstvIncomingImportRetryCounts.insert(
                        result.transferId, retryNumber);
                    const int delayMs = retryNumber == 1 ? 250 : 1'000;
                    const QString transferId = result.transferId;
                    QTimer::singleShot(
                        delayMs, this,
                        [this, transferId, retryNumber]() {
                            if (m_sstvIncomingImportRetryCounts.value(
                                    transferId, -1) != retryNumber) {
                                return;
                            }
                            if (!queueSstvIncomingImport(transferId)) {
                                m_sstvIncomingImportHandoffs.remove(transferId);
                                m_sstvIncomingImportRetryCounts.remove(
                                    transferId);
                                emit errorMessage(tr(
                                    "Incoming SSTV Gallery retry could not be "
                                    "queued; use Import again or restart "
                                    "Decodium"));
                            }
                        });
                    emit statusMessage(tr(
                        "Incoming SSTV Gallery import retry %1 of %2 scheduled: %3")
                                           .arg(retryNumber)
                                           .arg(kMaximumAutomaticRetries)
                                           .arg(detail));
                    return;
                }
                m_sstvIncomingImportHandoffs.remove(result.transferId);
                m_sstvIncomingImportRetryCounts.remove(result.transferId);
                emit errorMessage(
                    result.retryable
                        ? tr("Incoming SSTV Gallery import still needs retry; "
                             "use Import again or restart Decodium: %1")
                              .arg(detail)
                        : tr("Incoming SSTV Gallery import rejected: %1")
                              .arg(detail));
            }, Qt::QueuedConnection);
    connect(worker,
            &decodium::sstv::SstvStorageWorker::threadOwnershipViolation,
            this, [this](const QString& operation) {
                emit errorMessage(tr(
                    "SSTV storage thread ownership violation: %1")
                                      .arg(operation));
            }, Qt::QueuedConnection);
    m_sstvStorageThread->start();
    if (!QMetaObject::invokeMethod(
            worker, &decodium::sstv::SstvStorageWorker::initialize,
            Qt::QueuedConnection)) {
        const QString error = QStringLiteral(
            "could not queue SSTV storage initialization");
        m_sstvGalleryModel->setUnavailableError(error);
        setSstvRxSaveStatus(QStringLiteral("unavailable"), error);
        emit sstvStorageStateChanged();
    }
#endif
}

#if DECODIUM_HAS_SSTV
quint64 DecodiumBridge::enqueueSstvStoredRecord(
    decodium::sstv::SstvImageRecord record)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const QPointer<decodium::sstv::SstvStorageWorker> worker
        = m_sstvStorageWorker;
    if (!worker || !worker->isInitialized()) {
        return 0;
    }
    if (m_sstvStorageRequestId == 0) {
        m_sstvStorageRequestId = 1;
    }
    const quint64 requestId = m_sstvStorageRequestId++;
    if (!worker->enqueueDatabaseOperation(
            [record = std::move(record), requestId](
                decodium::sstv::SstvStorageWorker& storage) mutable {
                storage.insertRecord(std::move(record), requestId);
            })) {
        return 0;
    }
    return requestId;
}

bool DecodiumBridge::queueSstvIncomingImport(const QString& transferId)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const auto handoff = m_sstvIncomingImportHandoffs.constFind(transferId);
    const QPointer<decodium::sstv::SstvStorageWorker> worker
        = m_sstvStorageWorker;
    if (handoff == m_sstvIncomingImportHandoffs.cend() || !worker) {
        return false;
    }
    const QVariantMap immutableHandoff = *handoff;
    return worker->enqueueDatabaseOperation(
        [immutableHandoff](decodium::sstv::SstvStorageWorker& storage) {
            storage.importValidatedIncomingHandoff(immutableHandoff);
        });
}

void DecodiumBridge::setSstvRxSaveStatus(QString state, QString error)
{
    state = state.trimmed().toLower();
    error = error.trimmed().left(512);
    if (state.isEmpty()) {
        state = QStringLiteral("idle");
    }
    if (m_sstvRxSaveState == state && m_sstvRxSaveError == error) {
        return;
    }
    m_sstvRxSaveState = std::move(state);
    m_sstvRxSaveError = std::move(error);
    emit sstvRxSaveStateChanged();
}

bool DecodiumBridge::queueSstvRxImageSave(bool automatic)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const QPointer<decodium::sstv::SstvStorageWorker> worker =
        m_sstvStorageWorker;
    if (!m_sstvStorageReady || !worker || !worker->isInitialized()) {
        if (!automatic) {
            setSstvRxSaveStatus(
                QStringLiteral("error"),
                tr("SSTV storage is not ready"));
        }
        return false;
    }
    if (!m_sstvRxRuntime) {
        if (!automatic) {
            setSstvRxSaveStatus(
                QStringLiteral("error"), tr("SSTV RX is unavailable"));
        }
        return false;
    }

    const SstvRxRuntime::Snapshot before = m_sstvRxRuntime->snapshot();
    const std::shared_ptr<const decodium::sstv::SstvImageSnapshot> image =
        m_sstvRxRuntime->latestImageSnapshot();
    const SstvRxRuntime::Snapshot runtime = m_sstvRxRuntime->snapshot();
    const auto& summary = runtime.image;
    const bool coherentPublication =
        before.image.generation == summary.generation
        && before.image.acquisitionId == summary.acquisitionId
        && before.image.revision == summary.revision
        && image && image->revision == summary.revision
        && image->width == summary.width && image->height == summary.height;
    if (!summary.available || summary.acquisitionId == 0U
        || summary.coverage <= 0.0 || !coherentPublication) {
        if (!automatic) {
            setSstvRxSaveStatus(
                QStringLiteral("error"),
                summary.available
                    ? tr("The SSTV image is still changing; try saving again")
                    : tr("No received SSTV image is available to save"));
        }
        return false;
    }

    const bool terminal = summary.complete || summary.partial
        || summary.cancelled;
    if (automatic && !terminal) {
        return false;
    }
    const QString frameKey = QStringLiteral("%1:%2")
        .arg(summary.generation)
        .arg(summary.acquisitionId);
    const QString saveKey = terminal
        ? QStringLiteral("terminal:%1").arg(frameKey)
        : QStringLiteral("partial:%1:%2")
              .arg(frameKey)
              .arg(summary.revision);
    if (m_sstvRxSavedKeys.contains(saveKey)
        || m_sstvRxPendingSaveKeys.contains(saveKey)) {
        if (!automatic) {
            setSstvRxSaveStatus(QStringLiteral("saved"));
        }
        return false;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    decodium::sstv::SstvImageSaveRequest request;
    request.record.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    request.record.category = decodium::sstv::SstvImageCategory::Received;
    request.record.capturedAtUtc = now;
    if (m_sstvRxStartAcquisitionId == summary.acquisitionId
        && m_sstvRxStartUtc.isValid()) {
        request.record.capturedAtUtc = m_sstvRxStartUtc;
    }
    request.record.createdAtUtc = now;
    request.record.updatedAtUtc = now;
    request.record.mode = summary.mode.left(64);
    request.record.visCode = runtime.vis.primaryPayload >= 0
            && runtime.vis.primaryPayload <= 255
        ? runtime.vis.primaryPayload : -1;
    request.record.visValid = runtime.vis.valid;
    request.record.fskId = runtime.fskId.valid
        ? runtime.fskId.identifier.left(32) : QString {};
    request.record.localCallsign = m_callsign.trimmed().left(64);
    request.record.source = sstvSourceName(runtime.route.source.kind).left(128);
    request.record.frequencyHz = std::isfinite(m_frequency)
            && m_frequency > 0.0
            && m_frequency
                <= static_cast<double>(std::numeric_limits<qint64>::max())
        ? static_cast<qint64>(std::llround(m_frequency)) : 0;
    request.record.audioFrequencyHz =
        std::isfinite(runtime.afc.measuredOffsetHz)
        ? static_cast<qint64>(std::llround(runtime.afc.measuredOffsetHz))
        : 0;
    request.record.sourceSampleRateHz = runtime.activeSampleRate
            <= static_cast<std::uint32_t>(std::numeric_limits<int>::max())
        ? static_cast<int>(runtime.activeSampleRate) : 0;
    request.record.complete = summary.complete;
    request.record.completionPercent = summary.complete
        ? 100
        : std::clamp(
              static_cast<int>(std::floor(summary.coverage * 100.0)),
              0,
              99);
    request.record.slantCorrectionPpm =
        runtime.slant.appliedClockErrorPpm;
    request.record.qualityMetrics = {
        {QStringLiteral("afcMeasuredOffsetHz"),
         runtime.afc.measuredOffsetHz},
        {QStringLiteral("afcCorrectionHz"), runtime.afc.correctionHz},
        {QStringLiteral("afcConfidence"), runtime.afc.confidence},
        {QStringLiteral("slantMeasuredPpm"),
         runtime.slant.measuredClockErrorPpm},
        {QStringLiteral("slantConfidence"), runtime.slant.confidence},
        {QStringLiteral("syncConfidence"), runtime.sync.confidence},
        {QStringLiteral("signalSnrDb"), runtime.signal.snrDb},
        {QStringLiteral("signalRms"), runtime.signal.rms},
    };
    if (m_sstvRxRawAudioAcquisitionId == summary.acquisitionId
        && !m_sstvRxRawAudioPath.isEmpty()) {
        const decodium::sstv::SstvStorageLayout storageLayout(
            m_sstvStorageWorker->storageRoot());
        QString rawPathError;
        if (QFileInfo::exists(m_sstvRxRawAudioPath)
            && storageLayout.containsPath(
                m_sstvRxRawAudioPath, true, &rawPathError)) {
            request.record.rawAudioPath = m_sstvRxRawAudioPath;
        }
    }
    request.record.remote = false;
    if (!summary.complete) {
        request.record.note = summary.cancelled
            ? QStringLiteral("Native RX snapshot saved after cancellation")
            : QStringLiteral("Native RX partial snapshot");
    }
    request.fileNameTemplate = getSetting(
        QStringLiteral("SSTV/ImageNamingTemplate"),
        QStringLiteral("{date}_{time}_{mode}_{remoteCall}_{id}"))
                                   .toString()
                                   .trimmed();
    if (request.fileNameTemplate.isEmpty()) {
        request.fileNameTemplate = QStringLiteral(
            "{date}_{time}_{mode}_{remoteCall}_{id}");
    }

    if (m_sstvStorageRequestId == 0U) {
        m_sstvStorageRequestId = 1U;
    }
    const quint64 requestId = m_sstvStorageRequestId++;
    m_sstvRxSaveRequests.insert(requestId, saveKey);
    m_sstvRxPendingSaveKeys.insert(saveKey);
    setSstvRxSaveStatus(QStringLiteral("saving"));
    if (!worker->enqueueDatabaseOperation(
            [image, request = std::move(request), requestId](
                decodium::sstv::SstvStorageWorker& storage) mutable {
                request.image = sstvSnapshotToImage(*image);
                storage.storeAndInsertImage(std::move(request), requestId);
            })) {
        m_sstvRxSaveRequests.remove(requestId);
        m_sstvRxPendingSaveKeys.remove(saveKey);
        setSstvRxSaveStatus(
            QStringLiteral("error"),
            tr("Could not queue the SSTV image storage operation"));
        return false;
    }
    return true;
}

void DecodiumBridge::handleSstvStudioPreparedChanged()
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (!m_sstvStudioController || !m_sstvStudioController->preparedReady()) {
        return;
    }
    decodium::sstv::advanceSstvTxDraftGeneration(
        m_sstvStudioPreparedGeneration,
        m_sstvStudioDraftQueuedGeneration);
    queueSstvStudioDraftImage();
}

void DecodiumBridge::queueSstvStudioDraftImage()
{
    Q_ASSERT(QThread::currentThread() == thread());
    const QPointer<decodium::sstv::SstvStorageWorker> worker
        = m_sstvStorageWorker;
    const std::shared_ptr<const QImage> prepared
        = m_sstvStudioController
        ? m_sstvStudioController->preparedSnapshot()
        : std::shared_ptr<const QImage> {};
    if (!m_sstvStorageReady || !worker || !worker->isInitialized()
        || !prepared || prepared->isNull()) {
        return;
    }
    if (!decodium::sstv::sstvTxDraftNeedsArchive(
            m_sstvStudioPreparedGeneration,
            m_sstvStudioDraftQueuedGeneration)) {
        return;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    decodium::sstv::SstvTxGalleryArchiveContext context;
    context.eventAtUtc = now;
    context.mode = m_sstvStudioController->modeId().left(64);
    context.localCallsign = m_callsign.trimmed().left(64);
    context.localGrid = m_grid.trimmed().left(16);
    context.source = QStringLiteral("sstv-studio");
    context.frequencyHz = std::isfinite(m_frequency)
            && m_frequency > 0.0
            && m_frequency
                <= static_cast<double>(std::numeric_limits<qint64>::max())
        ? static_cast<qint64>(std::llround(m_frequency)) : 0;
    context.qualityMetrics = {
        {QStringLiteral("audioToneLowHz"), 1'200.0},
        {QStringLiteral("audioToneCentreHz"), 1'900.0},
        {QStringLiteral("audioToneHighHz"), 2'300.0},
        {QStringLiteral("txAccepted"), 0.0},
    };
    context.fileNameTemplate = getSetting(
        QStringLiteral("SSTV/ImageNamingTemplate"),
        QStringLiteral("{date}_{time}_{mode}_{remoteCall}_{id}"))
                                   .toString()
                                   .trimmed();
    const auto archive = decodium::sstv::makeSstvTxGalleryArchiveRequest(
        *prepared, decodium::sstv::SstvImageCategory::Draft, context);
    if (!archive.has_value()) {
        emit errorMessage(tr("The prepared SSTV Studio image could not be archived"));
        return;
    }

    if (m_sstvStorageRequestId == 0U) {
        m_sstvStorageRequestId = 1U;
    }
    const quint64 requestId = m_sstvStorageRequestId++;
    decodium::sstv::SstvImageSaveRequest request = *archive;
    if (!worker->enqueueDatabaseOperation(
            [prepared, request = std::move(request), requestId](
                decodium::sstv::SstvStorageWorker& storage) mutable {
                request.image = *prepared;
                storage.storeAndInsertImage(std::move(request), requestId);
            })) {
        emit errorMessage(tr("Could not queue the prepared SSTV Studio image for Gallery storage"));
        return;
    }
    m_sstvStudioDraftQueuedGeneration = m_sstvStudioPreparedGeneration;
    m_sstvTxGallerySaveRequests.insert(requestId, QStringLiteral("draft"));
}

void DecodiumBridge::queueSstvStudioTransmittedImage(
    std::shared_ptr<const QImage> prepared,
    QString fskId,
    quint64 sessionId)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const QPointer<decodium::sstv::SstvStorageWorker> worker
        = m_sstvStorageWorker;
    if (!m_sstvStorageReady || !worker || !worker->isInitialized()
        || !prepared || prepared->isNull() || !m_sstvStudioController) {
        emit statusMessage(tr(
            "SSTV TX was accepted, but Gallery storage is not ready; "
            "the transmitted image was not archived"));
        return;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    decodium::sstv::SstvTxGalleryArchiveContext context;
    context.eventAtUtc = now;
    context.mode = m_sstvStudioController->modeId().left(64);
    context.fskId = fskId.trimmed().left(128);
    context.localCallsign = m_callsign.trimmed().left(64);
    context.localGrid = m_grid.trimmed().left(16);
    context.source = QStringLiteral("sstv-studio");
    context.frequencyHz = std::isfinite(m_frequency)
            && m_frequency > 0.0
            && m_frequency
                <= static_cast<double>(std::numeric_limits<qint64>::max())
        ? static_cast<qint64>(std::llround(m_frequency)) : 0;
    context.qualityMetrics = {
        {QStringLiteral("audioToneLowHz"), 1'200.0},
        {QStringLiteral("audioToneCentreHz"), 1'900.0},
        {QStringLiteral("audioToneHighHz"), 2'300.0},
        {QStringLiteral("txAccepted"), 1.0},
        {QStringLiteral("txSessionId"),
         static_cast<double>(std::min<quint64>(
             sessionId, static_cast<quint64>(9'007'199'254'740'991ULL)))},
    };
    context.fileNameTemplate = getSetting(
        QStringLiteral("SSTV/ImageNamingTemplate"),
        QStringLiteral("{date}_{time}_{mode}_{remoteCall}_{id}"))
                                   .toString()
                                   .trimmed();
    const auto archive = decodium::sstv::makeSstvTxGalleryArchiveRequest(
        *prepared, decodium::sstv::SstvImageCategory::Transmitted, context);
    if (!archive.has_value()) {
        emit errorMessage(tr("The accepted SSTV TX image could not be archived"));
        return;
    }

    if (m_sstvStorageRequestId == 0U) {
        m_sstvStorageRequestId = 1U;
    }
    const quint64 requestId = m_sstvStorageRequestId++;
    decodium::sstv::SstvImageSaveRequest request = *archive;
    if (!worker->enqueueDatabaseOperation(
            [prepared, request = std::move(request), requestId](
                decodium::sstv::SstvStorageWorker& storage) mutable {
                request.image = *prepared;
                storage.storeAndInsertImage(std::move(request), requestId);
            })) {
        emit errorMessage(tr("Could not queue the accepted SSTV TX image for Gallery storage"));
        return;
    }
    m_sstvTxGallerySaveRequests.insert(requestId,
                                       QStringLiteral("transmitted"));
}

void DecodiumBridge::maybeAutoSaveSstvRxImage()
{
    if (m_sstvRxAutoSaveEnabled && m_sstvStorageReady) {
        static_cast<void>(queueSstvRxImageSave(true));
    }
}
#endif

bool DecodiumBridge::saveSstvRxImage()
{
#if DECODIUM_HAS_SSTV
    return queueSstvRxImageSave(false);
#else
    return false;
#endif
}

QVariantList DecodiumBridge::sstvExistingQsoChoices(
    const QString& search, int maximumRows)
{
#if DECODIUM_HAS_SSTV
    const int limit = qBound(1, maximumRows, 100);
    const QString path = effectiveAdifLogPath();
    const QFileInfo info(path);
    const QDateTime modified = info.exists()
        ? info.lastModified() : QDateTime {};
    const qint64 size = info.exists() ? info.size() : 0;
    QVariantList rows;
    bool hadReadyCache = false;
    {
        QMutexLocker locker(&m_qsoSearchCacheMutex);
        hadReadyCache = m_qsoSearchCacheReady;
        if (m_qsoSearchCacheReady
            && m_qsoSearchCachePath == path
            && m_qsoSearchCacheModified == modified
            && m_qsoSearchCacheSize == size) {
            rows = m_qsoSearchCacheRows;
        }
    }
    if (!hadReadyCache || rows.isEmpty()) {
        // An empty active logbook is a valid ready snapshot. Distinguish it
        // from a cache miss without ever parsing ADIF on the QML thread.
        bool cacheCurrent = false;
        {
            QMutexLocker locker(&m_qsoSearchCacheMutex);
            cacheCurrent = m_qsoSearchCacheReady
                && m_qsoSearchCachePath == path
                && m_qsoSearchCacheModified == modified
                && m_qsoSearchCacheSize == size;
        }
        if (!cacheCurrent) {
            m_sstvIssuedExistingQsoIds.clear();
            if (hadReadyCache) {
                invalidateQsoSearchCache();
            }
            warmLogCacheAsync();
            return {};
        }
    }

    const quint64 generation =
        m_qsoSearchCacheGeneration.load(std::memory_order_relaxed);
    if (m_sstvIssuedExistingQsoGeneration != generation) {
        m_sstvIssuedExistingQsoIds.clear();
        m_sstvIssuedExistingQsoGeneration = generation;
    }
    const QString needle = search.trimmed().left(128).toUpper();
    QVariantList choices;
    choices.reserve(std::min(limit, static_cast<int>(rows.size())));
    for (const QVariant& rowValue : rows) {
        QVariantMap row = rowValue.toMap();
        if (!needle.isEmpty()) {
            const QString searchable = QStringLiteral("%1\n%2\n%3\n%4\n%5\n%6")
                .arg(row.value(QStringLiteral("call")).toString(),
                     row.value(QStringLiteral("grid")).toString(),
                     row.value(QStringLiteral("comment")).toString(),
                     row.value(QStringLiteral("dateTime")).toString(),
                     row.value(QStringLiteral("band")).toString(),
                     row.value(QStringLiteral("mode")).toString())
                .toUpper();
            if (!searchable.contains(needle)) {
                continue;
            }
        }
        QString error;
        const QString qsoId = decodium::sstv::SstvQsoLog::associationIdForExistingQso(
            row.value(QStringLiteral("call")).toString(),
            row.value(QStringLiteral("dateTime")).toString(),
            row.value(QStringLiteral("mode")).toString(),
            row.value(QStringLiteral("band")).toString(), &error);
        if (qsoId.isEmpty()) {
            continue;
        }
        row.insert(QStringLiteral("qsoId"), qsoId);
        choices.append(row);
        m_sstvIssuedExistingQsoIds.insert(qsoId);
        if (choices.size() >= limit) {
            break;
        }
    }
    constexpr qsizetype kMaximumIssuedChoices = 4'096;
    if (m_sstvIssuedExistingQsoIds.size() > kMaximumIssuedChoices) {
        m_sstvIssuedExistingQsoIds.clear();
        for (const QVariant& choice : choices) {
            m_sstvIssuedExistingQsoIds.insert(
                choice.toMap().value(QStringLiteral("qsoId")).toString());
        }
    }
    return choices;
#else
    Q_UNUSED(search)
    Q_UNUSED(maximumRows)
    return {};
#endif
}

#if DECODIUM_HAS_SSTV
bool DecodiumBridge::isSstvExistingQsoChoiceCurrent(
    const QString& qsoId) const
{
    const QString normalized = qsoId.trimmed();
    const quint64 generation =
        m_qsoSearchCacheGeneration.load(std::memory_order_relaxed);
    if (normalized.isEmpty()
        || m_sstvIssuedExistingQsoGeneration != generation
        || !m_sstvIssuedExistingQsoIds.contains(normalized)) {
        return false;
    }

    const QString path = effectiveAdifLogPath();
    const QFileInfo info(path);
    const QDateTime modified = info.exists()
        ? info.lastModified() : QDateTime {};
    const qint64 size = info.exists() ? info.size() : 0;
    QMutexLocker locker(&m_qsoSearchCacheMutex);
    return m_qsoSearchCacheReady
        && m_qsoSearchCachePath == path
        && m_qsoSearchCacheModified == modified
        && m_qsoSearchCacheSize == size;
}
#endif

QVariantMap DecodiumBridge::logSstvQso(const QVariantMap& values)
{
    QVariantMap response {
        {QStringLiteral("accepted"), false},
        {QStringLiteral("requestId"), QString {}},
        {QStringLiteral("error"), QString {}}
    };
#if DECODIUM_HAS_SSTV
    if (m_shuttingDown || QCoreApplication::closingDown()) {
        response.insert(QStringLiteral("error"),
                        tr("Decodium is shutting down"));
        return response;
    }
    if (!m_sstvStorageReady || !m_sstvStorageWorker
        || !m_sstvStorageWorker->isInitialized()) {
        response.insert(QStringLiteral("error"),
                        tr("SSTV Gallery storage is not ready"));
        return response;
    }
    constexpr qsizetype kMaximumPendingQsoLogs = 32;
    if (m_sstvQsoLogRequests.size() >= kMaximumPendingQsoLogs) {
        response.insert(QStringLiteral("error"),
                        tr("Too many SSTV QSO operations are already pending"));
        return response;
    }

    decodium::sstv::SstvQsoLogRequest request;
    QString validationError;
    if (!sstvQsoRequestFromMap(values, &request, &validationError)) {
        response.insert(QStringLiteral("error"), validationError.left(512));
        return response;
    }
    if (!request.createNewQso
        && !isSstvExistingQsoChoiceCurrent(request.existingQsoId)) {
        m_sstvIssuedExistingQsoIds.clear();
        invalidateQsoSearchCache();
        warmLogCacheAsync();
        response.insert(
            QStringLiteral("error"),
            tr("Select an existing QSO from the current Decodium logbook list"));
        return response;
    }
    for (auto it = m_sstvQsoLogRequests.cbegin();
         it != m_sstvQsoLogRequests.cend(); ++it) {
        const QVariantMap queued = it->value(
            QStringLiteral("request")).toMap();
        if (queued.value(QStringLiteral("imageRecordId")).toString()
                == request.imageRecordId) {
            response.insert(QStringLiteral("error"),
                            tr("This SSTV image already has a pending QSO operation"));
            return response;
        }
    }

    constexpr quint64 kRequestNamespace = 0x5353545600000000ULL;
    constexpr quint64 kSerialMask = 0x00000000ffffffffULL;
    quint64 requestId = 0U;
    for (int attempt = 0; attempt < 64 && requestId == 0U; ++attempt) {
        const quint64 serial = m_sstvQsoStorageSerial++ & kSerialMask;
        if (serial == 0U) {
            continue;
        }
        const quint64 candidate = kRequestNamespace | serial;
        if (!m_sstvQsoLogRequests.contains(candidate)) {
            requestId = candidate;
        }
    }
    if (requestId == 0U) {
        response.insert(QStringLiteral("error"),
                        tr("Could not allocate an SSTV QSO request"));
        return response;
    }

    QVariantMap pending {
        {QStringLiteral("phase"), QStringLiteral("fetch")},
        {QStringLiteral("request"), values},
        {QStringLiteral("qsoCreated"), false},
        {QStringLiteral("qsoId"), QString {}}
    };
    m_sstvQsoLogRequests.insert(requestId, std::move(pending));
    const QPointer<decodium::sstv::SstvStorageWorker> worker
        = m_sstvStorageWorker;
    if (!worker->enqueueDatabaseOperation(
            [imageRecordId = request.imageRecordId, requestId](
                decodium::sstv::SstvStorageWorker& storage) {
                storage.fetchRecord(imageRecordId, requestId);
            })) {
        m_sstvQsoLogRequests.remove(requestId);
        response.insert(QStringLiteral("error"),
                        tr("Could not queue the SSTV Gallery preflight"));
        return response;
    }
    response.insert(QStringLiteral("accepted"), true);
    response.insert(QStringLiteral("requestId"), QString::number(requestId));
    return response;
#else
    Q_UNUSED(values)
    response.insert(QStringLiteral("error"),
                    tr("This Decodium build has no native SSTV support"));
    return response;
#endif
}

#if DECODIUM_HAS_SSTV
bool DecodiumBridge::queueSstvQsoAssociation(
    quint64 requestId,
    const QString& imageRecordId,
    const QString& qsoId)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const QPointer<decodium::sstv::SstvStorageWorker> worker
        = m_sstvStorageWorker;
    if (!worker || !worker->isInitialized()
        || !m_sstvQsoLogRequests.contains(requestId)
        || imageRecordId.isEmpty() || qsoId.isEmpty()) {
        return false;
    }
    return worker->enqueueDatabaseOperation(
        [imageRecordId, qsoId, requestId](
            decodium::sstv::SstvStorageWorker& storage) {
            storage.associateWithQso(imageRecordId, qsoId, requestId);
        });
}

void DecodiumBridge::finishSstvQsoLog(
    quint64 requestId,
    bool associationStored,
    const QString& error)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const QVariantMap pending = m_sstvQsoLogRequests.take(requestId);
    if (pending.isEmpty()) {
        return;
    }
    const QVariantMap request = pending.value(
        QStringLiteral("request")).toMap();
    const QString imageRecordId = request.value(
        QStringLiteral("imageRecordId")).toString();
    const QString qsoId = pending.value(QStringLiteral("qsoId")).toString();
    const bool qsoCreated = pending.value(
        QStringLiteral("qsoCreated")).toBool();
    QString detail = error.trimmed().left(1'024);
    if (!associationStored && detail.isEmpty()) {
        detail = tr("The SSTV QSO operation failed");
    }
    if (associationStored) {
        emit statusMessage(
            qsoCreated
                ? tr("SSTV QSO logged and image associated locally")
                : tr("SSTV image associated with the selected QSO"));
    } else {
        emit errorMessage(
            qsoCreated
                ? tr("SSTV QSO was logged, but image association failed: %1")
                      .arg(detail)
                : tr("SSTV QSO operation failed: %1").arg(detail));
    }
    emit sstvQsoLogFinished(QString::number(requestId), imageRecordId, qsoCreated,
                            associationStored, qsoId, detail);
}
#endif

void DecodiumBridge::disconnectSstvProducerTaps()
{
#if DECODIUM_HAS_SSTV
    Q_ASSERT(QThread::currentThread() == thread());
    auto disconnectTap = [](QMetaObject::Connection& connection) {
        if (connection) {
            QObject::disconnect(connection);
            connection = {};
        }
    };
    disconnectTap(m_sstvLocalAudioTap);
    disconnectTap(m_sstvLegacyAudioTap);
    disconnectTap(m_sstvTciAudioTap);
    disconnectTap(m_sstvDecoPortAudioTap);
    disconnectTap(m_sstvRtlPcmTap);
    disconnectTap(m_sstvRtlAudioTap);
#endif
}

void DecodiumBridge::disableSstvProducerTaps()
{
#if DECODIUM_HAS_SSTV
    Q_ASSERT(QThread::currentThread() == thread());
    disconnectSstvProducerTaps();
    if (m_sstvAudioRelay) {
        m_sstvAudioRelay->disableAndDrain();
    }
#endif
}

#if DECODIUM_HAS_SSTV && DECODIUM_HAS_HAMDRM
decodium::sstv::hamdrm::HamDrmStatus
DecodiumBridge::activateHamDrmRxTap()
{
    Q_ASSERT(QThread::currentThread() == thread());
    using decodium::sstv::hamdrm::HamDrmErrorCode;
    using decodium::sstv::hamdrm::HamDrmStatus;
    if (!m_hamDrmRxBackend || !m_sstvAudioRelay
        || m_shuttingDown || QCoreApplication::closingDown()) {
        return HamDrmStatus::failure(
            HamDrmErrorCode::IoFailure,
            "Decodium shared RX audio is unavailable");
    }
    if (sstvWavReplayActive()) {
        return HamDrmStatus::failure(
            HamDrmErrorCode::UnsupportedFeature,
            "HAMDRM live RX cannot replace an active SSTV WAV replay");
    }
    if (m_hamDrmRxRequested) {
        return HamDrmStatus::success();
    }

    const SstvAudioSourceKind kind = currentSstvAudioSourceKind();
    const quint32 streamId = currentSstvAudioStreamId(kind);
    if (kind == SstvAudioSourceKind::Unknown || streamId == 0U) {
        return HamDrmStatus::failure(
            HamDrmErrorCode::IoFailure,
            "no Decodium RX audio source is available for HAMDRM");
    }
    const bool monitoringWasAlreadyRequested =
        m_monitoring || m_monitorRequested;
    m_hamDrmRxRequested = true;
    m_hamDrmOwnsMonitoring = !monitoringWasAlreadyRequested;
    m_hamDrmRxSourceKind = static_cast<std::uint8_t>(kind);
    m_hamDrmRxStreamId = streamId;
    refreshSstvProducerTaps();
    if (!m_monitoring) {
        setMonitoring(true);
    }
    if (!m_monitoring) {
        m_hamDrmRxRequested = false;
        m_hamDrmOwnsMonitoring = false;
        m_hamDrmRxSourceKind = static_cast<std::uint8_t>(
            SstvAudioSourceKind::Unknown);
        m_hamDrmRxStreamId = 0U;
        refreshSstvProducerTaps();
        return HamDrmStatus::failure(
            HamDrmErrorCode::IoFailure,
            "HAMDRM RX requires an active Decodium monitor");
    }
    const SstvAudioSourceKind selected = currentSstvAudioSourceKind();
    m_hamDrmRxSourceKind = static_cast<std::uint8_t>(selected);
    m_hamDrmRxStreamId = currentSstvAudioStreamId(selected);
    refreshSstvProducerTaps();
    emit statusMessage(tr("HAMDRM RX attached to the Decodium audio monitor"));
    return HamDrmStatus::success();
}

void DecodiumBridge::deactivateHamDrmRxTap()
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (!m_hamDrmRxRequested) {
        return;
    }
    const bool transferMonitoring = m_hamDrmOwnsMonitoring
        && m_sstvRxRequested;
    const bool stopOwnedMonitor = m_hamDrmOwnsMonitoring
        && !m_sstvRxRequested && (m_monitoring || m_monitorRequested);
    if (transferMonitoring) {
        m_sstvOwnsMonitoring = true;
    }
    m_hamDrmRxRequested = false;
    m_hamDrmOwnsMonitoring = false;
    m_hamDrmRxSourceKind = static_cast<std::uint8_t>(
        SstvAudioSourceKind::Unknown);
    m_hamDrmRxStreamId = 0U;
    disableSstvProducerTaps();
    refreshSstvProducerTaps();
    if (stopOwnedMonitor) {
        setMonitoring(false);
    }
    emit statusMessage(tr("HAMDRM RX detached from the Decodium audio monitor"));
}
#endif

void DecodiumBridge::refreshSstvProducerTaps()
{
#if DECODIUM_HAS_SSTV
    Q_ASSERT(QThread::currentThread() == thread());
    // A tap rebind is a real producer-generation boundary, not merely a Qt
    // signal-list update.  A DirectConnection callback may already be inside
    // DecodiumSstvAudioRelay when QObject::disconnect() returns.  Drain that
    // callback before publishing a new route/backend: analog ingress rejects
    // stale route tokens, but the separate HAMDRM PCM backend deliberately
    // receives raw samples and has no analog-route token to reject.
    disableSstvProducerTaps();
    const bool analogActive = m_sstvRxRequested && m_sstvRxRuntime
        && m_sstvRxRuntime->state() == SstvRxRuntime::State::Running;
#if DECODIUM_HAS_HAMDRM
    const bool digitalActive = m_hamDrmRxRequested && m_hamDrmRxBackend
        && m_hamDrmRxBackend->active();
#else
    constexpr bool digitalActive = false;
#endif
    if ((!analogActive && !digitalActive) || !m_sstvAudioRelay) {
        return;
    }

    decodium::sstv::SstvRxRouteToken token;
    SstvAudioSourceKind selectedKind = SstvAudioSourceKind::Unknown;
    quint32 selectedStreamId = 0U;
    if (analogActive) {
        token = m_sstvRxRuntime->routeToken();
        if (!token.valid()) {
            return;
        }
        selectedKind = token.source.kind;
        selectedStreamId = token.source.streamId;
#if DECODIUM_HAS_HAMDRM
    } else {
        selectedKind = static_cast<SstvAudioSourceKind>(
            m_hamDrmRxSourceKind);
        selectedStreamId = m_hamDrmRxStreamId;
        if (selectedKind == SstvAudioSourceKind::Unknown
            || selectedStreamId == 0U) {
            return;
        }
#endif
    }
    if (selectedKind == SstvAudioSourceKind::Replay
        || sstvWavReplayActive()) {
        // Replay is the sole producer for this generation. Keep every live
        // DirectConnection detached and close the relay gate as an additional
        // guard against a callback already captured by a previous source.
        m_sstvAudioRelay->disableAndDrain();
        return;
    }
    const std::shared_ptr<DecodiumSstvAudioRelay> relay = m_sstvAudioRelay;
#if DECODIUM_HAS_HAMDRM
    relay->enable(analogActive ? m_sstvRxRuntime.get() : nullptr,
                  digitalActive ? m_hamDrmRxBackend : nullptr);
#else
    relay->enable(analogActive ? m_sstvRxRuntime.get() : nullptr);
#endif

    switch (selectedKind) {
    case SstvAudioSourceKind::LocalSoundCard:
    case SstvAudioSourceKind::WebSdr:
        if (m_audioSink) {
            m_sstvLocalAudioTap = connect(
                m_audioSink, &DecodiumAudioSink::audioSamplesProduced,
                this, [relay, token](QVector<short> samples) {
                    relay->submit(std::move(samples), SAMPLE_RATE, token);
                }, Qt::DirectConnection);
        }
        break;
    case SstvAudioSourceKind::LegacyBackend:
        if (m_legacyBackend) {
            m_sstvLegacyAudioTap = connect(
                m_legacyBackend, &DecodiumLegacyBackend::audioSamplesProduced,
                this, [relay, token](const QByteArray& pcmBytes) {
                    const int sampleCount = pcmBytes.size()
                        / static_cast<int>(sizeof(qint16));
                    if (sampleCount <= 0) {
                        return;
                    }
                    QVector<short> samples;
                    samples.reserve(sampleCount);
                    const auto* bytes = reinterpret_cast<const uchar*>(
                        pcmBytes.constData());
                    for (int index = 0; index < sampleCount; ++index) {
                        samples.append(qFromLittleEndian<qint16>(
                            bytes + index * static_cast<int>(sizeof(qint16))));
                    }
                    relay->submit(std::move(samples), SAMPLE_RATE, token);
                }, Qt::DirectConnection);
        }
        break;
    case SstvAudioSourceKind::Tci:
        if (m_hamlibCat) {
            m_sstvTciAudioTap = connect(
                m_hamlibCat,
                &DecodiumTransceiverManager::tciPcmSamplesProduced,
                this, [relay, token](const QVector<short>& samples) {
                    relay->submit(QVector<short>(samples), SAMPLE_RATE, token);
                }, Qt::DirectConnection);
        }
        break;
    case SstvAudioSourceKind::DecoPort:
        if (m_decoPortLink) {
            m_sstvDecoPortAudioTap = connect(
                m_decoPortLink, &DecoPortLink::rxAudioProduced,
                this, [relay, token, selectedStreamId](const QVector<short>& samples,
                                      quint64, quint32 streamId) {
                    if (streamId != selectedStreamId) {
                        return;
                    }
                    relay->submit(QVector<short>(samples), SAMPLE_RATE, token);
                }, Qt::DirectConnection);
        }
        break;
    case SstvAudioSourceKind::RtlSdr:
        if (m_rtlSdrInput) {
            if (rtlSdrGeneralReceiverConfigured()) {
                m_sstvRtlAudioTap = connect(
                    m_rtlSdrInput, &RtlSdrInput::audioSamplesProduced,
                    this, [relay, token](QVector<short> samples,
                                          int sampleRate) {
                        relay->submit(std::move(samples), sampleRate, token);
                    }, Qt::DirectConnection);
            } else {
                m_sstvRtlPcmTap = connect(
                    m_rtlSdrInput, &RtlSdrInput::pcmSamplesProduced,
                    this, [relay, token](QVector<short> samples) {
                        relay->submit(std::move(samples), SAMPLE_RATE, token);
                    }, Qt::DirectConnection);
            }
        }
        break;
    case SstvAudioSourceKind::Replay:
    case SstvAudioSourceKind::Unknown:
        break;
    }
#endif
}

void DecodiumBridge::shutdownSstvRuntime()
{
    m_sstvRxRequested = false;
    m_sstvOwnsMonitoring = false;
#if DECODIUM_HAS_SSTV
#if DECODIUM_HAS_HAMDRM
    m_hamDrmRxRequested = false;
    m_hamDrmOwnsMonitoring = false;
#endif
    if (m_sstvDiagnosticsController) {
        m_sstvDiagnosticsController->shutdown();
    }
    if (m_sstvRxAudioJobController) {
        m_sstvRxAudioJobController->shutdown();
    }
    if (m_sstvWavReplayController) {
        m_sstvWavReplayRestoreLive = false;
        m_sstvWavReplayController->shutdown();
    }
    if (m_sstvStudioController) {
        m_sstvStudioController->cancelWork();
    }
    if (m_sstvShareController) {
        m_sstvShareController->shutdown();
    }
    disableSstvProducerTaps();
#if DECODIUM_HAS_HAMDRM
    if (m_hamDrmRxBackend) {
        m_hamDrmRxBackend->shutdown();
    }
    if (m_hamDrmTxBackend) {
        m_hamDrmTxBackend->shutdown();
    }
#endif
    shutdownSstvTx();
    if (m_sstvRxRuntime) {
        m_sstvRxRuntime->shutdown();
    }
    shutdownSstvStorage();
#endif
}

void DecodiumBridge::shutdownSstvStorage()
{
#if DECODIUM_HAS_SSTV
    Q_ASSERT(QThread::currentThread() == thread());
    const bool wasReady = m_sstvStorageReady;
    m_sstvStorageReady = false;
    const QList<quint64> pendingQsoRequests
        = m_sstvQsoLogRequests.keys();
    for (quint64 requestId : pendingQsoRequests) {
        finishSstvQsoLog(
            requestId, false,
            tr("Decodium shut down before the SSTV QSO operation completed"));
    }
    m_sstvIssuedExistingQsoIds.clear();
    if (m_sstvGalleryModel) {
        m_sstvGalleryModel->setThumbnailProvider(nullptr);
        m_sstvGalleryModel->shutdown();
    }
    if (m_sstvThumbnailProvider) {
        m_sstvThumbnailProvider->shutdown();
        m_sstvThumbnailProvider = nullptr;
    }

    const QPointer<decodium::sstv::SstvStorageWorker> worker
        = m_sstvStorageWorker;
    if (worker && m_sstvStorageThread
        && m_sstvStorageThread->isRunning()) {
        QMetaObject::invokeMethod(
            worker.data(), &decodium::sstv::SstvStorageWorker::shutdown,
            Qt::BlockingQueuedConnection);
    }
    if (m_sstvStorageThread) {
        m_sstvStorageThread->quit();
        m_sstvStorageThread->wait();
        delete m_sstvStorageThread;
        m_sstvStorageThread = nullptr;
    }
    m_sstvStorageWorker = nullptr;
    m_sstvRxSaveRequests.clear();
    m_sstvTxGallerySaveRequests.clear();
    m_sstvIncomingImportHandoffs.clear();
    m_sstvIncomingImportRetryCounts.clear();
    m_sstvRxPendingSaveKeys.clear();
    if (wasReady) {
        emit sstvStorageStateChanged();
    }
#endif
}

decodium::sstv::SstvAudioSourceKind DecodiumBridge::currentSstvAudioSourceKind() const
{
#if DECODIUM_HAS_SSTV
    if (m_decoPortUseRemote) {
        return SstvAudioSourceKind::DecoPort;
    }
    // Mirror startAudioCapture(): an enabled RTL path falls through only when
    // discovery is complete, no dongle exists, and TCI audio is available.
    if (rtlSdrEnabled()
        && (m_rtlSdrDiscoveryPending
            || !m_rtlSdrDevices.isEmpty()
            || !usingTciAudioInput())) {
        return SstvAudioSourceKind::RtlSdr;
    }
    // A live native SSTV session is deliberately independent from the
    // application mode's legacy decoder.  In particular, FT8/FT2 normally
    // expose only the legacy PCM tap and startAudioCapture() is skipped; that
    // route is not a stable source for the bounded SSTV worker.  Once SSTV RX
    // has been requested, prefer the Decodium-owned local capture (unless a
    // higher-priority explicit source such as TCI/RTL/DecoPort was selected).
#ifdef Q_OS_LINUX
    if (m_audioInputDevice.startsWith(QStringLiteral("Pulse/PipeWire monitor: "),
                                      Qt::CaseInsensitive)) {
        return SstvAudioSourceKind::WebSdr;
    }
#endif
    if (m_sstvRxRequested && !usingTciAudioInput()) {
        return SstvAudioSourceKind::LocalSoundCard;
    }
    if (usingLegacyBackendForRx()
        && (!useModernSpectrumFeedWithLegacy()
            || !useDedicatedModernAudioCaptureWithLegacy())) {
        return SstvAudioSourceKind::LegacyBackend;
    }
    if (usingTciAudioInput()) {
        return SstvAudioSourceKind::Tci;
    }
    return SstvAudioSourceKind::LocalSoundCard;
#else
    return static_cast<decodium::sstv::SstvAudioSourceKind>(0U);
#endif
}

bool DecodiumBridge::nativeSstvRxForcesDedicatedAudioCapture() const
{
#if DECODIUM_HAS_SSTV
    // The flag is intentionally narrow: it is only true while analog SSTV
    // owns a live RX session.  HAMDRM and normal legacy/FT8 monitoring keep
    // their existing source arbitration.
    return m_sstvRxRequested
        && !m_decoPortUseRemote
        && !rtlSdrEnabled()
        && !usingTciAudioInput();
#else
    return false;
#endif
}

quint32 DecodiumBridge::currentSstvAudioStreamId(
    decodium::sstv::SstvAudioSourceKind kind) const
{
#if DECODIUM_HAS_SSTV
    switch (kind) {
    case SstvAudioSourceKind::LocalSoundCard:
    case SstvAudioSourceKind::WebSdr: {
        QString identity = m_activeRxInputDeviceId.isEmpty()
            ? m_audioInputDeviceId : m_activeRxInputDeviceId;
        if (identity.isEmpty()) {
            identity = m_activeRxInputDeviceName.isEmpty()
                ? m_audioInputDevice : m_activeRxInputDeviceName;
        }
        return stableSstvStreamId(identity.isEmpty()
                                      ? QStringView(u"decodium-default-input")
                                      : QStringView(identity));
    }
    case SstvAudioSourceKind::DecoPort:
        if (m_decoPortLink && m_decoPortLink->streamId() != 0U) {
            return m_decoPortLink->streamId();
        }
        if (m_decoPortLink && !m_decoPortLink->peerAddress().isEmpty()) {
            return stableSstvStreamId(QStringView(m_decoPortLink->peerAddress()));
        }
        return stableSstvStreamId(QStringView(u"decodium-decoport-rx"));
    case SstvAudioSourceKind::Tci:
        return stableSstvStreamId(QStringView(u"decodium-tci-rx"));
    case SstvAudioSourceKind::RtlSdr:
        return stableSstvStreamId(QStringView(u"decodium-rtl-sdr-rx"));
    case SstvAudioSourceKind::LegacyBackend:
        return stableSstvStreamId(QStringView(u"decodium-legacy-rx"));
    case SstvAudioSourceKind::Replay:
        return stableSstvStreamId(QStringView(u"decodium-replay"));
    case SstvAudioSourceKind::Unknown:
        break;
    }
#else
    Q_UNUSED(kind)
#endif
    return 0U;
}

void DecodiumBridge::selectSstvRxSource(
    decodium::sstv::SstvAudioSourceKind kind,
    quint32 streamId,
    bool resetExistingStream)
{
#if DECODIUM_HAS_SSTV
#if DECODIUM_HAS_HAMDRM
    const bool digitalActive = m_hamDrmRxRequested && m_hamDrmRxBackend
        && m_hamDrmRxBackend->active();
#else
    constexpr bool digitalActive = false;
#endif
    const bool analogActive = m_sstvRxRequested && m_sstvRxRuntime;
    if (!analogActive && !digitalActive) {
        return;
    }
    if (sstvWavReplayActive()) {
        // Hardware/source telemetry may continue while a file is decoded.
        // It must not steal or relabel the immutable Replay generation.
        return;
    }
    if (kind == SstvAudioSourceKind::Unknown || streamId == 0U) {
        emit errorMessage(tr("SSTV RX: invalid Decodium audio source"));
        return;
    }

    if (!analogActive) {
#if DECODIUM_HAS_HAMDRM
        const bool changed = m_hamDrmRxSourceKind
                != static_cast<std::uint8_t>(kind)
            || m_hamDrmRxStreamId != streamId;
        if (changed || resetExistingStream) {
            // Fence the old DirectConnection producer before resetting the
            // raw HAMDRM stream.  Otherwise a callback that started before
            // the source switch can enqueue old PCM into the new generation.
            disableSstvProducerTaps();
            m_hamDrmRxSourceKind = static_cast<std::uint8_t>(kind);
            m_hamDrmRxStreamId = streamId;
            m_hamDrmRxBackend->resetAudioStream();
            refreshSstvProducerTaps();
        }
#endif
        return;
    }

    const auto current = m_sstvRxRuntime->routeToken();
    const bool sourceChangeRequested = current.source.kind != kind
        || current.source.streamId != streamId;
    // Fence before either RX implementation changes generation.  The analog
    // runtime validates its route token, while HAMDRM intentionally accepts
    // raw PCM; draining here therefore prevents an already-running old
    // producer from crossing the shared source boundary.
    const bool producerFenceRequired = sourceChangeRequested
        || resetExistingStream;
    if (producerFenceRequired) {
        disableSstvProducerTaps();
    }
    bool changed = false;
    if (sourceChangeRequested) {
        changed = m_sstvRxRuntime->switchSource(kind, streamId);
    } else if (resetExistingStream) {
        // A queued chunk may exist before the worker has published either an
        // active rate or a processed-chunk counter.  A real capture restart is
        // still a generation boundary and must never reuse that pending PCM.
        changed = m_sstvRxRuntime->resetStream();
    }
    const auto selected = m_sstvRxRuntime->routeToken();
#if DECODIUM_HAS_HAMDRM
    bool digitalChanged = false;
    if (digitalActive && selected.valid()) {
        digitalChanged = m_hamDrmRxSourceKind
                != static_cast<std::uint8_t>(selected.source.kind)
            || m_hamDrmRxStreamId != selected.source.streamId
            || resetExistingStream;
        if (digitalChanged) {
            m_hamDrmRxSourceKind = static_cast<std::uint8_t>(
                selected.source.kind);
            m_hamDrmRxStreamId = selected.source.streamId;
            m_hamDrmRxBackend->resetAudioStream();
        }
    }
#else
    constexpr bool digitalChanged = false;
#endif
    bool selectedTapMissing = false;
    switch (selected.source.kind) {
    case SstvAudioSourceKind::LocalSoundCard:
    case SstvAudioSourceKind::WebSdr:
        selectedTapMissing = m_audioSink && !m_sstvLocalAudioTap;
        break;
    case SstvAudioSourceKind::LegacyBackend:
        selectedTapMissing = m_legacyBackend && !m_sstvLegacyAudioTap;
        break;
    case SstvAudioSourceKind::Tci:
        selectedTapMissing = m_hamlibCat && !m_sstvTciAudioTap;
        break;
    case SstvAudioSourceKind::DecoPort:
        selectedTapMissing = m_decoPortLink && !m_sstvDecoPortAudioTap;
        break;
    case SstvAudioSourceKind::RtlSdr:
        selectedTapMissing = m_rtlSdrInput
            && !m_sstvRtlPcmTap && !m_sstvRtlAudioTap;
        break;
    case SstvAudioSourceKind::Replay:
    case SstvAudioSourceKind::Unknown:
        break;
    }
    // The callback closure, rather than the callback body, owns the route
    // token. Rebind at a generation boundary, or once when a selected producer
    // has just been constructed lazily. Repeated DecoPort state telemetry must
    // not churn an otherwise stable audio tap.
    if (changed || digitalChanged || producerFenceRequired
        || selectedTapMissing) {
        refreshSstvProducerTaps();
    }
    if (changed) {
        emit sstvRxStateChanged();
        emit sstvRxSourceDeviceChanged();
        emit sstvRxSnapshotChanged();
    }
#else
    Q_UNUSED(kind)
    Q_UNUSED(streamId)
    Q_UNUSED(resetExistingStream)
#endif
}

bool DecodiumBridge::enterSstvWorkspace()
{
#if DECODIUM_HAS_SSTV
    if (m_sstvWorkspaceActive) {
        return true;
    }

    // Snapshot the user's normal RX state before changing anything. The
    // selected application mode and CAT frequency intentionally remain
    // untouched: SSTV owns the workspace/audio session, not the radio profile.
    m_sstvWorkspaceRestoreMonitoring = m_monitoring || m_monitorRequested;
    m_sstvWorkspaceActive = true;

    // Replace the previous monitor generation with the SSTV workspace's
    // visual-only capture. This invalidates delayed FT boundary callbacks and
    // quiesces a legacy decoder, but immediately reopens the shared PCM and
    // spectrum path so the dashboard panadapter keeps moving in the
    // background. If MONITOR was off, preserve that state.
    if (m_sstvWorkspaceRestoreMonitoring) {
        setMonitoring(false);
        setMonitoring(true);
    }
    if (m_decodeReleaseTimer) {
        m_decodeReleaseTimer->stop();
    }
    m_pendingDecodeReleaseQueue.clear();
    clearDecodeWindowsForModeChange(m_mode, QStringLiteral("SSTV"));
    resetFtxDecodeWorkersForModeChange(m_mode, QStringLiteral("SSTV"));
    resetRxPeriodAccumulation(false);

    qInfo().noquote()
        << QStringLiteral(
        "[SSTV] workspace entered: normal decoder suspended, panadapter preserved mode=%1 restoreMonitor=%2")
               .arg(m_mode)
               .arg(m_sstvWorkspaceRestoreMonitoring ? 1 : 0);
    return true;
#else
    emit errorMessage(tr("This Decodium build has no native SSTV support"));
    return false;
#endif
}

void DecodiumBridge::leaveSstvWorkspace()
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvWorkspaceActive) {
        return;
    }

    const bool restoreMonitoring = m_sstvWorkspaceRestoreMonitoring;

    // A close is also a hard ownership boundary. QML normally stops analogue
    // and digital SSTV first, but stopping the shared monitor here makes the
    // C++ API safe if the window is closed by the window manager or shutdown.
    if (m_monitoring || m_monitorRequested) {
        setMonitoring(false);
    }

    // Consume any final legacy result that completed while the workspace was
    // open. It belongs to the suspended dashboard mode and must not appear as
    // a fresh decode immediately after the normal monitor is restored.
    if (m_legacyBackend) {
        if (!m_legacyBackend->bandActivityLines().isEmpty()) {
            m_legacyBackend->clearBandActivity();
        }
        if (!m_legacyBackend->rxFrequencyLines().isEmpty()) {
            m_legacyBackend->clearRxFrequency();
        }
        m_legacyBandActivityRevision =
            m_legacyBackend->bandActivityRevision();
        m_legacyRxFrequencyRevision =
            m_legacyBackend->rxFrequencyRevision();
    }
    primeLegacyAllTxtCursor();

    m_sstvWorkspaceActive = false;
    m_sstvWorkspaceRestoreMonitoring = false;

    if (restoreMonitoring && !m_shuttingDown && !QCoreApplication::closingDown()
        && !m_transmitting && !m_tuning) {
        setMonitoring(true);
    }

    qInfo().noquote()
        << QStringLiteral(
               "[SSTV] workspace left: normal decoder restore mode=%1 monitor=%2")
               .arg(m_mode)
               .arg(restoreMonitoring ? 1 : 0);
#endif
}

bool DecodiumBridge::startSstvRx()
{
#if DECODIUM_HAS_SSTV
    initialiseSstvRuntime();
    if (!m_sstvRxRuntime) {
        emit errorMessage(tr("Native SSTV support is unavailable"));
        return false;
    }
    if (sstvWavReplayActive()) {
        emit errorMessage(tr(
            "SSTV RX monitor cannot replace an active WAV replay"));
        return false;
    }
    if (m_sstvRxRequested
        && m_sstvRxRuntime->state() == SstvRxRuntime::State::Running) {
        return true;
    }

    const bool retryingRequestedSession = m_sstvRxRequested;
    const bool previousMonitoringOwnership = m_sstvOwnsMonitoring;
    const bool monitoringWasAlreadyRequested =
        m_monitoring || m_monitorRequested;
    const bool legacyMonitorWasActive = usingLegacyBackendForRx()
        && m_monitoring
        && m_legacyBackend
        && m_legacyBackend->monitoring();
    auto rollbackStart = [this](bool releaseOwnedMonitoring) {
        const bool releaseDedicatedCapture =
            m_sstvOwnsDedicatedAudioCapture;
        const bool restoreLegacyMonitoring =
            m_sstvRestoresLegacyMonitoring;
        m_sstvRxRequested = false;
        disableSstvProducerTaps();
        if (m_sstvRxRuntime) {
            m_sstvRxRuntime->stop();
        }
        m_sstvOwnsMonitoring = false;
        m_sstvOwnsDedicatedAudioCapture = false;
        m_sstvRestoresLegacyMonitoring = false;
#if DECODIUM_HAS_HAMDRM
        if (m_hamDrmRxRequested && m_hamDrmRxBackend
            && m_hamDrmRxBackend->active()) {
            refreshSstvProducerTaps();
        }
#endif
        emit sstvRxStateChanged();
        emit sstvRxSnapshotChanged();
        if (releaseOwnedMonitoring && (m_monitoring || m_monitorRequested)) {
            setMonitoring(false);
        } else if (releaseDedicatedCapture) {
            stopAudioCapture();
            if (usingLegacyBackendForRx() && m_legacyBackend) {
                rearmLegacyPcmSpectrumFeed(
                    QStringLiteral("SSTV RX start rollback"));
            }
        }
        if (restoreLegacyMonitoring && m_legacyBackend) {
            // Rebuild the complete legacy RX generation after a failed SSTV
            // start as well. Restoring only the backend boolean would leave
            // the dashboard timers/audio feed stopped in the same way as a
            // normal SSTV close.
            m_monitorRequested = false;
            if (m_monitoring) {
                m_monitoring = false;
                emit monitoringChanged();
            }
            startRx();
        }
    };

    // Recover a previous cancelled/error session before creating a fresh
    // generation. Shutdown is terminal and only occurs during bridge teardown.
    const SstvRxRuntime::State previousState = m_sstvRxRuntime->state();
    if (previousState == SstvRxRuntime::State::Shutdown
        || previousState == SstvRxRuntime::State::Stopping) {
        emit errorMessage(tr("SSTV RX runtime is shutting down"));
        return false;
    }
    if (previousState != SstvRxRuntime::State::Inactive) {
        disableSstvProducerTaps();
        if (!m_sstvRxRuntime->stop()) {
            emit errorMessage(tr(
                "SSTV RX could not clear its previous session"));
            return false;
        }
    }

    // If the user starts SSTV while the legacy FT8/FT2 monitor is already
    // active, release that backend before opening the native capture.  The
    // previous state is restored by stopSstvRx().
    if (legacyMonitorWasActive) {
        m_sstvRestoresLegacyMonitoring = true;
        m_monitorRequested = false;
        m_legacyBackend->setMonitoring(false);
        // The legacy backend can leave its SoundInput object alive after the
        // backend state changes to stopped. Native SSTV must reopen that
        // endpoint on its own generation: reusing the old AudioQueue is
        // especially unreliable with BlackHole, where it may remain active
        // while delivering a permanent zero-RMS stream.
        stopAudioCapture();
        m_monitoring = false;
        emit monitoringChanged();
    }

    // Publish the request before selecting the initial source.  This is the
    // generation boundary that makes a manual/automatic SSTV session choose
    // the native capture even when the application mode is FT8/FT2.
    m_sstvRxRequested = true;
    m_sstvOwnsDedicatedAudioCapture =
        nativeSstvRxForcesDedicatedAudioCapture();

    const SstvAudioSourceKind initialKind = currentSstvAudioSourceKind();
    const quint32 initialStreamId = currentSstvAudioStreamId(initialKind);
    if (initialKind == SstvAudioSourceKind::Unknown || initialStreamId == 0U) {
        rollbackStart(retryingRequestedSession
                      && previousMonitoringOwnership);
        emit errorMessage(tr(
            "SSTV RX: no Decodium audio source is available"));
        return false;
    }

    if (!m_sstvRxRuntime->start(initialKind, initialStreamId)) {
        rollbackStart(retryingRequestedSession
                      && previousMonitoringOwnership);
        emit errorMessage(tr("SSTV RX worker could not be started"));
        return false;
    }

    m_sstvOwnsMonitoring = retryingRequestedSession
        ? previousMonitoringOwnership
        : !monitoringWasAlreadyRequested;
    if (legacyMonitorWasActive) {
        m_sstvOwnsMonitoring = true;
    }
    refreshSstvProducerTaps();
    emit sstvRxStateChanged();
    emit sstvRxSnapshotChanged();

    if (!m_monitoring) {
        setMonitoring(true);
    }
    if (!m_monitoring) {
        const bool releaseOwnedMonitoring = m_sstvOwnsMonitoring;
        rollbackStart(releaseOwnedMonitoring);
        emit errorMessage(tr(
            "SSTV RX requires an active Decodium monitor"));
        return false;
    }

    // If the Decodium monitor was already running through the legacy backend,
    // startRx() was not involved in this session.  Ensure the native capture
    // exists before binding the local SSTV tap; startAudioCapture() is
    // idempotent for an already-active endpoint.
    if (nativeSstvRxForcesDedicatedAudioCapture()) {
        startAudioCapture();
    }

    const SstvAudioSourceKind selectedKind = currentSstvAudioSourceKind();
    selectSstvRxSource(selectedKind, currentSstvAudioStreamId(selectedKind));
    emit statusMessage(tr("SSTV RX started from %1").arg(sstvRxSource()));
    return true;
#else
    emit errorMessage(tr(
        "This Decodium build has no native SSTV support"));
    return false;
#endif
}

void DecodiumBridge::stopSstvRx()
{
#if DECODIUM_HAS_SSTV && DECODIUM_HAS_HAMDRM
    const bool digitalContinues = m_hamDrmRxRequested
        && m_hamDrmRxBackend && m_hamDrmRxBackend->active();
#else
    constexpr bool digitalContinues = false;
#endif
    const bool stopDedicatedCapture = m_sstvOwnsDedicatedAudioCapture;
    const bool stopOwnedMonitor = m_sstvOwnsMonitoring
        && !digitalContinues && (m_monitoring || m_monitorRequested);
#if DECODIUM_HAS_SSTV && DECODIUM_HAS_HAMDRM
    if (m_sstvOwnsMonitoring && digitalContinues) {
        m_hamDrmOwnsMonitoring = true;
    }
#endif
    m_sstvRxRequested = false;
    m_sstvOwnsMonitoring = false;
    m_sstvOwnsDedicatedAudioCapture = false;
    const bool restoreLegacyMonitoring = m_sstvRestoresLegacyMonitoring;
    m_sstvRestoresLegacyMonitoring = false;
#if DECODIUM_HAS_SSTV
    if (sstvWavReplayActive()) {
        m_sstvWavReplayRestoreLive = false;
        m_sstvWavReplayController->cancel();
    }
    disableSstvProducerTaps();
    if (m_sstvRxRuntime) {
        m_sstvRxRuntime->stop();
    }
    if (digitalContinues) {
        refreshSstvProducerTaps();
    }
#endif
    emit sstvRxStateChanged();
    emit sstvRxSnapshotChanged();

    if (stopOwnedMonitor) {
        setMonitoring(false);
    } else if (stopDedicatedCapture) {
        // Return FT8/FT2 to its original single legacy PCM source after the
        // native SSTV session releases its dedicated capture.
        stopAudioCapture();
        if (usingLegacyBackendForRx() && m_legacyBackend) {
            rearmLegacyPcmSpectrumFeed(
                QStringLiteral("SSTV RX stopped"));
        }
    }
    if (restoreLegacyMonitoring && m_legacyBackend) {
        if (!digitalContinues) {
            // SSTV temporarily stopped the legacy monitor and took ownership
            // of the audio endpoint.  Merely setting the backend flag back to
            // true leaves the spectrum/period timers and PCM feed stopped,
            // which makes the dashboard panadapter appear frozen until the
            // user toggles Monitor OFF/ON manually.  Re-enter the normal RX
            // start path so audio capture, timers and visual feed are rebuilt
            // as one coherent generation.
            m_monitorRequested = false;
            if (m_monitoring) {
                m_monitoring = false;
                emit monitoringChanged();
            }
            startRx();
        } else {
            // HAMDRM may still own the shared monitor.  Do not tear down its
            // active generation; only restore the legacy backend mirror.
            m_monitorRequested = true;
            m_legacyBackend->setMonitoring(true);
            syncLegacyBackendState();
        }
    }
    emit statusMessage(tr("SSTV RX stopped"));
}

void DecodiumBridge::stopSstvRxForMonitorStop()
{
#if DECODIUM_HAS_SSTV && DECODIUM_HAS_HAMDRM
    const bool digitalActive = m_hamDrmRxRequested && m_hamDrmRxBackend
        && m_hamDrmRxBackend->active();
#else
    constexpr bool digitalActive = false;
#endif
    if (!m_sstvRxRequested && !digitalActive) {
        return;
    }
    m_sstvRxRequested = false;
    m_sstvOwnsMonitoring = false;
    m_sstvOwnsDedicatedAudioCapture = false;
    m_sstvRestoresLegacyMonitoring = false;
#if DECODIUM_HAS_SSTV
#if DECODIUM_HAS_HAMDRM
    if (digitalActive) {
        m_hamDrmRxRequested = false;
        m_hamDrmOwnsMonitoring = false;
        m_hamDrmRxBackend->failAudio(
            decodium::sstv::hamdrm::HamDrmStatus::failure(
                decodium::sstv::hamdrm::HamDrmErrorCode::IoFailure,
                "Decodium monitoring stopped during HAMDRM RX"));
    }
#endif
    if (sstvWavReplayActive()) {
        m_sstvWavReplayRestoreLive = false;
        m_sstvWavReplayController->cancel();
    }
    disableSstvProducerTaps();
    if (m_sstvRxRuntime) {
        m_sstvRxRuntime->stop();
    }
#endif
    emit sstvRxStateChanged();
    emit sstvRxSnapshotChanged();
}

bool DecodiumBridge::resetSstvRx()
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvRxRequested || !m_sstvRxRuntime
        || sstvWavReplayActive()) {
        return false;
    }
    const bool reset = m_sstvRxRuntime->resetStream();
    if (reset) {
        refreshSstvProducerTaps();
        emit sstvRxStateChanged();
        emit sstvRxSnapshotChanged();
    }
    return reset;
#else
    return false;
#endif
}

bool DecodiumBridge::abortSstvRxFrame()
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvRxRequested || !m_sstvRxRuntime
        || sstvWavReplayActive()) {
        return false;
    }
    const bool aborted = m_sstvRxRuntime->resetStream();
    if (aborted) {
        refreshSstvProducerTaps();
        emit sstvRxStateChanged();
        emit sstvRxSnapshotChanged();
        emit statusMessage(tr(
            "Current SSTV frame aborted; receiver returned to search"));
    }
    return aborted;
#else
    return false;
#endif
}

bool DecodiumBridge::updateSstvRxControls(const QVariantMap& controls)
{
#if DECODIUM_HAS_SSTV
    initialiseSstvRuntime();
    if (!m_sstvRxRuntime || m_sstvRedecodeActive) {
        return false;
    }
    SstvRxControlSettings parsed;
    QString error;
    if (!sstvRxSettingsFromMap(
            controls,
            m_sstvRxRuntime->rxControlSnapshot().settings,
            &parsed,
            &error)
        || !m_sstvRxRuntime->replaceRxControlSettings(parsed)) {
        emit errorMessage(error.isEmpty()
            ? tr("The SSTV RX controls were rejected") : error);
        return false;
    }

    persistSstvRxControls(parsed);
    return true;
#else
    Q_UNUSED(controls)
    return false;
#endif
}

void DecodiumBridge::resetSstvRxAfc()
{
#if DECODIUM_HAS_SSTV
    if (m_sstvRxRuntime) {
        m_sstvRxRuntime->resetRxAfc();
        emit sstvRxSnapshotChanged();
    }
#endif
}

void DecodiumBridge::resetSstvRxSlant()
{
#if DECODIUM_HAS_SSTV
    if (m_sstvRxRuntime) {
        m_sstvRxRuntime->resetRxSlant();
        emit sstvRxSnapshotChanged();
    }
#endif
}

bool DecodiumBridge::redecodeRecentSstv(
    const QVariantMap& parameters)
{
#if DECODIUM_HAS_SSTV
    initialiseSstvRuntime();
    if (!m_sstvRxRuntime || !m_sstvRxAudioJobController
        || m_sstvRxAudioJobController->busy()
        || sstvWavReplayActive() || m_sstvRedecodeActive) {
        return false;
    }
    const SstvRxRuntime::Snapshot snapshot = m_sstvRxRuntime->snapshot();
    if (snapshot.replay.retainedSamples == 0U) {
        emit errorMessage(tr("No retained SSTV audio is available to re-decode"));
        return false;
    }

    const SstvRxControlSettings controls =
        m_sstvRxRuntime->rxControlSnapshot().settings;
    SstvRxRedecodeParameters parsed;
    parsed.afcMode = controls.afcMode;
    parsed.frequencyCorrectionHz = controls.manualFrequencyCorrectionHz;
    parsed.slantMode = controls.slantMode;
    parsed.clockErrorPpm = controls.manualClockErrorPpm;
    if (parameters.contains(QStringLiteral("mode"))) {
        const QString mode = parameters.value(QStringLiteral("mode"))
                                 .toString().trimmed();
        if (mode.compare(QStringLiteral("auto"),
                         Qt::CaseInsensitive) != 0) {
            parsed.mode = mode.toStdString();
        }
    }
    if (parameters.contains(QStringLiteral("afcMode"))
        && !parseSstvRxAfcMode(parameters.value(QStringLiteral("afcMode")),
                               &parsed.afcMode)) {
        return false;
    }
    if (parameters.contains(QStringLiteral("frequencyCorrectionHz"))) {
        bool ok = false;
        parsed.frequencyCorrectionHz = parameters.value(
            QStringLiteral("frequencyCorrectionHz")).toDouble(&ok);
        if (!ok) {
            return false;
        }
    }
    if (parameters.contains(QStringLiteral("slantMode"))
        && !parseSstvRxSlantMode(
            parameters.value(QStringLiteral("slantMode")),
            &parsed.slantMode)) {
        return false;
    }
    if (parameters.contains(QStringLiteral("clockErrorPpm"))) {
        bool ok = false;
        parsed.clockErrorPpm = parameters.value(
            QStringLiteral("clockErrorPpm")).toDouble(&ok);
        if (!ok) {
            return false;
        }
    }
    if ((!parsed.mode.empty() && !selectableSstvRxMode(parsed.mode))
        || !decodium::sstv::SstvRxControlPolicy::
               redecodeParametersAreValid(parsed)) {
        emit errorMessage(tr("Invalid SSTV re-decode parameters"));
        return false;
    }
    const bool started = m_sstvRxAudioJobController
        ->prepareRecentRedecode(std::move(parsed));
    if (started) {
        emit sstvRxAudioJobChanged();
    }
    return started;
#else
    Q_UNUSED(parameters)
    return false;
#endif
}

bool DecodiumBridge::saveSstvRxRawAudio()
{
#if DECODIUM_HAS_SSTV
    initialiseSstvRuntime();
    if (!m_sstvStorageReady || !m_sstvStorageWorker
        || !m_sstvRxRuntime || !m_sstvRxAudioJobController
        || m_sstvRxAudioJobController->busy()) {
        return false;
    }
    const SstvRxRuntime::Snapshot snapshot = m_sstvRxRuntime->snapshot();
    const quint64 acquisitionId = snapshot.image.acquisitionId != 0U
        ? snapshot.image.acquisitionId
        : snapshot.replay.mostRecentAcquisitionId;
    if (snapshot.replay.retainedSamples == 0U || acquisitionId == 0U) {
        emit errorMessage(tr("No retained SSTV acquisition is available to save"));
        return false;
    }
    const decodium::sstv::SstvStorageLayout layout(
        m_sstvStorageWorker->storageRoot());
    const QString token = QDateTime::currentDateTimeUtc().toString(
        QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    const QString path = QDir(layout.wavExportRoot()).absoluteFilePath(
        QStringLiteral("rx-%1-%2.wav").arg(token).arg(acquisitionId));
    const bool started = m_sstvRxAudioJobController->exportRawAudio(
        QUrl::fromLocalFile(path), acquisitionId);
    if (started) {
        emit sstvRxAudioJobChanged();
    }
    return started;
#else
    return false;
#endif
}

void DecodiumBridge::cancelSstvRxAudioJob()
{
#if DECODIUM_HAS_SSTV
    if (m_sstvRxAudioJobController) {
        m_sstvRxAudioJobController->cancel();
    }
#endif
}

bool DecodiumBridge::startSstvWavReplay(const QUrl& localFile)
{
#if DECODIUM_HAS_SSTV
    initialiseSstvRuntime();
    if (!m_sstvRxRuntime || !m_sstvWavReplayController
        || m_sstvWavReplayController->active()) {
        return false;
    }

    const auto previous = m_sstvRxRuntime->routeToken();
    m_sstvWavReplayRestoreLive = m_sstvRxRequested
        && m_sstvRxRuntime->state() == SstvRxRuntime::State::Running
        && previous.valid()
        && previous.source.kind != SstvAudioSourceKind::Replay;
    m_sstvWavReplayRestoreKind = m_sstvWavReplayRestoreLive
        ? static_cast<std::uint8_t>(previous.source.kind) : 0U;
    m_sstvWavReplayRestoreStreamId = m_sstvWavReplayRestoreLive
        ? previous.source.streamId : 0U;

    // Close and drain every DirectConnection callback before the controller
    // creates the Replay generation. The file worker then becomes its only
    // producer and feeds the same SstvRxRuntime used by live reception.
    disableSstvProducerTaps();
    if (!m_sstvWavReplayController->startReplay(localFile)) {
        const QString detail = m_sstvWavReplayController->lastError();
        if (!restoreSstvLiveAfterReplay()) {
            emit errorMessage(tr(
                "SSTV WAV replay failed and the live RX route could not be restored"));
        }
        m_sstvWavReplayRestoreLive = false;
        m_sstvWavReplayRestoreKind = 0U;
        m_sstvWavReplayRestoreStreamId = 0U;
        emit sstvWavReplayChanged();
        emit sstvRxStateChanged();
        emit sstvRxSnapshotChanged();
        if (!detail.isEmpty()) {
            emit errorMessage(tr("SSTV WAV replay: %1").arg(detail));
        }
        return false;
    }

    m_sstvWavReplaySession = m_sstvWavReplayController->sessionId();
    emit sstvWavReplayChanged();
    emit sstvRxStateChanged();
    emit sstvRxSnapshotChanged();
    emit statusMessage(tr("SSTV WAV replay started: %1")
                           .arg(m_sstvWavReplayController->fileName()));
    return true;
#else
    Q_UNUSED(localFile)
    emit errorMessage(tr(
        "This Decodium build has no native SSTV WAV replay support"));
    return false;
#endif
}

void DecodiumBridge::cancelSstvWavReplay()
{
#if DECODIUM_HAS_SSTV
    if (m_sstvWavReplayController) {
        m_sstvWavReplayController->cancel();
    }
#endif
}

void DecodiumBridge::finishSstvWavReplay(bool completed,
                                         bool cancelled,
                                         quint64 sessionId)
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvWavReplayController
        || sessionId == 0U || sessionId != m_sstvWavReplaySession) {
        return;
    }
    const bool wasRedecode = m_sstvRedecodeActive;
    m_sstvWavReplaySession = 0U;
    if (m_sstvRedecodeActive && m_sstvRxRuntime) {
        SstvRxControlSettings restore =
            m_sstvRxRuntime->rxControlSnapshot().settings;
        QString restoreError;
        if (!sstvRxSettingsFromMap(
                m_sstvRedecodeRestoreControls,
                restore,
                &restore,
                &restoreError)
            || !m_sstvRxRuntime->replaceRxControlSettings(
                std::move(restore))) {
            emit errorMessage(tr(
                "SSTV re-decode ended but the prior RX controls could not be restored: %1")
                                  .arg(restoreError));
        }
        m_sstvRedecodeActive = false;
        m_sstvRedecodeRestoreControls.clear();
    }
    if (wasRedecode) {
        const SstvRxRuntime::Snapshot snapshot = m_sstvRxRuntime
            ? m_sstvRxRuntime->snapshot() : SstvRxRuntime::Snapshot {};
        QVariantMap fields {
            {QStringLiteral("state"),
             completed ? QStringLiteral("completed")
                       : (cancelled ? QStringLiteral("cancelled")
                                    : QStringLiteral("failed"))},
            {QStringLiteral("success"), completed && !cancelled},
            {QStringLiteral("coveragePermille"),
             static_cast<qulonglong>(std::llround(std::clamp(
                 snapshot.image.coverage, 0.0, 1.0) * 1'000.0))},
        };
        if (!snapshot.image.mode.isEmpty()) {
            fields.insert(QStringLiteral("modeId"), snapshot.image.mode);
        }
        try {
            decodium::sstv::recordSstvDiagnosticEvent(
                sstvRxLog(), completed && !cancelled
                    ? QtInfoMsg : QtWarningMsg,
                QStringLiteral("rx.redecode-completed"), fields);
        } catch (...) {
            // Diagnostics are best-effort and never own replay completion.
        }
    }
    const bool restoreRequested = m_sstvWavReplayRestoreLive;
    bool restored = true;
    if (restoreRequested) {
        restored = restoreSstvLiveAfterReplay();
    } else if (!completed && !m_sstvRxRequested && m_sstvRxRuntime
               && m_sstvRxRuntime->state()
                      != SstvRxRuntime::State::Inactive
               && m_sstvRxRuntime->state()
                      != SstvRxRuntime::State::Shutdown) {
        // A cancelled/failed standalone import has no useful snapshot to
        // retain. Join the native RX worker instead of leaving it idle.
        m_sstvRxRuntime->stop();
    }
    m_sstvWavReplayRestoreLive = false;
    m_sstvWavReplayRestoreKind = 0U;
    m_sstvWavReplayRestoreStreamId = 0U;
    if (m_sstvRxAudioJobController) {
        m_sstvRxAudioJobController->discardPreparedRedecode();
        emit sstvRxAudioJobChanged();
    }

    if (!restored) {
        m_sstvRxRequested = false;
        m_sstvOwnsMonitoring = false;
        disableSstvProducerTaps();
        emit errorMessage(tr(
            "SSTV WAV replay ended but the live RX source could not be restored"));
    } else if (completed) {
        emit statusMessage(tr(
            "SSTV WAV replay completed and drained"));
    } else if (cancelled) {
        emit statusMessage(tr("SSTV WAV replay cancelled"));
    }
    emit sstvWavReplayChanged();
    emit sstvRxStateChanged();
    emit sstvRxSnapshotChanged();
#else
    Q_UNUSED(completed)
    Q_UNUSED(cancelled)
    Q_UNUSED(sessionId)
#endif
}

bool DecodiumBridge::restoreSstvLiveAfterReplay()
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvWavReplayRestoreLive || !m_sstvRxRequested) {
        return true;
    }
    if (!m_sstvRxRuntime) {
        return false;
    }

    SstvAudioSourceKind kind = currentSstvAudioSourceKind();
    quint32 streamId = currentSstvAudioStreamId(kind);
    if (kind == SstvAudioSourceKind::Unknown || streamId == 0U) {
        kind = static_cast<SstvAudioSourceKind>(
            m_sstvWavReplayRestoreKind);
        streamId = m_sstvWavReplayRestoreStreamId;
    }
    if (kind == SstvAudioSourceKind::Unknown
        || kind == SstvAudioSourceKind::Replay || streamId == 0U) {
        return false;
    }

    bool selected = false;
    switch (m_sstvRxRuntime->state()) {
    case SstvRxRuntime::State::Running:
    case SstvRxRuntime::State::Cancelled:
        selected = m_sstvRxRuntime->switchSource(kind, streamId);
        break;
    case SstvRxRuntime::State::Inactive:
        selected = m_sstvRxRuntime->start(kind, streamId);
        break;
    case SstvRxRuntime::State::Error:
        selected = m_sstvRxRuntime->stop()
            && m_sstvRxRuntime->start(kind, streamId);
        break;
    case SstvRxRuntime::State::Stopping:
    case SstvRxRuntime::State::Shutdown:
        break;
    }
    if (!selected) {
        return false;
    }
    refreshSstvProducerTaps();
    return true;
#else
    return false;
#endif
}
