// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvRxRuntime.h"

#include "../analog/SstvAvtRxSession.h"
#include "../analog/SstvMartinM1RxSession.h"
#include "../analog/SstvMmsstvExtendedRxSession.h"
#include "../analog/SstvPdRxSession.h"
#include "../analog/SstvRobotRxSession.h"
#include "../analog/SstvScottieRxSession.h"
#include "../analog/SstvSequentialRgbRxSession.h"
#include "../core/SstvModeRegistry.h"
#include "../diagnostics/SstvDiagnosticLogging.h"
#include "../dsp/SstvFrequencyDemodulator.h"
#include "../dsp/SstvPreprocessor.h"
#include "../dsp/SstvResampler.h"
#include "../dsp/SstvToneDetector.h"
#include "../rx/SstvFskIdDetector.h"

#include <QMetaObject>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace decodium::sstv {
namespace {

constexpr std::uint64_t kNanosecondsPerSecond = 1'000'000'000ULL;
constexpr std::uint64_t kMicrosecondsPerSecond = 1'000'000ULL;
constexpr std::uint64_t kFnvOffset = 14'695'981'039'346'656'037ULL;
constexpr std::uint64_t kFnvPrime = 1'099'511'628'211ULL;

QString afcModeDiagnosticName(SstvRxAfcMode mode)
{
    switch (mode) {
    case SstvRxAfcMode::Off: return QStringLiteral("off");
    case SstvRxAfcMode::Automatic: return QStringLiteral("auto");
    case SstvRxAfcMode::Manual: return QStringLiteral("manual");
    }
    return QStringLiteral("invalid");
}

QString slantModeDiagnosticName(SstvRxSlantMode mode)
{
    switch (mode) {
    case SstvRxSlantMode::Off: return QStringLiteral("off");
    case SstvRxSlantMode::Automatic: return QStringLiteral("auto");
    case SstvRxSlantMode::Manual: return QStringLiteral("manual");
    }
    return QStringLiteral("invalid");
}

void recordRxSyncControlChanges(
    const SstvRxControlSettings& previous,
    const SstvRxControlSettings& next) noexcept
{
    try {
        if (previous.modeControl != next.modeControl
            || previous.manualMode != next.manualMode) {
            QVariantMap fields {
                {QStringLiteral("state"),
                 next.modeControl == SstvRxModeControl::Automatic
                     ? QStringLiteral("auto") : QStringLiteral("manual")},
            };
            if (next.modeControl == SstvRxModeControl::Manual) {
                fields.insert(QStringLiteral("modeId"),
                              QString::fromStdString(next.manualMode));
            }
            recordSstvDiagnosticEvent(
                sstvSyncLog(), QtInfoMsg,
                QStringLiteral("sync.mode-control-changed"), fields);
        }
        if (previous.modeLockEnabled != next.modeLockEnabled
            || previous.lockedMode != next.lockedMode) {
            QVariantMap fields {
                {QStringLiteral("active"), next.modeLockEnabled},
            };
            if (next.modeLockEnabled) {
                fields.insert(QStringLiteral("modeId"),
                              QString::fromStdString(next.lockedMode));
            }
            recordSstvDiagnosticEvent(
                sstvSyncLog(), QtInfoMsg,
                QStringLiteral("sync.mode-lock-changed"), fields);
        }
        if (previous.timingFallbackEnabled
            != next.timingFallbackEnabled) {
            recordSstvDiagnosticEvent(
                sstvSyncLog(), QtInfoMsg,
                QStringLiteral("sync.fallback-setting-changed"),
                {{QStringLiteral("active"),
                  next.timingFallbackEnabled}});
        }
        if (previous.afcMode != next.afcMode
            || std::abs(previous.manualFrequencyCorrectionHz
                        - next.manualFrequencyCorrectionHz) > 1.0e-9) {
            recordSstvDiagnosticEvent(
                sstvSyncLog(), QtInfoMsg,
                QStringLiteral("sync.afc-setting-changed"),
                {{QStringLiteral("state"),
                  afcModeDiagnosticName(next.afcMode)},
                 {QStringLiteral("offsetHz"),
                  next.manualFrequencyCorrectionHz}});
        }
        if (previous.slantMode != next.slantMode
            || std::abs(previous.manualClockErrorPpm
                        - next.manualClockErrorPpm) > 1.0e-9) {
            recordSstvDiagnosticEvent(
                sstvSyncLog(), QtInfoMsg,
                QStringLiteral("sync.slant-setting-changed"),
                {{QStringLiteral("state"),
                  slantModeDiagnosticName(next.slantMode)},
                 {QStringLiteral("slantPpm"),
                  next.manualClockErrorPpm}});
        }
    } catch (...) {
        // Logging is best-effort and cannot reject a valid control update.
    }
}

void recordVisDiagnostic(const SstvRxRuntime::VisSummary& summary) noexcept
{
    try {
        const bool accepted = summary.valid && summary.modeMapped;
        QVariantMap fields {
            {QStringLiteral("success"), accepted},
            {QStringLiteral("visCode"), summary.primaryPayload},
        };
        if (summary.modeMapped) {
            fields.insert(QStringLiteral("modeId"), summary.mappedMode);
        }
        if (!accepted) {
            fields.insert(QStringLiteral("reasonCode"),
                          summary.valid
                              ? QStringLiteral("unsupported-mode")
                              : QStringLiteral("invalid-frame"));
        }
        recordSstvDiagnosticEvent(
            sstvVisLog(), accepted ? QtInfoMsg : QtWarningMsg,
            accepted ? QStringLiteral("vis.accepted")
                     : QStringLiteral("vis.rejected"),
            fields);
    } catch (...) {
        // A diagnostic allocation failure must not escape the RX worker.
    }
}

void recordRxLifecycleDiagnostic(const QString& event,
                                 const QString& state,
                                 bool active) noexcept
{
    try {
        recordSstvDiagnosticEvent(
            sstvRxLog(), QtInfoMsg, event,
            {{QStringLiteral("state"), state},
             {QStringLiteral("active"), active}});
    } catch (...) {
        // Structured diagnostics are best-effort and never own RX lifecycle.
    }
}

void recordAfcAcquisitionDiagnostic(double offsetHz) noexcept
{
    try {
        recordSstvDiagnosticEvent(
            sstvSyncLog(), QtInfoMsg,
            QStringLiteral("sync.afc-acquired"),
            {{QStringLiteral("offsetHz"), offsetHz},
             {QStringLiteral("success"), true}});
    } catch (...) {
    }
}

void recordSlantAcquisitionDiagnostic(
    const SstvSlantControllerSnapshot& slant) noexcept
{
    try {
        recordSstvDiagnosticEvent(
            sstvSyncLog(), QtInfoMsg,
            QStringLiteral("sync.slant-acquired"),
            {{QStringLiteral("slantPpm"),
              slant.appliedClockErrorPpm},
             {QStringLiteral("count"),
              static_cast<qulonglong>(slant.observedSyncs)},
             {QStringLiteral("success"), true}});
    } catch (...) {
    }
}

void recordFallbackDiagnostic(const SstvFallbackResult& fallback,
                              const QString& event,
                              const QString& state,
                              bool success) noexcept
{
    try {
        QVariantMap fields {
            {QStringLiteral("state"), state},
            {QStringLiteral("success"), success},
            {QStringLiteral("count"),
             static_cast<qulonglong>(fallback.candidates.size())},
        };
        if (fallback.selectedMode.has_value()) {
            fields.insert(QStringLiteral("modeId"),
                          QString::fromStdString(*fallback.selectedMode));
        }
        recordSstvDiagnosticEvent(
            sstvSyncLog(), success ? QtInfoMsg : QtWarningMsg,
            event, fields);
    } catch (...) {
    }
}

template<typename Mode, std::size_t Count, typename SpecFunction>
std::optional<Mode> modeForStableId(std::string_view id,
                                    SpecFunction specFunction)
{
    for (std::size_t index = 0U; index < Count; ++index) {
        const Mode mode = static_cast<Mode>(index);
        const auto spec = specFunction(mode);
        if (spec.stableId != nullptr && id == spec.stableId) {
            return mode;
        }
    }
    return std::nullopt;
}

std::int32_t roundedClockErrorPpm(double value) noexcept
{
    const double bounded = std::max(-5'000.0, std::min(5'000.0, value));
    return static_cast<std::int32_t>(std::llround(bounded));
}

std::uint64_t samplesForPicoseconds(
    Picoseconds duration) noexcept
{
    if (duration.count <= 0) {
        return 0U;
    }
    constexpr std::uint64_t kPicosecondsPerSecond = 1'000'000'000'000ULL;
    const auto count = static_cast<std::uint64_t>(duration.count);
    const std::uint64_t whole = count / kPicosecondsPerSecond;
    const std::uint64_t remainder = count % kPicosecondsPerSecond;
    if (whole > std::numeric_limits<std::uint64_t>::max()
                    / SstvResampler::kOutputSampleRate) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return whole * SstvResampler::kOutputSampleRate
        + remainder * SstvResampler::kOutputSampleRate
            / kPicosecondsPerSecond;
}

std::chrono::nanoseconds streamTime(
    std::uint64_t epochUs,
    std::uint64_t sample) noexcept
{
    constexpr std::uint64_t kNanosecondsPerMicrosecond = 1'000ULL;
    const std::uint64_t maximum = static_cast<std::uint64_t>(
        std::numeric_limits<std::int64_t>::max());
    const std::uint64_t epochNs = epochUs > maximum
            / kNanosecondsPerMicrosecond
        ? maximum : epochUs * kNanosecondsPerMicrosecond;
    const std::uint64_t whole = sample / SstvResampler::kOutputSampleRate;
    const std::uint64_t remainder = sample
        % SstvResampler::kOutputSampleRate;
    const std::uint64_t sampleNs = whole > maximum / kNanosecondsPerSecond
        ? maximum
        : whole * kNanosecondsPerSecond
            + remainder * kNanosecondsPerSecond
                / SstvResampler::kOutputSampleRate;
    const std::uint64_t total = sampleNs > maximum - epochNs
        ? maximum : epochNs + sampleNs;
    return std::chrono::nanoseconds {static_cast<std::int64_t>(total)};
}

QString visRawBits(const SstvVisDecodeResult& decoded)
{
    QString result;
    result.reserve(static_cast<qsizetype>(decoded.rawBitsLsbFirst.size()));
    for (const SstvVisSymbol symbol : decoded.rawBitsLsbFirst) {
        switch (symbol) {
        case SstvVisSymbol::Zero:
            result.append(QLatin1Char('0'));
            break;
        case SstvVisSymbol::One:
            result.append(QLatin1Char('1'));
            break;
        default:
            result.append(QLatin1Char('?'));
            break;
        }
    }
    return result;
}

std::optional<SstvMartinMode> martinModeForVis(
    std::uint8_t visPayload) noexcept
{
    switch (visPayload) {
    case 44U:
        return SstvMartinMode::M1;
    case 40U:
        return SstvMartinMode::M2;
    case 36U:
        return SstvMartinMode::M3;
    case 32U:
        return SstvMartinMode::M4;
    default:
        return std::nullopt;
    }
}

std::optional<SstvScottieMode> scottieModeForVis(
    std::uint8_t visPayload) noexcept
{
    switch (visPayload) {
    case 60U:
        return SstvScottieMode::S1;
    case 56U:
        return SstvScottieMode::S2;
    case 52U:
        return SstvScottieMode::S3;
    case 48U:
        return SstvScottieMode::S4;
    case 76U:
        return SstvScottieMode::DX;
    default:
        return std::nullopt;
    }
}

std::optional<SstvRobotMode> robotModeForVis(
    std::uint8_t visPayload) noexcept
{
    return SstvRobotProtocol::modeForVis(visPayload);
}

std::optional<SstvSequentialRgbMode> sequentialRgbModeForVis(
    std::uint8_t visPayload) noexcept
{
    return SstvSequentialRgbProtocol::modeForVis(visPayload);
}

std::optional<SstvPdMode> pdModeForVis(
    std::uint8_t visPayload) noexcept
{
    return SstvPdProtocol::modeForVis(visPayload);
}

std::optional<SstvMmsstvMode> mmsstvModeForVis(
    const SstvVisDecodeResult& decoded) noexcept
{
    if (decoded.format != SstvVisFormat::Extended
        || !decoded.extension.has_value()
        || !decoded.extension->rawOctetKnown) {
        return std::nullopt;
    }
    return SstvMmsstvProtocol::modeForExtendedRaw(
        decoded.extension->rawOctet);
}

bool isTerminalRxState(SstvRxState state) noexcept
{
    return state == SstvRxState::Completed
        || state == SstvRxState::Partial
        || state == SstvRxState::Aborted
        || state == SstvRxState::Error;
}

qint64 cappedAdd(qint64 left, qint64 right) noexcept
{
    if (right > 0 && left > std::numeric_limits<qint64>::max() - right) {
        return std::numeric_limits<qint64>::max();
    }
    return left + right;
}

bool blockEndTimestamp(qint64 startNs,
                       std::size_t sampleCount,
                       std::uint32_t sampleRate,
                       qint64& endNs) noexcept
{
    if (startNs < 0 || sampleCount == 0U || sampleRate == 0U
        || sampleCount
               > std::numeric_limits<std::uint64_t>::max()
                     / kNanosecondsPerSecond) {
        return false;
    }

    const std::uint64_t numerator =
        static_cast<std::uint64_t>(sampleCount) * kNanosecondsPerSecond;
    std::uint64_t duration = numerator / sampleRate;
    if (numerator % sampleRate != 0U) {
        ++duration;
    }
    const auto maximum =
        static_cast<std::uint64_t>(std::numeric_limits<qint64>::max());
    if (duration > maximum
        || static_cast<std::uint64_t>(startNs) > maximum - duration) {
        return false;
    }
    endNs = startNs + static_cast<qint64>(duration);
    return true;
}

float normalizePcm16(short sample) noexcept
{
    const int value = static_cast<int>(sample);
    return value < 0 ? static_cast<float>(value) / 32'768.0F
                     : static_cast<float>(value) / 32'767.0F;
}

std::uint64_t sampleOffsetUs(std::uint64_t sampleIndex) noexcept
{
    const std::uint64_t whole =
        sampleIndex / SstvResampler::kOutputSampleRate;
    const std::uint64_t remainder =
        sampleIndex % SstvResampler::kOutputSampleRate;
    if (whole > std::numeric_limits<std::uint64_t>::max()
                    / kMicrosecondsPerSecond) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    const std::uint64_t base = whole * kMicrosecondsPerSecond;
    const std::uint64_t fraction =
        remainder * kMicrosecondsPerSecond
        / SstvResampler::kOutputSampleRate;
    if (base > std::numeric_limits<std::uint64_t>::max() - fraction) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return base + fraction;
}

std::optional<std::uint64_t> sampleIndexAtUs(
    std::uint64_t epochUs,
    std::uint64_t absoluteUs) noexcept
{
    if (absoluteUs < epochUs) {
        return std::nullopt;
    }
    const std::uint64_t deltaUs = absoluteUs - epochUs;
    const std::uint64_t wholeSeconds = deltaUs / kMicrosecondsPerSecond;
    const std::uint64_t remainderUs = deltaUs % kMicrosecondsPerSecond;
    if (wholeSeconds > std::numeric_limits<std::uint64_t>::max()
            / SstvResampler::kOutputSampleRate) {
        return std::nullopt;
    }
    const std::uint64_t wholeSamples =
        wholeSeconds * SstvResampler::kOutputSampleRate;
    const std::uint64_t fractionalSamples =
        remainderUs * SstvResampler::kOutputSampleRate
        / kMicrosecondsPerSecond;
    if (fractionalSamples
        > std::numeric_limits<std::uint64_t>::max() - wholeSamples) {
        return std::nullopt;
    }
    return wholeSamples + fractionalSamples;
}

std::uint64_t saturatingUnsignedAdd(std::uint64_t left,
                                    std::uint64_t right) noexcept
{
    return right > std::numeric_limits<std::uint64_t>::max() - left
        ? std::numeric_limits<std::uint64_t>::max()
        : left + right;
}

SstvVisToneKind visToneKind(const SstvToneObservation& observation) noexcept
{
    if (!observation.valid()) {
        return SstvVisToneKind::Unknown;
    }
    if (std::abs(observation.nominalFrequencyHz - 1'900.0) < 0.5) {
        return SstvVisToneKind::Leader;
    }
    if (std::abs(observation.nominalFrequencyHz - 1'200.0) < 0.5) {
        return SstvVisToneKind::Separator;
    }
    if (std::abs(observation.nominalFrequencyHz - 1'100.0) < 0.5) {
        return SstvVisToneKind::BitOne;
    }
    if (std::abs(observation.nominalFrequencyHz - 1'300.0) < 0.5) {
        return SstvVisToneKind::BitZero;
    }
    return SstvVisToneKind::Unknown;
}

std::optional<std::string> mapVisToSupportedMode(
    const SstvVisDecodeResult& decoded)
{
    // The registry is the only authority for VIS-to-mode mapping.  Catalogue
    // entries without complete protocol data and actual RX capability are not
    // promoted into a runtime mode merely because their names are known.
    const SstvModeRegistry registry = SstvModeRegistry::canonical();
    for (const SstvModeSpec& mode : registry.modes()) {
        if (!mode.protocolDataComplete || !mode.claimsRxSupport()
            || !mode.vis.has_value()) {
            continue;
        }
        const SstvVisSpec& vis = *mode.vis;
        if (decoded.format == SstvVisFormat::Standard
            && vis.encoding == VisEncoding::StandardSevenBit
            && decoded.primary.payloadKnown) {
            const std::uint8_t payload = decoded.primary.payload;
            if ((vis.standardCode.has_value()
                 && payload == *vis.standardCode)
                || std::find(vis.standardAliases.begin(),
                             vis.standardAliases.end(),
                             payload)
                       != vis.standardAliases.end()) {
                return mode.id;
            }
        }
        if (decoded.format == SstvVisFormat::Extended
            && vis.encoding == VisEncoding::Extended
            && decoded.primary.payloadKnown && decoded.extension.has_value()
            && decoded.extension->payloadKnown) {
            const std::vector<std::uint8_t> sequence {
                decoded.primary.payload,
                decoded.extension->payload,
            };
            if (sequence == vis.extendedSequence) {
                return mode.id;
            }
        }
    }
    return std::nullopt;
}

std::optional<std::string> mapNarrowVisToSupportedMode(
    const SstvNarrowVisDecodeResult& decoded)
{
    if (!decoded.valid) {
        return std::nullopt;
    }
    const std::vector<std::uint8_t> sequence(
        decoded.groups.cbegin(), decoded.groups.cend());
    const SstvModeRegistry registry = SstvModeRegistry::canonical();
    for (const SstvModeSpec& mode : registry.modes()) {
        if (!mode.protocolDataComplete || !mode.claimsRxSupport()
            || !mode.vis.has_value()
            || mode.vis->encoding != VisEncoding::Narrow24Bit) {
            continue;
        }
        if (mode.vis->extendedSequence == sequence) {
            return mode.id;
        }
    }
    return std::nullopt;
}

std::uint64_t hashSamples(std::uint64_t hash,
                          const std::vector<float>& samples) noexcept
{
    for (float sample : samples) {
        std::uint32_t bits = 0U;
        static_assert(sizeof(bits) == sizeof(sample),
                      "unexpected float representation size");
        std::memcpy(&bits, &sample, sizeof(bits));
        for (unsigned int shift = 0U; shift < 32U; shift += 8U) {
            hash ^= static_cast<std::uint8_t>(bits >> shift);
            hash *= kFnvPrime;
        }
    }
    return hash;
}

class LifecycleBoundaryRequest final
{
public:
    LifecycleBoundaryRequest(std::atomic<bool>& requested,
                             std::mutex& mutex,
                             std::condition_variable& changed) noexcept
        : m_requested(requested)
        , m_mutex(mutex)
        , m_changed(changed)
    {
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_requested.store(true, std::memory_order_release);
        }
        m_changed.notify_all();
    }

    ~LifecycleBoundaryRequest()
    {
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_requested.store(false, std::memory_order_release);
        }
        m_changed.notify_all();
    }

    LifecycleBoundaryRequest(const LifecycleBoundaryRequest&) = delete;
    LifecycleBoundaryRequest& operator=(const LifecycleBoundaryRequest&) = delete;

private:
    std::atomic<bool>& m_requested;
    std::mutex& m_mutex;
    std::condition_variable& m_changed;
};

class LifecycleCallGuard final
{
public:
    explicit LifecycleCallGuard(bool& active) noexcept
        : m_active(active)
    {
        m_active = true;
    }

    ~LifecycleCallGuard()
    {
        m_active = false;
    }

    LifecycleCallGuard(const LifecycleCallGuard&) = delete;
    LifecycleCallGuard& operator=(const LifecycleCallGuard&) = delete;

private:
    bool& m_active;
};

} // namespace

SstvMonotonicTimeline::SstvMonotonicTimeline(qint64 jitterToleranceNs)
    : m_jitterToleranceNs(jitterToleranceNs)
{
    if (jitterToleranceNs < 0
        || jitterToleranceNs > 1'000'000'000LL) {
        throw std::invalid_argument("invalid SSTV timestamp jitter tolerance");
    }
}

SstvMonotonicTimeline::Candidate SstvMonotonicTimeline::propose(
    qint64 observedLocalMonotonicNs,
    std::size_t sampleCount,
    std::uint32_t sampleRate) const noexcept
{
    Candidate result;
    if (observedLocalMonotonicNs < 0 || sampleCount == 0U
        || !SstvResampler::isSupportedInputRate(sampleRate)) {
        return result;
    }

    result.hadPreviousBlock = m_hasTimestamp;
    result.previousEndNs = m_nextTimestampNs;
    result.startNs = observedLocalMonotonicNs;
    if (m_hasTimestamp) {
        const qint64 jitterCeiling =
            cappedAdd(m_nextTimestampNs, m_jitterToleranceNs);
        if (observedLocalMonotonicNs <= jitterCeiling) {
            result.startNs = m_nextTimestampNs;
        } else {
            result.preservedGap = true;
            result.gapNs = observedLocalMonotonicNs - m_nextTimestampNs;
        }
    }

    if (!blockEndTimestamp(result.startNs,
                           sampleCount,
                           sampleRate,
                           result.endNs)) {
        return Candidate {};
    }
    result.valid = true;
    return result;
}

bool SstvMonotonicTimeline::commit(const Candidate& candidate) noexcept
{
    if (!candidate.valid || candidate.startNs < 0
        || candidate.endNs <= candidate.startNs
        || candidate.hadPreviousBlock != m_hasTimestamp
        || (m_hasTimestamp
            && candidate.previousEndNs != m_nextTimestampNs)) {
        return false;
    }
    m_nextTimestampNs = candidate.endNs;
    m_hasTimestamp = true;
    return true;
}

void SstvMonotonicTimeline::reset() noexcept
{
    m_nextTimestampNs = 0;
    m_hasTimestamp = false;
}

qint64 SstvMonotonicTimeline::jitterToleranceNs() const noexcept
{
    return m_jitterToleranceNs;
}

bool SstvMonotonicTimeline::hasTimestamp() const noexcept
{
    return m_hasTimestamp;
}

qint64 SstvMonotonicTimeline::nextTimestampNs() const noexcept
{
    return m_nextTimestampNs;
}

struct SstvRxRuntime::WorkerPipeline final
{
    std::uint64_t generation {0U};
    std::uint32_t inputSampleRate {0U};
    std::unique_ptr<SstvResampler> resampler;
    std::unique_ptr<SstvPreprocessor> preprocessor;
    std::unique_ptr<SstvFrequencyDemodulator> frequencyDemodulator;
    std::unique_ptr<SstvToneDetector> toneDetector;
    std::unique_ptr<SstvVisDetector> visDetector;
    std::unique_ptr<SstvNarrowVisDetector> narrowVisDetector;
    std::unique_ptr<SstvRxStateMachine> stateMachine;
    std::unique_ptr<SstvAfcController> afcController;
    std::unique_ptr<SstvSlantController> slantController;
    std::unique_ptr<SstvTimingFallbackDetector> fallbackDetector;
    std::unique_ptr<SstvFskIdDetector> fskIdDetector;
    std::unique_ptr<SstvMartinM1RxSession> martinM1Session;
    std::unique_ptr<SstvScottieRxSession> scottieSession;
    std::unique_ptr<SstvRobotRxSession> robotSession;
    std::unique_ptr<SstvSequentialRgbRxSession> sequentialRgbSession;
    std::unique_ptr<SstvPdRxSession> pdSession;
    std::unique_ptr<SstvAvtCountdownDetector> avtCountdownDetector;
    std::unique_ptr<SstvAvtRxSession> avtSession;
    std::unique_ptr<SstvMmsstvRxSession> mmsstvSession;
    std::optional<SstvVisClassifiedEvent> pendingVisRun;
    std::optional<SstvNarrowVisToneEvent> pendingNarrowVisRun;
    std::optional<SstvFallbackSyncPulse> pendingSyncPulse;
    double pendingSyncFrequencyWeight {0.0};
    double pendingSyncConfidenceSum {0.0};
    std::uint64_t pendingSyncObservationCount {0U};
    SstvRxControlSnapshot controls;
    SstvFallbackResult fallbackResult;
    SstvFskIdCandidate latestFskId;
    SstvAudioSource source;

    bool hasInputTimeline {false};
    qint64 expectedInputEndNs {0};
    bool hasSequence {false};
    std::uint64_t lastSequence {0U};
    std::uint64_t toneEpochUs {0U};
    std::uint64_t martinM1LinesReported {0U};
    std::uint64_t scottieLinesReported {0U};
    std::uint64_t robotLinesReported {0U};
    std::uint64_t sequentialRgbLinesReported {0U};
    std::uint64_t pdLinesReported {0U};
    std::uint64_t avtLinesReported {0U};
    std::uint64_t mmsstvLinesReported {0U};
    double avtFrequencyOffsetHz {0.0};
    std::uint64_t avtRetainedStartSample {0U};
    std::uint64_t lastFrequencySample {0U};
    bool hasFrequencySample {false};
    bool hasHeaderEvidenceRange {false};
    std::uint64_t headerEvidenceStartSample {0U};
    std::uint64_t headerEvidenceEndSample {0U};
    std::uint64_t syncPulseCount {0U};
    std::uint64_t syncLineIndex {0U};
    std::uint64_t nominalLinePeriodSamples {0U};
    std::uint64_t retainedAcquisitionId {0U};
    std::uint64_t lastCompletedAcquisitionId {0U};
    // A completed frame is followed by a tail of 1200 Hz/stop audio.  Do not
    // let that tail be mistaken for the leader of a second frame and replace
    // a valid image with a tiny partial acquisition.
    std::uint64_t suppressNativeAcquisitionUntilSample {0U};
    std::uint64_t currentChunkEndNs {0U};
    std::uint64_t progressiveUpdates {0U};
    std::uint64_t firstProgressiveUpdateNs {0U};
    std::uint64_t lastProgressiveUpdateNs {0U};
    std::uint64_t consumedAfcResetSerial {0U};
    std::uint64_t consumedSlantResetSerial {0U};
    bool afcAcquisitionLogged {false};
    bool slantAcquisitionLogged {false};
    bool fallbackUniqueLogged {false};
    bool fallbackAmbiguousLogged {false};
    bool fallbackNoMatchLogged {false};

    bool hasNativeSession() const noexcept
    {
        return martinM1Session || scottieSession || robotSession
            || sequentialRgbSession || pdSession || avtSession
            || mmsstvSession;
    }

    void holdOffNativeAcquisition(std::uint64_t imageEndSample) noexcept
    {
        constexpr std::uint64_t kHoldOffSamples =
            (static_cast<std::uint64_t>(SstvResampler::kOutputSampleRate)
             * 1'000U) / 1'000U;
        const std::uint64_t until = saturatingUnsignedAdd(
            imageEndSample, kHoldOffSamples);
        suppressNativeAcquisitionUntilSample = std::max(
            suppressNativeAcquisitionUntilSample, until);
    }

    void resetSignalPath(std::uint32_t rate, std::uint64_t epochUs)
    {
        inputSampleRate = rate;
        resampler = std::make_unique<SstvResampler>(rate);
        preprocessor->reset();
        frequencyDemodulator->reset(false);
        toneDetector->reset(false);
        toneEpochUs = epochUs;
        hasInputTimeline = false;
        hasSequence = false;
        lastFrequencySample = 0U;
        hasFrequencySample = false;
        pendingSyncPulse.reset();
        pendingSyncFrequencyWeight = 0.0;
        pendingSyncConfidenceSum = 0.0;
        pendingSyncObservationCount = 0U;
        if (slantController) {
            slantController->notifyDiscontinuity();
        }
        if (fallbackDetector) {
            fallbackDetector->reset();
        }
        if (fskIdDetector) {
            fskIdDetector->reset();
        }
    }
};

SstvRxRuntime::SstvRxRuntime(QObject* parent)
    : SstvRxRuntime(Config {}, parent)
{
}

SstvRxRuntime::SstvRxRuntime(Config config, QObject* parent)
    : QObject(parent)
    , m_config(validateConfig(config))
    , m_ingress(new SstvAudioIngress(m_config.ingress, this))
    , m_timeline(m_config.timestampJitterToleranceNs)
{
    m_snapshot.processedPcmHash = kFnvOffset;
    const SstvRxControlSnapshot controls = m_rxControlPolicy.snapshot();
    m_snapshot.controls.revision = controls.revision;
    m_snapshot.controls.replayRetentionSeconds =
        controls.settings.replayRetentionSeconds;
    m_snapshot.replay.retentionSeconds =
        controls.settings.replayRetentionSeconds;
    m_snapshot.replay.capacitySamples = m_retainedAudio.capacitySamples();
}

SstvRxRuntime::~SstvRxRuntime()
{
    if (!isOnOwnerThread()) {
        // A QObject with a child ingress cannot be made safe by merely joining
        // the DSP thread after deletion has already begun on the wrong thread.
        // Fail closed instead of detaching a worker that still owns raw access
        // to this object.  Integrators must disconnect producers and use the
        // affinity thread (normally QObject::deleteLater()).
        qFatal("SstvRxRuntime must be destroyed on its QObject affinity thread");
    }
    if (state() != State::Shutdown) {
        shutdown();
    } else {
        joinWorker();
    }
}

SstvRxRuntime::Config SstvRxRuntime::configuration() const noexcept
{
    return m_config;
}

SstvRxRuntime::State SstvRxRuntime::state() const noexcept
{
    return m_state.load(std::memory_order_acquire);
}

bool SstvRxRuntime::isRunning() const noexcept
{
    return state() == State::Running;
}

SstvRxRouteToken SstvRxRuntime::routeToken() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_timestampMutex);
    return m_route;
}

