// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../HamDrmTypes.h"
#include "../phy/HamDrmComplexTransform.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace decodium::sstv::hamdrm::channel {

constexpr std::size_t kHamDrmMaximumMscCellsPerFrame = 65'536U;

struct HamDrmChannelSubsetCapabilities final
{
    bool fac {true};
    bool sdc {false};
    bool msc {true};
    bool mscPartA {false};
    bool hierarchicalModulation {false};
};

struct HamDrmMlcProfile final
{
    HamDrmConstellation constellation {HamDrmConstellation::Qam16};
    HamDrmProtection protection {HamDrmProtection::High};
    std::size_t usefulCells {0U};
    std::size_t levelCount {0U};
    std::array<std::size_t, 3U> inputBitsPerLevel {};
    std::array<std::size_t, 3U> puncturePatternIndex {};
    // Zero means no bit interleaver; otherwise the exact t0 value.
    std::array<std::size_t, 3U> bitInterleaverT0 {};
    std::size_t inputBits {0U};
};

struct HamDrmMscDecodeResult final
{
    std::vector<std::uint8_t> bits;
    std::array<std::size_t, 3U> levelPathMetric {};
};

constexpr HamDrmChannelSubsetCapabilities hamDrmChannelSubsetCapabilities()
{
    return {};
}

// This is a deliberate capability boundary, not a stub: the pinned amateur
// QSSTV transmitter does not initialize or process SDC at all.
HamDrmStatus hamDrmSdcSupportStatus();

HamDrmValueResult<HamDrmMlcProfile> hamDrmMlcProfile(
    std::size_t usefulMscCells,
    HamDrmConstellation constellation,
    HamDrmProtection protection);

HamDrmValueResult<std::vector<phy::HamDrmComplex>> hamDrmEncodeMscCells(
    const std::vector<std::uint8_t>& bits,
    const HamDrmMlcProfile& profile);

HamDrmValueResult<HamDrmMscDecodeResult> hamDrmDecodeMscCells(
    const std::vector<phy::HamDrmComplex>& cells,
    const HamDrmMlcProfile& profile);

} // namespace decodium::sstv::hamdrm::channel
