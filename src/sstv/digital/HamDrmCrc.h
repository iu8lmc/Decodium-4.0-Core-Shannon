// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>

namespace decodium::sstv::hamdrm {

// Reflected CRC-16/X-25 utility. HAMDRM MOT groups use the distinct MSB-first
// CRC-16 implemented in channel::hamDrmChannelCrc16.
std::uint16_t hamDrmCrc16X25(const std::uint8_t* data,
                            std::size_t size) noexcept;

} // namespace decodium::sstv::hamdrm
