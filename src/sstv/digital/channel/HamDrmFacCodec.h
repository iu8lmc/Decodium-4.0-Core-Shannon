// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../HamDrmTypes.h"
#include "../phy/HamDrmComplexTransform.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace decodium::sstv::hamdrm::channel {

constexpr std::size_t kHamDrmFacPayloadBits = 48U;
constexpr std::size_t kHamDrmFacCells = 45U;

struct HamDrmFacParameters final
{
    std::size_t frameIdentity {0U};
    HamDrmOccupiedBandwidth occupiedBandwidth {
        HamDrmOccupiedBandwidth::Hz2300};
    HamDrmInterleaver interleaver {HamDrmInterleaver::Long};
    HamDrmConstellation mscConstellation {HamDrmConstellation::Qam16};
    HamDrmProtection protection {HamDrmProtection::High};
    std::uint8_t packetId {0U};
    std::string callsign;
};

struct HamDrmFacDecodeResult final
{
    HamDrmFacParameters parameters;
    std::string callsignFragment;
    std::size_t correctedBitMetric {0U};
};

HamDrmValueResult<std::vector<std::uint8_t>> hamDrmEncodeFacPayloadBits(
    const HamDrmFacParameters& parameters);
HamDrmValueResult<HamDrmFacDecodeResult> hamDrmDecodeFacPayloadBits(
    const std::vector<std::uint8_t>& payloadBits,
    std::size_t correctedBitMetric = 0U);

HamDrmValueResult<std::vector<phy::HamDrmComplex>> hamDrmEncodeFacCells(
    const HamDrmFacParameters& parameters);
HamDrmValueResult<HamDrmFacDecodeResult> hamDrmDecodeFacCells(
    const std::vector<phy::HamDrmComplex>& cells);

} // namespace decodium::sstv::hamdrm::channel
