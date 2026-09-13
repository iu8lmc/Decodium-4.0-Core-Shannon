// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../core/SstvModeRegistry.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace decodium::sstv {

struct SstvFallbackSyncPulse final
{
    std::uint64_t startSample {0U};
    std::uint64_t endSample {0U};
    double measuredFrequencyHz {0.0};
    double confidence {0.0};
    bool predicted {false};
};

enum class SstvFallbackStatus : std::uint8_t
{
    InsufficientData,
    Unique,
    Ambiguous,
    NoMatch,
    Disabled,
    InvalidInput,
    NonMonotonic,
};

struct SstvFallbackCandidate final
{
    std::string mode;
    double score {0.0};
    double confidence {0.0};
    double lineErrorPpm {0.0};
    double syncDurationErrorSamples {0.0};
    double syncFrequencyErrorHz {0.0};
};

struct SstvFallbackMetrics final
{
    std::uint64_t pulsesConsumed {0U};
    std::uint64_t pulsesAccepted {0U};
    std::uint64_t predictedPulsesIgnored {0U};
    std::uint64_t invalidPulses {0U};
    std::uint64_t nonMonotonicPulses {0U};
    std::uint64_t evaluations {0U};
    std::uint64_t uniqueSelections {0U};
    std::uint64_t ambiguousSelections {0U};
    std::uint64_t noMatches {0U};
    std::size_t peakRetainedPulses {0U};
};

struct SstvFallbackResult final
{
    SstvFallbackStatus status {SstvFallbackStatus::InsufficientData};
    std::optional<std::string> selectedMode;
    std::vector<SstvFallbackCandidate> candidates;
    std::vector<SstvFallbackSyncPulse> retainedPulses;
    double observedLinePeriodSamples {0.0};
    double observedSyncDurationSamples {0.0};
    double observedSyncFrequencyHz {0.0};
    double confidence {0.0};
    SstvFallbackMetrics metrics;
};

// VIS-less detector based exclusively on the canonical registry fallback
// signatures.  It never guesses through a timing conflict: candidates within
// ambiguityScoreMargin are returned as Ambiguous and selection fails closed.
// Accepted observed pulses are retained in a hard-bounded window so the
// chosen mode session can replay its initial sync anchors instead of losing
// the first image lines while detection was accumulating evidence.
class SstvTimingFallbackDetector final
{
public:
    struct Config final
    {
        std::uint32_t sampleRate {12'000U};
        std::size_t minimumPulses {4U};
        std::size_t maximumPulses {32U};
        std::size_t maximumReportedCandidates {8U};
        double minimumPulseConfidence {0.50};
        double maximumLineErrorPpm {3'000.0};
        double maximumSyncDurationErrorFraction {0.30};
        double minimumSyncDurationToleranceSamples {4.0};
        double maximumSyncFrequencyErrorHz {90.0};
        double ambiguityScoreMargin {0.12};
    };

    explicit SstvTimingFallbackDetector(const SstvModeRegistry& registry);
    SstvTimingFallbackDetector(const SstvModeRegistry& registry,
                               Config config);

    void setEnabled(bool enabled) noexcept;
    bool setLockedMode(std::optional<std::string> mode);
    SstvFallbackResult consume(const SstvFallbackSyncPulse& pulse);
    SstvFallbackResult evaluate() const;
    void reset() noexcept;
    std::size_t candidateSignatureCount() const noexcept;
    const Config& config() const noexcept;

private:
    struct Signature final
    {
        std::string mode;
        double linePeriodSamples {0.0};
        double syncDurationSamples {0.0};
        double syncFrequencyHz {0.0};
        double registryTolerancePpm {0.0};
    };

    static constexpr std::size_t MaximumCanonicalSignatures = 128U;

    static Config validate(Config config);
    static std::vector<Signature> makeSignatures(
        const SstvModeRegistry& registry,
        const Config& config);
    static double durationToSamples(Picoseconds duration,
                                    std::uint32_t sampleRate) noexcept;
    static double median(std::vector<double> values);
    static bool finiteUnit(double value) noexcept;
    static bool validModeName(const std::string& mode) noexcept;
    static void increment(std::uint64_t& value) noexcept;
    SstvFallbackResult evaluateUnlocked() const;

    Config m_config;
    std::vector<Signature> m_signatures;
    std::vector<SstvFallbackSyncPulse> m_pulses;
    std::optional<std::string> m_lockedMode;
    SstvFallbackMetrics m_metrics;
    bool m_enabled {true};
};

} // namespace decodium::sstv