SstvRxRuntime::Snapshot SstvRxRuntime::snapshot() const noexcept
{
    Snapshot result;
    {
        const std::lock_guard<std::mutex> lock(m_snapshotMutex);
        result = m_snapshot;
    }
    result.state = state();
    result.workerRunning = m_workerRunning.load(std::memory_order_acquire);
    result.revision = m_revision.load(std::memory_order_acquire);
    result.route = routeToken();
    result.producerRejectedCalls =
        m_producerRejectedCalls.load(std::memory_order_relaxed);
    result.wrongThreadLifecycleCalls =
        m_wrongThreadLifecycleCalls.load(std::memory_order_relaxed);
    result.ingress = m_ingress->stats();
    const SstvRxControlSnapshot controls = m_rxControlPolicy.snapshot();
    result.controls.modeControl = controls.settings.modeControl;
    result.controls.manualMode = QString::fromStdString(
        controls.settings.manualMode).left(
            static_cast<qsizetype>(m_config.maximumErrorCharacters));
    result.controls.modeLockEnabled = controls.settings.modeLockEnabled;
    result.controls.lockedMode = QString::fromStdString(
        controls.settings.lockedMode).left(
            static_cast<qsizetype>(m_config.maximumErrorCharacters));
    result.controls.receiveWithoutVis =
        controls.settings.receiveWithoutVis;
    result.controls.timingFallbackEnabled =
        controls.settings.timingFallbackEnabled;
    result.controls.afcMode = controls.settings.afcMode;
    result.controls.manualFrequencyCorrectionHz =
        controls.settings.manualFrequencyCorrectionHz;
    result.controls.slantMode = controls.settings.slantMode;
    result.controls.manualClockErrorPpm =
        controls.settings.manualClockErrorPpm;
    result.controls.replayRetentionSeconds =
        controls.settings.replayRetentionSeconds;
    result.controls.retainRawAudio = controls.settings.retainRawAudio;
    result.controls.diagnosticScopeEnabled =
        controls.settings.diagnosticScopeEnabled;
    result.controls.revision = controls.revision;
    return result;
}

std::shared_ptr<const SstvImageSnapshot>
SstvRxRuntime::latestImageSnapshot() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_snapshotMutex);
    return m_latestImageSnapshot;
}

SstvRxControlSnapshot SstvRxRuntime::rxControlSnapshot() const
{
    return m_rxControlPolicy.snapshot();
}

bool SstvRxRuntime::replaceRxControlSettings(
    SstvRxControlSettings settings)
{
    const SstvRxControlSettings previous =
        m_rxControlPolicy.snapshot().settings;
    const bool accepted = m_rxControlPolicy.replace(std::move(settings));
    if (accepted) {
        recordRxSyncControlChanges(
            previous, m_rxControlPolicy.snapshot().settings);
        notifyControlWaiters();
        scheduleSnapshotNotification(true);
    }
    return accepted;
}

bool SstvRxRuntime::setRxModeControl(SstvRxModeControl control,
                                     std::string manualMode)
{
    const SstvRxControlSettings previous =
        m_rxControlPolicy.snapshot().settings;
    const bool accepted = m_rxControlPolicy.setModeControl(
        control, std::move(manualMode));
    if (accepted) {
        recordRxSyncControlChanges(
            previous, m_rxControlPolicy.snapshot().settings);
        notifyControlWaiters();
        scheduleSnapshotNotification(true);
    }
    return accepted;
}

bool SstvRxRuntime::setRxModeLock(bool enabled, std::string lockedMode)
{
    const SstvRxControlSettings previous =
        m_rxControlPolicy.snapshot().settings;
    const bool accepted = m_rxControlPolicy.setModeLock(
        enabled, std::move(lockedMode));
    if (accepted) {
        recordRxSyncControlChanges(
            previous, m_rxControlPolicy.snapshot().settings);
        notifyControlWaiters();
        scheduleSnapshotNotification(true);
    }
    return accepted;
}

bool SstvRxRuntime::setRxReceiveWithoutVis(bool enabled)
{
    const bool accepted = m_rxControlPolicy.setReceiveWithoutVis(enabled);
    if (accepted) {
        notifyControlWaiters();
        scheduleSnapshotNotification(true);
    }
    return accepted;
}

bool SstvRxRuntime::setRxTimingFallbackEnabled(bool enabled)
{
    const SstvRxControlSettings previous =
        m_rxControlPolicy.snapshot().settings;
    const bool accepted = m_rxControlPolicy.setTimingFallbackEnabled(enabled);
    if (accepted) {
        recordRxSyncControlChanges(
            previous, m_rxControlPolicy.snapshot().settings);
        notifyControlWaiters();
        scheduleSnapshotNotification(true);
    }
    return accepted;
}

bool SstvRxRuntime::setRxAfc(SstvRxAfcMode mode,
                             double manualCorrectionHz)
{
    const SstvRxControlSettings previous =
        m_rxControlPolicy.snapshot().settings;
    const bool accepted = m_rxControlPolicy.setAfc(
        mode, manualCorrectionHz);
    if (accepted) {
        recordRxSyncControlChanges(
            previous, m_rxControlPolicy.snapshot().settings);
        notifyControlWaiters();
        scheduleSnapshotNotification(true);
    }
    return accepted;
}

bool SstvRxRuntime::setRxSlant(SstvRxSlantMode mode,
                               double manualClockErrorPpm)
{
    const SstvRxControlSettings previous =
        m_rxControlPolicy.snapshot().settings;
    const bool accepted = m_rxControlPolicy.setSlant(
        mode, manualClockErrorPpm);
    if (accepted) {
        recordRxSyncControlChanges(
            previous, m_rxControlPolicy.snapshot().settings);
        notifyControlWaiters();
        scheduleSnapshotNotification(true);
    }
    return accepted;
}

bool SstvRxRuntime::setRxReplayRetentionSeconds(std::uint32_t seconds)
{
    const bool accepted =
        m_rxControlPolicy.setReplayRetentionSeconds(seconds);
    if (accepted) {
        notifyControlWaiters();
        scheduleSnapshotNotification(true);
    }
    return accepted;
}

bool SstvRxRuntime::setRxRetainRawAudio(bool enabled)
{
    const bool accepted = m_rxControlPolicy.setRetainRawAudio(enabled);
    if (accepted) {
        scheduleSnapshotNotification(true);
    }
    return accepted;
}

bool SstvRxRuntime::setRxDiagnosticScopeEnabled(bool enabled)
{
    const bool accepted =
        m_rxControlPolicy.setDiagnosticScopeEnabled(enabled);
    if (accepted) {
        scheduleSnapshotNotification(true);
    }
    return accepted;
}

void SstvRxRuntime::resetRxAfc() noexcept
{
    m_rxControlPolicy.requestAfcReset();
    try {
        recordSstvDiagnosticEvent(
            sstvSyncLog(), QtInfoMsg, QStringLiteral("sync.afc-reset"));
    } catch (...) {
        // Diagnostics are best-effort and must not weaken the noexcept
        // lifecycle/control contract.
    }
    notifyControlWaiters();
    scheduleSnapshotNotification(true);
}

void SstvRxRuntime::resetRxSlant() noexcept
{
    m_rxControlPolicy.requestSlantReset();
    try {
        recordSstvDiagnosticEvent(
            sstvSyncLog(), QtInfoMsg, QStringLiteral("sync.slant-reset"));
    } catch (...) {
        // Diagnostics are best-effort and must not weaken the noexcept
        // lifecycle/control contract.
    }
    notifyControlWaiters();
    scheduleSnapshotNotification(true);
}

bool SstvRxRuntime::requestRxRedecode(
    SstvRxRedecodeParameters parameters)
{
    QVariantMap diagnosticFields {
        {QStringLiteral("offsetHz"), parameters.frequencyCorrectionHz},
        {QStringLiteral("slantPpm"), parameters.clockErrorPpm},
    };
    if (!parameters.mode.empty()) {
        diagnosticFields.insert(QStringLiteral("modeId"),
                                QString::fromStdString(parameters.mode));
    }
    const bool accepted = m_rxControlPolicy.requestRedecode(
        std::move(parameters));
    if (accepted) {
        diagnosticFields.insert(QStringLiteral("success"), true);
        try {
            recordSstvDiagnosticEvent(
                sstvRxLog(), QtInfoMsg,
                QStringLiteral("rx.redecode-requested"), diagnosticFields);
        } catch (...) {
            // Re-decode ownership never depends on diagnostics availability.
        }
        notifyControlWaiters();
        scheduleSnapshotNotification(true);
    }
    return accepted;
}

std::optional<SstvRxRetainedAudioSnapshot>
SstvRxRuntime::retainedAudioForAcquisition(
    std::uint64_t acquisitionId)
{
    return m_retainedAudio.snapshotAcquisition(acquisitionId);
}

SstvRxRetainedAudioSnapshot SstvRxRuntime::retainedRecentAudio()
{
    return m_retainedAudio.snapshotRecent();
}

bool SstvRxRuntime::enqueuePcm16(QVector<short> samples,
                                 int sampleRate,
                                 SstvRxRouteToken token) noexcept
{
    return enqueuePcm16At(std::move(samples),
                          sampleRate,
                          token,
                          localMonotonicNowNs());
}

bool SstvRxRuntime::enqueuePcm16At(QVector<short> samples,
                                   int sampleRate,
                                   SstvRxRouteToken token,
                                   qint64 observedLocalMonotonicNs) noexcept
{
    if (state() != State::Running || !token.valid()
        || sampleRate <= 0
        || !SstvResampler::isSupportedInputRate(
            static_cast<std::uint32_t>(sampleRate))) {
        saturatingAdd(m_producerRejectedCalls);
        return false;
    }

    const qsizetype signedCount = samples.size();
    if (signedCount <= 0) {
        saturatingAdd(m_producerRejectedCalls);
        return false;
    }
    const auto count = static_cast<std::size_t>(signedCount);

    const std::lock_guard<std::mutex> lock(m_timestampMutex);
    if (state() != State::Running || token != m_route) {
        saturatingAdd(m_producerRejectedCalls);
        return false;
    }

    const auto candidate = m_timeline.propose(
        observedLocalMonotonicNs,
        count,
        static_cast<std::uint32_t>(sampleRate));
    if (!candidate.valid) {
        saturatingAdd(m_producerRejectedCalls);
        return false;
    }

    SstvPcm16Metadata metadata;
    metadata.sampleRate = sampleRate;
    metadata.source = token.source;
    metadata.monotonicTimestampNs = candidate.startNs;
    metadata.generation = token.generation;
    if (!m_ingress->enqueuePcm16(std::move(samples), metadata)) {
        saturatingAdd(m_producerRejectedCalls);
        return false;
    }
    if (!m_timeline.commit(candidate)) {
        // The timestamp mutex makes this impossible without an internal defect.
        // The audio is already safely queued, so record the fault and force the
        // next callback to be rejected instead of fabricating another overlap.
        saturatingAdd(m_producerRejectedCalls);
        m_timeline.reset();
        return false;
    }
    return true;
}

qint64 SstvRxRuntime::localMonotonicNowNs() noexcept
{
    const auto value = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    if (value < 0) {
        return 0;
    }
    return value > std::numeric_limits<qint64>::max()
        ? std::numeric_limits<qint64>::max()
        : static_cast<qint64>(value);
}

bool SstvRxRuntime::start(SstvAudioSourceKind kind,
                          quint32 streamId,
                          quint64 firstSequence)
{
    if (!isOnOwnerThread()) {
        return rejectLifecycleCall();
    }
    if (m_lifecycleTransitionActive) {
        return false;
    }
    LifecycleCallGuard lifecycle(m_lifecycleTransitionActive);
    if (state() != State::Inactive || !sourceKindIsValid(kind)
        || m_worker.joinable()) {
        return false;
    }

    {
        const std::lock_guard<std::mutex> boundary(m_pipelineBoundaryMutex);
        const std::lock_guard<std::mutex> timestamp(m_timestampMutex);
        if (!m_ingress->activateSource(kind, streamId, firstSequence)) {
            return false;
        }
        updateRouteAfterLifecycleLocked();
        resetTimestampLocked();
        invalidatePublishedAcquisition(SstvRxCause::Reset);
    }

    m_stopRequested.store(false, std::memory_order_release);
    setStateOnOwner(State::Running);
    try {
        m_worker = std::thread(&SstvRxRuntime::workerMain, this);
    } catch (...) {
        m_stopRequested.store(true, std::memory_order_release);
        m_ingress->deactivate();
        {
            const std::lock_guard<std::mutex> timestamp(m_timestampMutex);
            m_route = {};
            resetTimestampLocked();
        }
        setStateOnOwner(State::Error);
        recordWorkerFailure(QStringLiteral("unable to create SSTV RX worker"));
        return false;
    }
    recordRxLifecycleDiagnostic(QStringLiteral("rx.started"),
                                QStringLiteral("running"), true);
    return true;
}

bool SstvRxRuntime::switchSource(SstvAudioSourceKind kind,
                                 quint32 streamId,
                                 quint64 firstSequence)
{
    if (!isOnOwnerThread()) {
        return rejectLifecycleCall();
    }
    if (m_lifecycleTransitionActive) {
        return false;
    }
    LifecycleCallGuard lifecycle(m_lifecycleTransitionActive);
    const State current = state();
    if ((current != State::Running && current != State::Cancelled)
        || !sourceKindIsValid(kind)) {
        return false;
    }

    {
        LifecycleBoundaryRequest request(m_lifecycleBoundaryRequested,
                                         m_controlMutex,
                                         m_controlChanged);
        const std::lock_guard<std::mutex> boundary(m_pipelineBoundaryMutex);
        const std::lock_guard<std::mutex> timestamp(m_timestampMutex);
        if (!m_ingress->switchSource(kind, streamId, firstSequence)) {
            return false;
        }
        updateRouteAfterLifecycleLocked();
        resetTimestampLocked();
        invalidatePublishedAcquisition(SstvRxCause::Reset);
    }
    setStateOnOwner(State::Running);
    notifyControlWaiters();
    recordRxLifecycleDiagnostic(QStringLiteral("rx.reset"),
                                QStringLiteral("source-switched"), true);
    return true;
}

bool SstvRxRuntime::resetStream(quint64 firstSequence)
{
    if (!isOnOwnerThread()) {
        return rejectLifecycleCall();
    }
    if (m_lifecycleTransitionActive) {
        return false;
    }
    LifecycleCallGuard lifecycle(m_lifecycleTransitionActive);
    if (state() != State::Running) {
        return false;
    }

    {
        LifecycleBoundaryRequest request(m_lifecycleBoundaryRequested,
                                         m_controlMutex,
                                         m_controlChanged);
        const std::lock_guard<std::mutex> boundary(m_pipelineBoundaryMutex);
        const std::lock_guard<std::mutex> timestamp(m_timestampMutex);
        if (!m_ingress->resetStream(firstSequence)) {
            return false;
        }
        updateRouteAfterLifecycleLocked();
        resetTimestampLocked();
        invalidatePublishedAcquisition(SstvRxCause::Reset);
    }
    notifyControlWaiters();
    scheduleSnapshotNotification(true);
    recordRxLifecycleDiagnostic(QStringLiteral("rx.reset"),
                                QStringLiteral("stream-reset"), true);
    return true;
}

bool SstvRxRuntime::cancel()
{
    if (!isOnOwnerThread()) {
        return rejectLifecycleCall();
    }
    if (m_lifecycleTransitionActive) {
        return false;
    }
    LifecycleCallGuard lifecycle(m_lifecycleTransitionActive);
    if (state() != State::Running) {
        return false;
    }

    {
        LifecycleBoundaryRequest request(m_lifecycleBoundaryRequested,
                                         m_controlMutex,
                                         m_controlChanged);
        const std::lock_guard<std::mutex> boundary(m_pipelineBoundaryMutex);
        const std::lock_guard<std::mutex> timestamp(m_timestampMutex);
        if (!m_ingress->cancel()) {
            return false;
        }
        resetTimestampLocked();
        invalidatePublishedAcquisition(SstvRxCause::Cancelled);
    }
    setStateOnOwner(State::Cancelled);
    notifyControlWaiters();
    recordRxLifecycleDiagnostic(QStringLiteral("rx.cancelled"),
                                QStringLiteral("cancelled"), false);
    return true;
}

bool SstvRxRuntime::restart(quint64 firstSequence)
{
    if (!isOnOwnerThread()) {
        return rejectLifecycleCall();
    }
    if (m_lifecycleTransitionActive) {
        return false;
    }
    LifecycleCallGuard lifecycle(m_lifecycleTransitionActive);
    if (state() != State::Cancelled) {
        return false;
    }

    {
        LifecycleBoundaryRequest request(m_lifecycleBoundaryRequested,
                                         m_controlMutex,
                                         m_controlChanged);
        const std::lock_guard<std::mutex> boundary(m_pipelineBoundaryMutex);
        const std::lock_guard<std::mutex> timestamp(m_timestampMutex);
        if (!m_ingress->restart(firstSequence)) {
            return false;
        }
        updateRouteAfterLifecycleLocked();
        resetTimestampLocked();
        invalidatePublishedAcquisition(SstvRxCause::Reset);
    }
    setStateOnOwner(State::Running);
    notifyControlWaiters();
    recordRxLifecycleDiagnostic(QStringLiteral("rx.restarted"),
                                QStringLiteral("running"), true);
    return true;
}

bool SstvRxRuntime::stop()
{
    if (!isOnOwnerThread()) {
        return rejectLifecycleCall();
    }
    if (m_lifecycleTransitionActive) {
        return false;
    }
    LifecycleCallGuard lifecycle(m_lifecycleTransitionActive);
    const State current = state();
    if (current == State::Inactive || current == State::Shutdown
        || current == State::Stopping) {
        return false;
    }

    setStateOnOwner(State::Stopping);
    m_stopRequested.store(true, std::memory_order_release);
    {
        LifecycleBoundaryRequest request(m_lifecycleBoundaryRequested,
                                         m_controlMutex,
                                         m_controlChanged);
        const std::lock_guard<std::mutex> boundary(m_pipelineBoundaryMutex);
        const std::lock_guard<std::mutex> timestamp(m_timestampMutex);
        const auto ingressState = m_ingress->state();
        if (ingressState == SstvAudioIngress::State::Active
            || ingressState == SstvAudioIngress::State::Cancelled) {
            m_ingress->deactivate();
        }
        m_route = {};
        resetTimestampLocked();
        invalidatePublishedAcquisition(SstvRxCause::MonitoringStopped);
    }
    notifyControlWaiters();
    joinWorker();
    setStateOnOwner(State::Inactive);
    recordRxLifecycleDiagnostic(QStringLiteral("rx.stopped"),
                                QStringLiteral("inactive"), false);
    return true;
}

bool SstvRxRuntime::shutdown()
{
    if (!isOnOwnerThread()) {
        return rejectLifecycleCall();
    }
    if (m_lifecycleTransitionActive) {
        return false;
    }
    LifecycleCallGuard lifecycle(m_lifecycleTransitionActive);
    if (state() == State::Shutdown) {
        return false;
    }

    setStateOnOwner(State::Stopping);
    m_stopRequested.store(true, std::memory_order_release);
    {
        LifecycleBoundaryRequest request(m_lifecycleBoundaryRequested,
                                         m_controlMutex,
                                         m_controlChanged);
        const std::lock_guard<std::mutex> boundary(m_pipelineBoundaryMutex);
        const std::lock_guard<std::mutex> timestamp(m_timestampMutex);
        if (m_ingress->state() != SstvAudioIngress::State::Shutdown) {
            m_ingress->shutdown();
        }
        m_route = {};
        resetTimestampLocked();
        invalidatePublishedAcquisition(SstvRxCause::Disabled);
    }
    notifyControlWaiters();
    joinWorker();
    setStateOnOwner(State::Shutdown);
    recordRxLifecycleDiagnostic(QStringLiteral("rx.shutdown"),
                                QStringLiteral("shutdown"), false);
    return true;
}

SstvRxRuntime::Config SstvRxRuntime::validateConfig(Config config)
{
    // SstvAudioIngress validates its own capacities.  Validate the runtime-only
    // bounds here before constructing any worker state.
    if (config.timestampJitterToleranceNs < 0
        || config.timestampJitterToleranceNs > 1'000'000'000LL
        || config.snapshotNotificationIntervalMs < 20U
        || config.snapshotNotificationIntervalMs > 10'000U
        || config.maximumErrorCharacters == 0U
        || config.maximumErrorCharacters > 4'096U
        || config.maximumDiagnosticScopePoints == 0U
        || config.maximumDiagnosticScopePoints > 2'048U) {
        throw std::invalid_argument("invalid SSTV RX runtime configuration");
    }
    return config;
}

bool SstvRxRuntime::sourceKindIsValid(SstvAudioSourceKind kind) noexcept
{
    switch (kind) {
    case SstvAudioSourceKind::LocalSoundCard:
    case SstvAudioSourceKind::LegacyBackend:
    case SstvAudioSourceKind::DecoPort:
    case SstvAudioSourceKind::Tci:
    case SstvAudioSourceKind::WebSdr:
    case SstvAudioSourceKind::RtlSdr:
    case SstvAudioSourceKind::Replay:
        return true;
    case SstvAudioSourceKind::Unknown:
        return false;
    }
    return false;
}

void SstvRxRuntime::saturatingAdd(std::atomic<std::uint64_t>& value,
                                  std::uint64_t increment) noexcept
{
    std::uint64_t observed = value.load(std::memory_order_relaxed);
    const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
    for (;;) {
        const std::uint64_t desired = increment > maximum - observed
            ? maximum
            : observed + increment;
        if (value.compare_exchange_weak(observed,
                                        desired,
                                        std::memory_order_relaxed,
                                        std::memory_order_relaxed)) {
            return;
        }
    }
}

void SstvRxRuntime::saturatingAdd(std::uint64_t& value,
                                  std::uint64_t increment) noexcept
{
    value = increment > std::numeric_limits<std::uint64_t>::max() - value
        ? std::numeric_limits<std::uint64_t>::max()
        : value + increment;
}

bool SstvRxRuntime::isOnOwnerThread() const noexcept
{
    return QThread::currentThread() == thread();
}

bool SstvRxRuntime::rejectLifecycleCall() noexcept
{
    saturatingAdd(m_wrongThreadLifecycleCalls);
    scheduleSnapshotNotification(true);
    return false;
}

void SstvRxRuntime::setStateOnOwner(State next)
{
    m_state.store(next, std::memory_order_release);
    scheduleSnapshotNotification(true);
    scheduleRuntimeStateNotification(next, m_ingress->generation());
}

