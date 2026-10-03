// SPDX-License-Identifier: GPL-3.0-or-later

#include "HamDrmChannelCrc.h"

#include <stdexcept>

namespace decodium::sstv::hamdrm::channel {

namespace {

template<typename Word>
Word crcMsbFirst(const std::uint8_t* data,
                 std::size_t size,
                 Word polynomial,
                 Word initial,
                 Word highBit) noexcept
{
    Word remainder = initial;
    if (data == nullptr && size != 0U) {
        return static_cast<Word>(remainder ^ initial);
    }
    for (std::size_t index = 0U; index < size; ++index) {
        remainder ^= static_cast<Word>(
            static_cast<Word>(data[index]) << (sizeof(Word) * 8U - 8U));
        for (unsigned bit = 0U; bit < 8U; ++bit) {
            const bool top = (remainder & highBit) != 0U;
            remainder = static_cast<Word>(remainder << 1U);
            if (top) {
                remainder = static_cast<Word>(remainder ^ polynomial);
            }
        }
    }
    return static_cast<Word>(remainder ^ initial);
}

} // namespace

std::uint8_t hamDrmChannelCrc8(const std::uint8_t* data,
                              std::size_t size) noexcept
{
    return crcMsbFirst<std::uint8_t>(data, size, 0x1DU, 0xFFU, 0x80U);
}

std::uint16_t hamDrmChannelCrc16(const std::uint8_t* data,
                                std::size_t size) noexcept
{
    return crcMsbFirst<std::uint16_t>(data, size, 0x1021U, 0xFFFFU, 0x8000U);
}

std::vector<std::uint8_t> hamDrmAppendChannelCrc16(
    const std::vector<std::uint8_t>& payload)
{
    if (payload.size() > (1U << 24U)) {
        throw std::length_error("HAMDRM channel CRC payload exceeds bound");
    }
    std::vector<std::uint8_t> packet = payload;
    const std::uint16_t crc = hamDrmChannelCrc16(payload.data(),
                                                 payload.size());
    packet.push_back(static_cast<std::uint8_t>(crc >> 8U));
    packet.push_back(static_cast<std::uint8_t>(crc & 0xFFU));
    return packet;
}

HamDrmValueResult<std::vector<std::uint8_t>> hamDrmVerifyAndStripChannelCrc16(
    const std::vector<std::uint8_t>& packet)
{
    if (packet.size() < 2U) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::Truncated,
                                      "HAMDRM channel CRC16 is missing")};
    }
    const std::size_t payloadSize = packet.size() - 2U;
    const std::uint16_t received = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(packet[payloadSize]) << 8U)
        | packet[payloadSize + 1U]);
    const std::uint16_t expected = hamDrmChannelCrc16(packet.data(),
                                                      payloadSize);
    if (received != expected) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::CrcMismatch,
                                      "HAMDRM channel CRC16 mismatch")};
    }
    return {std::vector<std::uint8_t>(packet.begin(),
                                     packet.begin()
                                         + static_cast<std::ptrdiff_t>(
                                             payloadSize)),
            HamDrmStatus::success()};
}

} // namespace decodium::sstv::hamdrm::channel
