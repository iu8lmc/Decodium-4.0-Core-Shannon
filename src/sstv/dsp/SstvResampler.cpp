// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvResampler.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace decodium::sstv {
namespace {

constexpr std::array<std::uint32_t, 10> kSupportedInputRates {
    8'000U,
    11'025U,
    12'000U,
    16'000U,
    22'050U,
    24'000U,
    32'000U,
    44'100U,
    48'000U,
    96'000U,
};

constexpr double kPi = 3.141592653589793238462643383279502884;

double normalizedSinc(double value) noexcept
{
    if (std::abs(value) < 1.0e-12) {
        return 1.0;
    }
    const double angle = kPi * value;
    return std::sin(angle) / angle;
}

double blackmanWindow(double distance) noexcept
{
    const double normalized = distance
        / static_cast<double>(SstvResampler::kKernelHalfWidth);
    if (normalized > 1.0) {
        return 0.0;
    }
    return 0.42 + 0.5 * std::cos(kPi * normalized)
        + 0.08 * std::cos(2.0 * kPi * normalized);
}

} // namespace

SstvResampler::SstvResampler(std::uint32_t inputSampleRate)
    : m_inputSampleRate(inputSampleRate)
{
    if (!isSupportedInputRate(inputSampleRate)) {
        throw std::invalid_argument("unsupported SSTV input sample rate");
    }

    // Retain a 5% transition band below the lower Nyquist limit.  At all
    // supported rates this leaves the complete analog SSTV 1.1--2.3 kHz tone
    // range well inside the pass band while suppressing down-sampling aliases.
    const double conversionRatio = static_cast<double>(kOutputSampleRate)
        / static_cast<double>(m_inputSampleRate);
    m_cutoffCyclesPerInputSample = 0.475 * std::min(1.0, conversionRatio);
}

bool SstvResampler::isSupportedInputRate(std::uint32_t sampleRate) noexcept
{
    return std::find(kSupportedInputRates.begin(), kSupportedInputRates.end(), sampleRate)
        != kSupportedInputRates.end();
}

std::uint32_t SstvResampler::inputSampleRate() const noexcept
{
    return m_inputSampleRate;
}

std::uint32_t SstvResampler::outputSampleRate() const noexcept
{
    return kOutputSampleRate;
}

std::size_t SstvResampler::latencyInputSamples() const noexcept
{
    return kKernelHalfWidth;
}

std::vector<float> SstvResampler::process(const float* samples,
                                          std::size_t sampleCount)
{
    if (m_flushed) {
        throw std::logic_error("SSTV resampler must be reset after flush");
    }
    if (sampleCount > kMaxInputSamplesPerCall) {
        throw std::length_error("SSTV resampler input block is too large");
    }
    if (sampleCount != 0U && samples == nullptr) {
        throw std::invalid_argument("SSTV resampler received a null input block");
    }
    if (sampleCount > maximumAcceptedInputCount() - m_totalInputSamples) {
        throw std::overflow_error("SSTV resampler stream is too long");
    }

    // Validation precedes every state mutation so a bad callback block cannot
    // poison the resampler history or phase.
    for (std::size_t index = 0U; index < sampleCount; ++index) {
        if (!std::isfinite(samples[index])) {
            throw std::invalid_argument("SSTV resampler input must be finite");
        }
    }

    if (sampleCount != 0U) {
        m_input.insert(m_input.end(), samples, samples + sampleCount);
        m_totalInputSamples += static_cast<std::uint64_t>(sampleCount);
    }

    // The largest supported expansion is 3/2 (8 kHz -> 12 kHz).  This reserve
    // remains bounded by kMaxInputSamplesPerCall and never controls correctness.
    const std::size_t reserveCount = sampleCount + sampleCount / 2U + 2U;
    std::vector<float> output;
    output.reserve(reserveCount);

    while (m_sourceWhole <= std::numeric_limits<std::uint64_t>::max()
               - static_cast<std::uint64_t>(kKernelHalfWidth)
           && m_sourceWhole + static_cast<std::uint64_t>(kKernelHalfWidth)
               < m_totalInputSamples) {
        output.push_back(renderCurrentOutput());
        advanceSourcePosition();
        ++m_totalOutputSamples;
    }

    trimConsumedHistory();
    return output;
}

std::vector<float> SstvResampler::process(const std::vector<float>& samples)
{
    return process(samples.data(), samples.size());
}

std::vector<float> SstvResampler::flush()
{
    if (m_flushed) {
        return {};
    }

    const std::uint64_t target = targetOutputCount();
    if (m_totalOutputSamples > target) {
        throw std::logic_error("SSTV resampler emitted beyond stream duration");
    }
    const std::uint64_t remaining = target - m_totalOutputSamples;
    if (remaining > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::length_error("SSTV resampler tail cannot fit in memory");
    }

    std::vector<float> output;
    output.reserve(static_cast<std::size_t>(remaining));
    while (m_totalOutputSamples < target) {
        output.push_back(renderCurrentOutput());
        advanceSourcePosition();
        ++m_totalOutputSamples;
    }

    m_input.clear();
    m_bufferStartIndex = m_totalInputSamples;
    m_flushed = true;
    return output;
}