void SstvRxRuntime::scheduleRuntimeStateNotification(
    State next,
    std::uint64_t generation) noexcept
{
    bool shouldPost = false;
    {
        const std::lock_guard<std::mutex> lock(m_notificationMutex);
        m_pendingState = next;
        m_pendingStateGeneration = generation;
        if (!m_stateNotificationPending) {
            m_stateNotificationPending = true;
            shouldPost = true;
        }
    }
    if (!shouldPost) {
        return;
    }
    if (!QMetaObject::invokeMethod(
            this,
            "deliverRuntimeStateNotification",
            Qt::QueuedConnection)) {
        const std::lock_guard<std::mutex> lock(m_notificationMutex);
        m_stateNotificationPending = false;
    }
}

void SstvRxRuntime::deliverRuntimeStateNotification() noexcept
{
    State pending = State::Inactive;
    std::uint64_t generation = 0U;
    {
        const std::lock_guard<std::mutex> lock(m_notificationMutex);
        if (!m_stateNotificationPending) {
            return;
        }
        pending = m_pendingState;
        generation = m_pendingStateGeneration;
        m_stateNotificationPending = false;
    }
    // Rapid owner-thread transitions are intentionally coalesced.  Never tell
    // observers that an obsolete state is current.
    if (state() == pending) {
        Q_EMIT runtimeStateChanged(pending, generation);
    }
}

void SstvRxRuntime::updateRouteAfterLifecycleLocked()
{
    m_route.source = m_ingress->activeSource();
    m_route.generation = m_ingress->generation();
}

void SstvRxRuntime::resetTimestampLocked() noexcept
{
    m_timeline.reset();
}

void SstvRxRuntime::invalidatePublishedAcquisition(
    SstvRxCause cause) noexcept
{
    const std::lock_guard<std::mutex> lock(m_snapshotMutex);
    m_snapshot.generationChunksProcessed = 0U;
    m_snapshot.activeSampleRate = 0U;
    m_snapshot.lastChunkSequence = 0U;
    m_snapshot.lastChunkStartNs = 0;
    m_snapshot.lastFrequencyHz = 0.0;
    m_snapshot.lastFrequencyConfidence = 0.0;
    m_snapshot.processedPcmHash = kFnvOffset;
    m_snapshot.rxState = SstvRxState::Disabled;
    m_snapshot.rxCause = cause;
    m_snapshot.vis = {};
    m_snapshot.image = {};
    m_latestImageSnapshot.reset();
    m_snapshot.lastError.clear();
}

void SstvRxRuntime::notifyControlWaiters() noexcept
{
    // Synchronize with both condition-variable predicate evaluations.  Merely
    // notifying after changing their atomic inputs could otherwise lose the
    // transition in the narrow interval between predicate evaluation and the
    // wait operation.
    {
        const std::lock_guard<std::mutex> control(m_controlMutex);
    }
    m_controlChanged.notify_all();
}

void SstvRxRuntime::joinWorker() noexcept
{
    if (!m_worker.joinable()) {
        return;
    }
    if (m_worker.get_id() == std::this_thread::get_id()) {
        qFatal("SstvRxRuntime cannot join or destroy itself from its DSP worker");
    }
    m_worker.join();
}

void SstvRxRuntime::workerMain() noexcept
{
    m_workerRunning.store(true, std::memory_order_release);
    {
        const std::lock_guard<std::mutex> lock(m_snapshotMutex);
        saturatingAdd(m_snapshot.workerStarts);
    }
    scheduleSnapshotNotification(true);

    try {
        WorkerPipeline pipeline;
        resetWorkerPipeline(pipeline, m_ingress->generation(), 0);

        const auto awaitLifecycleBoundary = [this] {
            if (!m_lifecycleBoundaryRequested.load(
                    std::memory_order_acquire)) {
                return;
            }
            std::unique_lock<std::mutex> control(m_controlMutex);
            m_controlChanged.wait(control, [this] {
                return m_stopRequested.load(std::memory_order_acquire)
                    || !m_lifecycleBoundaryRequested.load(
                        std::memory_order_acquire);
            });
        };

        while (!m_stopRequested.load(std::memory_order_acquire)) {
            awaitLifecycleBoundary();
            if (m_stopRequested.load(std::memory_order_acquire)) {
                break;
            }

            SstvPcm16Chunk chunk;
            const auto waitResult = m_ingress->waitPop(chunk);
            if (m_stopRequested.load(std::memory_order_acquire)) {
                break;
            }
            // A lifecycle request made while waitPop() was returning wins the
            // next DSP boundary.  The popped chunk is checked again against
            // the ingress generation after the owner transition completes.
            awaitLifecycleBoundary();
            if (m_stopRequested.load(std::memory_order_acquire)) {
                break;
            }

            if (waitResult == SstvAudioIngress::WaitResult::Cancelled) {
                const std::uint64_t generation = m_ingress->generation();
                {
                    const std::lock_guard<std::mutex> boundary(
                        m_pipelineBoundaryMutex);
                    resetWorkerPipeline(pipeline, generation, 0);
                }

                // Key the sleep to the ingress state, which changes before the
                // public runtime state signal is emitted.  This closes the
                // short cancel-transition window without polling waitPop().
                if (m_ingress->state()
                    == SstvAudioIngress::State::Cancelled) {
                    std::unique_lock<std::mutex> control(m_controlMutex);
                    m_controlChanged.wait(control, [this, generation] {
                        return m_stopRequested.load(std::memory_order_acquire)
                            || m_ingress->state()
                                != SstvAudioIngress::State::Cancelled
                            || m_ingress->generation() != generation;
                    });
                }
                continue;
            }

            const std::lock_guard<std::mutex> boundary(
                m_pipelineBoundaryMutex);
            if (m_stopRequested.load(std::memory_order_acquire)) {
                break;
            }
            if (!m_ingress->isActive()
                || chunk.generation != m_ingress->generation()) {
                {
                    const std::lock_guard<std::mutex> lock(m_snapshotMutex);
                    saturatingAdd(m_snapshot.staleChunksDiscarded);
                }
                if (pipeline.generation != m_ingress->generation()) {
                    resetWorkerPipeline(
                        pipeline, m_ingress->generation(), 0);
                }
                scheduleSnapshotNotification();
                continue;
            }
            if (pipeline.generation != chunk.generation) {
                resetWorkerPipeline(
                    pipeline, chunk.generation, chunk.startTime.count());
            }
            processChunk(pipeline, std::move(chunk));
        }
        if (pipeline.stateMachine) {
            const std::uint64_t eventMs =
                pipeline.stateMachine->metrics().lastEventAtMs;
            terminateMartinM1ForDiscontinuity(pipeline, eventMs);
            terminateScottieForDiscontinuity(pipeline, eventMs);
            terminateRobotForDiscontinuity(pipeline, eventMs);
            terminateSequentialRgbForDiscontinuity(pipeline, eventMs);
            terminatePdForDiscontinuity(pipeline, eventMs);
            terminateAvtForDiscontinuity(pipeline, eventMs);
            terminateMmsstvForDiscontinuity(pipeline, eventMs);
        }
    } catch (const std::exception& error) {
        recordWorkerFailure(QString::fromUtf8(error.what()));
    } catch (...) {
        recordWorkerFailure(QStringLiteral("unknown SSTV RX worker failure"));
    }

    m_workerRunning.store(false, std::memory_order_release);
    {
        const std::lock_guard<std::mutex> lock(m_snapshotMutex);
        saturatingAdd(m_snapshot.workerStops);
    }
    scheduleSnapshotNotification(true);
}

