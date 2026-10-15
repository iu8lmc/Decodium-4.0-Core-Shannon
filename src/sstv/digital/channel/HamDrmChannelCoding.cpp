// SPDX-License-Identifier: GPL-3.0-or-later

#include "HamDrmChannelCoding.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace decodium::sstv::hamdrm::channel {

namespace {

constexpr std::size_t kTailBits = 6U;
constexpr std::size_t kStateCount = 64U;
constexpr std::array<std::uint8_t, 4U> kGeneratorMasks {
    0155U, 0117U, 0123U, 0155U
};

struct PuncturePattern final
{
    std::array<HamDrmPunctureMask, 4U> masks {};
    std::size_t length {0U};
    std::size_t outputBits {0U};
};

constexpr std::array<PuncturePattern, 11U> kPatterns {{
    {{{HamDrmPunctureMask::G0G1G2G3}}, 1U, 4U},
    {{}, 0U, 0U},
    {{{HamDrmPunctureMask::G0G1G2}}, 1U, 3U},
    {{}, 0U, 0U},
    {{{HamDrmPunctureMask::G0G1}}, 1U, 2U},
    {{}, 0U, 0U},
    {{{HamDrmPunctureMask::G0G1,
       HamDrmPunctureMask::G0,
       HamDrmPunctureMask::G0G1}}, 3U, 5U},
    {{{HamDrmPunctureMask::G0G1,
       HamDrmPunctureMask::G0}}, 2U, 3U},
    {{}, 0U, 0U},
    {{{HamDrmPunctureMask::G0G1,
       HamDrmPunctureMask::G0,
       HamDrmPunctureMask::G0}}, 3U, 4U},
    {{{HamDrmPunctureMask::G0G1,
       HamDrmPunctureMask::G0,
       HamDrmPunctureMask::G0,
       HamDrmPunctureMask::G0}}, 4U, 5U},
}};

constexpr std::array<std::array<HamDrmPunctureMask, kTailBits>, 5U>
    kTailPatterns {{
        {{HamDrmPunctureMask::G0G1, HamDrmPunctureMask::G0G1,
          HamDrmPunctureMask::G0G1, HamDrmPunctureMask::G0G1,
          HamDrmPunctureMask::G0G1, HamDrmPunctureMask::G0G1}},
        {{HamDrmPunctureMask::G0G1G2, HamDrmPunctureMask::G0G1,
          HamDrmPunctureMask::G0G1, HamDrmPunctureMask::G0G1,
          HamDrmPunctureMask::G0G1, HamDrmPunctureMask::G0G1}},
        {{HamDrmPunctureMask::G0G1G2, HamDrmPunctureMask::G0G1,
          HamDrmPunctureMask::G0G1, HamDrmPunctureMask::G0G1G2,
          HamDrmPunctureMask::G0G1, HamDrmPunctureMask::G0G1}},
        {{HamDrmPunctureMask::G0G1G2, HamDrmPunctureMask::G0G1G2,
          HamDrmPunctureMask::G0G1, HamDrmPunctureMask::G0G1G2,
          HamDrmPunctureMask::G0G1, HamDrmPunctureMask::G0G1}},
        {{HamDrmPunctureMask::G0G1G2, HamDrmPunctureMask::G0G1G2,
          HamDrmPunctureMask::G0G1, HamDrmPunctureMask::G0G1G2,
          HamDrmPunctureMask::G0G1G2, HamDrmPunctureMask::G0G1}},
    }};

std::size_t maskBits(HamDrmPunctureMask mask) noexcept
{
    std::uint8_t value = static_cast<std::uint8_t>(mask);
    std::size_t count = 0U;
    while (value != 0U) {
        count += static_cast<std::size_t>(value & 1U);
        value = static_cast<std::uint8_t>(value >> 1U);
    }
    return count;
}

std::uint8_t parity(std::uint8_t value) noexcept
{
    value ^= static_cast<std::uint8_t>(value >> 4U);
    value ^= static_cast<std::uint8_t>(value >> 2U);
    value ^= static_cast<std::uint8_t>(value >> 1U);
    return static_cast<std::uint8_t>(value & 1U);
}

HamDrmStatus codingError(HamDrmErrorCode code, const char* detail)
{
    return HamDrmStatus::failure(code, detail);
}

bool isBinary(const std::vector<std::uint8_t>& bits) noexcept
{
    return std::all_of(bits.begin(), bits.end(),
                       [](std::uint8_t bit) { return bit <= 1U; });
}

} // namespace

