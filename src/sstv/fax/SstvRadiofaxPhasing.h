// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "SstvRadiofax.h"
#include "../dsp/SstvFrequencyDemodulator.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace decodium::sstv {

// Keep clock estimation within the same rate tolerance used for acquisition.
inline constexpr double kRadiofaxMaximumClockDeviation = 0.02;

struct SstvRadiofaxPhaseLock final {
    std::uint16_t linesPerMinute {0U};
    SstvFaxPhasePattern pattern {SstvFaxPhasePattern::Symmetric};
    std::uint64_t latestWhiteEdgeSample {0U};
    std::uint64_t detectedPeriodSamples {0U};
};

// Measures the period of confirmed phasing pulses after acquisition. The
// rational result avoids rounding a fractional line to whole audio samples.
struct SstvRadiofaxClockEstimate final {
    std::uint64_t samples {0U};
    std::uint64_t lines {0U};
};

class SstvRadiofaxClockTracker final {
public:
    SstvRadiofaxClockTracker(std::uint32_t sampleRate, SstvRadiofaxPhaseLock lock,
                             double frequencyOffsetHz = 0.0);
    void consume(const SstvFrequencyObservation& observation) noexcept;
    std::optional<SstvRadiofaxClockEstimate> estimate() const noexcept;
    bool valid() const noexcept { return valid_; }
private:
    std::uint32_t rate_;
    SstvRadiofaxPhaseLock lock_;
    double offset_;
    std::uint64_t edge_;
    std::uint64_t first_ {0U};
    std::uint64_t last_ {0U};
    std::uint64_t lines_ {0U};
    std::uint64_t lastObservation_ {0U};
    unsigned pulses_ {0U};
    int tone_ {1};
    bool haveFirst_ {false};
    bool valid_ {true};
};

// Recognises a stable phasing pattern, not the beginning or end of a fax.
// Leading edges can differ from the physical edge by the frontend's latency.
// The caller must separately identify image start and replay buffered audio.
class SstvRadiofaxPhasingDetector final {
public:
    static constexpr std::size_t MaximumObservationsPerConsume = 8192U;
    explicit SstvRadiofaxPhasingDetector(std::uint32_t sampleRate,
                                        double frequencyOffsetHz = 0.0);
    std::optional<SstvRadiofaxPhaseLock> consume(const SstvFrequencyObservation* data,
                                                 std::size_t count);
    std::optional<SstvRadiofaxPhaseLock> consume(const std::vector<SstvFrequencyObservation>& data);
    void reset() noexcept;
private:
    std::uint32_t sampleRate_;
    double offset_;
    std::uint64_t lastSample_ {0U};
    std::uint64_t whiteEdge_ {0U};
    std::uint64_t whiteDuration_ {0U};
    bool hasLast_ {false};
    bool hasWhiteEdge_ {false};
    bool hasWhiteDuration_ {false};
    int tone_ {0};
    std::uint16_t candidateRpm_ {0U};
    SstvFaxPhasePattern candidatePattern_ {SstvFaxPhasePattern::Symmetric};
    unsigned matches_ {0U};
    std::optional<SstvRadiofaxPhaseLock> lock_;
};

} // namespace decodium::sstv

namespace decodium::sstv {

// Reports the start of the first *observed* line inconsistent with a locked
// phasing pattern. The caller must buffer/replay that line. Image content may
// itself resemble phasing, so this is a candidate, not an unambiguous marker.
class SstvRadiofaxImageStartDetector final {
public:
    SstvRadiofaxImageStartDetector(std::uint32_t sampleRate, SstvRadiofaxPhaseLock lock,
                                   double frequencyOffsetHz = 0.0);
    std::optional<std::uint64_t> consume(const SstvFrequencyObservation* data, std::size_t count);
    std::optional<std::uint64_t> consume(const std::vector<SstvFrequencyObservation>& data);
    void setClockEstimate(std::optional<SstvRadiofaxClockEstimate> estimate) noexcept;
    bool valid() const noexcept;
private:
    void finishLine();
    std::uint64_t lineStart(std::uint64_t line) const noexcept;
    std::uint32_t sampleRate_;
    SstvRadiofaxPhaseLock lock_;
    double offset_;
    std::uint64_t line_ {0U};
    std::uint64_t lastSample_ {0U};
    std::uint64_t gapLimit_ {0U};
    unsigned expectedWhite_ {0U};
    unsigned expectedBlack_ {0U};
    unsigned matchedWhite_ {0U};
    unsigned matchedBlack_ {0U};
    bool valid_ {true};
    bool hasLast_ {false};
    std::optional<std::uint64_t> result_;
    std::optional<SstvRadiofaxClockEstimate> clock_;
};

} // namespace decodium::sstv
