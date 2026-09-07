// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvTimingFallbackDetector.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

#if defined(__GNUC__) && !defined(__clang__)
// GCC 13's libstdc++ optional move diagnostics can report a false positive
// for the value-initialized optional<string> member in SstvFallbackResult.
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

namespace decodium::sstv {

SstvTimingFallbackDetector::SstvTimingFallbackDetector(
    const SstvModeRegistry& registry)
    : SstvTimingFallbackDetector(registry, Config {})
{
}

SstvTimingFallbackDetector::SstvTimingFallbackDetector(
    const SstvModeRegistry& registry,
    Config config)
    : m_config(validate(config))
    , m_signatures(makeSignatures(registry, m_config))
{
    m_pulses.reserve(m_config.maximumPulses);
}

void SstvTimingFallbackDetector::setEnabled(bool enabled) noexcept
{
    m_enabled = enabled;
}

bool SstvTimingFallbackDetector::setLockedMode(
    std::optional<std::string> mode)
{
    if (mode && !validModeName(*mode)) {
        return false;
    }
    m_lockedMode = std::move(mode);
    return true;
}

SstvFallbackResult SstvTimingFallbackDetector::consume(
    const SstvFallbackSyncPulse& pulse)
{
    increment(m_metrics.pulsesConsumed);
    if (!m_enabled) {
        SstvFallbackResult result = evaluateUnlocked();
        result.status = SstvFallbackStatus::Disabled;
        result.selectedMode.reset();
        return result;
    }
    if (pulse.predicted) {
        increment(m_metrics.predictedPulsesIgnored);
        return evaluateUnlocked();
    }
    if (pulse.endSample <= pulse.startSample
        || !std::isfinite(pulse.measuredFrequencyHz)
        || pulse.measuredFrequencyHz <= 0.0
        || !finiteUnit(pulse.confidence)) {
        increment(m_metrics.invalidPulses);
        SstvFallbackResult result = evaluateUnlocked();
        result.status = SstvFallbackStatus::InvalidInput;
        result.selectedMode.reset();
        return result;
    }
    if (pulse.confidence < m_config.minimumPulseConfidence) {
        increment(m_metrics.invalidPulses);
        return evaluateUnlocked();
    }
    if (!m_pulses.empty()
        && pulse.startSample <= m_pulses.back().startSample) {
        increment(m_metrics.nonMonotonicPulses);
        SstvFallbackResult result = evaluateUnlocked();
        result.status = SstvFallbackStatus::NonMonotonic;
        result.selectedMode.reset();
        return result;
    }

    if (m_pulses.size() == m_config.maximumPulses) {
        m_pulses.erase(m_pulses.begin());
    }
    m_pulses.push_back(pulse);
    increment(m_metrics.pulsesAccepted);
    m_metrics.peakRetainedPulses = std::max(
        m_metrics.peakRetainedPulses, m_pulses.size());
    increment(m_metrics.evaluations);

    SstvFallbackResult result = evaluateUnlocked();
    if (result.status == SstvFallbackStatus::Unique) {
        increment(m_metrics.uniqueSelections);
    } else if (result.status == SstvFallbackStatus::Ambiguous) {
        increment(m_metrics.ambiguousSelections);
    } else if (result.status == SstvFallbackStatus::NoMatch) {
        increment(m_metrics.noMatches);
    }
    result.metrics = m_metrics;
    return result;
}

SstvFallbackResult SstvTimingFallbackDetector::evaluate() const
{
    return evaluateUnlocked();
}

void SstvTimingFallbackDetector::reset() noexcept
{
    m_pulses.clear();
    m_metrics = {};
}

std::size_t SstvTimingFallbackDetector::candidateSignatureCount() const
    noexcept
{
    return m_signatures.size();
}

const SstvTimingFallbackDetector::Config&
SstvTimingFallbackDetector::config() const noexcept
{
    return m_config;
}

SstvTimingFallbackDetector::Config SstvTimingFallbackDetector::validate(
    Config config)
{
    if (config.sampleRate < 1'000U || config.sampleRate > 192'000U
        || config.minimumPulses < 3U
        || config.maximumPulses < config.minimumPulses
        || config.maximumPulses > 128U
        || config.maximumReportedCandidates == 0U
        || config.maximumReportedCandidates > 32U
        || !finiteUnit(config.minimumPulseConfidence)
        || !std::isfinite(config.maximumLineErrorPpm)
        || config.maximumLineErrorPpm <= 0.0
        || config.maximumLineErrorPpm > 100'000.0
        || !std::isfinite(config.maximumSyncDurationErrorFraction)
        || config.maximumSyncDurationErrorFraction <= 0.0
        || config.maximumSyncDurationErrorFraction > 1.0
        || !std::isfinite(config.minimumSyncDurationToleranceSamples)
        || config.minimumSyncDurationToleranceSamples <= 0.0
        || !std::isfinite(config.maximumSyncFrequencyErrorHz)
        || config.maximumSyncFrequencyErrorHz <= 0.0
        || config.maximumSyncFrequencyErrorHz > 500.0
        || !std::isfinite(config.ambiguityScoreMargin)
        || config.ambiguityScoreMargin < 0.0
        || config.ambiguityScoreMargin > 1.0) {
        throw std::invalid_argument("invalid SSTV timing fallback config");
    }
    return config;
}

std::vector<SstvTimingFallbackDetector::Signature>
SstvTimingFallbackDetector::makeSignatures(
    const SstvModeRegistry& registry,
    const Config& config)
{
    std::vector<Signature> signatures;
    signatures.reserve(std::min(registry.modes().size(),
                                MaximumCanonicalSignatures));
    for (const SstvModeSpec& mode : registry.modes()) {
        if (mode.classification != ModeClassification::AnalogSstv
            || !mode.claimsRxSupport()
            || !mode.fallbackSignature.nominalLineDuration
            || !mode.fallbackSignature.nominalSyncDuration
            || !mode.fallbackSignature.syncFrequencyHz
            || mode.fallbackSignature.discriminator.empty()) {
            continue;
        }
        if (signatures.size() == MaximumCanonicalSignatures) {
            throw std::length_error(
                "SSTV timing fallback signature hard bound exceeded");
        }
        const double line = durationToSamples(
            *mode.fallbackSignature.nominalLineDuration,
            config.sampleRate);
        const double sync = durationToSamples(
            *mode.fallbackSignature.nominalSyncDuration,
            config.sampleRate);
        const double frequency = static_cast<double>(
            *mode.fallbackSignature.syncFrequencyHz);
        if (!(line > 0.0) || !(sync > 0.0) || !(frequency > 0.0)) {
            continue;
        }
        const double tolerance = mode.timing.tolerancePpm
            ? static_cast<double>(*mode.timing.tolerancePpm)
            : config.maximumLineErrorPpm;
        signatures.push_back(Signature {
            mode.id,
            line,
            sync,
            frequency,
            std::max(1.0, tolerance)});
    }
    return signatures;
}

double SstvTimingFallbackDetector::durationToSamples(
    Picoseconds duration,
    std::uint32_t sampleRate) noexcept
{
    if (duration.count <= 0) {
        return 0.0;
    }
    return static_cast<double>(duration.count)
        * static_cast<double>(sampleRate)
        / static_cast<double>(kPicosecondsPerSecond);
}

double SstvTimingFallbackDetector::median(std::vector<double> values)
{
    const std::size_t middle = values.size() / 2U;
    std::nth_element(values.begin(),
                     values.begin() + static_cast<std::ptrdiff_t>(middle),
                     values.end());
    double result = values[middle];
    if ((values.size() % 2U) == 0U) {
        const auto lower = std::max_element(
            values.begin(),
            values.begin() + static_cast<std::ptrdiff_t>(middle));
        result = (*lower + result) * 0.5;
    }
    return result;
}

bool SstvTimingFallbackDetector::finiteUnit(double value) noexcept
{
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

bool SstvTimingFallbackDetector::validModeName(
    const std::string& mode) noexcept
{
    if (mode.empty() || mode.size() > 64U) {
        return false;
    }
    return std::all_of(mode.begin(), mode.end(), [](char rawCharacter) {
        const auto character = static_cast<unsigned char>(rawCharacter);
        return (character >= 'a' && character <= 'z')
            || (character >= 'A' && character <= 'Z')
            || (character >= '0' && character <= '9')
            || character == '-' || character == '_' || character == '.';
    });
}

void SstvTimingFallbackDetector::increment(std::uint64_t& value) noexcept
{
    if (value != std::numeric_limits<std::uint64_t>::max()) {
        ++value;
    }
}

SstvFallbackResult SstvTimingFallbackDetector::evaluateUnlocked() const
{
    SstvFallbackResult result;
    result.retainedPulses = m_pulses;
    result.metrics = m_metrics;
    if (!m_enabled) {
        result.status = SstvFallbackStatus::Disabled;
        return result;
    }
    if (m_pulses.size() < m_config.minimumPulses) {
        return result;
    }

    std::vector<double> periods;
    std::vector<double> durations;
    std::vector<double> frequencies;
    periods.reserve(m_pulses.size() - 1U);
    durations.reserve(m_pulses.size());
    frequencies.reserve(m_pulses.size());
    double confidenceSum = 0.0;
    for (std::size_t index = 0U; index < m_pulses.size(); ++index) {
        const SstvFallbackSyncPulse& pulse = m_pulses[index];
        durations.push_back(static_cast<double>(pulse.endSample
                                                - pulse.startSample));
        frequencies.push_back(pulse.measuredFrequencyHz);
        confidenceSum += pulse.confidence;
        if (index != 0U) {
            periods.push_back(static_cast<double>(pulse.startSample
                                                  - m_pulses[index - 1U]
                                                        .startSample));
        }
    }
    result.observedLinePeriodSamples = median(std::move(periods));
    result.observedSyncDurationSamples = median(std::move(durations));
    result.observedSyncFrequencyHz = median(std::move(frequencies));
    result.confidence = confidenceSum
        / static_cast<double>(m_pulses.size());

    std::vector<SstvFallbackCandidate> matching;
    matching.reserve(m_signatures.size());
    for (const Signature& signature : m_signatures) {
        if (m_lockedMode && signature.mode != *m_lockedMode) {
            continue;
        }
        const double lineErrorPpm
            = (result.observedLinePeriodSamples - signature.linePeriodSamples)
            / signature.linePeriodSamples * 1'000'000.0;
        const double lineLimit = std::min(
            m_config.maximumLineErrorPpm,
            std::max(300.0, signature.registryTolerancePpm));
        const double syncError = result.observedSyncDurationSamples
            - signature.syncDurationSamples;
        const double syncLimit = std::max(
            m_config.minimumSyncDurationToleranceSamples,
            signature.syncDurationSamples
                * m_config.maximumSyncDurationErrorFraction);
        const double frequencyError = result.observedSyncFrequencyHz
            - signature.syncFrequencyHz;
        if (std::abs(lineErrorPpm) > lineLimit
            || std::abs(syncError) > syncLimit
            || std::abs(frequencyError)
                > m_config.maximumSyncFrequencyErrorHz) {
            continue;
        }
        const double lineScore = std::abs(lineErrorPpm) / lineLimit;
        const double syncScore = std::abs(syncError) / syncLimit;
        const double frequencyScore = std::abs(frequencyError)
            / m_config.maximumSyncFrequencyErrorHz;
        const double score = std::sqrt((lineScore * lineScore
                                        + syncScore * syncScore
                                        + frequencyScore * frequencyScore)
                                       / 3.0);
        matching.push_back(SstvFallbackCandidate {
            signature.mode,
            score,
            result.confidence * std::max(0.0, 1.0 - score),
            lineErrorPpm,
            syncError,
            frequencyError});
    }
    std::sort(matching.begin(), matching.end(), [](const auto& left,
                                                   const auto& right) {
        if (left.score != right.score) {
            return left.score < right.score;
        }
        return left.mode < right.mode;
    });
    if (matching.size() > m_config.maximumReportedCandidates) {
        matching.resize(m_config.maximumReportedCandidates);
    }
    result.candidates = matching;
    if (matching.empty()) {
        result.status = SstvFallbackStatus::NoMatch;
        return result;
    }
    if (matching.size() > 1U
        && matching[1U].score - matching[0U].score
            <= m_config.ambiguityScoreMargin) {
        result.status = SstvFallbackStatus::Ambiguous;
        result.selectedMode.reset();
        return result;
    }
    result.status = SstvFallbackStatus::Unique;
    result.selectedMode = matching.front().mode;
    result.confidence = matching.front().confidence;
    return result;
}

} // namespace decodium::sstv

#if defined(__GNUC__) && !defined(__clang__)
#    pragma GCC diagnostic pop
#endif