std::vector<std::uint8_t> hamDrmEnergyDisperse(
    const std::vector<std::uint8_t>& bits)
{
    if (bits.size() > kHamDrmMaximumChannelBits || !isBinary(bits)) {
        throw std::invalid_argument("invalid HAMDRM energy-dispersal input");
    }
    std::uint16_t state = 0x01FFU;
    std::vector<std::uint8_t> output(bits.size(), 0U);
    for (std::size_t index = 0U; index < bits.size(); ++index) {
        const std::uint8_t prbs = static_cast<std::uint8_t>(
            ((state >> 4U) ^ (state >> 8U)) & 1U);
        const unsigned int shifted = static_cast<unsigned int>(state) << 1U;
        state = static_cast<std::uint16_t>(
            (shifted | static_cast<unsigned int>(prbs)) & 0x01FFU);
        output[index] = static_cast<std::uint8_t>(bits[index] ^ prbs);
    }
    return output;
}

std::vector<HamDrmPunctureMask> hamDrmPunctureSchedule(
    std::size_t inputBitCount,
    std::size_t patternIndex,
    std::size_t encodedBitCount,
    HamDrmPunctureTailMode tailMode)
{
    if (inputBitCount > kHamDrmMaximumChannelBits
            || encodedBitCount > 4U * kHamDrmMaximumChannelBits
            || patternIndex >= kPatterns.size()
            || kPatterns[patternIndex].length == 0U) {
        throw std::invalid_argument("invalid HAMDRM puncturing parameters");
    }
    const auto& pattern = kPatterns[patternIndex];
    std::vector<HamDrmPunctureMask> schedule;
    schedule.reserve(inputBitCount + kTailBits);
    for (std::size_t index = 0U; index < inputBitCount; ++index) {
        schedule.push_back(pattern.masks[index % pattern.length]);
    }

    if (tailMode == HamDrmPunctureTailMode::Fac) {
        for (std::size_t tail = 0U; tail < kTailBits; ++tail) {
            schedule.push_back(
                pattern.masks[(inputBitCount + tail) % pattern.length]);
        }
    } else {
        if (encodedBitCount < 12U) {
            throw std::invalid_argument("HAMDRM MSC encoded block is too short");
        }
        const std::size_t tailIndex =
            (encodedBitCount - 12U) % pattern.outputBits;
        if (tailIndex >= kTailPatterns.size()) {
            throw std::invalid_argument("unsupported HAMDRM MSC tail pattern");
        }
        schedule.insert(schedule.end(), kTailPatterns[tailIndex].begin(),
                        kTailPatterns[tailIndex].end());
    }

    std::size_t scheduledBits = 0U;
    for (const auto mask : schedule) {
        scheduledBits += maskBits(mask);
    }
    if (scheduledBits != encodedBitCount) {
        throw std::invalid_argument("HAMDRM puncturing schedule length mismatch");
    }
    return schedule;
}

HamDrmValueResult<std::vector<std::uint8_t>> hamDrmConvolutionEncode(
    const std::vector<std::uint8_t>& bits,
    std::size_t patternIndex,
    std::size_t encodedBitCount,
    HamDrmPunctureTailMode tailMode)
{
    if (!isBinary(bits)) {
        return {std::nullopt,
                codingError(HamDrmErrorCode::Malformed,
                            "HAMDRM convolutional input is not binary")};
    }
    std::vector<HamDrmPunctureMask> schedule;
    try {
        schedule = hamDrmPunctureSchedule(bits.size(), patternIndex,
                                          encodedBitCount, tailMode);
    } catch (const std::exception& error) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::InvalidArgument,
                                      error.what())};
    }

    std::vector<std::uint8_t> output;
    output.reserve(encodedBitCount);
    std::uint8_t shiftRegister = 0U;
    for (std::size_t step = 0U; step < schedule.size(); ++step) {
        shiftRegister = static_cast<std::uint8_t>(shiftRegister << 1U);
        if (step < bits.size()) {
            shiftRegister = static_cast<std::uint8_t>(shiftRegister
                                                      | bits[step]);
        }
        const auto mask = static_cast<std::uint8_t>(schedule[step]);
        for (std::size_t generator = 0U; generator < kGeneratorMasks.size();
             ++generator) {
            if ((mask & static_cast<std::uint8_t>(1U << generator)) != 0U) {
                output.push_back(parity(static_cast<std::uint8_t>(
                    shiftRegister & kGeneratorMasks[generator])));
            }
        }
    }
    return {std::move(output), HamDrmStatus::success()};
}