void SstvResampler::reset() noexcept
{
    m_input.clear();
    m_bufferStartIndex = 0U;
    m_totalInputSamples = 0U;
    m_totalOutputSamples = 0U;
    m_sourceWhole = 0U;
    m_sourceRemainder = 0U;
    m_flushed = false;
}

bool SstvResampler::isFlushed() const noexcept
{
    return m_flushed;
}

std::uint64_t SstvResampler::totalInputSamples() const noexcept
{
    return m_totalInputSamples;
}

std::uint64_t SstvResampler::totalOutputSamples() const noexcept
{
    return m_totalOutputSamples;
}

std::size_t SstvResampler::bufferedInputSamples() const noexcept
{
    return m_input.size();
}

float SstvResampler::renderCurrentOutput() const
{
    const double fraction = static_cast<double>(m_sourceRemainder)
        / static_cast<double>(kOutputSampleRate);
    double weighted = 0.0;
    double weightSum = 0.0;
    const auto halfWidth = static_cast<std::int64_t>(kKernelHalfWidth);

    for (std::int64_t offset = -halfWidth; offset <= halfWidth; ++offset) {
        const double distance = fraction - static_cast<double>(offset);
        const double window = blackmanWindow(std::abs(distance));
        if (window == 0.0) {
            continue;
        }
        const double weight = 2.0 * m_cutoffCyclesPerInputSample
            * normalizedSinc(2.0 * m_cutoffCyclesPerInputSample * distance)
            * window;
        weightSum += weight;

        std::uint64_t sampleIndex = 0U;
        bool indexIsValid = true;
        if (offset < 0) {
            const auto magnitude = static_cast<std::uint64_t>(-offset);
            if (m_sourceWhole < magnitude) {
                indexIsValid = false;
            } else {
                sampleIndex = m_sourceWhole - magnitude;
            }
        } else {
            const auto magnitude = static_cast<std::uint64_t>(offset);
            if (m_sourceWhole > std::numeric_limits<std::uint64_t>::max() - magnitude) {
                indexIsValid = false;
            } else {
                sampleIndex = m_sourceWhole + magnitude;
            }
        }

        if (indexIsValid && sampleIndex >= m_bufferStartIndex
            && sampleIndex < m_totalInputSamples) {
            const std::uint64_t relative = sampleIndex - m_bufferStartIndex;
            if (relative < static_cast<std::uint64_t>(m_input.size())) {
                weighted += weight * static_cast<double>(m_input[static_cast<std::size_t>(relative)]);
            }
        }
    }

    if (std::abs(weightSum) < 1.0e-15) {
        throw std::runtime_error("SSTV resampler generated an invalid FIR phase");
    }
    return static_cast<float>(weighted / weightSum);
}

void SstvResampler::advanceSourcePosition()
{
    const std::uint32_t accumulated = m_sourceRemainder + m_inputSampleRate;
    const std::uint64_t wholeAdvance = accumulated / kOutputSampleRate;
    if (m_sourceWhole > std::numeric_limits<std::uint64_t>::max() - wholeAdvance) {
        throw std::overflow_error("SSTV resampler phase overflow");
    }
    m_sourceWhole += wholeAdvance;
    m_sourceRemainder = accumulated % kOutputSampleRate;
}

void SstvResampler::trimConsumedHistory() noexcept
{
    const std::uint64_t retainedFrom = m_sourceWhole
            > static_cast<std::uint64_t>(kKernelHalfWidth)
        ? m_sourceWhole - static_cast<std::uint64_t>(kKernelHalfWidth)
        : 0U;
    while (!m_input.empty() && m_bufferStartIndex < retainedFrom) {
        m_input.pop_front();
        ++m_bufferStartIndex;
    }
}

std::uint64_t SstvResampler::targetOutputCount() const
{
    // Decomposition avoids overflowing totalInput * 12000.
    const std::uint64_t whole = m_totalInputSamples / m_inputSampleRate;
    const std::uint64_t remainder = m_totalInputSamples % m_inputSampleRate;
    if (whole > std::numeric_limits<std::uint64_t>::max() / kOutputSampleRate) {
        throw std::overflow_error("SSTV resampler output count overflow");
    }
    const std::uint64_t base = whole * kOutputSampleRate;
    const std::uint64_t fraction = remainder * kOutputSampleRate / m_inputSampleRate;
    if (base > std::numeric_limits<std::uint64_t>::max() - fraction) {
        throw std::overflow_error("SSTV resampler output count overflow");
    }
    return base + fraction;
}

std::uint64_t SstvResampler::maximumAcceptedInputCount() const noexcept
{
    // Largest input duration whose exact converted-frame count fits uint64_t.
    const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
    const std::uint64_t phaseHeadroom = static_cast<std::uint64_t>(kKernelHalfWidth)
        + static_cast<std::uint64_t>(m_inputSampleRate);
    if (m_inputSampleRate >= kOutputSampleRate) {
        return maximum - phaseHeadroom;
    }

    const std::uint64_t whole = maximum / kOutputSampleRate;
    const std::uint64_t remainder = maximum % kOutputSampleRate;
    const std::uint64_t base = whole * m_inputSampleRate;
    const std::uint64_t fraction = remainder * m_inputSampleRate / kOutputSampleRate;
    return base + fraction - phaseHeadroom;
}

} // namespace decodium::sstv
