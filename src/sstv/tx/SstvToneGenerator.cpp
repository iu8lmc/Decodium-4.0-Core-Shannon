// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvToneGenerator.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace decodium::sstv {
namespace {

constexpr long double kPhaseModulus = 18'446'744'073'709'551'616.0L; // 2^64
constexpr long double kTwoPi =
    6.2831853071795864769252867665590057683943387987502L;

std::int16_t toPcm16(double sample) noexcept
{
    if (sample >= 1.0) {
        return std::numeric_limits<std::int16_t>::max();
    }
    if (sample <= -1.0) {
        return std::numeric_limits<std::int16_t>::min();
    }

    return static_cast<std::int16_t>(
        std::lround(sample * std::numeric_limits<std::int16_t>::max()));
}

} // namespace

SstvToneGenerator::SstvToneGenerator(std::uint32_t sampleRate, double headroom)
    : m_sampleRate(sampleRate)
    , m_headroom(headroom)
    , m_timing(sampleRate)
{
    if (!isSupportedSampleRate(sampleRate)) {
        throw std::invalid_argument("unsupported SSTV TX sample rate");
    }
    if (!std::isfinite(headroom) || headroom <= 0.0 || headroom > 1.0) {
        throw std::invalid_argument("SSTV TX headroom must be in (0, 1]");
    }
}

bool SstvToneGenerator::isSupportedSampleRate(std::uint32_t sampleRate) noexcept
{
    return sampleRate >= kMinimumSampleRate
        && sampleRate <= kMaximumSampleRate;
}

std::uint32_t SstvToneGenerator::sampleRate() const noexcept
{
    return m_sampleRate;
}

double SstvToneGenerator::headroom() const noexcept
{
    return m_headroom;
}

std::uint64_t SstvToneGenerator::samplesForDuration(Picoseconds duration)
{
    return m_timing.samplesFor(duration);
}

std::uint64_t SstvToneGenerator::scheduledSamples() const noexcept
{
    return m_timing.totalSamples();
}

std::uint64_t SstvToneGenerator::timingRemainder() const noexcept
{
    return m_timing.fractionalRemainder();
}

void SstvToneGenerator::validateTone(double frequencyHz, double level) const
{
    const double nyquist = static_cast<double>(m_sampleRate) / 2.0;
    if (!std::isfinite(frequencyHz)
        || frequencyHz <= 0.0
        || frequencyHz >= nyquist) {
        throw std::invalid_argument(
            "SSTV TX tone frequency must be finite, positive and below Nyquist");
    }
    if (!std::isfinite(level) || level < 0.0) {
        throw std::invalid_argument(
            "SSTV TX tone level must be finite and non-negative");
    }

    // Reject frequencies below the representable DDS resolution instead of
    // silently producing DC.  This does not affect the SSTV audio band.
    if (phaseIncrement(frequencyHz) == 0U) {
        throw std::invalid_argument("SSTV TX tone is below DDS resolution");
    }
}

std::size_t SstvToneGenerator::generateFloat(double frequencyHz,
                                             double level,
                                             float* output,
                                             std::size_t sampleCount)
{
    return generate(frequencyHz, level, output, sampleCount);
}

std::size_t SstvToneGenerator::generatePcm16(double frequencyHz,
                                             double level,
                                             std::int16_t* output,
                                             std::size_t sampleCount)
{
    return generate(frequencyHz, level, output, sampleCount);
}

const SstvToneMetrics& SstvToneGenerator::metrics() const noexcept
{
    return m_metrics;
}

double SstvToneGenerator::phaseTurns() const noexcept
{
    return static_cast<double>(
        static_cast<long double>(m_phase) / kPhaseModulus);
}

void SstvToneGenerator::resetMetrics() noexcept
{
    m_metrics = {};
}

void SstvToneGenerator::reset() noexcept
{
    m_phase = 0;
    m_timing.reset();
    resetMetrics();
    clearCancellation();
}

void SstvToneGenerator::cancel() noexcept
{
    m_cancelRequested.store(true, std::memory_order_relaxed);
}

bool SstvToneGenerator::cancelled() const noexcept
{
    return m_cancelRequested.load(std::memory_order_relaxed);
}

void SstvToneGenerator::clearCancellation() noexcept
{
    m_cancelRequested.store(false, std::memory_order_relaxed);
}

std::uint64_t SstvToneGenerator::phaseIncrement(double frequencyHz) const
{
    // Rounding to the nearest 64-bit tuning word gives sub-nanohertz
    // resolution at normal audio rates while unsigned wrap performs exact
    // modulo-one phase accumulation without an ever-growing floating value.
    const long double turnsPerSample =
        static_cast<long double>(frequencyHz)
        / static_cast<long double>(m_sampleRate);
    const long double scaled = std::ldexp(turnsPerSample, 64);
    return static_cast<std::uint64_t>(std::floor(scaled + 0.5L));
}

double SstvToneGenerator::nextSample(std::uint64_t increment,
                                     double level) noexcept
{
    const long double phase =
        static_cast<long double>(m_phase) / kPhaseModulus;
    const double raw = m_headroom * level
        * std::sin(static_cast<double>(phase * kTwoPi));
    m_phase += increment; // unsigned overflow is the DDS phase wrap

    const double magnitude = std::abs(raw);
    m_metrics.peakBeforeClamp =
        std::max(m_metrics.peakBeforeClamp, magnitude);

    if (raw > 1.0 || raw < -1.0) {
        ++m_metrics.clippedSamples;
    }

    const double clamped = std::clamp(raw, -1.0, 1.0);
    m_metrics.peakAfterClamp =
        std::max(m_metrics.peakAfterClamp, std::abs(clamped));
    ++m_metrics.samplesGenerated;
    return clamped;
}

template<typename OutputSample>
std::size_t SstvToneGenerator::generate(double frequencyHz,
                                        double level,
                                        OutputSample* output,
                                        std::size_t sampleCount)
{
    static_assert(std::is_same_v<OutputSample, float>
                      || std::is_same_v<OutputSample, std::int16_t>,
                  "unsupported SSTV tone output type");

    validateTone(frequencyHz, level);
    if (sampleCount != 0U && output == nullptr) {
        throw std::invalid_argument("SSTV TX output buffer must not be null");
    }
    if (sampleCount
        > std::numeric_limits<std::uint64_t>::max()
            - m_metrics.samplesGenerated) {
        throw std::overflow_error("SSTV TX generated sample count overflow");
    }

    const std::uint64_t increment = phaseIncrement(frequencyHz);
    std::size_t generated = 0;
    while (generated < sampleCount && !cancelled()) {
        const double sample = nextSample(increment, level);
        if constexpr (std::is_same_v<OutputSample, float>) {
            output[generated] = static_cast<float>(sample);
        } else {
            output[generated] = toPcm16(sample);
        }
        ++generated;
    }
    return generated;
}

} // namespace decodium::sstv
