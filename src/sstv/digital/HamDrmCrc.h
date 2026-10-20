// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>

namespace decodium::sstv::hamdrm {

// CRC-16/X-25 as used by HAMDRM MOT data groups: poly 0x1021, reflected
// processing, init 0xffff, refin/refout true, xorout 0xffff.
std::uint16_t hamDrmCrc16X25(const std::uint8_t* data,
                            std::size_t size) noexcept;

} // namespace decodium::sstv::hamdrm
