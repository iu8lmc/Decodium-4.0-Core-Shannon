// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../HamDrmTypes.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace decodium::sstv::hamdrm::channel {

constexpr std::size_t kHamDrmMaximumChannelBits = 1U << 20U;

enum class HamDrmPunctureTailMode : std::uint8_t {
    Msc,
    Fac,
};

// Bit n selects generator n.  The values make the DRM patterns 0001, 0011,
// 0101, 0111 and 1111 directly auditable without depending on legacy enums.
enum class HamDrmPunctureMask : std::uint8_t {
    G0 = 0x01U,
    G0G1 = 0x03U,
    G0G2 = 0x05U,
    G0G1G2 = 0x07U,
    G0G1G2G3 = 0x0FU,
};

struct HamDrmConvolutionDecodeResult final
{
    std::vector<std::uint8_t> bits;
    std::size_t pathMetric {0U};
};

std::vector<std::uint8_t> hamDrmEnergyDisperse(
    const std::vector<std::uint8_t>& bits);

std::vector<HamDrmPunctureMask> hamDrmPunctureSchedule(
    std::size_t inputBitCount,
    std::size_t patternIndex,
    std::size_t encodedBitCount,
    HamDrmPunctureTailMode tailMode);

HamDrmValueResult<std::vector<std::uint8_t>> hamDrmConvolutionEncode(
    const std::vector<std::uint8_t>& bits,
    std::size_t patternIndex,
    std::size_t encodedBitCount,
    HamDrmPunctureTailMode tailMode);

HamDrmValueResult<HamDrmConvolutionDecodeResult> hamDrmViterbiDecode(
    const std::vector<std::uint8_t>& encodedBits,
    std::size_t inputBitCount,
    std::size_t patternIndex,
    HamDrmPunctureTailMode tailMode);

} // namespace decodium::sstv::hamdrm::channel