HamDrmValueResult<HamDrmConvolutionDecodeResult> hamDrmViterbiDecode(
    const std::vector<std::uint8_t>& encodedBits,
    std::size_t inputBitCount,
    std::size_t patternIndex,
    HamDrmPunctureTailMode tailMode)
{
    if (!isBinary(encodedBits)) {
        return {std::nullopt,
                codingError(HamDrmErrorCode::Malformed,
                            "HAMDRM Viterbi input is not binary")};
    }
    std::vector<HamDrmPunctureMask> schedule;
    try {
        schedule = hamDrmPunctureSchedule(inputBitCount, patternIndex,
                                          encodedBits.size(), tailMode);
    } catch (const std::exception& error) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::InvalidArgument,
                                      error.what())};
    }

    constexpr std::size_t infinity =
        std::numeric_limits<std::size_t>::max() / 4U;
    std::array<std::size_t, kStateCount> metric {};
    std::array<std::size_t, kStateCount> nextMetric {};
    metric.fill(infinity);
    metric[0U] = 0U;
    std::vector<std::uint8_t> predecessors(schedule.size() * kStateCount,
                                            0U);
    std::size_t observedOffset = 0U;

    for (std::size_t step = 0U; step < schedule.size(); ++step) {
        nextMetric.fill(infinity);
        const auto punctureMask = static_cast<std::uint8_t>(schedule[step]);
        for (std::size_t state = 0U; state < kStateCount; ++state) {
            if (metric[state] == infinity) {
                continue;
            }
            for (std::uint8_t input = 0U; input <= 1U; ++input) {
                const std::uint8_t shiftRegister = static_cast<std::uint8_t>(
                    (state << 1U) | input);
                const std::size_t nextState = shiftRegister & 0x3FU;
                std::size_t branchMetric = 0U;
                std::size_t localOffset = observedOffset;
                for (std::size_t generator = 0U;
                     generator < kGeneratorMasks.size(); ++generator) {
                    if ((punctureMask
                         & static_cast<std::uint8_t>(1U << generator)) != 0U) {
                        const std::uint8_t expected = parity(
                            static_cast<std::uint8_t>(
                                shiftRegister & kGeneratorMasks[generator]));
                        branchMetric += static_cast<std::size_t>(
                            expected != encodedBits[localOffset]);
                        ++localOffset;
                    }
                }
                const std::size_t candidate = metric[state] + branchMetric;
                if (candidate < nextMetric[nextState]) {
                    nextMetric[nextState] = candidate;
                    predecessors[step * kStateCount + nextState] =
                        static_cast<std::uint8_t>(state);
                }
            }
        }
        observedOffset += maskBits(schedule[step]);
        metric = nextMetric;
    }

    if (metric[0U] == infinity) {
        return {std::nullopt,
                codingError(HamDrmErrorCode::Malformed,
                            "HAMDRM Viterbi trellis has no terminated path")};
    }
    std::vector<std::uint8_t> decodedWithTail(schedule.size(), 0U);
    std::size_t state = 0U;
    for (std::size_t step = schedule.size(); step-- > 0U;) {
        decodedWithTail[step] = static_cast<std::uint8_t>(state & 1U);
        state = predecessors[step * kStateCount + state];
    }
    if (state != 0U
            || std::any_of(decodedWithTail.begin()
                               + static_cast<std::ptrdiff_t>(inputBitCount),
                           decodedWithTail.end(),
                           [](std::uint8_t bit) { return bit != 0U; })) {
        return {std::nullopt,
                codingError(HamDrmErrorCode::Malformed,
                            "HAMDRM Viterbi path violates zero termination")};
    }
    decodedWithTail.resize(inputBitCount);
    return {HamDrmConvolutionDecodeResult {std::move(decodedWithTail),
                                           metric[0U]},
            HamDrmStatus::success()};
}

} // namespace decodium::sstv::hamdrm::channel