bool SstvRxRuntime::processChunk(WorkerPipeline& pipeline,
                                 SstvPcm16Chunk chunk)
{
    const auto dspStartedAt = std::chrono::steady_clock::now();
    if (chunk.samples.isEmpty()
        || !SstvResampler::isSupportedInputRate(chunk.sampleRate)
        || chunk.startTime.count() < 0
        || chunk.generation != pipeline.generation) {
        const std::lock_guard<std::mutex> lock(m_snapshotMutex);
        saturatingAdd(m_snapshot.processingFailures);
        return false;
    }

    pipeline.source = chunk.source;
    applyControlSnapshot(pipeline);

    qint64 chunkEndNs = 0;
    if (!blockEndTimestamp(chunk.startTime.count(),
                           static_cast<std::size_t>(chunk.samples.size()),
                           chunk.sampleRate,
                           chunkEndNs)) {
        const std::lock_guard<std::mutex> lock(m_snapshotMutex);
        saturatingAdd(m_snapshot.processingFailures);
        return false;
    }

    bool discontinuity = false;
    std::uint64_t gapNs = 0U;
    if (pipeline.hasInputTimeline) {
        if (chunk.startTime.count() < pipeline.expectedInputEndNs) {
            const std::lock_guard<std::mutex> lock(m_snapshotMutex);
            saturatingAdd(m_snapshot.processingFailures);
            return false;
        }
        if (chunk.startTime.count() > pipeline.expectedInputEndNs) {
            discontinuity = true;
            gapNs = static_cast<std::uint64_t>(
                chunk.startTime.count() - pipeline.expectedInputEndNs);
        }
    }
    if (pipeline.hasSequence
        && (pipeline.lastSequence == std::numeric_limits<std::uint64_t>::max()
            || chunk.sequence != pipeline.lastSequence + 1U)) {
        discontinuity = true;
    }

    const std::uint64_t chunkStartUs =
        static_cast<std::uint64_t>(chunk.startTime.count()) / 1'000U;
    if (!pipeline.resampler) {
        pipeline.resetSignalPath(chunk.sampleRate, chunkStartUs);
    } else if (pipeline.inputSampleRate != chunk.sampleRate) {
        const std::lock_guard<std::mutex> lock(m_snapshotMutex);
        saturatingAdd(m_snapshot.processingFailures);
        return false;
    } else if (discontinuity) {
        if (qEnvironmentVariableIsSet("DECODIUM_SSTV_TRACE_TIMELINE")) {
            qInfo().noquote()
                << "[SSTV][TIMELINE] discontinuity"
                << "gap_ns=" << gapNs
                << "sequence=" << chunk.sequence
                << "previous_sequence=" << pipeline.lastSequence
                << "native_session=" << pipeline.hasNativeSession();
        }
        // Preserve VIS/state-machine acquisition history so the first new tone
        // carries the actual timeline gap into SstvVisDetector.  Reset every
        // continuous-signal component to prevent FIR/phase state from bridging
        // missing audio.
        terminateMartinM1ForDiscontinuity(
            pipeline, chunkStartUs / 1'000U);
        terminateScottieForDiscontinuity(
            pipeline, chunkStartUs / 1'000U);
        terminateRobotForDiscontinuity(
            pipeline, chunkStartUs / 1'000U);
        terminateSequentialRgbForDiscontinuity(
            pipeline, chunkStartUs / 1'000U);
        terminatePdForDiscontinuity(
            pipeline, chunkStartUs / 1'000U);
        terminateAvtForDiscontinuity(
            pipeline, chunkStartUs / 1'000U);
        terminateMmsstvForDiscontinuity(
            pipeline, chunkStartUs / 1'000U);
        pipeline.resetSignalPath(chunk.sampleRate, chunkStartUs);
        const std::lock_guard<std::mutex> lock(m_snapshotMutex);
        saturatingAdd(m_snapshot.discontinuities);
        saturatingAdd(m_snapshot.preservedGapNanoseconds, gapNs);
    }

    std::vector<float> normalized(
        static_cast<std::size_t>(chunk.samples.size()));
    for (qsizetype index = 0; index < chunk.samples.size(); ++index) {
        normalized[static_cast<std::size_t>(index)] =
            normalizePcm16(chunk.samples[index]);
    }
    std::vector<float> resampled = pipeline.resampler->process(normalized);
    std::vector<float> preprocessed =
        pipeline.preprocessor->process(resampled);
    SstvAudioChunk retainedChunk;
    retainedChunk.source = chunk.source;
    retainedChunk.sampleRate = SstvResampler::kOutputSampleRate;
    retainedChunk.startTime = chunk.startTime;
    retainedChunk.sequence = chunk.sequence;
    retainedChunk.samples = preprocessed;
    static_cast<void>(m_retainedAudio.append(std::move(retainedChunk)));
    std::vector<SstvFrequencyObservation> frequencies =
        pipeline.frequencyDemodulator->consume(preprocessed);
    std::vector<SstvToneObservation> tones;

    const std::uint64_t nowMs =
        static_cast<std::uint64_t>(chunk.startTime.count()) / 1'000'000U;
    const std::uint64_t chunkEndMs =
        static_cast<std::uint64_t>(chunkEndNs) / 1'000'000U;
    const std::uint64_t chunkEndSample = sampleIndexAtUs(
        pipeline.toneEpochUs,
        static_cast<std::uint64_t>(chunkEndNs) / 1'000U).value_or(0U);
    pipeline.currentChunkEndNs = static_cast<std::uint64_t>(chunkEndNs);
    pipeline.stateMachine->dispatch(nowMs, SstvRxTick {});

    // Close an image that reaches its exact end in this chunk before looking
    // for another VIS header.  Otherwise a leader immediately following the
    // final image sample is discarded merely because the old session was
    // still alive when the tone loop began.  Tone windows overlapping the
    // old image remain excluded; detection resumes only at its exclusive end.
    std::optional<std::uint64_t> resumeToneAtSample;
    const bool hadNativeSession = pipeline.hasNativeSession();
    if (pipeline.martinM1Session) {
        const std::uint64_t imageEnd =
            pipeline.martinM1Session->imageEndSample();
        consumeMartinM1Observations(pipeline, frequencies, chunkEndMs);
        if (!pipeline.martinM1Session) {
            resumeToneAtSample = imageEnd;
        }
    } else if (pipeline.scottieSession) {
        const std::uint64_t imageEnd =
            pipeline.scottieSession->imageEndSample();
        consumeScottieObservations(pipeline, frequencies, chunkEndMs);
        if (!pipeline.scottieSession) {
            resumeToneAtSample = imageEnd;
        }
    } else if (pipeline.robotSession) {
        const std::uint64_t imageEnd =
            pipeline.robotSession->imageEndSample();
        consumeRobotObservations(
            pipeline, frequencies, chunkEndMs, chunkEndSample);
        if (!pipeline.robotSession) {
            resumeToneAtSample = imageEnd;
        }
    } else if (pipeline.sequentialRgbSession) {
        const std::uint64_t imageEnd =
            pipeline.sequentialRgbSession->imageEndSample();
        consumeSequentialRgbObservations(pipeline, frequencies, chunkEndMs);
        if (!pipeline.sequentialRgbSession) {
            resumeToneAtSample = imageEnd;
        }
    } else if (pipeline.pdSession) {
        const std::uint64_t imageEnd = pipeline.pdSession->imageEndSample();
        consumePdObservations(pipeline, frequencies, chunkEndMs);
        if (!pipeline.pdSession) {
            resumeToneAtSample = imageEnd;
        }
    } else if (pipeline.avtSession) {
        const std::uint64_t imageEnd = pipeline.avtSession->imageEndSample();
        consumeAvtObservations(pipeline, frequencies, chunkEndMs);
        if (!pipeline.avtSession) {
            resumeToneAtSample = imageEnd;
        }
    } else if (pipeline.mmsstvSession) {
        const std::uint64_t imageEnd =
            pipeline.mmsstvSession->imageEndSample();
        consumeMmsstvObservations(pipeline, frequencies, chunkEndMs);
        if (!pipeline.mmsstvSession) {
            resumeToneAtSample = imageEnd;
        }
    }
    const bool retainedNativeSession = hadNativeSession
        && pipeline.hasNativeSession();

    if (resumeToneAtSample.has_value()) {
        // The old image and the new leader may meet inside one worker chunk.
        // Start the VIS detector from a clean frame and, at the exact boundary,
        // discard the tone detector's overlap window while preserving absolute
        // sample coordinates.  Otherwise a phase-dependent window containing
        // both image pixels and leader can reject the only following VIS.
        pipeline.pendingVisRun.reset();
        pipeline.pendingNarrowVisRun.reset();
        pipeline.visDetector->reset(false);
        pipeline.narrowVisDetector->reset(false);
        const std::uint64_t toneStart =
            pipeline.toneDetector->metrics().samplesConsumed;
        const std::uint64_t available = preprocessed.size();
        const std::uint64_t toneEnd = saturatingUnsignedAdd(
            toneStart, available);
        if (*resumeToneAtSample > toneStart
            && *resumeToneAtSample < toneEnd) {
            const std::size_t prefix = static_cast<std::size_t>(
                *resumeToneAtSample - toneStart);
            static_cast<void>(pipeline.toneDetector->consume(
                preprocessed.data(), prefix));
            pipeline.toneDetector->resetAtStreamSample(
                *resumeToneAtSample, false);
            tones = pipeline.toneDetector->consume(
                preprocessed.data() + prefix,
                preprocessed.size() - prefix);
        } else if (*resumeToneAtSample <= toneStart) {
            pipeline.toneDetector->resetAtStreamSample(toneStart, false);
            tones = pipeline.toneDetector->consume(preprocessed);
        } else {
            tones = pipeline.toneDetector->consume(preprocessed);
        }
    } else {
        tones = pipeline.toneDetector->consume(preprocessed);
    }

    std::vector<SstvVisClassifiedEvent> events;
    events.reserve(std::min(
        tones.size(), SstvVisDetector::MaximumEventsPerConsume));
    const std::uint64_t hopDurationUs = std::max<std::uint64_t>(
        1U,
        static_cast<std::uint64_t>(
            pipeline.toneDetector->config().hopSamples)
            * kMicrosecondsPerSecond
            / SstvResampler::kOutputSampleRate);

    auto consumeEvents = [&] {
        if (events.empty()) {
            return;
        }
        const bool hasLeader = std::any_of(
            events.begin(), events.end(), [](const auto& event) {
                return event.tone == SstvVisToneKind::Leader
                    && event.confidence > 0.0;
            });
        if (hasLeader
            && (pipeline.stateMachine->state()
                    == SstvRxState::SearchingLeader
                || isTerminalRxState(pipeline.stateMachine->state()))) {
            const auto leader = std::find_if(
                events.begin(), events.end(), [](const auto& event) {
                    return event.tone == SstvVisToneKind::Leader
                        && event.confidence > 0.0;
                });
            const std::uint64_t observedAtMs = std::max(
                pipeline.stateMachine->metrics().lastEventAtMs,
                saturatingUnsignedAdd(leader->startTimeUs,
                                      leader->durationUs)
                    / 1'000U);
            pipeline.stateMachine->dispatch(
                observedAtMs,
                SstvRxLeaderObserved {
                    leader->confidence,
                    leader->frequencyOffsetHz,
                });
        }

        const auto results = pipeline.visDetector->consumeClassified(events);
        const auto detectorState = pipeline.visDetector->state();
        if (pipeline.stateMachine->state() == SstvRxState::LeaderCandidate
            && detectorState != SstvVisDetectorState::SearchingLeader
            && detectorState != SstvVisDetectorState::FirstLeader) {
            const std::uint64_t confirmedAtMs = std::max(
                pipeline.stateMachine->metrics().lastEventAtMs,
                saturatingUnsignedAdd(events.back().startTimeUs,
                                      events.back().durationUs)
                    / 1'000U);
            pipeline.stateMachine->dispatch(confirmedAtMs,
                                            SstvRxLeaderConfirmed {});
        }
        recordVisResults(pipeline, results, chunk.generation);
        events.clear();
    };

    auto flushPendingRun = [&] {
        if (!pipeline.pendingVisRun.has_value()) {
            return;
        }
        events.push_back(*pipeline.pendingVisRun);
        pipeline.pendingVisRun.reset();
        if (events.size() == SstvVisDetector::MaximumEventsPerConsume) {
            consumeEvents();
        }
    };

    std::vector<SstvNarrowVisToneEvent> narrowEvents;
    narrowEvents.reserve(std::min(
        tones.size(), SstvNarrowVisDetector::MaximumEventsPerConsume));
    auto consumeNarrowEvents = [&] {
        if (narrowEvents.empty()) {
            return;
        }
        const SstvNarrowVisDetectorState beforeState =
            pipeline.narrowVisDetector->state();
        const auto results = pipeline.narrowVisDetector->consume(
            narrowEvents);
        if (beforeState == SstvNarrowVisDetectorState::AwaitingGuard
            && pipeline.narrowVisDetector->state()
                == SstvNarrowVisDetectorState::ReadingSymbols
            && pipeline.stateMachine->state()
                == SstvRxState::LeaderCandidate) {
            const SstvNarrowVisToneEvent& guard = narrowEvents.back();
            const std::uint64_t confirmedAtMs = std::max(
                pipeline.stateMachine->metrics().lastEventAtMs,
                saturatingUnsignedAdd(guard.startTimeUs,
                                      guard.durationUs)
                    / 1'000U);
            pipeline.stateMachine->dispatch(confirmedAtMs,
                                            SstvRxLeaderConfirmed {});
        }
        recordNarrowVisResults(pipeline, results, chunk.generation);
        narrowEvents.clear();
    };

    auto flushPendingNarrowRun = [&] {
        if (!pipeline.pendingNarrowVisRun.has_value()) {
            return;
        }
        narrowEvents.push_back(*pipeline.pendingNarrowVisRun);
        pipeline.pendingNarrowVisRun.reset();
        if (narrowEvents.size()
            == SstvNarrowVisDetector::MaximumEventsPerConsume) {
            consumeNarrowEvents();
        }
    };

    for (const SstvToneObservation& tone : tones) {
        if (resumeToneAtSample.has_value()
            && tone.startSample < *resumeToneAtSample) {
            continue;
        }
        if (pipeline.hasNativeSession()) {
            pipeline.pendingVisRun.reset();
            pipeline.pendingNarrowVisRun.reset();
            continue;
        }
        const std::uint64_t offsetUs = sampleOffsetUs(tone.startSample);
        const std::uint64_t absoluteStartUs = saturatingUnsignedAdd(
            pipeline.toneEpochUs, offsetUs);

        const bool narrowTone = tone.valid()
            && (std::abs(tone.nominalFrequencyHz
                         - SstvNarrowVisCodec::OneFrequencyHz) < 0.5
                || std::abs(tone.nominalFrequencyHz
                            - SstvNarrowVisCodec::ZeroFrequencyHz) < 0.5);
        if (narrowTone) {
            SstvNarrowVisToneEvent narrowEvent;
            narrowEvent.startTimeUs = absoluteStartUs;
            narrowEvent.durationUs = hopDurationUs;
            narrowEvent.frequencyHz = tone.detectedFrequencyHz;
            narrowEvent.confidence = tone.confidence;
            if (pipeline.pendingNarrowVisRun.has_value()) {
                SstvNarrowVisToneEvent& pending =
                    *pipeline.pendingNarrowVisRun;
                const bool pendingOne =
                    std::abs(pending.frequencyHz
                             - SstvNarrowVisCodec::OneFrequencyHz)
                    < std::abs(pending.frequencyHz
                               - SstvNarrowVisCodec::ZeroFrequencyHz);
                const bool currentOne =
                    std::abs(tone.nominalFrequencyHz
                             - SstvNarrowVisCodec::OneFrequencyHz) < 0.5;
                const std::uint64_t pendingEnd = saturatingUnsignedAdd(
                    pending.startTimeUs, pending.durationUs);
                if (pendingOne == currentOne
                    && narrowEvent.startTimeUs == pendingEnd) {
                    const long double oldWeight =
                        static_cast<long double>(pending.durationUs);
                    const long double newWeight =
                        static_cast<long double>(narrowEvent.durationUs);
                    const long double totalWeight = oldWeight + newWeight;
                    pending.frequencyHz = static_cast<double>(
                        (oldWeight * pending.frequencyHz
                         + newWeight * narrowEvent.frequencyHz)
                        / totalWeight);
                    pending.confidence = static_cast<double>(
                        (oldWeight * pending.confidence
                         + newWeight * narrowEvent.confidence)
                        / totalWeight);
                    pending.durationUs = saturatingUnsignedAdd(
                        pending.durationUs, narrowEvent.durationUs);
                } else {
                    flushPendingNarrowRun();
                    consumeNarrowEvents();
                    if (!pipeline.hasNativeSession()) {
                        pipeline.pendingNarrowVisRun = narrowEvent;
                    }
                }
            } else {
                pipeline.pendingNarrowVisRun = narrowEvent;
            }
        } else {
            flushPendingNarrowRun();
            consumeNarrowEvents();
            if (tone.valid()
                && pipeline.narrowVisDetector->state()
                    != SstvNarrowVisDetectorState::SearchingLeader) {
                SstvNarrowVisToneEvent unknown;
                unknown.startTimeUs = absoluteStartUs;
                unknown.durationUs = hopDurationUs;
                unknown.frequencyHz = tone.valid()
                    && std::isfinite(tone.detectedFrequencyHz)
                    ? tone.detectedFrequencyHz : 1'500.0;
                unknown.confidence = tone.valid() ? tone.confidence : 0.0;
                narrowEvents.push_back(unknown);
                consumeNarrowEvents();
            }
        }

        if (pipeline.hasNativeSession()) {
            pipeline.pendingVisRun.reset();
            pipeline.pendingNarrowVisRun.reset();
            events.clear();
            continue;
        }
        if (pipeline.narrowVisDetector->state()
            == SstvNarrowVisDetectorState::ReadingSymbols) {
            pipeline.pendingVisRun.reset();
            events.clear();
            pipeline.visDetector->reset(false);
            continue;
        }

        SstvVisClassifiedEvent event;
        event.startTimeUs = absoluteStartUs;
        event.durationUs = hopDurationUs;
        event.tone = visToneKind(tone);
        event.frequencyOffsetHz = tone.valid()
            ? tone.frequencyOffsetHz
            : 0.0;
        event.confidence = tone.valid() ? tone.confidence : 0.0;

        // SstvVisDetector stores a bounded diagnostic trace of physical tone
        // runs, not every overlapping FFT window.  Collapse adjacent equal
        // classifications here.  Ambiguous transition windows stay omitted
        // during the leader phases, but are retained while reading VIS data:
        // Scottie has no leading image sync to close a trailing 1200 Hz stop,
        // so that final half-window is needed as explicit unknown coverage.
        if (event.tone == SstvVisToneKind::Unknown
            || !(event.confidence > 0.0)) {
            flushPendingRun();
            consumeEvents();
            const SstvVisDetectorState detectorState =
                pipeline.visDetector->state();
            if (detectorState == SstvVisDetectorState::ReadingBits
                || detectorState == SstvVisDetectorState::AwaitingStop) {
                events.push_back(event);
                consumeEvents();
            }
            continue;
        }
        if (pipeline.pendingVisRun.has_value()) {
            SstvVisClassifiedEvent& pending = *pipeline.pendingVisRun;
            // The overlapping FFT window at the 1200 Hz start-bit boundary
            // can classify the following data bit one hop early.  Feeding
            // that shortened separator to SstvVisDetector makes it infer an
            // invalid VIS clock (notably for odd Robot B/W aliases and VIS
            // 59).  Bound the correction to this one protocol transition:
            // accept at most one hop of early BitZero/BitOne classification,
            // close the separator at its canonical 30 ms boundary, and let
            // the next observation begin the data-bit run.
            if (pending.tone == SstvVisToneKind::Separator
                && (event.tone == SstvVisToneKind::BitZero
                    || event.tone == SstvVisToneKind::BitOne)) {
                constexpr std::uint64_t kVisSymbolDurationUs = 30'000U;
                const std::uint64_t expectedEnd = saturatingUnsignedAdd(
                    pending.startTimeUs, kVisSymbolDurationUs);
                if (event.startTimeUs < expectedEnd
                    && expectedEnd - event.startTimeUs <= hopDurationUs) {
                    pending.durationUs = kVisSymbolDurationUs;
                    flushPendingRun();
                    continue;
                }
            }
            const std::uint64_t pendingEnd = saturatingUnsignedAdd(
                pending.startTimeUs, pending.durationUs);
            if (pending.tone == event.tone
                && event.startTimeUs == pendingEnd) {
                const long double oldWeight =
                    static_cast<long double>(pending.durationUs);
                const long double newWeight =
                    static_cast<long double>(event.durationUs);
                const long double totalWeight = oldWeight + newWeight;
                pending.frequencyOffsetHz = static_cast<double>(
                    (oldWeight * pending.frequencyOffsetHz
                     + newWeight * event.frequencyOffsetHz)
                    / totalWeight);
                pending.confidence = static_cast<double>(
                    (oldWeight * pending.confidence
                     + newWeight * event.confidence)
                    / totalWeight);
                pending.durationUs = saturatingUnsignedAdd(
                    pending.durationUs, event.durationUs);
                continue;
            }
            flushPendingRun();
            if (pipeline.hasNativeSession()) {
                continue;
            }
        }
        pipeline.pendingVisRun = event;
    }
    consumeEvents();
    // VIS processing above establishes the exact bounded header coordinates.
    // AFC can now use only that range (or trusted sync in an existing image),
    // never same-chunk luminance that merely happens to be near 1900 Hz.
    updateAfcFromTones(pipeline, tones, frequencies, resumeToneAtSample);
    updateSyncFallbackAndFsk(pipeline, tones, chunkEndMs);
    consumeAvtCountdownObservations(pipeline, frequencies, chunkEndMs);

    if (!retainedNativeSession) {
        consumeMartinM1Observations(pipeline, frequencies, chunkEndMs);
        consumeScottieObservations(pipeline, frequencies, chunkEndMs);
        consumeRobotObservations(
            pipeline, frequencies, chunkEndMs, chunkEndSample);
        consumeSequentialRgbObservations(pipeline, frequencies, chunkEndMs);
        consumePdObservations(pipeline, frequencies, chunkEndMs);
        consumeAvtObservations(pipeline, frequencies, chunkEndMs);
        consumeMmsstvObservations(pipeline, frequencies, chunkEndMs);
    }
    if (!frequencies.empty()) {
        pipeline.lastFrequencySample = frequencies.back().centreSample;
        pipeline.hasFrequencySample = true;
    }

    pipeline.expectedInputEndNs = chunkEndNs;
    pipeline.hasInputTimeline = true;
    pipeline.lastSequence = chunk.sequence;
    pipeline.hasSequence = true;

    const auto dspFinishedAt = std::chrono::steady_clock::now();
    const auto elapsedSigned = std::chrono::duration_cast<
        std::chrono::nanoseconds>(dspFinishedAt - dspStartedAt).count();
    const std::uint64_t dspElapsedNs = elapsedSigned <= 0
        ? 1U : static_cast<std::uint64_t>(elapsedSigned);
    const std::size_t clippedSamples = static_cast<std::size_t>(
        std::count_if(preprocessed.begin(), preprocessed.end(),
                      [](float sample) {
                          return std::abs(sample) >= 0.999F;
                      }));
    const double clippingFraction = preprocessed.empty() ? 0.0
        : static_cast<double>(clippedSamples)
            / static_cast<double>(preprocessed.size());
    const SstvAfcSnapshot afc = pipeline.afcController->snapshot();
    const SstvSlantControllerSnapshot slant =
        pipeline.slantController->snapshot();
    const SstvRxRetainedAudio::Config retainedConfig =
        m_retainedAudio.config();
    QVector<ScopePoint> scope;
    if (pipeline.controls.settings.diagnosticScopeEnabled
        && !frequencies.empty()) {
        const std::size_t maximum = m_config.maximumDiagnosticScopePoints;
        const std::size_t stride = std::max<std::size_t>(
            1U, (frequencies.size() + maximum - 1U) / maximum);
        scope.reserve(static_cast<qsizetype>(std::min(
            maximum, frequencies.size())));
        for (std::size_t index = 0U; index < frequencies.size();
             index += stride) {
            const SstvFrequencyObservation& observation = frequencies[index];
            if (!observation.valid()) {
                continue;
            }
            scope.push_back(ScopePoint {
                observation.centreSample,
                observation.correctedFrequencyHz,
                observation.confidence,
                observation.rms,
                observation.snrDb});
            if (static_cast<std::size_t>(scope.size()) == maximum) {
                break;
            }
        }
    }

    {
        const std::lock_guard<std::mutex> lock(m_snapshotMutex);
        saturatingAdd(m_snapshot.generationChunksProcessed);
        saturatingAdd(m_snapshot.chunksProcessed);
        saturatingAdd(
            m_snapshot.samplesConverted,
            static_cast<std::uint64_t>(normalized.size()));
        saturatingAdd(
            m_snapshot.samplesResampled,
            static_cast<std::uint64_t>(resampled.size()));
        saturatingAdd(
            m_snapshot.samplesPreprocessed,
            static_cast<std::uint64_t>(preprocessed.size()));
        saturatingAdd(
            m_snapshot.frequencyObservations,
            static_cast<std::uint64_t>(frequencies.size()));
        saturatingAdd(
            m_snapshot.toneObservations,
            static_cast<std::uint64_t>(tones.size()));
        m_snapshot.lastChunkSequence = chunk.sequence;
        m_snapshot.lastChunkStartNs = chunk.startTime.count();
        m_snapshot.activeSampleRate = chunk.sampleRate;
        m_snapshot.processedPcmHash =
            hashSamples(m_snapshot.processedPcmHash, preprocessed);
        for (auto it = frequencies.rbegin(); it != frequencies.rend(); ++it) {
            if (it->valid()) {
                m_snapshot.lastFrequencyHz = it->correctedFrequencyHz;
                m_snapshot.lastFrequencyConfidence = it->confidence;
                break;
            }
        }
        m_snapshot.rxState = pipeline.stateMachine->state();
        m_snapshot.rxCause = pipeline.stateMachine->metrics().lastCause;
        m_snapshot.afc.mode = afc.mode;
        m_snapshot.afc.measuredOffsetHz = afc.measuredOffsetHz;
        m_snapshot.afc.correctionHz = afc.correctionHz;
        m_snapshot.afc.confidence = afc.confidence;
        m_snapshot.afc.acceptedReferences = afc.acceptedReferences;
        m_snapshot.afc.rejectedReferences = afc.rejectedReferences;
        m_snapshot.afc.rejectedImageObservations =
            afc.rejectedImageObservations;
        m_snapshot.slant.mode = slant.mode;
        m_snapshot.slant.estimateValid = slant.estimateValid;
        m_snapshot.slant.measuredClockErrorPpm =
            slant.measuredClockErrorPpm;
        m_snapshot.slant.appliedClockErrorPpm =
            slant.appliedClockErrorPpm;
        m_snapshot.slant.confidence = slant.confidence;
        m_snapshot.slant.observedSyncs = slant.observedSyncs;
        m_snapshot.slant.rejectedSyncs = slant.rejectedSyncs;
        m_snapshot.sync.observed = pipeline.syncPulseCount != 0U;
        m_snapshot.sync.locked = pipeline.stateMachine->state()
                == SstvRxState::Receiving
            || slant.observedSyncs >= 4U;
        m_snapshot.sync.confidence = slant.estimateValid
            ? slant.confidence : pipeline.fallbackResult.confidence;
        m_snapshot.sync.pulseCount = pipeline.syncPulseCount;
        m_snapshot.sync.currentLine =
            pipeline.stateMachine->metrics().decodedLines;
        m_snapshot.sync.missedLines =
            pipeline.stateMachine->metrics().consecutiveMissingSync;
        m_snapshot.fallback.status = pipeline.fallbackResult.status;
        m_snapshot.fallback.selectedMode =
            pipeline.fallbackResult.selectedMode.has_value()
            ? QString::fromStdString(
                  *pipeline.fallbackResult.selectedMode).left(
                    static_cast<qsizetype>(m_config.maximumErrorCharacters))
            : QString {};
        m_snapshot.fallback.candidateCount =
            pipeline.fallbackResult.candidates.size();
        m_snapshot.fallback.confidence =
            pipeline.fallbackResult.confidence;
        m_snapshot.fallback.observedLinePeriodSamples =
            pipeline.fallbackResult.observedLinePeriodSamples;
        m_snapshot.fallback.observedSyncDurationSamples =
            pipeline.fallbackResult.observedSyncDurationSamples;
        m_snapshot.fallback.ambiguityCount =
            pipeline.fallbackResult.metrics.ambiguousSelections;
        m_snapshot.signal.clippingFraction = clippingFraction;
        for (auto it = frequencies.rbegin(); it != frequencies.rend(); ++it) {
            if (it->valid()) {
                m_snapshot.signal.rms = it->rms;
                m_snapshot.signal.snrDb = it->snrDb;
                m_snapshot.signal.confidence = it->confidence;
                break;
            }
        }
        m_snapshot.replay.sampleRate = retainedConfig.sampleRate;
        m_snapshot.replay.retentionSeconds =
            retainedConfig.retentionSeconds;
        m_snapshot.replay.retainedSamples =
            m_retainedAudio.retainedSamples();
        m_snapshot.replay.capacitySamples =
            m_retainedAudio.capacitySamples();
        m_snapshot.replay.acquisitionDescriptors =
            m_retainedAudio.acquisitionDescriptorCount();
        m_snapshot.replay.mostRecentAcquisitionId =
            m_retainedAudio.mostRecentAcquisitionId();
        saturatingAdd(m_snapshot.performance.measuredDspBlocks);
        saturatingAdd(m_snapshot.performance.totalDspBlockNanoseconds,
                      dspElapsedNs);
        m_snapshot.performance.averageDspBlockNanoseconds =
            m_snapshot.performance.totalDspBlockNanoseconds
            / m_snapshot.performance.measuredDspBlocks;
        m_snapshot.performance.maximumDspBlockNanoseconds = std::max(
            m_snapshot.performance.maximumDspBlockNanoseconds,
            dspElapsedNs);
        m_snapshot.performance.progressiveUpdates =
            pipeline.progressiveUpdates;
        const std::uint64_t progressiveSpan =
            pipeline.lastProgressiveUpdateNs
                > pipeline.firstProgressiveUpdateNs
            ? pipeline.lastProgressiveUpdateNs
                - pipeline.firstProgressiveUpdateNs
            : 0U;
        m_snapshot.performance.progressiveUpdatesPerSecond =
            progressiveSpan == 0U || pipeline.progressiveUpdates < 2U
            ? 0.0
            : static_cast<double>(pipeline.progressiveUpdates - 1U)
                * static_cast<double>(kNanosecondsPerSecond)
                / static_cast<double>(progressiveSpan);
        m_snapshot.scope = std::move(scope);
    }
    scheduleSnapshotNotification();
    return true;
}

void SstvRxRuntime::resetWorkerPipeline(WorkerPipeline& pipeline,
                                        std::uint64_t generation,
                                        qint64 epochNs)
{
    if (pipeline.stateMachine) {
        const std::uint64_t eventMs =
            pipeline.stateMachine->metrics().lastEventAtMs;
        terminateMartinM1ForDiscontinuity(pipeline, eventMs);
        terminateScottieForDiscontinuity(pipeline, eventMs);
        terminateRobotForDiscontinuity(pipeline, eventMs);
        terminateSequentialRgbForDiscontinuity(pipeline, eventMs);
        terminatePdForDiscontinuity(pipeline, eventMs);
        terminateAvtForDiscontinuity(pipeline, eventMs);
        terminateMmsstvForDiscontinuity(pipeline, eventMs);
    }
    pipeline.generation = generation;
    pipeline.inputSampleRate = 0U;
    pipeline.resampler.reset();
    pipeline.preprocessor = std::make_unique<SstvPreprocessor>();
    SstvFrequencyDemodulatorConfig demodulatorConfig =
        SstvFrequencyDemodulatorConfig::sstvDefaults();
    // A fast Martin M2/M4 pixel is about 2.75 samples at the native 12 kHz
    // DSP rate.  The demodulator's minimum validated three-sample averaging
    // window and one-sample hops retain overlapping observations while
    // keeping work bounded; decoder accumulation tolerates transition hops.
    demodulatorConfig.averagingSamples = 3U;
    demodulatorConfig.hopSamples = 1U;
    pipeline.frequencyDemodulator =
        std::make_unique<SstvFrequencyDemodulator>(demodulatorConfig);
    SstvToneDetectorConfig toneConfig =
        SstvToneDetectorConfig::sstvDefaults(
            static_cast<double>(SstvResampler::kOutputSampleRate));
    // The VIS break is only 10 ms.  The generic 22 ms FSK-oriented window
    // cannot observe it as a distinct run and merges both 300 ms leaders.
    // Five-millisecond windows with 2.5 ms hops resolve that mandatory break
    // and still cover every 30 ms VIS symbol with twelve observations.
    toneConfig.windowSamples = 60U;
    toneConfig.hopSamples = 30U;
    pipeline.toneDetector =
        std::make_unique<SstvToneDetector>(std::move(toneConfig));
    SstvVisDetectorConfig visConfig;
    // With 5 ms FFT windows a phase-unfavourable 10 ms VIS break has only
    // two fully discriminated 2.5 ms hops.  Permit that exact quantisation;
    // the two 300 ms leaders, start symbol, payload/parity and stop checks
    // continue to provide the acquisition gate.
    visConfig.headerDurationTolerance = 0.50;
    pipeline.visDetector =
        std::make_unique<SstvVisDetector>(visConfig);
    pipeline.narrowVisDetector =
        std::make_unique<SstvNarrowVisDetector>();
    pipeline.controls = m_rxControlPolicy.snapshot();
    SstvRxPolicy statePolicy;
    if (pipeline.controls.settings.modeLockEnabled) {
        statePolicy.lockedMode = pipeline.controls.settings.lockedMode;
        // A lock constrains selection but never bypasses timing evidence.
        // VIS-less acquisition is started below only after a unique canonical
        // fallback match; conflicts therefore remain fail-closed.
        statePolicy.useLockedModeWithoutVis = false;
    }
    pipeline.stateMachine =
        std::make_unique<SstvRxStateMachine>(std::move(statePolicy));
    pipeline.afcController = std::make_unique<SstvAfcController>();
    pipeline.afcController->configure(
        pipeline.controls.settings.afcMode,
        pipeline.controls.settings.manualFrequencyCorrectionHz);
    pipeline.frequencyDemodulator->setAfcCorrectionHz(
        pipeline.afcController->snapshot().correctionHz);
    pipeline.slantController = std::make_unique<SstvSlantController>();
    pipeline.fallbackDetector =
        std::make_unique<SstvTimingFallbackDetector>(
            SstvModeRegistry::canonical());
    pipeline.fallbackDetector->setEnabled(
        pipeline.controls.settings.timingFallbackEnabled);
    const std::optional<std::string> fallbackLock =
        pipeline.controls.settings.modeControl == SstvRxModeControl::Manual
        ? std::optional<std::string> {
              pipeline.controls.settings.manualMode}
        : (pipeline.controls.settings.modeLockEnabled
              ? std::optional<std::string> {
                    pipeline.controls.settings.lockedMode}
              : std::nullopt);
    static_cast<void>(pipeline.fallbackDetector->setLockedMode(fallbackLock));
    pipeline.fskIdDetector = std::make_unique<SstvFskIdDetector>();
    static_cast<void>(m_retainedAudio.setRetentionSeconds(
        pipeline.controls.settings.replayRetentionSeconds));
    pipeline.martinM1Session.reset();
    pipeline.scottieSession.reset();
    pipeline.robotSession.reset();
    pipeline.sequentialRgbSession.reset();
    pipeline.pdSession.reset();
    pipeline.avtCountdownDetector.reset();
    pipeline.avtSession.reset();
    pipeline.mmsstvSession.reset();
    pipeline.pendingVisRun.reset();
    pipeline.pendingNarrowVisRun.reset();
    pipeline.pendingSyncPulse.reset();
    pipeline.pendingSyncFrequencyWeight = 0.0;
    pipeline.pendingSyncConfidenceSum = 0.0;
    pipeline.pendingSyncObservationCount = 0U;
    pipeline.fallbackResult = {};
    pipeline.latestFskId = {};
    pipeline.source = {};
    pipeline.hasInputTimeline = false;
    pipeline.expectedInputEndNs = 0;
    pipeline.hasSequence = false;
    pipeline.lastSequence = 0U;
    pipeline.toneEpochUs = epochNs < 0
        ? 0U
        : static_cast<std::uint64_t>(epochNs) / 1'000U;
    pipeline.martinM1LinesReported = 0U;
    pipeline.scottieLinesReported = 0U;
    pipeline.robotLinesReported = 0U;
    pipeline.sequentialRgbLinesReported = 0U;
    pipeline.pdLinesReported = 0U;
    pipeline.avtLinesReported = 0U;
    pipeline.mmsstvLinesReported = 0U;
    pipeline.avtFrequencyOffsetHz = 0.0;
    pipeline.avtRetainedStartSample = 0U;
    pipeline.lastFrequencySample = 0U;
    pipeline.hasFrequencySample = false;
    pipeline.hasHeaderEvidenceRange = false;
    pipeline.headerEvidenceStartSample = 0U;
    pipeline.headerEvidenceEndSample = 0U;
    pipeline.syncPulseCount = 0U;
    pipeline.syncLineIndex = 0U;
    pipeline.nominalLinePeriodSamples = 0U;
    pipeline.retainedAcquisitionId = 0U;
    pipeline.currentChunkEndNs = 0U;
    pipeline.suppressNativeAcquisitionUntilSample = 0U;
    pipeline.progressiveUpdates = 0U;
    pipeline.firstProgressiveUpdateNs = 0U;
    pipeline.lastProgressiveUpdateNs = 0U;
    pipeline.consumedAfcResetSerial = pipeline.controls.afcResetSerial;
    pipeline.consumedSlantResetSerial = pipeline.controls.slantResetSerial;
    pipeline.afcAcquisitionLogged = false;
    pipeline.slantAcquisitionLogged = false;
    pipeline.fallbackUniqueLogged = false;
    pipeline.fallbackAmbiguousLogged = false;
    pipeline.fallbackNoMatchLogged = false;

    const std::uint64_t nowMs = epochNs < 0
        ? 0U
        : static_cast<std::uint64_t>(epochNs) / 1'000'000U;
    pipeline.stateMachine->dispatch(nowMs, SstvRxEnable {});
    pipeline.stateMachine->dispatch(nowMs, SstvRxStartMonitoring {});

    {
        const std::lock_guard<std::mutex> lock(m_snapshotMutex);
        saturatingAdd(m_snapshot.pipelineResets);
        m_snapshot.generationChunksProcessed = 0U;
        m_snapshot.activeSampleRate = 0U;
        m_snapshot.lastChunkSequence = 0U;
        m_snapshot.lastChunkStartNs = 0;
        m_snapshot.lastFrequencyHz = 0.0;
        m_snapshot.lastFrequencyConfidence = 0.0;
        m_snapshot.processedPcmHash = kFnvOffset;
        m_snapshot.rxState = pipeline.stateMachine->state();
        m_snapshot.rxCause = pipeline.stateMachine->metrics().lastCause;
        m_snapshot.afc = {};
        m_snapshot.afc.mode = pipeline.controls.settings.afcMode;
        m_snapshot.slant = {};
        m_snapshot.slant.mode = pipeline.controls.settings.slantMode;
        m_snapshot.sync = {};
        m_snapshot.fallback = {};
        m_snapshot.fskId = {};
        m_snapshot.signal = {};
        m_snapshot.scope.clear();
        m_snapshot.vis = {};
        m_snapshot.image = {};
        m_latestImageSnapshot.reset();
        m_snapshot.lastError.clear();
    }
    scheduleSnapshotNotification(true);
}

void SstvRxRuntime::applyControlSnapshot(WorkerPipeline& pipeline)
{
    const SstvRxControlSnapshot next = m_rxControlPolicy.snapshot();
    const bool revisionChanged = next.revision != pipeline.controls.revision;
    pipeline.controls = next;

    if (pipeline.afcController && pipeline.frequencyDemodulator) {
        if (pipeline.consumedAfcResetSerial != next.afcResetSerial) {
            pipeline.afcController->reset();
            pipeline.consumedAfcResetSerial = next.afcResetSerial;
            pipeline.afcAcquisitionLogged = false;
        }
        pipeline.afcController->configure(
            next.settings.afcMode,
            next.settings.manualFrequencyCorrectionHz);
        pipeline.frequencyDemodulator->setAfcCorrectionHz(
            pipeline.afcController->snapshot().correctionHz);
    }
    if (pipeline.slantController) {
        if (pipeline.consumedSlantResetSerial != next.slantResetSerial) {
            pipeline.slantController->reset();
            pipeline.consumedSlantResetSerial = next.slantResetSerial;
            pipeline.slantAcquisitionLogged = false;
        }
        if (pipeline.nominalLinePeriodSamples != 0U) {
            pipeline.slantController->configure(
                pipeline.nominalLinePeriodSamples,
                next.settings.slantMode,
                next.settings.manualClockErrorPpm);
        }
    }
    if (pipeline.fallbackDetector) {
        pipeline.fallbackDetector->setEnabled(
            next.settings.timingFallbackEnabled);
        const std::optional<std::string> lock =
            next.settings.modeControl == SstvRxModeControl::Manual
            ? std::optional<std::string> {next.settings.manualMode}
            : (next.settings.modeLockEnabled
                  ? std::optional<std::string> {next.settings.lockedMode}
                  : std::nullopt);
        if (!pipeline.fallbackDetector->setLockedMode(lock)) {
            pipeline.fallbackDetector->setEnabled(false);
        }
    }
    if (revisionChanged) {
        if (pipeline.stateMachine
            && !pipeline.stateMachine->hasActiveSession()) {
            static_cast<void>(pipeline.stateMachine->setModeLock(
                next.settings.modeLockEnabled
                    ? std::optional<std::string> {next.settings.lockedMode}
                    : std::nullopt));
            static_cast<void>(pipeline.stateMachine->setNoVisFallbackMode(
                std::nullopt));

        }
        static_cast<void>(m_retainedAudio.setRetentionSeconds(
            next.settings.replayRetentionSeconds));
    }

    // A manual mode is an explicit operator choice and must not be routed
    // through VIS/fallback discovery.  This applies both when the control is
    // changed during an active stream and when it was already selected before
    // the worker pipeline was created (in which case revisionChanged is false
    // on the first chunk).
    if (pipeline.stateMachine
        && !pipeline.stateMachine->hasActiveSession()
        && next.settings.modeControl == SstvRxModeControl::Manual
        && pipeline.stateMachine->state() == SstvRxState::SearchingLeader
        && SstvRxControlPolicy::modeNameIsValid(next.settings.manualMode)) {
        const std::uint64_t eventMs = std::max(
            pipeline.currentChunkEndNs / 1'000'000U,
            pipeline.stateMachine->metrics().lastEventAtMs);
        pipeline.stateMachine->dispatch(
            eventMs, SstvRxManualMode {next.settings.manualMode});
        if (pipeline.stateMachine->state() == SstvRxState::ModeDetected) {
            pipeline.stateMachine->dispatch(eventMs, SstvRxModeReady {});
        }
        // The mode is armed here; the native session is instantiated by the
        // first validated sync pulse below so its image offset is aligned to
        // the live waveform rather than sample zero.
    }
}

void SstvRxRuntime::updateAfcFromTones(
    WorkerPipeline& pipeline,
    const std::vector<SstvToneObservation>& tones,
    std::vector<SstvFrequencyObservation>& frequencies,
    std::optional<std::uint64_t> resumeToneAtSample)
{
    if (!pipeline.afcController || !pipeline.frequencyDemodulator) {
        return;
    }
    for (const SstvToneObservation& tone : tones) {
        if (!tone.valid()) {
            continue;
        }
        const bool inValidatedHeader = pipeline.hasHeaderEvidenceRange
            && tone.startSample >= pipeline.headerEvidenceStartSample
            && tone.centreSample <= pipeline.headerEvidenceEndSample;
        const bool beforeResumeBoundary = resumeToneAtSample.has_value()
            && tone.startSample < *resumeToneAtSample;
        const bool withinImage = beforeResumeBoundary
            || (pipeline.hasNativeSession() && !inValidatedHeader);
        SstvAfcEvidenceRole role = SstvAfcEvidenceRole::Untrusted;
        bool trusted = false;
        if (withinImage) {
            if (std::abs(tone.nominalFrequencyHz - 1'200.0) < 0.5) {
                role = SstvAfcEvidenceRole::TrustedLineSync;
                trusted = tone.confidence >= 0.55;
            } else {
                // This explicit rejection is the integration invariant that
                // prevents luminance/chroma content from steering AFC.
                role = SstvAfcEvidenceRole::ImageData;
            }
        } else if (inValidatedHeader) {
            if (std::abs(tone.nominalFrequencyHz - 1'900.0) < 0.5) {
                role = SstvAfcEvidenceRole::Leader;
                trusted = true;
            } else if (std::abs(tone.nominalFrequencyHz - 1'200.0) < 0.5) {
                role = SstvAfcEvidenceRole::HeaderBreak;
                trusted = true;
            } else if (std::abs(tone.nominalFrequencyHz - 1'100.0) < 0.5
                       || std::abs(tone.nominalFrequencyHz - 1'300.0)
                           < 0.5) {
                role = SstvAfcEvidenceRole::VisControl;
                trusted = true;
            }
        }
        const SstvAfcUpdate update = pipeline.afcController->consume(
            SstvAfcEvidence {
                role,
                tone.sequence,
                tone.centreSample,
                tone.detectedFrequencyHz,
                tone.nominalFrequencyHz,
                tone.confidence,
                trusted});
        if (update.correctionChanged) {
            pipeline.frequencyDemodulator->setAfcCorrectionHz(
                update.snapshot.correctionHz);
            if (!pipeline.afcAcquisitionLogged) {
                recordAfcAcquisitionDiagnostic(
                    update.snapshot.correctionHz);
                pipeline.afcAcquisitionLogged = true;
            }
        }
    }

    const double correction =
        pipeline.frequencyDemodulator->afcCorrectionHz();
    for (SstvFrequencyObservation& observation : frequencies) {
        observation.afcCorrectionHz = correction;
        if (std::isfinite(observation.rawFrequencyHz)) {
            observation.correctedFrequencyHz =
                observation.rawFrequencyHz - correction;
        }
    }
}

void SstvRxRuntime::updateSyncFallbackAndFsk(
    WorkerPipeline& pipeline,
    const std::vector<SstvToneObservation>& tones,
    std::uint64_t eventMs)
{
    if (!pipeline.fallbackDetector || !pipeline.slantController
        || !pipeline.fskIdDetector || !pipeline.toneDetector) {
        return;
    }

    const auto tryBeginFallback = [&] {
        if (pipeline.hasNativeSession()
            || pipeline.fallbackResult.status
                != SstvFallbackStatus::Unique
            || !pipeline.fallbackResult.selectedMode.has_value()) {
            return;
        }
        const SstvRxControlSettings& settings = pipeline.controls.settings;
        const bool manual = settings.modeControl
            == SstvRxModeControl::Manual;
        if (!manual && !settings.receiveWithoutVis) {
            return;
        }
        const std::string& mode = *pipeline.fallbackResult.selectedMode;
        if (pipeline.fallbackResult.retainedPulses.empty()) {
            return;
        }
        if (pipeline.fallbackResult.retainedPulses.front().startSample
            < pipeline.suppressNativeAcquisitionUntilSample) {
            return;
        }
        if (pipeline.stateMachine->state() != SstvRxState::SearchingLeader
            && !isTerminalRxState(pipeline.stateMachine->state())) {
            return;
        }
        if (isTerminalRxState(pipeline.stateMachine->state())) {
            pipeline.stateMachine->dispatch(eventMs, SstvRxTick {});
        }
        if (pipeline.stateMachine->state() != SstvRxState::SearchingLeader) {
            return;
        }

        if (manual) {
            pipeline.stateMachine->dispatch(eventMs, SstvRxManualMode {mode});
        } else {
            static_cast<void>(pipeline.stateMachine->setNoVisFallbackMode(
                mode));
            pipeline.stateMachine->dispatch(
                eventMs,
                SstvRxLeaderObserved {
                    pipeline.fallbackResult.confidence,
                    pipeline.afcController->snapshot().measuredOffsetHz});
            pipeline.stateMachine->dispatch(eventMs,
                                            SstvRxLeaderConfirmed {});
            pipeline.stateMachine->dispatch(eventMs,
                                            SstvRxVisUnavailable {});
        }
        if (pipeline.stateMachine->state() == SstvRxState::ModeDetected) {
            pipeline.stateMachine->dispatch(eventMs, SstvRxModeReady {});
        }
        if (pipeline.stateMachine->state() != SstvRxState::WaitingForSync) {
            return;
        }

        std::uint64_t imageStart =
            pipeline.fallbackResult.retainedPulses.front().startSample;
        const auto scottie = modeForStableId<SstvScottieMode, 5U>(
            mode, [](SstvScottieMode value) {
                return SstvScottieProtocol::spec(value);
            });
        if (scottie.has_value()) {
            const std::uint64_t embeddedOffset = samplesForPicoseconds(
                SstvScottieProtocol::spec(*scottie).embeddedSyncOffset);
            imageStart = imageStart > embeddedOffset
                ? imageStart - embeddedOffset : 0U;
        }
        const double clockError = settings.slantMode
                == SstvRxSlantMode::Manual
            ? settings.manualClockErrorPpm
            : (pipeline.fallbackResult.candidates.empty()
                  ? 0.0
                  : pipeline.fallbackResult.candidates.front().lineErrorPpm);
        if (!beginNativeSessionByModeId(pipeline,
                                        mode,
                                        imageStart,
                                        eventMs,
                                        clockError)) {
            pipeline.stateMachine->dispatch(
                eventMs,
                SstvRxFailure {
                    SstvRxErrorCode::DspFailure,
                    "canonical SSTV timing fallback could not start mode"});
        }
    };

    const auto consumePulse = [&](SstvFallbackSyncPulse pulse) {
        ++pipeline.syncPulseCount;
        {
            const std::lock_guard<std::mutex> lock(m_snapshotMutex);
            m_snapshot.sync.lastPulseStartSample = pulse.startSample;
        }
        if (pipeline.hasNativeSession()
            && pipeline.nominalLinePeriodSamples != 0U) {
            const SstvSlantControllerSnapshot slant =
                pipeline.slantController->observe(SstvSlantObservation {
                pipeline.syncLineIndex,
                pulse.startSample,
                pulse.confidence,
                pulse.predicted});
            ++pipeline.syncLineIndex;
            if (!pipeline.slantAcquisitionLogged
                && slant.mode == SstvRxSlantMode::Automatic
                && slant.estimateValid) {
                recordSlantAcquisitionDiagnostic(slant);
                pipeline.slantAcquisitionLogged = true;
            }
        } else {
            pipeline.fallbackResult =
                pipeline.fallbackDetector->consume(pulse);
            if (pipeline.controls.settings.modeControl
                    == SstvRxModeControl::Manual
                && pipeline.stateMachine->state()
                    == SstvRxState::WaitingForSync
                && SstvRxControlPolicy::modeNameIsValid(
                       pipeline.controls.settings.manualMode)) {
                if (pulse.startSample
                    < pipeline.suppressNativeAcquisitionUntilSample) {
                    return;
                }
                static_cast<void>(beginNativeSessionByModeId(
                    pipeline,
                    pipeline.controls.settings.manualMode,
                    pulse.startSample,
                    eventMs,
                    0.0));
                return;
            }
            if (pipeline.fallbackResult.status
                    == SstvFallbackStatus::Unique
                && !pipeline.fallbackUniqueLogged) {
                recordFallbackDiagnostic(
                    pipeline.fallbackResult,
                    QStringLiteral("sync.fallback-selected"),
                    QStringLiteral("unique"), true);
                pipeline.fallbackUniqueLogged = true;
            } else if (pipeline.fallbackResult.status
                           == SstvFallbackStatus::Ambiguous
                       && !pipeline.fallbackAmbiguousLogged) {
                recordFallbackDiagnostic(
                    pipeline.fallbackResult,
                    QStringLiteral("sync.fallback-ambiguous"),
                    QStringLiteral("ambiguous"), false);
                pipeline.fallbackAmbiguousLogged = true;
            } else if (pipeline.fallbackResult.status
                           == SstvFallbackStatus::NoMatch
                       && !pipeline.fallbackNoMatchLogged) {
                recordFallbackDiagnostic(
                    pipeline.fallbackResult,
                    QStringLiteral("sync.fallback-no-match"),
                    QStringLiteral("no-match"), false);
                pipeline.fallbackNoMatchLogged = true;
            }
            tryBeginFallback();
        }
    };

    const std::uint64_t windowSamples = static_cast<std::uint64_t>(
        pipeline.toneDetector->config().windowSamples);
    const std::uint64_t hopSamples = static_cast<std::uint64_t>(
        pipeline.toneDetector->config().hopSamples);
    for (const SstvToneObservation& tone : tones) {
        const bool syncTone = tone.valid()
            && std::abs(tone.nominalFrequencyHz - 1'200.0) < 0.5;
        if (!syncTone) {
            if (pipeline.pendingSyncPulse.has_value()) {
                consumePulse(*pipeline.pendingSyncPulse);
                pipeline.pendingSyncPulse.reset();
                pipeline.pendingSyncFrequencyWeight = 0.0;
                pipeline.pendingSyncConfidenceSum = 0.0;
                pipeline.pendingSyncObservationCount = 0U;
            }
            continue;
        }
        const std::uint64_t endSample = saturatingUnsignedAdd(
            tone.startSample, windowSamples);
        if (pipeline.pendingSyncPulse.has_value()
            && tone.startSample
                <= saturatingUnsignedAdd(
                    pipeline.pendingSyncPulse->endSample, hopSamples)) {
            pipeline.pendingSyncPulse->endSample = std::max(
                pipeline.pendingSyncPulse->endSample, endSample);
            pipeline.pendingSyncFrequencyWeight +=
                tone.detectedFrequencyHz * tone.confidence;
            pipeline.pendingSyncConfidenceSum += tone.confidence;
            ++pipeline.pendingSyncObservationCount;
            pipeline.pendingSyncPulse->measuredFrequencyHz =
                pipeline.pendingSyncConfidenceSum > 0.0
                ? pipeline.pendingSyncFrequencyWeight
                    / pipeline.pendingSyncConfidenceSum
                : tone.detectedFrequencyHz;
            pipeline.pendingSyncPulse->confidence =
                pipeline.pendingSyncConfidenceSum
                / static_cast<double>(
                    pipeline.pendingSyncObservationCount);
            continue;
        }
        if (pipeline.pendingSyncPulse.has_value()) {
            consumePulse(*pipeline.pendingSyncPulse);
        }
        pipeline.pendingSyncPulse = SstvFallbackSyncPulse {
            tone.startSample,
            endSample,
            tone.detectedFrequencyHz,
            tone.confidence,
            false};
        pipeline.pendingSyncFrequencyWeight =
            tone.detectedFrequencyHz * tone.confidence;
        pipeline.pendingSyncConfidenceSum = tone.confidence;
        pipeline.pendingSyncObservationCount = 1U;
    }

    const std::vector<SstvFskIdCandidate> candidates =
        pipeline.fskIdDetector->consume(tones);
    for (const SstvFskIdCandidate& candidate : candidates) {
        pipeline.latestFskId = candidate;
        {
            const std::lock_guard<std::mutex> lock(m_snapshotMutex);
            m_snapshot.fskId.available = true;
            m_snapshot.fskId.valid = candidate.valid();
            m_snapshot.fskId.identifier = QString::fromStdString(
                candidate.decoded.text).left(
                    static_cast<qsizetype>(m_config.maximumErrorCharacters));
            m_snapshot.fskId.confidence = candidate.confidence;
            m_snapshot.fskId.rawSymbolCount =
                candidate.decoded.rawSymbols.size();
            m_snapshot.fskId.completedAtSample = candidate.completedAtSample;
        }
        if (candidate.valid()) {
            const std::uint64_t acquisitionId =
                pipeline.retainedAcquisitionId != 0U
                ? pipeline.retainedAcquisitionId
                : pipeline.lastCompletedAcquisitionId;
            if (acquisitionId != 0U) {
                static_cast<void>(m_retainedAudio.associateFskId(
                    acquisitionId,
                    streamTime(pipeline.toneEpochUs,
                               candidate.completedAtSample),
                    candidate.decoded.text));
            }
        }
    }
}

void SstvRxRuntime::recordVisResults(
    WorkerPipeline& pipeline,
    const std::vector<SstvVisDetection>& results,
    std::uint64_t generation)
{
    for (const SstvVisDetection& detection : results) {
        bool preserveValidatedSummary = false;
        {
            const std::lock_guard<std::mutex> lock(m_snapshotMutex);
            preserveValidatedSummary = m_snapshot.vis.valid
                && m_snapshot.vis.modeMapped
                && !detection.valid();
        }
        const auto mapped = detection.valid()
            ? mapVisToSupportedMode(detection.codecResult)
            : std::nullopt;
        const bool mappedNormalAvt = mapped.has_value()
            && detection.codecResult.format == SstvVisFormat::Standard
            && detection.codecResult.primary.payloadKnown
            && SstvAvtProtocol::normalModeForVis(
                   detection.codecResult.primary.payload).has_value();
        // Until the protected countdown has established (or completed) the
        // AVT absolute-time session, its 1600/1900/2200 Hz data can resemble
        // fragments of a new standard leader. Preserve the authoritative
        // mapped AVT summary rather than replacing it with those incidental
        // invalid/unmapped detector results. This guard is strictly scoped to
        // an armed/active AVT acquisition and vanishes on completion, reset,
        // exhaustion or discontinuity, so a later real VIS remains visible.
        if ((pipeline.avtCountdownDetector || pipeline.avtSession)
            && !mappedNormalAvt) {
            continue;
        }
        VisSummary summary;
        summary.available = true;
        summary.valid = detection.valid();
        summary.status = detection.status;
        summary.cause = detection.cause;
        summary.format = detection.codecResult.format;
        summary.confidence = detection.confidence;
        summary.frameStartedAtUs = detection.frameStartedAtUs;
        summary.frameEndedAtUs = detection.frameEndedAtUs;
        summary.rawBits = visRawBits(detection.codecResult);
        if (detection.codecResult.complete
            && detection.codecResult.startValid) {
            const auto start = sampleIndexAtUs(
                pipeline.toneEpochUs, detection.frameStartedAtUs);
            const auto end = sampleIndexAtUs(
                pipeline.toneEpochUs, detection.frameEndedAtUs);
            if (start.has_value() && end.has_value() && *end > *start) {
                pipeline.hasHeaderEvidenceRange = true;
                pipeline.headerEvidenceStartSample = *start;
                pipeline.headerEvidenceEndSample = *end;
            }
        }
        if (detection.codecResult.primary.payloadKnown) {
            summary.primaryPayload =
                static_cast<int>(detection.codecResult.primary.payload);
        }
        if (detection.codecResult.extension.has_value()
            && detection.codecResult.extension->payloadKnown) {
            summary.extensionPayload = static_cast<int>(
                detection.codecResult.extension->payload);
        }

        if (mapped.has_value()) {
            summary.modeMapped = true;
            summary.mappedMode = QString::fromStdString(*mapped).left(
                static_cast<qsizetype>(m_config.maximumErrorCharacters));
        }
        const std::uint64_t eventMs = std::max(
            detection.frameEndedAtUs / 1'000U,
            pipeline.stateMachine->metrics().lastEventAtMs);
        if (pipeline.stateMachine->state() == SstvRxState::LeaderCandidate) {
            pipeline.stateMachine->dispatch(eventMs,
                                            SstvRxLeaderConfirmed {});
        }
        if (pipeline.stateMachine->state() == SstvRxState::ReadingVis) {
            if (detection.valid() && mapped.has_value()) {
                pipeline.stateMachine->dispatch(
                    eventMs,
                    SstvRxVisDecoded {*mapped, detection.confidence});
                if (pipeline.stateMachine->state()
                    == SstvRxState::ModeDetected) {
                    pipeline.stateMachine->dispatch(eventMs,
                                                    SstvRxModeReady {});
                    bool nativeStartFailed = false;
                    if (pipeline.stateMachine->state()
                        == SstvRxState::WaitingForSync) {
                        if (const auto mode = mmsstvModeForVis(
                                detection.codecResult);
                            mode.has_value()) {
                            nativeStartFailed = !beginMmsstvSession(
                                pipeline,
                                eventMs,
                                *mode,
                                detection.frameEndedAtUs,
                                detection.estimatedFrequencyOffsetHz);
                        } else if (
                            detection.codecResult.primary.payloadKnown
                            && SstvAvtProtocol::normalModeForVis(
                                   detection.codecResult.primary.payload)
                                   .has_value()) {
                            nativeStartFailed = !armAvtCountdown(
                                pipeline,
                                detection,
                                detection.codecResult.primary.payload);
                        } else if (
                            detection.codecResult.primary.payloadKnown
                            && martinModeForVis(
                                   detection.codecResult.primary.payload)
                                   .has_value()) {
                            nativeStartFailed = !beginMartinM1Session(
                                pipeline,
                                detection,
                                eventMs,
                                detection.codecResult.primary.payload);
                        } else if (
                            detection.codecResult.primary.payloadKnown
                            && scottieModeForVis(
                                   detection.codecResult.primary.payload)
                                   .has_value()) {
                            nativeStartFailed = !beginScottieSession(
                                pipeline,
                                detection,
                                eventMs,
                                detection.codecResult.primary.payload);
                        } else if (
                            detection.codecResult.primary.payloadKnown
                            && robotModeForVis(
                                   detection.codecResult.primary.payload)
                                   .has_value()) {
                            nativeStartFailed = !beginRobotSession(
                                pipeline,
                                detection,
                                eventMs,
                                detection.codecResult.primary.payload);
                        } else if (
                            detection.codecResult.primary.payloadKnown
                            && sequentialRgbModeForVis(
                                   detection.codecResult.primary.payload)
                                   .has_value()) {
                            nativeStartFailed = !beginSequentialRgbSession(
                                pipeline,
                                detection,
                                eventMs,
                                detection.codecResult.primary.payload);
                        } else if (
                            detection.codecResult.primary.payloadKnown
                            && pdModeForVis(
                                   detection.codecResult.primary.payload)
                                   .has_value()) {
                            nativeStartFailed = !beginPdSession(
                                pipeline,
                                detection,
                                eventMs,
                                detection.codecResult.primary.payload);
                        }
                    }
                    if (nativeStartFailed) {
                        pipeline.stateMachine->dispatch(
                            eventMs,
                            SstvRxFailure {
                                SstvRxErrorCode::DspFailure,
                                "native SSTV image start is outside the "
                                "active DSP timeline"});
                    }
                }
            } else if (detection.valid()) {
                // A valid raw VIS without an authoritative supported registry
                // mapping remains visible in VisSummary.  It is deliberately
                // not mislabelled as a mode.
                pipeline.stateMachine->dispatch(eventMs,
                                                SstvRxVisUnavailable {});
            } else {
                pipeline.stateMachine->dispatch(eventMs,
                                                SstvRxVisRejected {});
            }
        }

        {
            const std::lock_guard<std::mutex> lock(m_snapshotMutex);
            if (!preserveValidatedSummary) {
                m_snapshot.vis = summary;
            }
            m_snapshot.rxState = pipeline.stateMachine->state();
            m_snapshot.rxCause = pipeline.stateMachine->metrics().lastCause;
        }

        if (preserveValidatedSummary) {
            continue;
        }

        recordVisDiagnostic(summary);

        bool shouldPost = false;
        {
            const std::lock_guard<std::mutex> lock(m_notificationMutex);
            m_pendingVisGeneration = generation;
            m_pendingVis = summary;
            if (!m_visNotificationPending) {
                m_visNotificationPending = true;
                shouldPost = true;
            }
        }
        if (shouldPost
            && !QMetaObject::invokeMethod(
                this,
                "deliverVisNotification",
                Qt::QueuedConnection)) {
            const std::lock_guard<std::mutex> lock(m_notificationMutex);
            m_visNotificationPending = false;
        }
    }
}

void SstvRxRuntime::recordNarrowVisResults(
    WorkerPipeline& pipeline,
    const std::vector<SstvNarrowVisDetection>& results,
    std::uint64_t generation)
{
    constexpr std::uint64_t kNarrowHeaderDurationUs =
        static_cast<std::uint64_t>(
            SstvNarrowVisCodec::FrameDuration.count / 1'000'000LL);
    for (const SstvNarrowVisDetection& detection : results) {
        VisSummary summary;
        summary.available = true;
        summary.valid = detection.valid();
        summary.format = SstvVisFormat::Narrow;
        summary.confidence = detection.confidence;
        summary.frameStartedAtUs = detection.frameStartedAtUs;
        summary.frameEndedAtUs = detection.frameEndedAtUs;
        if (detection.valid()) {
            summary.rawBits.reserve(
                static_cast<qsizetype>(
                    detection.codecResult.bitsLsbFirst.size()));
            for (const bool bit : detection.codecResult.bitsLsbFirst) {
                summary.rawBits.append(bit ? QLatin1Char('1')
                                           : QLatin1Char('0'));
            }
            const auto start = sampleIndexAtUs(
                pipeline.toneEpochUs, detection.frameStartedAtUs);
            const auto end = sampleIndexAtUs(
                pipeline.toneEpochUs, detection.frameEndedAtUs);
            if (start.has_value() && end.has_value() && *end > *start) {
                pipeline.hasHeaderEvidenceRange = true;
                pipeline.headerEvidenceStartSample = *start;
                pipeline.headerEvidenceEndSample = *end;
            }
        }
        if (detection.codecResult.mode.has_value()) {
            summary.primaryPayload = static_cast<int>(
                detection.codecResult.payload);
        }
        switch (detection.status) {
        case SstvNarrowVisDetectionStatus::Decoded:
            summary.status = SstvVisDetectionStatus::Decoded;
            summary.cause = SstvVisDetectionCause::None;
            break;
        case SstvNarrowVisDetectionStatus::Rejected:
            summary.status = SstvVisDetectionStatus::Rejected;
            summary.cause = SstvVisDetectionCause::CodecRejected;
            break;
        case SstvNarrowVisDetectionStatus::Truncated:
            summary.status = SstvVisDetectionStatus::Truncated;
            summary.cause = SstvVisDetectionCause::EndOfInput;
            break;
        case SstvNarrowVisDetectionStatus::Cancelled:
            summary.status = SstvVisDetectionStatus::Cancelled;
            summary.cause = SstvVisDetectionCause::Cancelled;
            break;
        case SstvNarrowVisDetectionStatus::InvalidInput:
            summary.status = SstvVisDetectionStatus::InvalidInput;
            summary.cause = SstvVisDetectionCause::InvalidObservation;
            break;
        }

        const auto mapped = detection.valid()
            ? mapNarrowVisToSupportedMode(detection.codecResult)
            : std::nullopt;
        if (mapped.has_value()) {
            summary.modeMapped = true;
            summary.mappedMode = QString::fromStdString(*mapped).left(
                static_cast<qsizetype>(m_config.maximumErrorCharacters));
        }

        const std::uint64_t eventMs = std::max(
            detection.frameEndedAtUs / 1'000U,
            pipeline.stateMachine->metrics().lastEventAtMs);
        if (detection.valid()
            && (pipeline.stateMachine->state()
                    == SstvRxState::SearchingLeader
                || isTerminalRxState(pipeline.stateMachine->state()))) {
            pipeline.stateMachine->dispatch(
                eventMs,
                SstvRxLeaderObserved {
                    detection.confidence,
                    detection.estimatedFrequencyOffsetHz});
        }
        if (detection.valid()
            && pipeline.stateMachine->state()
                == SstvRxState::LeaderCandidate) {
            pipeline.stateMachine->dispatch(eventMs,
                                            SstvRxLeaderConfirmed {});
        }
        if (detection.valid()
            && pipeline.stateMachine->state() == SstvRxState::ReadingVis) {
            if (mapped.has_value()
                && detection.codecResult.mode.has_value()) {
                pipeline.stateMachine->dispatch(
                    eventMs,
                    SstvRxVisDecoded {*mapped, detection.confidence});
                if (pipeline.stateMachine->state()
                    == SstvRxState::ModeDetected) {
                    pipeline.stateMachine->dispatch(eventMs,
                                                    SstvRxModeReady {});
                    bool nativeStartFailed = false;
                    if (pipeline.stateMachine->state()
                        == SstvRxState::WaitingForSync) {
                        const auto mode =
                            SstvMmsstvProtocol::modeForNarrowPayload(
                                detection.codecResult.payload);
                        const std::uint64_t imageStartUs =
                            saturatingUnsignedAdd(
                                detection.frameStartedAtUs,
                                kNarrowHeaderDurationUs);
                        nativeStartFailed = !mode.has_value()
                            || !beginMmsstvSession(
                                pipeline,
                                eventMs,
                                *mode,
                                imageStartUs,
                                detection.estimatedFrequencyOffsetHz);
                    }
                    if (nativeStartFailed) {
                        pipeline.stateMachine->dispatch(
                            eventMs,
                            SstvRxFailure {
                                SstvRxErrorCode::DspFailure,
                                "native narrow SSTV image start is outside "
                                "the active DSP timeline"});
                    }
                }
            } else {
                pipeline.stateMachine->dispatch(eventMs,
                                                SstvRxVisUnavailable {});
            }
        }

        {
            const std::lock_guard<std::mutex> lock(m_snapshotMutex);
            m_snapshot.vis = summary;
            m_snapshot.rxState = pipeline.stateMachine->state();
            m_snapshot.rxCause = pipeline.stateMachine->metrics().lastCause;
        }

        recordVisDiagnostic(summary);

        bool shouldPost = false;
        {
            const std::lock_guard<std::mutex> lock(m_notificationMutex);
            m_pendingVisGeneration = generation;
            m_pendingVis = summary;
            if (!m_visNotificationPending) {
                m_visNotificationPending = true;
                shouldPost = true;
            }
        }
        if (shouldPost
            && !QMetaObject::invokeMethod(
                this,
                "deliverVisNotification",
                Qt::QueuedConnection)) {
            const std::lock_guard<std::mutex> lock(m_notificationMutex);
            m_visNotificationPending = false;
        }
    }
}

bool SstvRxRuntime::beginNativeSessionByModeId(
    WorkerPipeline& pipeline,
    const std::string& modeId,
    std::uint64_t imageStartSample,
    std::uint64_t eventMs,
    double clockErrorPpm)
{
    if (pipeline.hasNativeSession() || !pipeline.frequencyDemodulator
        || pipeline.stateMachine->state() != SstvRxState::WaitingForSync) {
        return false;
    }
    const std::int32_t clock = roundedClockErrorPpm(clockErrorPpm);
    bool started = false;

    if (const auto mode = modeForStableId<SstvMartinMode, 4U>(
            modeId, [](SstvMartinMode value) {
                return SstvMartinM1Protocol::spec(value);
            }); mode.has_value()) {
        const SstvMartinModeSpec spec = SstvMartinM1Protocol::spec(*mode);
        SstvMartinM1RxSessionConfig config;
        config.mode = *mode;
        config.sampleRate = SstvResampler::kOutputSampleRate;
        config.imageStartSample = imageStartSample;
        config.observationSpanSamples = static_cast<std::uint32_t>(
            pipeline.frequencyDemodulator->config().hopSamples);
        config.clockErrorPpm = clock;
        config.frequencyOffsetHz = 0.0;
        pipeline.martinM1Session =
            std::make_unique<SstvMartinM1RxSession>(config);
        pipeline.martinM1LinesReported = 0U;
        pipeline.nominalLinePeriodSamples =
            samplesForPicoseconds(spec.lineDuration);
        started = true;
    } else if (const auto mode = modeForStableId<SstvScottieMode, 5U>(
                   modeId, [](SstvScottieMode value) {
                       return SstvScottieProtocol::spec(value);
                   }); mode.has_value()) {
        const SstvScottieModeSpec spec = SstvScottieProtocol::spec(*mode);
        SstvScottieRxSessionConfig config;
        config.mode = *mode;
        config.sampleRate = SstvResampler::kOutputSampleRate;
        config.imageStartSample = imageStartSample;
        config.observationSpanSamples = static_cast<std::uint32_t>(
            pipeline.frequencyDemodulator->config().hopSamples);
        config.clockErrorPpm = clock;
        config.frequencyOffsetHz = 0.0;
        pipeline.scottieSession =
            std::make_unique<SstvScottieRxSession>(config);
        pipeline.scottieLinesReported = 0U;
        pipeline.nominalLinePeriodSamples =
            samplesForPicoseconds(spec.lineDuration);
        started = true;
    } else if (const auto mode = modeForStableId<SstvRobotMode, 8U>(
                   modeId, [](SstvRobotMode value) {
                       return SstvRobotProtocol::spec(value);
                   }); mode.has_value()) {
        const SstvRobotModeSpec spec = SstvRobotProtocol::spec(*mode);
        SstvRobotRxSessionConfig config;
        config.mode = *mode;
        config.sampleRate = SstvResampler::kOutputSampleRate;
        config.imageStartSample = imageStartSample;
        config.observationSpanSamples = static_cast<std::uint32_t>(
            pipeline.frequencyDemodulator->config().hopSamples);
        config.clockErrorPpm = clock;
        config.frequencyOffsetHz = 0.0;
        config.preserveTerminalGuard = true;
        config.allowTerminalRowRecovery = true;
        pipeline.robotSession =
            std::make_unique<SstvRobotRxSession>(config);
        pipeline.robotLinesReported = 0U;
        pipeline.nominalLinePeriodSamples =
            samplesForPicoseconds(spec.lineDuration);
        started = true;
    } else if (const auto mode =
                   modeForStableId<SstvSequentialRgbMode, 6U>(
                       modeId, [](SstvSequentialRgbMode value) {
                           return SstvSequentialRgbProtocol::spec(value);
                       }); mode.has_value()) {
        const SstvSequentialRgbModeSpec spec =
            SstvSequentialRgbProtocol::spec(*mode);
        SstvSequentialRgbRxSessionConfig config;
        config.mode = *mode;
        config.sampleRate = SstvResampler::kOutputSampleRate;
        config.imageStartSample = imageStartSample;
        config.observationSpanSamples = static_cast<std::uint32_t>(
            pipeline.frequencyDemodulator->config().hopSamples);
        config.clockErrorPpm = clock;
        config.frequencyOffsetHz = 0.0;
        pipeline.sequentialRgbSession =
            std::make_unique<SstvSequentialRgbRxSession>(config);
        pipeline.sequentialRgbLinesReported = 0U;
        pipeline.nominalLinePeriodSamples =
            samplesForPicoseconds(spec.lineDuration);
        started = true;
    } else if (const auto mode = modeForStableId<SstvPdMode, 7U>(
                   modeId, [](SstvPdMode value) {
                       return SstvPdProtocol::spec(value);
                   }); mode.has_value()) {
        const SstvPdModeSpec spec = SstvPdProtocol::spec(*mode);
        SstvPdRxSessionConfig config;
        config.mode = *mode;
        config.sampleRate = SstvResampler::kOutputSampleRate;
        config.imageStartSample = imageStartSample;
        config.observationSpanSamples = static_cast<std::uint32_t>(
            pipeline.frequencyDemodulator->config().hopSamples);
        config.clockErrorPpm = clock;
        config.frequencyOffsetHz = 0.0;
        pipeline.pdSession = std::make_unique<SstvPdRxSession>(config);
        pipeline.pdLinesReported = 0U;
        pipeline.nominalLinePeriodSamples =
            samplesForPicoseconds(spec.linePairDuration);
        started = true;
    } else if (const auto mode = modeForStableId<SstvMmsstvMode, 19U>(
                   modeId, [](SstvMmsstvMode value) {
                       return SstvMmsstvProtocol::spec(value);
                   }); mode.has_value()) {
        const SstvMmsstvModeSpec spec = SstvMmsstvProtocol::spec(*mode);
        SstvMmsstvRxSessionConfig config;
        config.mode = *mode;
        config.sampleRate = SstvResampler::kOutputSampleRate;
        config.imageStartSample = imageStartSample;
        config.observationSpanSamples = static_cast<std::uint32_t>(
            pipeline.frequencyDemodulator->config().hopSamples);
        config.clockErrorPpm = clock;
        config.frequencyOffsetHz = 0.0;
        pipeline.mmsstvSession =
            std::make_unique<SstvMmsstvRxSession>(config);
        pipeline.mmsstvLinesReported = 0U;
        pipeline.nominalLinePeriodSamples =
            samplesForPicoseconds(spec.scanDuration);
        started = true;
    }

    if (!started || pipeline.nominalLinePeriodSamples == 0U) {
        return false;
    }
    pipeline.slantController->configure(
        pipeline.nominalLinePeriodSamples,
        pipeline.controls.settings.slantMode,
        pipeline.controls.settings.manualClockErrorPpm);
    pipeline.syncLineIndex = 0U;
    for (const SstvFallbackSyncPulse& pulse :
         pipeline.fallbackResult.retainedPulses) {
        pipeline.slantController->observe(SstvSlantObservation {
            pipeline.syncLineIndex,
            pulse.startSample,
            pulse.confidence,
            pulse.predicted});
        ++pipeline.syncLineIndex;
    }
    const SstvSlantControllerSnapshot initialSlant =
        pipeline.slantController->snapshot();
    if (!pipeline.slantAcquisitionLogged
        && initialSlant.mode == SstvRxSlantMode::Automatic
        && initialSlant.estimateValid) {
        recordSlantAcquisitionDiagnostic(initialSlant);
        pipeline.slantAcquisitionLogged = true;
    }
    pipeline.stateMachine->dispatch(eventMs, SstvRxSyncObserved {});
    if (pipeline.stateMachine->state() != SstvRxState::Receiving) {
        pipeline.martinM1Session.reset();
        pipeline.scottieSession.reset();
        pipeline.robotSession.reset();
        pipeline.sequentialRgbSession.reset();
        pipeline.pdSession.reset();
        pipeline.mmsstvSession.reset();
        return false;
    }
    beginRetainedAcquisition(pipeline, imageStartSample);
    if (pipeline.martinM1Session) {
        publishMartinM1Image(pipeline, true);
    } else if (pipeline.scottieSession) {
        publishScottieImage(pipeline, true);
    } else if (pipeline.robotSession) {
        publishRobotImage(pipeline, true);
    } else if (pipeline.sequentialRgbSession) {
        publishSequentialRgbImage(pipeline, true);
    } else if (pipeline.pdSession) {
        publishPdImage(pipeline, true);
    } else if (pipeline.mmsstvSession) {
        publishMmsstvImage(pipeline, true);
    }
    return true;
}

void SstvRxRuntime::beginRetainedAcquisition(
    WorkerPipeline& pipeline,
    std::uint64_t imageStartSample)
{
    const std::uint64_t sessionId =
        pipeline.stateMachine->metrics().currentSessionId;
    if (sessionId == 0U || pipeline.source.kind
            == SstvAudioSourceKind::Unknown) {
        return;
    }
    // State-machine session ids intentionally restart on a route generation;
    // the retained store spans route switches, so namespace its descriptor.
    std::uint64_t acquisitionId = (pipeline.generation << 32U)
        ^ (sessionId & 0xffff'ffffULL);
    if (acquisitionId == 0U) {
        acquisitionId = sessionId;
    }
    if (m_retainedAudio.beginAcquisition(
            acquisitionId,
            pipeline.source,
            streamTime(pipeline.toneEpochUs, imageStartSample))) {
        pipeline.retainedAcquisitionId = acquisitionId;
    }
}

void SstvRxRuntime::closeRetainedAcquisition(
    WorkerPipeline& pipeline,
    std::uint64_t imageEndSample,
    bool complete,
    const std::string& mode)
{
    if (pipeline.retainedAcquisitionId == 0U) {
        return;
    }
    const SstvAfcSnapshot afc = pipeline.afcController->snapshot();
    const SstvSlantControllerSnapshot slant =
        pipeline.slantController->snapshot();
    const std::string fskId = pipeline.latestFskId.valid()
        ? pipeline.latestFskId.decoded.text : std::string {};
    if (m_retainedAudio.closeAcquisition(
            pipeline.retainedAcquisitionId,
            streamTime(pipeline.toneEpochUs, imageEndSample),
            complete,
            mode,
            fskId,
            afc.correctionHz,
            slant.appliedClockErrorPpm)) {
        pipeline.lastCompletedAcquisitionId =
            pipeline.retainedAcquisitionId;
        pipeline.retainedAcquisitionId = 0U;
    }
}

void SstvRxRuntime::recordProgressiveUpdate(
    WorkerPipeline& pipeline) noexcept
{
    saturatingAdd(pipeline.progressiveUpdates);
    if (pipeline.firstProgressiveUpdateNs == 0U) {
        pipeline.firstProgressiveUpdateNs = pipeline.currentChunkEndNs;
    }
    pipeline.lastProgressiveUpdateNs = pipeline.currentChunkEndNs;
}

bool SstvRxRuntime::beginMartinM1Session(
    WorkerPipeline& pipeline,
    const SstvVisDetection& detection,
    std::uint64_t eventMs,
    std::uint8_t visPayload)
{
    if (pipeline.hasNativeSession() || !pipeline.frequencyDemodulator) {
        return false;
    }
    const std::optional<SstvMartinMode> mode =
        martinModeForVis(visPayload);
    const auto imageStart = sampleIndexAtUs(
        pipeline.toneEpochUs, detection.frameEndedAtUs);
    if (!mode.has_value() || !imageStart.has_value()) {
        return false;
    }

    const double clockError = pipeline.controls.settings.slantMode
            == SstvRxSlantMode::Manual
        ? pipeline.controls.settings.manualClockErrorPpm : 0.0;
    SstvMartinM1RxSessionConfig sessionConfig;
    sessionConfig.sampleRate = SstvResampler::kOutputSampleRate;
    sessionConfig.imageStartSample = *imageStart;
    sessionConfig.observationSpanSamples = static_cast<std::uint32_t>(
        pipeline.frequencyDemodulator->config().hopSamples);
    sessionConfig.clockErrorPpm = roundedClockErrorPpm(clockError);
    sessionConfig.frequencyOffsetHz = 0.0;
    sessionConfig.mode = *mode;
    pipeline.martinM1Session =
        std::make_unique<SstvMartinM1RxSession>(sessionConfig);
    pipeline.martinM1LinesReported = 0U;
    pipeline.nominalLinePeriodSamples = samplesForPicoseconds(
        SstvMartinM1Protocol::spec(*mode).lineDuration);
    pipeline.slantController->configure(
        pipeline.nominalLinePeriodSamples,
        pipeline.controls.settings.slantMode,
        pipeline.controls.settings.manualClockErrorPpm);
    pipeline.syncLineIndex = 0U;
    pipeline.stateMachine->dispatch(eventMs, SstvRxSyncObserved {});
    if (pipeline.stateMachine->state() != SstvRxState::Receiving) {
        pipeline.martinM1Session.reset();
        return false;
    }
    const auto retainedStart = sampleIndexAtUs(
        pipeline.toneEpochUs, detection.frameStartedAtUs);
    beginRetainedAcquisition(
        pipeline, retainedStart.value_or(*imageStart));
    publishMartinM1Image(pipeline, true);
    return true;
}

bool SstvRxRuntime::beginScottieSession(
    WorkerPipeline& pipeline,
    const SstvVisDetection& detection,
    std::uint64_t eventMs,
    std::uint8_t visPayload)
{
    if (pipeline.hasNativeSession() || !pipeline.frequencyDemodulator) {
        return false;
    }
    const std::optional<SstvScottieMode> mode =
        scottieModeForVis(visPayload);
    const auto imageStart = sampleIndexAtUs(
        pipeline.toneEpochUs, detection.frameEndedAtUs);
    if (!mode.has_value() || !imageStart.has_value()) {
        return false;
    }

    const double clockError = pipeline.controls.settings.slantMode
            == SstvRxSlantMode::Manual
        ? pipeline.controls.settings.manualClockErrorPpm : 0.0;
    SstvScottieRxSessionConfig sessionConfig;
    sessionConfig.mode = *mode;
    sessionConfig.sampleRate = SstvResampler::kOutputSampleRate;
    sessionConfig.imageStartSample = *imageStart;
    sessionConfig.observationSpanSamples = static_cast<std::uint32_t>(
        pipeline.frequencyDemodulator->config().hopSamples);
    sessionConfig.clockErrorPpm = roundedClockErrorPpm(clockError);
    sessionConfig.frequencyOffsetHz = 0.0;
    pipeline.scottieSession =
        std::make_unique<SstvScottieRxSession>(sessionConfig);
    pipeline.scottieLinesReported = 0U;
    pipeline.nominalLinePeriodSamples = samplesForPicoseconds(
        SstvScottieProtocol::spec(*mode).lineDuration);
    pipeline.slantController->configure(
        pipeline.nominalLinePeriodSamples,
        pipeline.controls.settings.slantMode,
        pipeline.controls.settings.manualClockErrorPpm);
    pipeline.syncLineIndex = 0U;
    if (pipeline.stateMachine->state() != SstvRxState::WaitingForSync) {
        pipeline.scottieSession.reset();
        return false;
    }

    // Scottie has no line-leading sync.  WaitingForSync is retained until the
    // first real embedded pulse after G/B; the session buffers only one
    // bounded row window so those pre-sync components can then be replayed.
    static_cast<void>(eventMs);
    const auto retainedStart = sampleIndexAtUs(
        pipeline.toneEpochUs, detection.frameStartedAtUs);
    beginRetainedAcquisition(
        pipeline, retainedStart.value_or(*imageStart));
    publishScottieImage(pipeline, true);
    return true;
}

bool SstvRxRuntime::beginRobotSession(
    WorkerPipeline& pipeline,
    const SstvVisDetection& detection,
    std::uint64_t eventMs,
    std::uint8_t visPayload)
{
    if (pipeline.hasNativeSession() || !pipeline.frequencyDemodulator) {
        return false;
    }
    const std::optional<SstvRobotMode> mode = robotModeForVis(visPayload);
    const auto imageStart = sampleIndexAtUs(
        pipeline.toneEpochUs, detection.frameEndedAtUs);
    if (!mode.has_value() || !imageStart.has_value()) {
        return false;
    }

    const double clockError = pipeline.controls.settings.slantMode
            == SstvRxSlantMode::Manual
        ? pipeline.controls.settings.manualClockErrorPpm : 0.0;
    SstvRobotRxSessionConfig config;
    config.mode = *mode;
    config.sampleRate = SstvResampler::kOutputSampleRate;
    config.imageStartSample = *imageStart;
    config.observationSpanSamples = static_cast<std::uint32_t>(
        pipeline.frequencyDemodulator->config().hopSamples);
    config.clockErrorPpm = roundedClockErrorPpm(clockError);
    config.frequencyOffsetHz = 0.0;
    config.preserveTerminalGuard = true;
    config.allowTerminalRowRecovery = true;
    pipeline.robotSession = std::make_unique<SstvRobotRxSession>(config);
    pipeline.robotLinesReported = 0U;
    pipeline.nominalLinePeriodSamples = samplesForPicoseconds(
        SstvRobotProtocol::spec(*mode).lineDuration);
    pipeline.slantController->configure(
        pipeline.nominalLinePeriodSamples,
        pipeline.controls.settings.slantMode,
        pipeline.controls.settings.manualClockErrorPpm);
    pipeline.syncLineIndex = 0U;
    pipeline.stateMachine->dispatch(eventMs, SstvRxSyncObserved {});
    if (pipeline.stateMachine->state() != SstvRxState::Receiving) {
        pipeline.robotSession.reset();
        return false;
    }
    const auto retainedStart = sampleIndexAtUs(
        pipeline.toneEpochUs, detection.frameStartedAtUs);
    beginRetainedAcquisition(
        pipeline, retainedStart.value_or(*imageStart));
    publishRobotImage(pipeline, true);
    return true;
}

bool SstvRxRuntime::beginSequentialRgbSession(
    WorkerPipeline& pipeline,
    const SstvVisDetection& detection,
    std::uint64_t eventMs,
    std::uint8_t visPayload)
{
    if (pipeline.hasNativeSession() || !pipeline.frequencyDemodulator) {
        return false;
    }
    const std::optional<SstvSequentialRgbMode> mode =
        sequentialRgbModeForVis(visPayload);
    const auto imageStart = sampleIndexAtUs(
        pipeline.toneEpochUs, detection.frameEndedAtUs);
    if (!mode.has_value() || !imageStart.has_value()) {
        return false;
    }

    const double clockError = pipeline.controls.settings.slantMode
            == SstvRxSlantMode::Manual
        ? pipeline.controls.settings.manualClockErrorPpm : 0.0;
    SstvSequentialRgbRxSessionConfig config;
    config.mode = *mode;
    config.sampleRate = SstvResampler::kOutputSampleRate;
    config.imageStartSample = *imageStart;
    config.observationSpanSamples = static_cast<std::uint32_t>(
        pipeline.frequencyDemodulator->config().hopSamples);
    config.clockErrorPpm = roundedClockErrorPpm(clockError);
    config.frequencyOffsetHz = 0.0;
    pipeline.sequentialRgbSession =
        std::make_unique<SstvSequentialRgbRxSession>(config);
    pipeline.sequentialRgbLinesReported = 0U;
    pipeline.nominalLinePeriodSamples = samplesForPicoseconds(
        SstvSequentialRgbProtocol::spec(*mode).lineDuration);
    pipeline.slantController->configure(
        pipeline.nominalLinePeriodSamples,
        pipeline.controls.settings.slantMode,
        pipeline.controls.settings.manualClockErrorPpm);
    pipeline.syncLineIndex = 0U;
    pipeline.stateMachine->dispatch(eventMs, SstvRxSyncObserved {});
    if (pipeline.stateMachine->state() != SstvRxState::Receiving) {
        pipeline.sequentialRgbSession.reset();
        return false;
    }
    const auto retainedStart = sampleIndexAtUs(
        pipeline.toneEpochUs, detection.frameStartedAtUs);
    beginRetainedAcquisition(
        pipeline, retainedStart.value_or(*imageStart));
    publishSequentialRgbImage(pipeline, true);
    return true;
}

bool SstvRxRuntime::beginPdSession(
    WorkerPipeline& pipeline,
    const SstvVisDetection& detection,
    std::uint64_t eventMs,
    std::uint8_t visPayload)
{
    if (pipeline.hasNativeSession() || !pipeline.frequencyDemodulator) {
        return false;
    }
    const std::optional<SstvPdMode> mode = pdModeForVis(visPayload);
    const auto imageStart = sampleIndexAtUs(
        pipeline.toneEpochUs, detection.frameEndedAtUs);
    if (!mode.has_value() || !imageStart.has_value()) {
        return false;
    }

    const double clockError = pipeline.controls.settings.slantMode
            == SstvRxSlantMode::Manual
        ? pipeline.controls.settings.manualClockErrorPpm : 0.0;
    SstvPdRxSessionConfig config;
    config.mode = *mode;
    config.sampleRate = SstvResampler::kOutputSampleRate;
    config.imageStartSample = *imageStart;
    config.observationSpanSamples = static_cast<std::uint32_t>(
        pipeline.frequencyDemodulator->config().hopSamples);
    config.clockErrorPpm = roundedClockErrorPpm(clockError);
    config.frequencyOffsetHz = 0.0;
    pipeline.pdSession = std::make_unique<SstvPdRxSession>(config);
    pipeline.pdLinesReported = 0U;
    pipeline.nominalLinePeriodSamples = samplesForPicoseconds(
        SstvPdProtocol::spec(*mode).linePairDuration);
    pipeline.slantController->configure(
        pipeline.nominalLinePeriodSamples,
        pipeline.controls.settings.slantMode,
        pipeline.controls.settings.manualClockErrorPpm);
    pipeline.syncLineIndex = 0U;
    pipeline.stateMachine->dispatch(eventMs, SstvRxSyncObserved {});
    if (pipeline.stateMachine->state() != SstvRxState::Receiving) {
        pipeline.pdSession.reset();
        return false;
    }
    const auto retainedStart = sampleIndexAtUs(
        pipeline.toneEpochUs, detection.frameStartedAtUs);
    beginRetainedAcquisition(
        pipeline, retainedStart.value_or(*imageStart));
    publishPdImage(pipeline, true);
    return true;
}

bool SstvRxRuntime::armAvtCountdown(
    WorkerPipeline& pipeline,
    const SstvVisDetection& detection,
    std::uint8_t visPayload)
{
    if (pipeline.hasNativeSession() || pipeline.avtCountdownDetector
        || !pipeline.frequencyDemodulator) {
        return false;
    }
    const std::optional<SstvAvtMode> mode =
        SstvAvtProtocol::normalModeForVis(visPayload);
    const auto searchStart = sampleIndexAtUs(
        pipeline.toneEpochUs, detection.frameEndedAtUs);
    if (!mode.has_value() || !searchStart.has_value()) {
        return false;
    }

    // The shared demodulator owns AFC; countdown/image decoders always see
    // already-corrected observations and therefore use zero local offset.
    pipeline.avtFrequencyOffsetHz = 0.0;
    pipeline.avtRetainedStartSample = sampleIndexAtUs(
        pipeline.toneEpochUs, detection.frameStartedAtUs).value_or(
            *searchStart);
    SstvAvtCountdownDetectorConfig config;
    config.expectedMode = *mode;
    config.sampleRate = SstvResampler::kOutputSampleRate;
    config.searchStartSample = *searchStart;
    // The countdown detector recovers symbol edges from the demodulator's
    // analysis-window centres. This is deliberately not the one-sample hop.
    config.observationSpanSamples = static_cast<std::uint32_t>(
        pipeline.frequencyDemodulator->config().averagingSamples);
    config.frequencyOffsetHz = pipeline.avtFrequencyOffsetHz;
    pipeline.avtCountdownDetector =
        std::make_unique<SstvAvtCountdownDetector>(config);
    return true;
}

void SstvRxRuntime::consumeAvtCountdownObservations(
    WorkerPipeline& pipeline,
    const std::vector<SstvFrequencyObservation>& observations,
    std::uint64_t eventMs)
{
    if (!pipeline.avtCountdownDetector || observations.empty()) {
        return;
    }

    std::optional<SstvAvtCountdownDetection> acquired;
    std::size_t offset = 0U;
    while (offset < observations.size() && !acquired.has_value()
           && pipeline.avtCountdownDetector->state()
               == SstvAvtCountdownDetectorState::Searching) {
        const std::size_t count = std::min(
            observations.size() - offset,
            SstvAvtCountdownDetector::MaximumObservationsPerConsume);
        acquired = pipeline.avtCountdownDetector->consume(
            observations.data() + offset, count);
        offset += count;
    }

    if (acquired.has_value()) {
        const bool started = beginAvtSession(pipeline, *acquired, eventMs);
        pipeline.avtCountdownDetector.reset();
        if (!started) {
            pipeline.stateMachine->dispatch(
                eventMs,
                SstvRxFailure {
                    SstvRxErrorCode::DspFailure,
                    "protected AVT countdown image start is outside the "
                    "active DSP timeline"});
        }
        return;
    }

    if (pipeline.avtCountdownDetector->state()
        != SstvAvtCountdownDetectorState::Searching) {
        pipeline.avtCountdownDetector.reset();
        if (pipeline.stateMachine->state() == SstvRxState::WaitingForSync) {
            pipeline.stateMachine->dispatch(eventMs, SstvRxCancel {});
        }
    }
}

bool SstvRxRuntime::beginAvtSession(
    WorkerPipeline& pipeline,
    const SstvAvtCountdownDetection& detection,
    std::uint64_t eventMs)
{
    if (pipeline.hasNativeSession() || !pipeline.frequencyDemodulator
        || !detection.acquired
        || pipeline.stateMachine->state() != SstvRxState::WaitingForSync) {
        return false;
    }

    SstvAvtRxSessionConfig config;
    config.mode = detection.mode;
    config.sampleRate = SstvResampler::kOutputSampleRate;
    config.imageStartSample = detection.imageStartSample;
    config.observationSpanSamples = static_cast<std::uint32_t>(
        pipeline.frequencyDemodulator->config().hopSamples);
    config.frequencyOffsetHz = pipeline.avtFrequencyOffsetHz;
    const double clockError = pipeline.controls.settings.slantMode
            == SstvRxSlantMode::Manual
        ? pipeline.controls.settings.manualClockErrorPpm : 0.0;
    config.clockErrorPpm = roundedClockErrorPpm(clockError);
    pipeline.avtSession = std::make_unique<SstvAvtRxSession>(config);
    pipeline.avtLinesReported = 0U;
    pipeline.nominalLinePeriodSamples = samplesForPicoseconds(
        SstvAvtProtocol::spec(detection.mode).lineDuration);
    pipeline.slantController->configure(
        pipeline.nominalLinePeriodSamples,
        pipeline.controls.settings.slantMode,
        pipeline.controls.settings.manualClockErrorPpm);
    pipeline.syncLineIndex = 0U;
    pipeline.stateMachine->dispatch(eventMs, SstvRxSyncObserved {});
    if (pipeline.stateMachine->state() != SstvRxState::Receiving) {
        pipeline.avtSession.reset();
        return false;
    }
    beginRetainedAcquisition(
        pipeline, pipeline.avtRetainedStartSample);
    publishAvtImage(pipeline, true);
    return true;
}

bool SstvRxRuntime::beginMmsstvSession(
    WorkerPipeline& pipeline,
    std::uint64_t eventMs,
    SstvMmsstvMode mode,
    std::uint64_t imageStartUs,
    double estimatedFrequencyOffsetHz)
{
    if (pipeline.hasNativeSession() || !pipeline.frequencyDemodulator) {
        return false;
    }
    const auto imageStart = sampleIndexAtUs(
        pipeline.toneEpochUs, imageStartUs);
    if (!imageStart.has_value()) {
        return false;
    }
    static_cast<void>(estimatedFrequencyOffsetHz);
    const double clockError = pipeline.controls.settings.slantMode
            == SstvRxSlantMode::Manual
        ? pipeline.controls.settings.manualClockErrorPpm : 0.0;
    SstvMmsstvRxSessionConfig config;
    config.mode = mode;
    config.sampleRate = SstvResampler::kOutputSampleRate;
    config.imageStartSample = *imageStart;
    config.observationSpanSamples = static_cast<std::uint32_t>(
        pipeline.frequencyDemodulator->config().hopSamples);
    config.clockErrorPpm = roundedClockErrorPpm(clockError);
    config.frequencyOffsetHz = 0.0;
    pipeline.mmsstvSession = std::make_unique<SstvMmsstvRxSession>(config);
    pipeline.mmsstvLinesReported = 0U;
    const SstvMmsstvModeSpec modeSpec = SstvMmsstvProtocol::spec(mode);
    pipeline.nominalLinePeriodSamples = samplesForPicoseconds(
        modeSpec.scanDuration);
    pipeline.slantController->configure(
        pipeline.nominalLinePeriodSamples,
        pipeline.controls.settings.slantMode,
        pipeline.controls.settings.manualClockErrorPpm);
    pipeline.syncLineIndex = 0U;
    pipeline.stateMachine->dispatch(eventMs, SstvRxSyncObserved {});
    if (pipeline.stateMachine->state() != SstvRxState::Receiving) {
        pipeline.mmsstvSession.reset();
        return false;
    }
    const std::uint64_t retainedHeaderSamples = samplesForPicoseconds(
        modeSpec.headerDuration);
    beginRetainedAcquisition(
        pipeline,
        *imageStart > retainedHeaderSamples
            ? *imageStart - retainedHeaderSamples : 0U);
    publishMmsstvImage(pipeline, true);
    return true;
}

void SstvRxRuntime::consumeMartinM1Observations(
    WorkerPipeline& pipeline,
    const std::vector<SstvFrequencyObservation>& observations,
    std::uint64_t eventMs)
{
    if (!pipeline.martinM1Session || observations.empty()) {
        return;
    }

    bool imageChanged = false;
    std::size_t offset = 0U;
    while (offset < observations.size()
           && pipeline.martinM1Session->state()
               == SstvMartinM1RxSessionState::Receiving) {
        const std::size_t count = std::min(
            observations.size() - offset,
            SstvMartinM1RxSession::MaximumObservationsPerConsume);
        const SstvMartinM1RxSessionUpdate update =
            pipeline.martinM1Session->consume(
                observations.data() + offset, count);
        offset += count;
        imageChanged = imageChanged || update.imageChanged;

        if (update.linesPublished > pipeline.martinM1LinesReported) {
            const std::uint64_t delta =
                update.linesPublished - pipeline.martinM1LinesReported;
            const std::uint32_t decodedLines = static_cast<std::uint32_t>(
                std::min<std::uint64_t>(
                    delta,
                    std::numeric_limits<std::uint32_t>::max()));
            pipeline.stateMachine->dispatch(
                eventMs,
                SstvRxLineObservation {
                    update.observedLineSyncs > 0U,
                    decodedLines});
            pipeline.martinM1LinesReported = update.linesPublished;
        }
    }

    const SstvMartinM1RxSessionState sessionState =
        pipeline.martinM1Session->state();
    if (sessionState == SstvMartinM1RxSessionState::Receiving) {
        if (imageChanged) {
            publishMartinM1Image(pipeline, false);
        }
        return;
    }

    const bool usablePartial =
        pipeline.martinM1Session->imageFrame().coverage() > 0.0;
    if (sessionState == SstvMartinM1RxSessionState::Complete) {
        const std::uint64_t lines =
            pipeline.martinM1Session->decoderMetrics().linesPublished;
        pipeline.stateMachine->dispatch(
            eventMs,
            SstvRxFrameCompleted {
                static_cast<std::uint32_t>(std::min<std::uint64_t>(
                    lines,
                    std::numeric_limits<std::uint32_t>::max()))});
    } else if (sessionState == SstvMartinM1RxSessionState::Partial) {
        pipeline.stateMachine->dispatch(
            eventMs, SstvRxInputEnded {usablePartial});
    } else {
        pipeline.stateMachine->dispatch(eventMs, SstvRxCancel {});
    }
    publishMartinM1Image(pipeline, true);
    pipeline.martinM1Session.reset();
}

void SstvRxRuntime::terminateMartinM1ForDiscontinuity(
    WorkerPipeline& pipeline,
    std::uint64_t eventMs)
{
    if (!pipeline.martinM1Session) {
        return;
    }
    const std::uint64_t nextSample = pipeline.hasFrequencySample
        ? saturatingUnsignedAdd(pipeline.lastFrequencySample, 1U)
        : pipeline.martinM1Session->imageStartSample();
    const SstvMartinM1RxSessionState sessionState =
        pipeline.martinM1Session->notifyDiscontinuity(nextSample);
    const bool usablePartial =
        pipeline.martinM1Session->imageFrame().coverage() > 0.0;
    if (sessionState == SstvMartinM1RxSessionState::Complete) {
        const std::uint64_t lines =
            pipeline.martinM1Session->decoderMetrics().linesPublished;
        pipeline.stateMachine->dispatch(
            eventMs,
            SstvRxFrameCompleted {
                static_cast<std::uint32_t>(std::min<std::uint64_t>(
                    lines,
                    std::numeric_limits<std::uint32_t>::max()))});
    } else {
        pipeline.stateMachine->dispatch(
            eventMs, SstvRxInputEnded {usablePartial});
    }
    publishMartinM1Image(pipeline, true);
    pipeline.martinM1Session.reset();
}

void SstvRxRuntime::publishMartinM1Image(WorkerPipeline& pipeline,
                                         bool force)
{
    if (!pipeline.martinM1Session) {
        return;
    }
    auto image = std::make_shared<SstvImageSnapshot>(
        pipeline.martinM1Session->snapshot());
    const SstvMartinM1RxSessionState sessionState =
        pipeline.martinM1Session->state();
    const SstvMartinModeSpec spec = SstvMartinM1Protocol::spec(
        pipeline.martinM1Session->mode());

    ImageSummary summary;
    summary.available = true;
    summary.complete = sessionState == SstvMartinM1RxSessionState::Complete
        || image->isComplete();
    summary.partial = sessionState == SstvMartinM1RxSessionState::Partial;
    summary.cancelled = sessionState == SstvMartinM1RxSessionState::Cancelled
        || image->cancelled;
    summary.acquisitionId =
        pipeline.stateMachine->metrics().currentSessionId;
    summary.generation = pipeline.generation;
    summary.revision = image->revision;
    summary.linesPublished =
        pipeline.martinM1Session->decoderMetrics().linesPublished;
    summary.width = image->width;
    summary.height = image->height;
    summary.coveredComponents = image->coveredComponents;
    summary.completedPixels = image->completedPixels;
    summary.coverage = image->coverage();
    summary.mode = QString::fromLatin1(spec.stableId);
    recordProgressiveUpdate(pipeline);
    if (sessionState != SstvMartinM1RxSessionState::Receiving) {
        const std::uint64_t retainedEnd = summary.complete
            ? pipeline.martinM1Session->imageEndSample()
            : (pipeline.hasFrequencySample
                  ? saturatingUnsignedAdd(pipeline.lastFrequencySample, 1U)
                  : saturatingUnsignedAdd(
                        pipeline.martinM1Session->imageStartSample(), 1U));
        closeRetainedAcquisition(
            pipeline, retainedEnd, summary.complete, spec.stableId);
    }

    {
        const std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_snapshot.image = std::move(summary);
        m_latestImageSnapshot = std::move(image);
    }
    scheduleSnapshotNotification(force);
}

void SstvRxRuntime::consumeScottieObservations(
    WorkerPipeline& pipeline,
    const std::vector<SstvFrequencyObservation>& observations,
    std::uint64_t eventMs)
{
    if (!pipeline.scottieSession || observations.empty()) {
        return;
    }

    bool imageChanged = false;
    std::size_t offset = 0U;
    while (offset < observations.size()
           && pipeline.scottieSession->state()
               == SstvScottieRxSessionState::Receiving) {
        const std::size_t count = std::min(
            observations.size() - offset,
            SstvScottieRxSession::MaximumObservationsPerConsume);
        const SstvScottieRxSessionUpdate update =
            pipeline.scottieSession->consume(
                observations.data() + offset, count);
        offset += count;
        imageChanged = imageChanged || update.imageChanged;

        if (update.observedLineSyncs > 0U
            && (pipeline.stateMachine->state()
                    == SstvRxState::WaitingForSync
                || pipeline.stateMachine->state()
                    == SstvRxState::RecoveringSync)) {
            pipeline.stateMachine->dispatch(eventMs,
                                            SstvRxSyncObserved {});
        }
        if (update.linesPublished > pipeline.scottieLinesReported
            && (pipeline.stateMachine->state() == SstvRxState::Receiving
                || pipeline.stateMachine->state()
                    == SstvRxState::RecoveringSync)) {
            const std::uint64_t delta =
                update.linesPublished - pipeline.scottieLinesReported;
            const std::uint32_t decodedLines = static_cast<std::uint32_t>(
                std::min<std::uint64_t>(
                    delta,
                    std::numeric_limits<std::uint32_t>::max()));
            pipeline.stateMachine->dispatch(
                eventMs,
                SstvRxLineObservation {
                    update.observedLineSyncs > 0U,
                    decodedLines});
            pipeline.scottieLinesReported = update.linesPublished;
        }
    }

    const SstvScottieRxSessionState sessionState =
        pipeline.scottieSession->state();
    if (sessionState == SstvScottieRxSessionState::Receiving) {
        if (imageChanged) {
            publishScottieImage(pipeline, false);
        }
        return;
    }

    const bool usablePartial =
        pipeline.scottieSession->imageFrame().coverage() > 0.0;
    if (sessionState == SstvScottieRxSessionState::Complete) {
        const std::uint64_t lines =
            pipeline.scottieSession->decoderMetrics().linesPublished;
        pipeline.stateMachine->dispatch(
            eventMs,
            SstvRxFrameCompleted {
                static_cast<std::uint32_t>(std::min<std::uint64_t>(
                    lines,
                    std::numeric_limits<std::uint32_t>::max()))});
    } else if (sessionState == SstvScottieRxSessionState::Partial) {
        pipeline.stateMachine->dispatch(
            eventMs, SstvRxInputEnded {usablePartial});
    } else {
        pipeline.stateMachine->dispatch(eventMs, SstvRxCancel {});
    }
    publishScottieImage(pipeline, true);
    pipeline.scottieSession.reset();
}

void SstvRxRuntime::terminateScottieForDiscontinuity(
    WorkerPipeline& pipeline,
    std::uint64_t eventMs)
{
    if (!pipeline.scottieSession) {
        return;
    }
    const std::uint64_t nextSample = pipeline.hasFrequencySample
        ? saturatingUnsignedAdd(pipeline.lastFrequencySample, 1U)
        : pipeline.scottieSession->imageStartSample();
    const SstvScottieRxSessionState sessionState =
        pipeline.scottieSession->notifyDiscontinuity(nextSample);
    const bool usablePartial =
        pipeline.scottieSession->imageFrame().coverage() > 0.0;
    if (sessionState == SstvScottieRxSessionState::Complete) {
        const std::uint64_t lines =
            pipeline.scottieSession->decoderMetrics().linesPublished;
        pipeline.stateMachine->dispatch(
            eventMs,
            SstvRxFrameCompleted {
                static_cast<std::uint32_t>(std::min<std::uint64_t>(
                    lines,
                    std::numeric_limits<std::uint32_t>::max()))});
    } else {
        pipeline.stateMachine->dispatch(
            eventMs, SstvRxInputEnded {usablePartial});
    }
    publishScottieImage(pipeline, true);
    pipeline.scottieSession.reset();
}

void SstvRxRuntime::publishScottieImage(WorkerPipeline& pipeline,
                                        bool force)
{
    if (!pipeline.scottieSession) {
        return;
    }
    auto image = std::make_shared<SstvImageSnapshot>(
        pipeline.scottieSession->snapshot());
    const SstvScottieRxSessionState sessionState =
        pipeline.scottieSession->state();
    const SstvScottieModeSpec spec = SstvScottieProtocol::spec(
        pipeline.scottieSession->mode());

    ImageSummary summary;
    summary.available = true;
    summary.complete = sessionState == SstvScottieRxSessionState::Complete
        || image->isComplete();
    summary.partial = sessionState == SstvScottieRxSessionState::Partial;
    summary.cancelled =
        sessionState == SstvScottieRxSessionState::Cancelled
        || image->cancelled;
    summary.acquisitionId =
        pipeline.stateMachine->metrics().currentSessionId;
    summary.generation = pipeline.generation;
    summary.revision = image->revision;
    summary.linesPublished =
        pipeline.scottieSession->decoderMetrics().linesPublished;
    summary.width = image->width;
    summary.height = image->height;
    summary.coveredComponents = image->coveredComponents;
    summary.completedPixels = image->completedPixels;
    summary.coverage = image->coverage();
    summary.mode = QString::fromLatin1(spec.stableId);
    recordProgressiveUpdate(pipeline);
    if (sessionState != SstvScottieRxSessionState::Receiving) {
        const std::uint64_t retainedEnd = summary.complete
            ? pipeline.scottieSession->imageEndSample()
            : (pipeline.hasFrequencySample
                  ? saturatingUnsignedAdd(pipeline.lastFrequencySample, 1U)
                  : saturatingUnsignedAdd(
                        pipeline.scottieSession->imageStartSample(), 1U));
        closeRetainedAcquisition(
            pipeline, retainedEnd, summary.complete, spec.stableId);
    }

    {
        const std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_snapshot.image = std::move(summary);
        m_latestImageSnapshot = std::move(image);
    }
    scheduleSnapshotNotification(force);
}

void SstvRxRuntime::consumeRobotObservations(
    WorkerPipeline& pipeline,
    const std::vector<SstvFrequencyObservation>& observations,
    std::uint64_t eventMs,
    std::uint64_t inputEndSample)
{
    if (!pipeline.robotSession || observations.empty()) {
        return;
    }
    bool imageChanged = false;
    std::size_t offset = 0U;
    while (offset < observations.size()
           && pipeline.robotSession->state()
               == SstvRobotRxSessionState::Receiving) {
        const std::size_t count = std::min(
            observations.size() - offset,
            SstvRobotRxSession::MaximumObservationsPerConsume);
        const SstvRobotRxSessionUpdate update =
            pipeline.robotSession->consume(
                observations.data() + offset, count);
        offset += count;
        imageChanged = imageChanged || update.imageChanged;
        if (update.linesPublished > pipeline.robotLinesReported) {
            const std::uint64_t delta =
                update.linesPublished - pipeline.robotLinesReported;
            pipeline.stateMachine->dispatch(
                eventMs,
                SstvRxLineObservation {
                    update.observedLineSyncs > 0U,
                    static_cast<std::uint32_t>(std::min<std::uint64_t>(
                        delta,
                        std::numeric_limits<std::uint32_t>::max()))});
            pipeline.robotLinesReported = update.linesPublished;
        }
    }

    const SstvRobotRxSessionState sessionState =
        pipeline.robotSession->state();
    if (sessionState == SstvRobotRxSessionState::Receiving
        && inputEndSample >= pipeline.robotSession->imageEndSample()) {
        // The demodulator intentionally emits no frequency observations for
        // the silent/tail portion after the final scanline.  Without an
        // explicit stream-end check the Robot session remains Receiving
        // forever and the decoder never flushes its final sync predictions.
        // Close only after the whole input chunk has been consumed, so the
        // last valid observations still reach the image accumulator.
        finishRobotAtImageEnd(pipeline, eventMs);
        return;
    }
    if (qEnvironmentVariableIsSet("DECODIUM_SSTV_TRACE_FRAME")
        && sessionState != SstvRobotRxSessionState::Receiving) {
        const SstvImageSnapshot snapshot = pipeline.robotSession->snapshot();
        std::uint32_t firstIncomplete = snapshot.height;
        while (firstIncomplete != 0U
               && !snapshot.isScanlineComplete(firstIncomplete - 1U)) {
            --firstIncomplete;
        }
        std::uint32_t firstIncompleteFromTop = snapshot.height;
        std::uint32_t incompleteRows = 0U;
        for (std::uint32_t row = 0U; row < snapshot.height; ++row) {
            if (!snapshot.isScanlineComplete(row)) {
                firstIncompleteFromTop = std::min(
                    firstIncompleteFromTop, row);
                ++incompleteRows;
            }
        }
        qInfo().noquote()
            << "[SSTV][FRAME] robot terminal state="
            << static_cast<int>(sessionState)
            << "lines=" << pipeline.robotSession->decoderMetrics().linesPublished
            << "coverage=" << pipeline.robotSession->imageFrame().coverage()
            << "image_start_sample=" << pipeline.robotSession->imageStartSample()
            << "image_end_sample=" << pipeline.robotSession->imageEndSample()
            << "tail_first_incomplete=" << firstIncomplete
            << "first_incomplete_from_top=" << firstIncompleteFromTop
            << "incomplete_rows=" << incompleteRows
            << "tail_prev_complete="
            << (firstIncomplete > 0U
                && snapshot.isScanlineComplete(firstIncomplete - 1U))
            << "terminal_row_recovery="
            << pipeline.robotSession->config().allowTerminalRowRecovery;
    }
    if (sessionState == SstvRobotRxSessionState::Receiving) {
        if (imageChanged) {
            publishRobotImage(pipeline, false);
        }
        return;
    }
    const bool usablePartial =
        pipeline.robotSession->imageFrame().coverage() > 0.0;
    if (sessionState == SstvRobotRxSessionState::Complete) {
        pipeline.holdOffNativeAcquisition(
            pipeline.robotSession->imageEndSample());
        const std::uint64_t lines =
            pipeline.robotSession->decoderMetrics().linesPublished;
        pipeline.stateMachine->dispatch(
            eventMs,
            SstvRxFrameCompleted {
                static_cast<std::uint32_t>(std::min<std::uint64_t>(
                    lines,
                    std::numeric_limits<std::uint32_t>::max()))});
    } else if (sessionState == SstvRobotRxSessionState::Partial) {
        // A late stop/tail can leave only a few terminal Robot lines missing.
        // Keep that useful bounded partial image visible and suppress the
        // immediate tail-induced false VIS instead of replacing it with a
        // fresh 1–2 line acquisition.
        if (pipeline.robotSession->imageFrame().coverage() >= 0.75) {
            pipeline.holdOffNativeAcquisition(
                pipeline.robotSession->imageEndSample());
        }
        pipeline.stateMachine->dispatch(
            eventMs, SstvRxInputEnded {usablePartial});
    } else {
        pipeline.stateMachine->dispatch(eventMs, SstvRxCancel {});
    }
    publishRobotImage(pipeline, true);
    pipeline.robotSession.reset();
}

void SstvRxRuntime::finishRobotAtImageEnd(
    WorkerPipeline& pipeline,
    std::uint64_t eventMs)
{
    if (!pipeline.robotSession) {
        return;
    }
    const SstvRobotRxSessionState sessionState =
        pipeline.robotSession->finish();
    if (qEnvironmentVariableIsSet("DECODIUM_SSTV_TRACE_FRAME")) {
        const SstvRobotDecoderMetrics metrics =
            pipeline.robotSession->decoderMetrics();
        qInfo().noquote()
            << "[SSTV][FRAME] robot terminal state="
            << static_cast<int>(sessionState)
            << "lines=" << metrics.linesPublished
            << "coverage=" << pipeline.robotSession->imageFrame().coverage()
            << "image_start_sample=" << pipeline.robotSession->imageStartSample()
            << "image_end_sample=" << pipeline.robotSession->imageEndSample()
            << "sync_inputs=" << metrics.syncInputs
            << "sync_observed=" << metrics.observedSyncs
            << "sync_predicted=" << metrics.predictedSyncs
            << "sync_rejected=" << metrics.rejectedSyncs
            << "stored_anchors=" << metrics.storedSyncAnchors
            << "observations=" << metrics.observationInputs
            << "accepted=" << metrics.acceptedObservations
            << "unanchored=" << metrics.unanchoredObservations
            << "non_pixel=" << metrics.nonPixelObservations
            << "out_of_line=" << metrics.outOfLineObservations
            << "terminal_row_recovery="
            << pipeline.robotSession->config().allowTerminalRowRecovery;
    }
    const bool usablePartial =
        pipeline.robotSession->imageFrame().coverage() > 0.0;
    if (sessionState == SstvRobotRxSessionState::Complete) {
        pipeline.holdOffNativeAcquisition(
            pipeline.robotSession->imageEndSample());
        const std::uint64_t lines =
            pipeline.robotSession->decoderMetrics().linesPublished;
        pipeline.stateMachine->dispatch(
            eventMs,
            SstvRxFrameCompleted {
                static_cast<std::uint32_t>(std::min<std::uint64_t>(
                    lines,
                    std::numeric_limits<std::uint32_t>::max()))});
    } else if (sessionState == SstvRobotRxSessionState::Partial) {
        if (pipeline.robotSession->imageFrame().coverage() >= 0.75) {
            pipeline.holdOffNativeAcquisition(
                pipeline.robotSession->imageEndSample());
        }
        pipeline.stateMachine->dispatch(
            eventMs, SstvRxInputEnded {usablePartial});
    } else {
        pipeline.stateMachine->dispatch(eventMs, SstvRxCancel {});
    }
    publishRobotImage(pipeline, true);
    pipeline.robotSession.reset();
}

void SstvRxRuntime::terminateRobotForDiscontinuity(
    WorkerPipeline& pipeline,
    std::uint64_t eventMs)
{
    if (!pipeline.robotSession) {
        return;
    }
    const std::uint64_t nextSample = pipeline.hasFrequencySample
        ? saturatingUnsignedAdd(pipeline.lastFrequencySample, 1U)
        : pipeline.robotSession->imageStartSample();
    const SstvRobotRxSessionState sessionState =
        pipeline.robotSession->notifyDiscontinuity(nextSample);
    const bool usablePartial =
        pipeline.robotSession->imageFrame().coverage() > 0.0;
    if (sessionState == SstvRobotRxSessionState::Complete) {
        const std::uint64_t lines =
            pipeline.robotSession->decoderMetrics().linesPublished;
        pipeline.stateMachine->dispatch(
            eventMs,
            SstvRxFrameCompleted {
                static_cast<std::uint32_t>(std::min<std::uint64_t>(
                    lines,
                    std::numeric_limits<std::uint32_t>::max()))});
    } else {
        pipeline.stateMachine->dispatch(
            eventMs, SstvRxInputEnded {usablePartial});
    }
    publishRobotImage(pipeline, true);
    pipeline.robotSession.reset();
}

void SstvRxRuntime::publishRobotImage(WorkerPipeline& pipeline, bool force)
{
    if (!pipeline.robotSession) {
        return;
    }
    auto image = std::make_shared<SstvImageSnapshot>(
        pipeline.robotSession->snapshot());
    const SstvRobotRxSessionState sessionState =
        pipeline.robotSession->state();
    const SstvRobotModeSpec spec = SstvRobotProtocol::spec(
        pipeline.robotSession->mode());

    ImageSummary summary;
    summary.available = true;
    summary.complete = sessionState == SstvRobotRxSessionState::Complete
        || image->isComplete();
    summary.partial = sessionState == SstvRobotRxSessionState::Partial;
    summary.cancelled = sessionState == SstvRobotRxSessionState::Cancelled
        || image->cancelled;
    summary.acquisitionId =
        pipeline.stateMachine->metrics().currentSessionId;
    summary.generation = pipeline.generation;
    summary.revision = image->revision;
    summary.linesPublished =
        pipeline.robotSession->decoderMetrics().linesPublished;
    summary.width = image->width;
    summary.height = image->height;
    summary.coveredComponents = image->coveredComponents;
    summary.completedPixels = image->completedPixels;
    summary.coverage = image->coverage();
    summary.mode = QString::fromLatin1(spec.stableId);
    recordProgressiveUpdate(pipeline);
    if (sessionState != SstvRobotRxSessionState::Receiving) {
        const std::uint64_t retainedEnd = summary.complete
            ? pipeline.robotSession->imageEndSample()
            : (pipeline.hasFrequencySample
                  ? saturatingUnsignedAdd(pipeline.lastFrequencySample, 1U)
                  : saturatingUnsignedAdd(
                        pipeline.robotSession->imageStartSample(), 1U));
        closeRetainedAcquisition(
            pipeline, retainedEnd, summary.complete, spec.stableId);
    }
    {
        const std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_snapshot.image = std::move(summary);
        m_latestImageSnapshot = std::move(image);
    }
    scheduleSnapshotNotification(force);
}

void SstvRxRuntime::consumeSequentialRgbObservations(
    WorkerPipeline& pipeline,
    const std::vector<SstvFrequencyObservation>& observations,
    std::uint64_t eventMs)
{
    if (!pipeline.sequentialRgbSession || observations.empty()) {
        return;
    }
    bool imageChanged = false;
    std::size_t offset = 0U;
    while (offset < observations.size()
           && pipeline.sequentialRgbSession->state()
               == SstvSequentialRgbRxSessionState::Receiving) {
        const std::size_t count = std::min(
            observations.size() - offset,
            SstvSequentialRgbRxSession::MaximumObservationsPerConsume);
        const SstvSequentialRgbRxSessionUpdate update =
            pipeline.sequentialRgbSession->consume(
                observations.data() + offset, count);
        offset += count;
        imageChanged = imageChanged || update.imageChanged;
        if (update.linesPublished > pipeline.sequentialRgbLinesReported) {
            const std::uint64_t delta = update.linesPublished
                - pipeline.sequentialRgbLinesReported;
            pipeline.stateMachine->dispatch(
                eventMs,
                SstvRxLineObservation {
                    update.observedLineSyncs > 0U,
                    static_cast<std::uint32_t>(std::min<std::uint64_t>(
                        delta,
                        std::numeric_limits<std::uint32_t>::max()))});
            pipeline.sequentialRgbLinesReported = update.linesPublished;
        }
    }

    const SstvSequentialRgbRxSessionState sessionState =
        pipeline.sequentialRgbSession->state();
    if (sessionState == SstvSequentialRgbRxSessionState::Receiving) {
        if (imageChanged) {
            publishSequentialRgbImage(pipeline, false);
        }
        return;
    }
    const bool usablePartial =
        pipeline.sequentialRgbSession->imageFrame().coverage() > 0.0;
    if (sessionState == SstvSequentialRgbRxSessionState::Complete) {
        const std::uint64_t lines =
            pipeline.sequentialRgbSession->decoderMetrics().linesPublished;
        pipeline.stateMachine->dispatch(
            eventMs,
            SstvRxFrameCompleted {
                static_cast<std::uint32_t>(std::min<std::uint64_t>(
                    lines,
                    std::numeric_limits<std::uint32_t>::max()))});
    } else if (sessionState == SstvSequentialRgbRxSessionState::Partial) {
        pipeline.stateMachine->dispatch(
            eventMs, SstvRxInputEnded {usablePartial});
    } else {
        pipeline.stateMachine->dispatch(eventMs, SstvRxCancel {});
    }
    publishSequentialRgbImage(pipeline, true);
    pipeline.sequentialRgbSession.reset();
}

void SstvRxRuntime::terminateSequentialRgbForDiscontinuity(
    WorkerPipeline& pipeline,
    std::uint64_t eventMs)
{
    if (!pipeline.sequentialRgbSession) {
        return;
    }
    const std::uint64_t nextSample = pipeline.hasFrequencySample
        ? saturatingUnsignedAdd(pipeline.lastFrequencySample, 1U)
        : pipeline.sequentialRgbSession->imageStartSample();
    const SstvSequentialRgbRxSessionState sessionState =
        pipeline.sequentialRgbSession->notifyDiscontinuity(nextSample);
    const bool usablePartial =
        pipeline.sequentialRgbSession->imageFrame().coverage() > 0.0;
    if (sessionState == SstvSequentialRgbRxSessionState::Complete) {
        const std::uint64_t lines =
            pipeline.sequentialRgbSession->decoderMetrics().linesPublished;
        pipeline.stateMachine->dispatch(
            eventMs,
            SstvRxFrameCompleted {
                static_cast<std::uint32_t>(std::min<std::uint64_t>(
                    lines,
                    std::numeric_limits<std::uint32_t>::max()))});
    } else {
        pipeline.stateMachine->dispatch(
            eventMs, SstvRxInputEnded {usablePartial});
    }
    publishSequentialRgbImage(pipeline, true);
    pipeline.sequentialRgbSession.reset();
}

void SstvRxRuntime::publishSequentialRgbImage(WorkerPipeline& pipeline,
                                               bool force)
{
    if (!pipeline.sequentialRgbSession) {
        return;
    }
    auto image = std::make_shared<SstvImageSnapshot>(
        pipeline.sequentialRgbSession->snapshot());
    const SstvSequentialRgbRxSessionState sessionState =
        pipeline.sequentialRgbSession->state();
    const SstvSequentialRgbModeSpec spec = SstvSequentialRgbProtocol::spec(
        pipeline.sequentialRgbSession->mode());

    ImageSummary summary;
    summary.available = true;
    summary.complete =
        sessionState == SstvSequentialRgbRxSessionState::Complete
        || image->isComplete();
    summary.partial =
        sessionState == SstvSequentialRgbRxSessionState::Partial;
    summary.cancelled =
        sessionState == SstvSequentialRgbRxSessionState::Cancelled
        || image->cancelled;
    summary.acquisitionId =
        pipeline.stateMachine->metrics().currentSessionId;
    summary.generation = pipeline.generation;
    summary.revision = image->revision;
    summary.linesPublished =
        pipeline.sequentialRgbSession->decoderMetrics().linesPublished;
    summary.width = image->width;
    summary.height = image->height;
    summary.coveredComponents = image->coveredComponents;
    summary.completedPixels = image->completedPixels;
    summary.coverage = image->coverage();
    summary.mode = QString::fromLatin1(spec.stableId);
    recordProgressiveUpdate(pipeline);
    if (sessionState != SstvSequentialRgbRxSessionState::Receiving) {
        const std::uint64_t retainedEnd = summary.complete
            ? pipeline.sequentialRgbSession->imageEndSample()
            : (pipeline.hasFrequencySample
                  ? saturatingUnsignedAdd(pipeline.lastFrequencySample, 1U)
                  : saturatingUnsignedAdd(
                        pipeline.sequentialRgbSession->imageStartSample(),
                        1U));
        closeRetainedAcquisition(
            pipeline, retainedEnd, summary.complete, spec.stableId);
    }
    {
        const std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_snapshot.image = std::move(summary);
        m_latestImageSnapshot = std::move(image);
    }
    scheduleSnapshotNotification(force);
}

void SstvRxRuntime::consumePdObservations(
    WorkerPipeline& pipeline,
    const std::vector<SstvFrequencyObservation>& observations,
    std::uint64_t eventMs)
{
    if (!pipeline.pdSession || observations.empty()) {
        return;
    }
    bool imageChanged = false;
    std::size_t offset = 0U;
    while (offset < observations.size()
           && pipeline.pdSession->state()
               == SstvPdRxSessionState::Receiving) {
        const std::size_t count = std::min(
            observations.size() - offset,
            SstvPdRxSession::MaximumObservationsPerConsume);
        const SstvPdRxSessionUpdate update = pipeline.pdSession->consume(
            observations.data() + offset, count);
        offset += count;
        imageChanged = imageChanged || update.imageChanged;
        if (update.linesPublished > pipeline.pdLinesReported) {
            const std::uint64_t delta =
                update.linesPublished - pipeline.pdLinesReported;
            pipeline.stateMachine->dispatch(
                eventMs,
                SstvRxLineObservation {
                    update.observedPairSyncs > 0U,
                    static_cast<std::uint32_t>(std::min<std::uint64_t>(
                        delta,
                        std::numeric_limits<std::uint32_t>::max()))});
            pipeline.pdLinesReported = update.linesPublished;
        }
    }

    const SstvPdRxSessionState sessionState = pipeline.pdSession->state();
    if (sessionState == SstvPdRxSessionState::Receiving) {
        if (imageChanged) {
            publishPdImage(pipeline, false);
        }
        return;
    }
    const bool usablePartial = pipeline.pdSession->imageFrame().coverage() > 0.0;
    if (sessionState == SstvPdRxSessionState::Complete) {
        const std::uint64_t lines =
            pipeline.pdSession->decoderMetrics().linesPublished;
        pipeline.stateMachine->dispatch(
            eventMs,
            SstvRxFrameCompleted {
                static_cast<std::uint32_t>(std::min<std::uint64_t>(
                    lines,
                    std::numeric_limits<std::uint32_t>::max()))});
    } else if (sessionState == SstvPdRxSessionState::Partial) {
        pipeline.stateMachine->dispatch(
            eventMs, SstvRxInputEnded {usablePartial});
    } else {
        pipeline.stateMachine->dispatch(eventMs, SstvRxCancel {});
    }
    publishPdImage(pipeline, true);
    pipeline.pdSession.reset();
}

void SstvRxRuntime::terminatePdForDiscontinuity(
    WorkerPipeline& pipeline,
    std::uint64_t eventMs)
{
    if (!pipeline.pdSession) {
        return;
    }
    const std::uint64_t nextSample = pipeline.hasFrequencySample
        ? saturatingUnsignedAdd(pipeline.lastFrequencySample, 1U)
        : pipeline.pdSession->imageStartSample();
    const SstvPdRxSessionState sessionState =
        pipeline.pdSession->notifyDiscontinuity(nextSample);
    const bool usablePartial = pipeline.pdSession->imageFrame().coverage() > 0.0;
    if (sessionState == SstvPdRxSessionState::Complete) {
        const std::uint64_t lines =
            pipeline.pdSession->decoderMetrics().linesPublished;
        pipeline.stateMachine->dispatch(
            eventMs,
            SstvRxFrameCompleted {
                static_cast<std::uint32_t>(std::min<std::uint64_t>(
                    lines,
                    std::numeric_limits<std::uint32_t>::max()))});
    } else {
        pipeline.stateMachine->dispatch(
            eventMs, SstvRxInputEnded {usablePartial});
    }
    publishPdImage(pipeline, true);
    pipeline.pdSession.reset();
}

void SstvRxRuntime::publishPdImage(WorkerPipeline& pipeline, bool force)
{
    if (!pipeline.pdSession) {
        return;
    }
    auto image = std::make_shared<SstvImageSnapshot>(
        pipeline.pdSession->snapshot());
    const SstvPdRxSessionState sessionState = pipeline.pdSession->state();
    const SstvPdModeSpec spec = SstvPdProtocol::spec(
        pipeline.pdSession->mode());

    ImageSummary summary;
    summary.available = true;
    summary.complete = sessionState == SstvPdRxSessionState::Complete
        || image->isComplete();
    summary.partial = sessionState == SstvPdRxSessionState::Partial;
    summary.cancelled = sessionState == SstvPdRxSessionState::Cancelled
        || image->cancelled;
    summary.acquisitionId =
        pipeline.stateMachine->metrics().currentSessionId;
    summary.generation = pipeline.generation;
    summary.revision = image->revision;
    summary.linesPublished =
        pipeline.pdSession->decoderMetrics().linesPublished;
    summary.width = image->width;
    summary.height = image->height;
    summary.coveredComponents = image->coveredComponents;
    summary.completedPixels = image->completedPixels;
    summary.coverage = image->coverage();
    summary.mode = QString::fromLatin1(spec.stableId);
    recordProgressiveUpdate(pipeline);
    if (sessionState != SstvPdRxSessionState::Receiving) {
        const std::uint64_t retainedEnd = summary.complete
            ? pipeline.pdSession->imageEndSample()
            : (pipeline.hasFrequencySample
                  ? saturatingUnsignedAdd(pipeline.lastFrequencySample, 1U)
                  : saturatingUnsignedAdd(
                        pipeline.pdSession->imageStartSample(), 1U));
        closeRetainedAcquisition(
            pipeline, retainedEnd, summary.complete, spec.stableId);
    }
    {
        const std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_snapshot.image = std::move(summary);
        m_latestImageSnapshot = std::move(image);
    }
    scheduleSnapshotNotification(force);
}

void SstvRxRuntime::consumeAvtObservations(
    WorkerPipeline& pipeline,
    const std::vector<SstvFrequencyObservation>& observations,
    std::uint64_t eventMs)
{
    if (!pipeline.avtSession || observations.empty()) {
        return;
    }
    bool imageChanged = false;
    std::size_t offset = 0U;
    while (offset < observations.size()
           && pipeline.avtSession->state()
               == SstvAvtRxSessionState::Receiving) {
        const std::size_t count = std::min(
            observations.size() - offset,
            SstvAvtRxSession::MaximumObservationsPerConsume);
        const SstvAvtRxSessionUpdate update = pipeline.avtSession->consume(
            observations.data() + offset, count);
        offset += count;
        imageChanged = imageChanged || update.imageChanged;
        if (update.linesPublished > pipeline.avtLinesReported) {
            const std::uint64_t delta =
                update.linesPublished - pipeline.avtLinesReported;
            // AVT has no physical line-sync pulse. This trusted progression
            // flag tells the generic lifecycle state machine that the
            // countdown-anchored absolute mapper delivered complete rows; it
            // does not alter the sync-free protocol metadata or waveform.
            pipeline.stateMachine->dispatch(
                eventMs,
                SstvRxLineObservation {
                    true,
                    static_cast<std::uint32_t>(std::min<std::uint64_t>(
                        delta,
                        std::numeric_limits<std::uint32_t>::max()))});
            pipeline.avtLinesReported = update.linesPublished;
        }
    }

    const SstvAvtRxSessionState sessionState =
        pipeline.avtSession->state();
    if (sessionState == SstvAvtRxSessionState::Receiving) {
        if (imageChanged) {
            publishAvtImage(pipeline, false);
        }
        return;
    }
    const bool usablePartial =
        pipeline.avtSession->imageFrame().coverage() > 0.0;
    if (sessionState == SstvAvtRxSessionState::Complete) {
        const std::uint64_t lines =
            pipeline.avtSession->decoderMetrics().linesPublished;
        pipeline.stateMachine->dispatch(
            eventMs,
            SstvRxFrameCompleted {
                static_cast<std::uint32_t>(std::min<std::uint64_t>(
                    lines,
                    std::numeric_limits<std::uint32_t>::max()))});
    } else if (sessionState == SstvAvtRxSessionState::Partial) {
        pipeline.stateMachine->dispatch(
            eventMs, SstvRxInputEnded {usablePartial});
    } else {
        pipeline.stateMachine->dispatch(eventMs, SstvRxCancel {});
    }
    publishAvtImage(pipeline, true);
    pipeline.avtSession.reset();
}

void SstvRxRuntime::terminateAvtForDiscontinuity(
    WorkerPipeline& pipeline,
    std::uint64_t eventMs)
{
    if (pipeline.avtCountdownDetector) {
        pipeline.avtCountdownDetector->cancel();
        pipeline.avtCountdownDetector.reset();
        if (pipeline.stateMachine->state() == SstvRxState::WaitingForSync) {
            pipeline.stateMachine->dispatch(eventMs, SstvRxCancel {});
        }
    }
    if (!pipeline.avtSession) {
        return;
    }
    const std::uint64_t nextSample = pipeline.hasFrequencySample
        ? saturatingUnsignedAdd(pipeline.lastFrequencySample, 1U)
        : pipeline.avtSession->imageStartSample();
    const SstvAvtRxSessionState sessionState =
        pipeline.avtSession->notifyDiscontinuity(nextSample);
    const bool usablePartial =
        pipeline.avtSession->imageFrame().coverage() > 0.0;
    if (sessionState == SstvAvtRxSessionState::Complete) {
        const std::uint64_t lines =
            pipeline.avtSession->decoderMetrics().linesPublished;
        pipeline.stateMachine->dispatch(
            eventMs,
            SstvRxFrameCompleted {
                static_cast<std::uint32_t>(std::min<std::uint64_t>(
                    lines,
                    std::numeric_limits<std::uint32_t>::max()))});
    } else {
        pipeline.stateMachine->dispatch(
            eventMs, SstvRxInputEnded {usablePartial});
    }
    publishAvtImage(pipeline, true);
    pipeline.avtSession.reset();
}

void SstvRxRuntime::publishAvtImage(WorkerPipeline& pipeline, bool force)
{
    if (!pipeline.avtSession) {
        return;
    }
    auto image = std::make_shared<SstvImageSnapshot>(
        pipeline.avtSession->snapshot());
    const SstvAvtRxSessionState sessionState =
        pipeline.avtSession->state();
    const SstvAvtModeSpec spec = SstvAvtProtocol::spec(
        pipeline.avtSession->mode());

    ImageSummary summary;
    summary.available = true;
    summary.complete = sessionState == SstvAvtRxSessionState::Complete
        || image->isComplete();
    summary.partial = sessionState == SstvAvtRxSessionState::Partial;
    summary.cancelled = sessionState == SstvAvtRxSessionState::Cancelled
        || image->cancelled;
    summary.acquisitionId =
        pipeline.stateMachine->metrics().currentSessionId;
    summary.generation = pipeline.generation;
    summary.revision = image->revision;
    summary.linesPublished =
        pipeline.avtSession->decoderMetrics().linesPublished;
    summary.width = image->width;
    summary.height = image->height;
    summary.coveredComponents = image->coveredComponents;
    summary.completedPixels = image->completedPixels;
    summary.coverage = image->coverage();
    summary.mode = QString::fromLatin1(spec.stableId);
    recordProgressiveUpdate(pipeline);
    if (sessionState != SstvAvtRxSessionState::Receiving) {
        const std::uint64_t retainedEnd = summary.complete
            ? pipeline.avtSession->imageEndSample()
            : (pipeline.hasFrequencySample
                  ? saturatingUnsignedAdd(pipeline.lastFrequencySample, 1U)
                  : saturatingUnsignedAdd(
                        pipeline.avtSession->imageStartSample(), 1U));
        closeRetainedAcquisition(
            pipeline, retainedEnd, summary.complete, spec.stableId);
    }
    {
        const std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_snapshot.image = std::move(summary);
        m_latestImageSnapshot = std::move(image);
    }
    scheduleSnapshotNotification(force);
}

void SstvRxRuntime::consumeMmsstvObservations(
    WorkerPipeline& pipeline,
    const std::vector<SstvFrequencyObservation>& observations,
    std::uint64_t eventMs)
{
    if (!pipeline.mmsstvSession || observations.empty()) {
        return;
    }
    bool imageChanged = false;
    std::size_t offset = 0U;
    while (offset < observations.size()
           && pipeline.mmsstvSession->state()
               == SstvMmsstvRxSessionState::Receiving) {
        const std::size_t count = std::min(
            observations.size() - offset,
            SstvMmsstvRxSession::MaximumObservationsPerConsume);
        const SstvMmsstvRxSessionUpdate update =
            pipeline.mmsstvSession->consume(
                observations.data() + offset, count);
        offset += count;
        imageChanged = imageChanged || update.imageChanged;
        if (update.linesPublished > pipeline.mmsstvLinesReported) {
            const std::uint64_t delta = update.linesPublished
                - pipeline.mmsstvLinesReported;
            pipeline.stateMachine->dispatch(
                eventMs,
                SstvRxLineObservation {
                    update.observedScanSyncs > 0U,
                    static_cast<std::uint32_t>(std::min<std::uint64_t>(
                        delta,
                        std::numeric_limits<std::uint32_t>::max()))});
            pipeline.mmsstvLinesReported = update.linesPublished;
        }
    }

    const SstvMmsstvRxSessionState sessionState =
        pipeline.mmsstvSession->state();
    if (sessionState == SstvMmsstvRxSessionState::Receiving) {
        if (imageChanged) {
            publishMmsstvImage(pipeline, false);
        }
        return;
    }
    const bool usablePartial =
        pipeline.mmsstvSession->imageFrame().coverage() > 0.0;
    if (sessionState == SstvMmsstvRxSessionState::Complete) {
        const std::uint64_t lines =
            pipeline.mmsstvSession->decoderMetrics().linesPublished;
        pipeline.stateMachine->dispatch(
            eventMs,
            SstvRxFrameCompleted {
                static_cast<std::uint32_t>(std::min<std::uint64_t>(
                    lines,
                    std::numeric_limits<std::uint32_t>::max()))});
    } else if (sessionState == SstvMmsstvRxSessionState::Partial) {
        pipeline.stateMachine->dispatch(
            eventMs, SstvRxInputEnded {usablePartial});
    } else {
        pipeline.stateMachine->dispatch(eventMs, SstvRxCancel {});
    }
    publishMmsstvImage(pipeline, true);
    pipeline.mmsstvSession.reset();
}

void SstvRxRuntime::terminateMmsstvForDiscontinuity(
    WorkerPipeline& pipeline,
    std::uint64_t eventMs)
{
    if (!pipeline.mmsstvSession) {
        return;
    }
    const std::uint64_t nextSample = pipeline.hasFrequencySample
        ? saturatingUnsignedAdd(pipeline.lastFrequencySample, 1U)
        : pipeline.mmsstvSession->imageStartSample();
    const SstvMmsstvRxSessionState sessionState =
        pipeline.mmsstvSession->notifyDiscontinuity(nextSample);
    const bool usablePartial =
        pipeline.mmsstvSession->imageFrame().coverage() > 0.0;
    if (sessionState == SstvMmsstvRxSessionState::Complete) {
        const std::uint64_t lines =
            pipeline.mmsstvSession->decoderMetrics().linesPublished;
        pipeline.stateMachine->dispatch(
            eventMs,
            SstvRxFrameCompleted {
                static_cast<std::uint32_t>(std::min<std::uint64_t>(
                    lines,
                    std::numeric_limits<std::uint32_t>::max()))});
    } else {
        pipeline.stateMachine->dispatch(
            eventMs, SstvRxInputEnded {usablePartial});
    }
    publishMmsstvImage(pipeline, true);
    pipeline.mmsstvSession.reset();
}

void SstvRxRuntime::publishMmsstvImage(WorkerPipeline& pipeline, bool force)
{
    if (!pipeline.mmsstvSession) {
        return;
    }
    auto image = std::make_shared<SstvImageSnapshot>(
        pipeline.mmsstvSession->snapshot());
    const SstvMmsstvRxSessionState sessionState =
        pipeline.mmsstvSession->state();
    const SstvMmsstvModeSpec spec = SstvMmsstvProtocol::spec(
        pipeline.mmsstvSession->mode());

    ImageSummary summary;
    summary.available = true;
    summary.complete = sessionState == SstvMmsstvRxSessionState::Complete
        || image->isComplete();
    summary.partial = sessionState == SstvMmsstvRxSessionState::Partial;
    summary.cancelled = sessionState == SstvMmsstvRxSessionState::Cancelled
        || image->cancelled;
    summary.acquisitionId =
        pipeline.stateMachine->metrics().currentSessionId;
    summary.generation = pipeline.generation;
    summary.revision = image->revision;
    summary.linesPublished =
        pipeline.mmsstvSession->decoderMetrics().linesPublished;
    summary.width = image->width;
    summary.height = image->height;
    summary.coveredComponents = image->coveredComponents;
    summary.completedPixels = image->completedPixels;
    summary.coverage = image->coverage();
    summary.mode = QString::fromLatin1(spec.stableId);
    recordProgressiveUpdate(pipeline);
    if (sessionState != SstvMmsstvRxSessionState::Receiving) {
        const std::uint64_t retainedEnd = summary.complete
            ? pipeline.mmsstvSession->imageEndSample()
            : (pipeline.hasFrequencySample
                  ? saturatingUnsignedAdd(pipeline.lastFrequencySample, 1U)
                  : saturatingUnsignedAdd(
                        pipeline.mmsstvSession->imageStartSample(), 1U));
        closeRetainedAcquisition(
            pipeline, retainedEnd, summary.complete, spec.stableId);
    }
    {
        const std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_snapshot.image = std::move(summary);
        m_latestImageSnapshot = std::move(image);
    }
    scheduleSnapshotNotification(force);
}

void SstvRxRuntime::deliverVisNotification() noexcept
{
    std::uint64_t generation = 0U;
    VisSummary summary;
    {
        const std::lock_guard<std::mutex> lock(m_notificationMutex);
        if (!m_visNotificationPending) {
            return;
        }
        generation = m_pendingVisGeneration;
        summary = m_pendingVis;
        m_visNotificationPending = false;
    }

    // A queued delivery may outlive switch/reset/cancel/stop.  Generation and
    // active state are checked at the final owner-thread boundary, so no old
    // acquisition can publish after its lifecycle transition returns.
    const SstvRxRouteToken route = routeToken();
    if (state() != State::Running || route.generation != generation) {
        return;
    }

    Q_EMIT visDetectionAvailable(
        generation,
        static_cast<int>(summary.status),
        static_cast<int>(summary.cause),
        static_cast<int>(summary.format),
        summary.primaryPayload,
        summary.extensionPayload,
        summary.confidence,
        summary.mappedMode);
}

void SstvRxRuntime::recordWorkerFailure(const QString& detail) noexcept
{
    const QString bounded = detail.left(
        static_cast<qsizetype>(m_config.maximumErrorCharacters));
    std::uint64_t failureCount = 0U;
    {
        const std::lock_guard<std::mutex> lock(m_snapshotMutex);
        saturatingAdd(m_snapshot.processingFailures);
        failureCount = m_snapshot.processingFailures;
        m_snapshot.lastError = bounded;
        m_snapshot.rxState = SstvRxState::Error;
        m_snapshot.rxCause = SstvRxCause::Failure;
    }
    try {
        recordSstvDiagnosticEvent(
            sstvRxLog(), QtCriticalMsg,
            QStringLiteral("rx.worker-error"),
            {{QStringLiteral("errorCode"),
              QStringLiteral("dsp-worker-failure")},
             {QStringLiteral("count"),
              static_cast<qulonglong>(failureCount)},
             {QStringLiteral("success"), false}});
    } catch (...) {
        // Never let diagnostic allocation interfere with fatal RX teardown.
    }
    m_state.store(State::Error, std::memory_order_release);
    m_stopRequested.store(true, std::memory_order_release);
    scheduleRuntimeStateNotification(State::Error,
                                     m_ingress->generation());
    bool shouldPost = false;
    {
        const std::lock_guard<std::mutex> lock(m_notificationMutex);
        m_pendingWorkerError = bounded;
        if (!m_workerErrorNotificationPending) {
            m_workerErrorNotificationPending = true;
            shouldPost = true;
        }
    }
    if (shouldPost
        && !QMetaObject::invokeMethod(
            this,
            "deliverWorkerErrorNotification",
            Qt::QueuedConnection)) {
        const std::lock_guard<std::mutex> lock(m_notificationMutex);
        m_workerErrorNotificationPending = false;
    }
    scheduleSnapshotNotification(true);
}

void SstvRxRuntime::deliverWorkerErrorNotification() noexcept
{
    QString detail;
    {
        const std::lock_guard<std::mutex> lock(m_notificationMutex);
        if (!m_workerErrorNotificationPending) {
            return;
        }
        detail = m_pendingWorkerError;
        m_workerErrorNotificationPending = false;
    }
    // shutdown()/stop() supersedes a queued fatal worker report.
    if (state() == State::Error) {
        Q_EMIT workerError(detail);
    }
}

void SstvRxRuntime::scheduleSnapshotNotification(bool force) noexcept
{
    m_revision.fetch_add(1U, std::memory_order_acq_rel);
    if (force) {
        m_snapshotForcePending.store(true, std::memory_order_release);
    }

    bool expected = false;
    if (!m_snapshotNotificationPending.compare_exchange_strong(
            expected,
            true,
            std::memory_order_acq_rel,
            std::memory_order_relaxed)) {
        return;
    }
    if (!QMetaObject::invokeMethod(
            this,
            "deliverSnapshotNotification",
            Qt::QueuedConnection)) {
        m_snapshotNotificationPending.store(false,
                                            std::memory_order_release);
    }
}

void SstvRxRuntime::deliverSnapshotNotification() noexcept
{
    if (!m_snapshotNotificationPending.load(std::memory_order_acquire)) {
        return;
    }

    const qint64 nowNs = localMonotonicNowNs();
    const qint64 intervalNs = static_cast<qint64>(
        m_config.snapshotNotificationIntervalMs)
        * 1'000'000LL;
    const qint64 last =
        m_lastSnapshotNotificationNs.load(std::memory_order_relaxed);
    const bool force =
        m_snapshotForcePending.exchange(false, std::memory_order_acq_rel);
    if (!force && last > 0 && nowNs >= last
        && nowNs - last < intervalNs) {
        const qint64 remainingNs = intervalNs - (nowNs - last);
        const qint64 remainingMs = std::max<qint64>(
            1,
            (remainingNs + 999'999LL) / 1'000'000LL);
        QTimer::singleShot(static_cast<int>(remainingMs),
                           this,
                           &SstvRxRuntime::deliverSnapshotNotification);
        return;
    }

    m_lastSnapshotNotificationNs.store(nowNs, std::memory_order_relaxed);
    // Clear pending before loading the delivered revision.  An update racing
    // before this store is included in the loaded revision; an update racing
    // after it posts the next bounded delivery.  No trailing edge is lost.
    m_snapshotNotificationPending.store(false, std::memory_order_release);
    const std::uint64_t revision =
        m_revision.load(std::memory_order_acquire);
    Q_EMIT snapshotAvailable(revision);
}

} // namespace decodium::sstv
