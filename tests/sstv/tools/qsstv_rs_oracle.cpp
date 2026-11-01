// SPDX-License-Identifier: GPL-3.0-or-later
// Developer oracle: links the separately checked-out QSSTV rs.cpp unchanged.
#include "src/sstv/digital/HamDrmRsCodec.h"
#include "rs.h"
#include "reedsolomoncoder.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace decodium::sstv::hamdrm;

int main()
{
    constexpr std::array<int, 4> sizes {224, 192, 160, 128};
    for (unsigned level = 1; level <= 4; ++level) {
        const auto k = static_cast<std::size_t>(sizes[level - 1U]);
        for (const std::size_t length : {220U, 997U}) {
            std::vector<std::uint8_t> image(length);
            for (std::size_t i = 0; i < image.size(); ++i)
                image[i] = static_cast<std::uint8_t>((i * 41U + level * 19U) & 255U);
            const auto encoded = encodeHamDrmRs(image, "jpg", level);
            if (!encoded.ok()) return 1;
            QByteArray reference(reinterpret_cast<const char*>(image.data()), static_cast<qsizetype>(image.size()));
            reedSolomonCoder referenceEncoder;
            if (!referenceEncoder.encode(reference, QStringLiteral("jpg"), static_cast<eRSType>(level))) return 5;
            if (reference.size() != static_cast<qsizetype>(encoded.value->size())
                || !std::equal(encoded.value->begin(), encoded.value->end(),
                    reinterpret_cast<const std::uint8_t*>(reference.constData()))) return 6;
            const auto rows = encoded.value->size() / 255U;
            init_rs(static_cast<int>(k));
            for (std::size_t row = 0; row < rows; ++row) {
                std::array<dtype, 255> block {};
                for (std::size_t column = 0; column < k; ++column)
                    block[column] = encoded.value->at(column * rows + row);
                encode_rs(block.data(), block.data() + k);
                for (std::size_t column = k; column < block.size(); ++column)
                    if (block[column] != encoded.value->at(column * rows + row)) return 2;
                std::array<int, 255> positions {};
                constexpr std::size_t erasureCount = 10U;
                for (std::size_t i = 0; i < erasureCount; ++i) {
                    positions[i] = static_cast<int>(i * 7U);
                    block[static_cast<std::size_t>(positions[i])] ^= 0xa5U;
                }
                if (eras_dec_rs(block.data(), positions.data(), static_cast<int>(erasureCount)) < 0) return 3;
                for (std::size_t column = 0; column < block.size(); ++column)
                    if (block[column] != encoded.value->at(column * rows + row)) return 4;
            }
            auto damaged = *encoded.value;
            std::vector<std::size_t> declared;
            std::array<dtype, 255> referenceDamaged {};
            std::array<int, 255> referenceErasures {};
            for (std::size_t column = 0; column < 255U; ++column)
                referenceDamaged[column] = encoded.value->at(column * rows);
            for (std::size_t i = 0; i < 10U; ++i) {
                const auto offset = i * 7U * rows;
                damaged[offset] ^= 0xa5U;
                referenceDamaged[i * 7U] ^= 0xa5U;
                if (i < 4U) {
                    declared.push_back(offset);
                    referenceErasures[i] = static_cast<int>(i * 7U);
                }
            }
            if (eras_dec_rs(referenceDamaged.data(), referenceErasures.data(), 4) < 0) return 8;
            for (std::size_t column = 0; column < 255U; ++column)
                if (referenceDamaged[column] != encoded.value->at(column * rows)) return 9;
            const auto corrected = decodeHamDrmRs(damaged, level, declared);
            if (!corrected.ok() || corrected.value->bytes != image
                || corrected.value->repairedContainer != *encoded.value) return 7;
        }
    }
    std::cout << "QSSTV parity and erasure A/B checks passed for rs1-rs4\n";
    return 0;
}
