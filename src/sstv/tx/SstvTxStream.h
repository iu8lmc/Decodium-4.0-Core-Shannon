// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvToneGenerator.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace decodium::sstv {

enum class SstvTxSegmentRole : std::uint8_t {
    Header,
    Vis,
    Image,
    FskId,
    Tail
};

// A mode encoder produces this neutral tone plan only after its ModeSpec has
// been independently verified.  This streaming layer deliberately contains
// no analogue-mode timing constants of its own.
struct SstvToneSegment final
{
    double frequencyHz {0.0};
    Picoseconds duration;
    double level {1.0};
    SstvTxSegmentRole role {SstvTxSegmentRole::Image};
};

struct SstvTxSegmentCursor final
{
    std::size_t planIndex {0};
    SstvTxSegmentRole role {SstvTxSegmentRole::Image};
    double frequencyHz {0.0};
    Picoseconds duration;
    std::uint64_t sampleCount {0};
    std::uint64_t samplesProduced {0};
};

// Bounded pull stream over a tone plan.  Only the plan and DDS state are held;
// PCM is generated directly into the caller-provided chunk.
class SstvTxStream final
{
public:
    explicit SstvTxStream(
        std::uint32_t sampleRate,
        std::vector<SstvToneSegment> plan,
        double headroom = kDefaultSstvTxHeadroom);

    SstvTxStream(const SstvTxStream&) = delete;
    SstvTxStream& operator=(const SstvTxStream&) = delete;

    std::uint32_t sampleRate() const noexcept;
    std::size_t plannedSegmentCount() const noexcept;
    Picoseconds totalDuration() const noexcept;
    std::uint64_t totalSamples() const noexcept;
    std::uint64_t producedSamples() const noexcept;
    std::uint64_t remainingSamples() const noexcept;
    double progress() const noexcept;

    bool complete() const noexcept;
    bool cancelled() const noexcept;
    std::optional<SstvTxSegmentCursor> currentSegment() const noexcept;

    std::size_t pullFloat(float* output, std::size_t capacity);
    std::size_t pullPcm16(std::int16_t* output, std::size_t capacity);

    const SstvToneMetrics& metrics() const noexcept;

    // cancel() may be called concurrently with a pull.  reset() is a
    // single-owner operation and restarts timing, phase and progress.
    void cancel() noexcept;
    void reset();

private:
    struct RuntimeSegment final
    {
        SstvToneSegment definition;
        std::uint64_t sampleCount {0};
    };

    template<typename OutputSample>
    std::size_t pull(OutputSample* output, std::size_t capacity);

    void validateAndLoadPlan(std::vector<SstvToneSegment> plan);
    void rebuildSchedule();
    void skipCompletedSegments() noexcept;

    SstvToneGenerator m_generator;
    std::vector<RuntimeSegment> m_segments;
    Picoseconds m_totalDuration;
    std::uint64_t m_totalSamples {0};
    std::uint64_t m_producedSamples {0};
    std::size_t m_segmentIndex {0};
    std::uint64_t m_segmentOffset {0};
};

} // namespace decodium::sstv
