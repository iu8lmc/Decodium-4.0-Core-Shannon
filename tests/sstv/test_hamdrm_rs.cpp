// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/sstv/digital/HamDrmRsCodec.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace decodium::sstv::hamdrm;

int main()
{
    for (unsigned level = 1; level <= 4; ++level) {
        const std::size_t k = std::array<std::size_t, 4> {224U, 192U, 160U, 128U}[level - 1U];
        for (const auto length : {k - 7U, k - 6U, k - 1U, k, k + 1U, 2U * k - 7U, 2U * k}) {
            const std::vector<std::uint8_t> input(length, 0xa7U);
            const auto encoded = encodeHamDrmRs(input, "png", level);
            if (!encoded.ok()) return 7;
            const auto decoded = decodeHamDrmRs(*encoded.value, level);
            if (!decoded.ok() || decoded.value->bytes != input) return 8;
        }
        for (const std::size_t size : {220U, 997U}) {
            std::vector<std::uint8_t> original(size);
            for (std::size_t i = 0; i < size; ++i)
                original[i] = static_cast<std::uint8_t>((i * 41U + level * 19U) & 255U);
            const auto encoded = encodeHamDrmRs(original, "jpg", level);
            if (!encoded.ok()) return 1;
            const auto decoded = decodeHamDrmRs(*encoded.value, level);
            if (!decoded.ok() || decoded.value->bytes != original || decoded.value->extension != "jpg") return 2;
            auto damaged = *encoded.value;
            const std::size_t chunks = damaged.size() / 255U;
            std::vector<std::size_t> erased;
            for (std::size_t i = 0; i < std::min<std::size_t>(10U, damaged.size()); ++i) {
                const std::size_t offset = i * chunks;
                erased.push_back(offset);
                damaged[offset] ^= 0xa5U;
            }
            const auto repaired = decodeHamDrmRs(damaged, level, erased);
            if (!repaired.ok() || repaired.value->bytes != original) return 3;
            const auto corrected = decodeHamDrmRs(damaged, level);
            if (!corrected.ok() || corrected.value->bytes != original) return 4;
            erased.push_back(erased.front());
            if (decodeHamDrmRs(damaged, level, erased).ok()) return 5;
            for (unsigned pattern = 0; pattern < 4; ++pattern) {
                auto mixed = *encoded.value;
                std::vector<std::size_t> known;
                for (std::size_t i = 0; i < 13U; ++i) {
                    const std::size_t position = ((i * 17U + pattern * 11U) % 255U) * chunks;
                    mixed[position] ^= static_cast<std::uint8_t>(0x31U + i);
                    if (i < 5U) known.push_back(position);
                }
                const auto repairedMixed = decodeHamDrmRs(mixed, level, known);
                if (!repairedMixed.ok() || repairedMixed.value->bytes != original
                    || repairedMixed.value->repairedContainer != *encoded.value) return 11;
            }
            // Exercise the exact correction boundary: 2 * errors + erasures = parity.
            const std::size_t parity = 255U - k;
            for (const std::size_t knownCount : std::array<std::size_t, 3>{1U, 7U, parity}) {
                auto boundary = *encoded.value;
                std::vector<std::size_t> known;
                const std::size_t errors = (parity - knownCount) / 2U;
                for (std::size_t i = 0; i < knownCount + errors; ++i) {
                    const std::size_t position = ((i * 19U + 3U) % 255U) * chunks;
                    boundary[position] ^= static_cast<std::uint8_t>(1U + (i % 255U));
                    if (i < knownCount) known.push_back(position);
                }
                const auto recovered = decodeHamDrmRs(boundary, level, known);
                if (!recovered.ok() || recovered.value->bytes != original
                    || recovered.value->repairedContainer != *encoded.value) {
                    std::cerr << "boundary failed level=" << level << " size=" << size
                              << " erasures=" << knownCount << " errors=" << errors << '\n';
                    return 12;
                }
            }
        }
    }
    auto excess = encodeHamDrmRs(std::vector<std::uint8_t>(997U, 0x42U), "jpg", 1U);
    if (!excess.ok()) return 9;
    const auto rows = excess.value->size() / 255U;
    for (std::size_t i = 0; i < 16U; ++i) excess.value->at(i * rows) ^= 0xa5U;
    if (decodeHamDrmRs(*excess.value, 1U).ok()) return 10;
    if (encodeHamDrmRs({1U}, "exe", 1U).ok() || decodeHamDrmRs({1U}, 1U).ok()) return 6;
    std::cout << "HAMDRM RS A/B and erasure checks passed\n";
    return 0;
}
