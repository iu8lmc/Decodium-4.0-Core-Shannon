// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../HamDrmTypes.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace decodium::sstv::hamdrm::waveform {

constexpr std::size_t kHamDrmPacketOverheadBytes = 3U;

struct HamDrmPacketParameters final
{
    std::size_t bodyBytes {0U};
    std::uint8_t packetId {0U};
    std::size_t maximumDataUnitBytes {16U * 1024U};
};

struct HamDrmPacketizedDataUnit final
{
    std::vector<std::vector<std::uint8_t>> packets;
    std::uint8_t nextContinuityIndex {0U};
};

HamDrmValueResult<HamDrmPacketizedDataUnit> hamDrmPacketizeDataUnit(
    const std::vector<std::uint8_t>& dataUnit,
    const HamDrmPacketParameters& parameters,
    std::uint8_t initialContinuityIndex);

struct HamDrmPacketReassemblyResult final
{
    std::optional<std::vector<std::uint8_t>> dataUnit;
};

class HamDrmPacketReassembler final
{
public:
    explicit HamDrmPacketReassembler(HamDrmPacketParameters parameters);

    HamDrmValueResult<HamDrmPacketReassemblyResult> push(
        const std::vector<std::uint8_t>& packet);
    void reset() noexcept;

private:
    HamDrmPacketParameters parameters_;
    std::vector<std::uint8_t> pending_;
    std::uint8_t lastContinuityIndex_ {0U};
    bool active_ {false};
};

} // namespace decodium::sstv::hamdrm::waveform
