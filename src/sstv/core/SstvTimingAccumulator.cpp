// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvTimingAccumulator.h"

#include <limits>
#include <stdexcept>

namespace decodium::sstv {
namespace {

struct QuotientRemainder final
{
    std::uint64_t quotient {0};
    std::uint64_t remainder {0};
};

// Computes (multiplicand * multiplier) / divisor without forming the possibly
// overflowing product.  multiplicand must be smaller than divisor.  The
// multiplier is deliberately 32-bit because it is an audio sample rate.
QuotientRemainder multiplyAndDivide(std::uint64_t multiplicand,
                                   std::uint32_t multiplier,
                                   std::uint64_t divisor)
{
    QuotientRemainder result;
    QuotientRemainder term {0, multiplicand};
    std::uint32_t bits = multiplier;

    while (bits != 0U) {
        if ((bits & 1U) != 0U) {
            if (result.quotient > std::numeric_limits<std::uint64_t>::max() - term.quotient) {
                throw std::overflow_error("SSTV timing quotient overflow");
            }
            result.quotient += term.quotient;

            // Both remainders are below divisor.  The fixed divisor is 1e12,
            // so their sum cannot overflow uint64_t.
            result.remainder += term.remainder;
            if (result.remainder >= divisor) {
                result.remainder -= divisor;
                if (result.quotient == std::numeric_limits<std::uint64_t>::max()) {
                    throw std::overflow_error("SSTV timing quotient overflow");
                }
                ++result.quotient;
            }
        }

        bits >>= 1U;
        if (bits == 0U) {
            break;
        }

        if (term.quotient > std::numeric_limits<std::uint64_t>::max() / 2U) {
            throw std::overflow_error("SSTV timing quotient overflow");
        }
        term.quotient *= 2U;
        term.remainder *= 2U;
        if (term.remainder >= divisor) {
            term.remainder -= divisor;
            if (term.quotient == std::numeric_limits<std::uint64_t>::max()) {
                throw std::overflow_error("SSTV timing quotient overflow");
            }
            ++term.quotient;
        }
    }

    return result;
}

} // namespace

SstvTimingAccumulator::SstvTimingAccumulator(std::uint32_t sampleRate)
    : m_sampleRate(sampleRate)
{
    if (sampleRate == 0U) {
        throw std::invalid_argument("SSTV sample rate must be positive");
    }
}

std::uint32_t SstvTimingAccumulator::sampleRate() const noexcept
{
    return m_sampleRate;
}

std::uint64_t SstvTimingAccumulator::samplesFor(Picoseconds duration)
{
    if (duration.count < 0) {
        throw std::invalid_argument("SSTV duration must not be negative");
    }

    const auto value = static_cast<std::uint64_t>(duration.count);
    const auto divisor = static_cast<std::uint64_t>(kPicosecondsPerSecond);
    const std::uint64_t wholeSeconds = value / divisor;
    const std::uint64_t partialSecond = value % divisor;

    if (wholeSeconds != 0U
        && wholeSeconds > std::numeric_limits<std::uint64_t>::max() / m_sampleRate) {
        throw std::overflow_error("SSTV timing sample count overflow");
    }
    std::uint64_t emitted = wholeSeconds * m_sampleRate;

    auto fraction = multiplyAndDivide(partialSecond, m_sampleRate, divisor);
    if (emitted > std::numeric_limits<std::uint64_t>::max() - fraction.quotient) {
        throw std::overflow_error("SSTV timing sample count overflow");
    }
    emitted += fraction.quotient;

    // Work on local values so a throwing call does not partially mutate the
    // accumulator.
    std::uint64_t nextRemainder = fraction.remainder + m_fractionalRemainder;
    if (nextRemainder >= divisor) {
        nextRemainder -= divisor;
        if (emitted == std::numeric_limits<std::uint64_t>::max()) {
            throw std::overflow_error("SSTV timing sample count overflow");
        }
        ++emitted;
    }

    if (m_totalSamples > std::numeric_limits<std::uint64_t>::max() - emitted) {
        throw std::overflow_error("SSTV timing accumulated sample count overflow");
    }

    m_totalSamples += emitted;
    m_fractionalRemainder = nextRemainder;
    return emitted;
}

std::uint64_t SstvTimingAccumulator::totalSamples() const noexcept
{
    return m_totalSamples;
}

std::uint64_t SstvTimingAccumulator::fractionalRemainder() const noexcept
{
    return m_fractionalRemainder;
}

void SstvTimingAccumulator::reset() noexcept
{
    m_totalSamples = 0;
    m_fractionalRemainder = 0;
}

} // namespace decodium::sstv
