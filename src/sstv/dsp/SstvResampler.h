// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace decodium::sstv {

// Stateful sample-rate conversion into the native SSTV processing rate.
//
// The implementation uses a windowed-sinc low-pass interpolator and an exact
// rational phase accumulator.  It is intended for SSTV audio-band processing,
// not as a general-purpose mastering/sample-rate-conversion library.  In
// particular, it deliberately trades a small, fixed look-ahead latency for
// deterministic chunk-independent output.
class SstvResampler final
{
public:
    static constexpr std::uint32_t kOutputSampleRate = 12'000U;
    static constexpr std::size_t kKernelHalfWidth = 96U;
    static constexpr std::size_t kMaxInputSamplesPerCall = 1U << 20U;

    explicit SstvResampler(std::uint32_t inputSampleRate);

    SstvResampler(const SstvResampler&) = delete;
    SstvResampler& operator=(const SstvResampler&) = delete;
    SstvResampler(SstvResampler&&) = delete;
    SstvResampler& operator=(SstvResampler&&) = delete;

    static bool isSupportedInputRate(std::uint32_t sampleRate) noexcept;

    std::uint32_t inputSampleRate() const noexcept;
    std::uint32_t outputSampleRate() const noexcept;
    std::size_t latencyInputSamples() const noexcept;

    // Adds one bounded block and returns every output sample for which the
    // symmetric FIR has enough real input look-ahead.  Empty blocks are valid.
    // Invalid/non-finite input is rejected transactionally.
    std::vector<float> process(const float* samples, std::size_t sampleCount);
    std::vector<float> process(const std::vector<float>& samples);

    // Zero-pads only the fixed FIR tail and returns the remaining samples.  A
    // completed stream contains exactly floor(inputFrames * 12000 / inputRate)
    // frames.  Processing after flush requires reset(); repeated flushes are
    // harmless and return an empty vector.
    std::vector<float> flush();

    void reset() noexcept;
    bool isFlushed() const noexcept;
    std::uint64_t totalInputSamples() const noexcept;
    std::uint64_t totalOutputSamples() const noexcept;
    std::size_t bufferedInputSamples() const noexcept;

private:
    float renderCurrentOutput() const;
    void advanceSourcePosition();
    void trimConsumedHistory() noexcept;
    std::uint64_t targetOutputCount() const;
    std::uint64_t maximumAcceptedInputCount() const noexcept;

    std::uint32_t m_inputSampleRate {0U};
    double m_cutoffCyclesPerInputSample {0.0};

    std::deque<float> m_input;
    std::uint64_t m_bufferStartIndex {0U};
    std::uint64_t m_totalInputSamples {0U};
    std::uint64_t m_totalOutputSamples {0U};

    // Exact source position of the next output sample.  The position is
    // m_sourceWhole + m_sourceRemainder / kOutputSampleRate.
    std::uint64_t m_sourceWhole {0U};
    std::uint32_t m_sourceRemainder {0U};
    bool m_flushed {false};
};

} // namespace decodium::sstv
