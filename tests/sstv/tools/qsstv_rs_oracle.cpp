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

QString rxDRMImagesPath = QStringLiteral("/tmp");

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
    // Compare the observed 10,546/10,745-byte discrepancy with synthetic
    // RS1 objects of that length and an adjacent control. This does not
    // establish the original recording's RS level or container contents.
    for (const std::size_t length : {10546U, 10547U}) {
        std::vector<std::uint8_t> image(length);
        for (std::size_t i = 0; i < length; ++i)
            image[i] = static_cast<std::uint8_t>((i * 41U + 19U) & 255U);
        auto encoded = encodeHamDrmRs(image, "jpg", 1U);
        if (!encoded.ok()) return 10;
        const auto own = decodeHamDrmRs(*encoded.value, 1U);
        if (!own.ok() || own.value->bytes != image) return 11;
        QByteArray wire(reinterpret_cast<const char*>(encoded.value->data()),
                        static_cast<qsizetype>(encoded.value->size()));
        QList<int> erasures {1, static_cast<int>(wire.size())};
        QString filename;
        QByteArray recovered;
        reedSolomonCoder decoder;
        if (!decoder.decode(wire, QStringLiteral("fixture.rs1"), filename,
                            recovered, QStringLiteral("rs1"), erasures)) return 12;
        const std::size_t surplus = 224U - length % 224U;
        const int signedSurplus = static_cast<signed char>(surplus);
        const auto rows = encoded.value->size() / 255U;
        const auto qsstvSize = std::min(rows * 224U - 7U,
            static_cast<std::size_t>(static_cast<int>(rows * 224U)
                - (static_cast<char>(-1) < 0 ? signedSurplus : static_cast<int>(surplus))));
        if (recovered.size() != static_cast<qsizetype>(qsstvSize)
            || qsstvSize < length
            || !std::equal(image.begin(), image.end(),
                           reinterpret_cast<const std::uint8_t*>(recovered.constData()))
            || !std::all_of(recovered.constData() + length, recovered.constData() + qsstvSize,
                            [](char byte) { return byte == 0; })) return 13;
        std::cout << "RS1 length " << length << ": QSSTV " << qsstvSize
                  << ", trailing zeros " << qsstvSize - length << '\n';
    }
    std::cout << "QSSTV parity, erasure and RS1 decoder A/B checks passed\n";
    return 0;
}
