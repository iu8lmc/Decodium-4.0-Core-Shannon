// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../HamDrmTypes.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace decodium::sstv::hamdrm::channel {

// HAMDRM/DRM uses MSB-first CRCs here.  These routines intentionally remain
// separate from transport-layer CRC helpers with reflected bit ordering.
std::uint8_t hamDrmChannelCrc8(const std::uint8_t* data,
                              std::size_t size) noexcept;
std::uint16_t hamDrmChannelCrc16(const std::uint8_t* data,
                                std::size_t size) noexcept;

std::vector<std::uint8_t> hamDrmAppendChannelCrc16(
    const std::vector<std::uint8_t>& payload);
HamDrmValueResult<std::vector<std::uint8_t>> hamDrmVerifyAndStripChannelCrc16(
    const std::vector<std::uint8_t>& packet);

} // namespace decodium::sstv::hamdrm::channel
