// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../core/SstvTimingAccumulator.h"

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace decodium::sstv {

// The default -1 dBFS peak leaves deterministic headroom for resampling and
// audio-device conversion downstream of the SSTV encoder.
constexpr double kDefaultSstvTxHeadroom = 0.8912509381337456;

struct SstvToneMetrics final
{
    std::uint64_t samplesGenerated {0};
    double peakBeforeClamp {0.0};
    double peakAfterClamp {0.0};
    std::uint64_t clippedSamples {0};
};

// A pull-oriented, phase-continuous direct digital synthesizer.  The waveform
// state has a fixed-size footprint independent of transmission duration.
//
// Rendering and reset are single-owner operations.  cancel() is the sole
// method intended to be called concurrently, for example by the bridge TX
// watchdog.  Cancellation is observed at a sample boundary.
class SstvToneGenerator final
{
public:
    static constexpr std::uint32_t kMinimumSampleRate = 8'000;
    static constexpr std::uint32_t kMaximumSampleRate = 384'000;

    explicit SstvToneGenerator(
        std::uint32_t sampleRate,
        double headroom = kDefaultSstvTxHeadroom);

    SstvToneGenerator(const SstvToneGenerator&) = delete;
    SstvToneGenerator& operator=(const SstvToneGenerator&) = delete;

    static bool isSupportedSampleRate(std::uint32_t sampleRate) noexcept;

    std::uint32_t sampleRate() const noexcept;
    double headroom() const noexcept;

    // Converts an exact protocol duration to a sample count, carrying the
    // fractional sample remainder across calls.  Scheduling does not alter
    // phase or signal metrics.
    std::uint64_t samplesForDuration(Picoseconds duration);
    std::uint64_t scheduledSamples() const noexcept;
    std::uint64_t timingRemainder() const noexcept;

    // level is a non-negative linear multiplier applied before clamping.
    // Values above unity are accepted intentionally so the caller can inspect
    // clipping through metrics().  frequencyHz must be strictly below Nyquist.
    void validateTone(double frequencyHz, double level) const;

    std::size_t generateFloat(double frequencyHz,
                              double level,
                              float* output,
                              std::size_t sampleCount);
    std::size_t generatePcm16(double frequencyHz,
                              double level,
                              std::int16_t* output,
                              std::size_t sampleCount);

    const SstvToneMetrics& metrics() const noexcept;
    double phaseTurns() const noexcept;

    void resetMetrics() noexcept;

    // Resets phase, timing carry, metrics and cancellation.  Tone changes and
    // segment boundaries never call this implicitly.
    void reset() noexcept;

    void cancel() noexcept;
    bool cancelled() const noexcept;
    void clearCancellation() noexcept;

private:
    template<typename OutputSample>
    std::size_t generate(double frequencyHz,
                         double level,
                         OutputSample* output,
                         std::size_t sampleCount);

    std::uint64_t phaseIncrement(double frequencyHz) const;
    double nextSample(std::uint64_t increment, double level) noexcept;

    std::uint32_t m_sampleRate {0};
    double m_headroom {kDefaultSstvTxHeadroom};
    std::uint64_t m_phase {0};
    SstvTimingAccumulator m_timing;
    SstvToneMetrics m_metrics;
    std::atomic_bool m_cancelRequested {false};
};

} // namespace decodium::sstv
