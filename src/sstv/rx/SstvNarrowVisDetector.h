// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../core/SstvNarrowVisCodec.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace decodium::sstv {

enum class SstvNarrowVisDetectorState : std::uint8_t
{
    SearchingLeader,
    AwaitingGuard,
    ReadingSymbols,
    Cancelled,
};

enum class SstvNarrowVisDetectionStatus : std::uint8_t
{
    Decoded,
    Rejected,
    Truncated,
    Cancelled,
    InvalidInput,
};

struct SstvNarrowVisToneEvent final
{
    std::uint64_t startTimeUs {0U};
    std::uint64_t durationUs {0U};
    double frequencyHz {0.0};
    double confidence {0.0};
};

struct SstvNarrowVisDetection final
{
    SstvNarrowVisDetectionStatus status {
        SstvNarrowVisDetectionStatus::Rejected};
    SstvNarrowVisDecodeResult codecResult;
    std::uint64_t frameStartedAtUs {0U};
    std::uint64_t frameEndedAtUs {0U};
    double estimatedFrequencyOffsetHz {0.0};
    double confidence {0.0};

    bool valid() const noexcept
    {
        return status == SstvNarrowVisDetectionStatus::Decoded
            && codecResult.valid;
    }
};

struct SstvNarrowVisDetectorConfig final
{
    double frequencyToleranceHz {90.0};
    double durationTolerance {0.30};
    double minimumConfidence {0.30};
    std::uint64_t maximumGapUs {5'000U};
};

struct SstvNarrowVisDetectorMetrics final
{
    std::uint64_t eventsConsumed {0U};
    std::uint64_t framesStarted {0U};
    std::uint64_t framesDecoded {0U};
    std::uint64_t framesRejected {0U};
    std::uint64_t invalidInputs {0U};
};

// Streaming physical N-VIS detector.  Runs at 1900/2100 Hz may span multiple
// 22 ms symbols; the detector divides them against one symbol epoch and then
// delegates all preamble/complement/mode validity to SstvNarrowVisCodec.
class SstvNarrowVisDetector final
{
public:
    static constexpr std::size_t MaximumEventsPerConsume = 256U;
    static constexpr std::size_t SymbolCount = 25U; // start + 24 data

    explicit SstvNarrowVisDetector(
        SstvNarrowVisDetectorConfig config = {});

    std::vector<SstvNarrowVisDetection> consume(
        const SstvNarrowVisToneEvent* events,
        std::size_t count);
    std::vector<SstvNarrowVisDetection> consume(
        const std::vector<SstvNarrowVisToneEvent>& events);
    std::optional<SstvNarrowVisDetection> finish(std::uint64_t nowUs);
    std::optional<SstvNarrowVisDetection> cancel(std::uint64_t nowUs);
    void reset(bool clearMetrics = false) noexcept;

    SstvNarrowVisDetectorState state() const noexcept;
    const SstvNarrowVisDetectorMetrics& metrics() const noexcept;

private:
    enum class Tone : std::uint8_t { One, Zero, Unknown };

    static void validateConfig(const SstvNarrowVisDetectorConfig& config);
    Tone classify(double frequencyHz) const noexcept;
    bool durationMatches(std::uint64_t actual,
                         std::uint64_t nominal) const noexcept;
    void reject(std::vector<SstvNarrowVisDetection>& output,
                std::uint64_t endedAtUs);
    void resetFrame() noexcept;
    std::optional<std::size_t> symbolRunLength(
        std::uint64_t durationUs) const noexcept;
    void complete(std::vector<SstvNarrowVisDetection>& output,
                  std::uint64_t endedAtUs);
    static void saturatingAdd(std::uint64_t& value,
                              std::uint64_t increment = 1U) noexcept;

    SstvNarrowVisDetectorConfig config_;
    SstvNarrowVisDetectorMetrics metrics_;
    SstvNarrowVisDetectorState state_ {
        SstvNarrowVisDetectorState::SearchingLeader};
    std::uint64_t frameStartedAtUs_ {0U};
    std::uint64_t expectedNextUs_ {0U};
    std::vector<bool> symbols_;
    double confidence_ {1.0};
    long double offsetTimeSum_ {0.0L};
    std::uint64_t offsetDurationUs_ {0U};
    bool haveTimeline_ {false};
    std::uint64_t lastEventEndUs_ {0U};
};

} // namespace decodium::sstv
