// SPDX-License-Identifier: GPL-3.0-or-later

#include "HamDrmCrc.h"

namespace decodium::sstv::hamdrm {

std::uint16_t hamDrmCrc16X25(const std::uint8_t* data,
                            std::size_t size) noexcept
{
    if (data == nullptr && size != 0U) {
        return 0U;
    }

    std::uint16_t crc = 0xffffU;
    for (std::size_t index = 0U; index < size; ++index) {
        crc ^= data[index];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1U) != 0U
                ? static_cast<std::uint16_t>((crc >> 1U) ^ 0x8408U)
                : static_cast<std::uint16_t>(crc >> 1U);
        }
    }
    return static_cast<std::uint16_t>(crc ^ 0xffffU);
}

} // namespace decodium::sstv::hamdrm
