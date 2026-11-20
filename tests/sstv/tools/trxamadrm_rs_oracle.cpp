// SPDX-License-Identifier: GPL-3.0-or-later
// Compare an external RS1-RS4 container with Decodium's bounded codec.
#include "src/sstv/digital/HamDrmRsCodec.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <array>
#include <vector>

// Linked to one unmodified archived RS decoder per executable.
extern "C" int eras_dec_rs(unsigned char* data, int* erasures, int count);

int main(int argc, char** argv)
{
    if (argc != 4 || argv[3][0] < '1' || argv[3][0] > '4' || argv[3][1] != '\0') return 2;
    const auto level = static_cast<unsigned>(argv[3][0] - '0');
    const auto read = [](const char* path) {
        std::ifstream input(path, std::ios::binary);
        return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(input), {});
    };
    const auto original = read(argv[1]);
    const auto external = read(argv[2]);
    if (original.empty() || external.empty()) return 3;
    const auto encoded = decodium::sstv::hamdrm::encodeHamDrmRs(original, "jpg", level);
    const auto decoded = decodium::sstv::hamdrm::decodeHamDrmRs(external, level);
    if (!encoded.ok() || *encoded.value != external || !decoded.ok()
        || decoded.value->bytes != original || decoded.value->extension != "jpg"
        || external.size() % 255U != 0U) return 1;
    const auto chunks = external.size() / 255U;
    auto damaged = external;
    std::array<unsigned char, 255> referenceBlock {};
    for (std::size_t i = 0; i < referenceBlock.size(); ++i)
        referenceBlock[i] = external[i * chunks];
    for (std::size_t i = 0; i < 10U; ++i) {
        const auto position = (i * 19U + 3U) % 255U;
        const auto mask = static_cast<unsigned char>(i + 1U);
        damaged[position * chunks] ^= mask;
        referenceBlock[position] ^= mask;
    }
    const auto repaired = decodium::sstv::hamdrm::decodeHamDrmRs(damaged, level);
    std::array<int, 255> noErasures {};
    const int referenceCorrections = eras_dec_rs(referenceBlock.data(), noErasures.data(), 0);
    bool referenceExact = referenceCorrections >= 0;
    for (std::size_t i = 0; i < referenceBlock.size(); ++i)
        referenceExact = referenceExact && referenceBlock[i] == external[i * chunks];
    std::cout << "encoded_exact=" << (encoded.ok() && *encoded.value == external)
              << " decoded_exact=" << (decoded.ok() && decoded.value->bytes == original)
              << " extension_jpg=" << (decoded.ok() && decoded.value->extension == "jpg")
              << " repaired_exact=" << (repaired.ok() && repaired.value->bytes == original
                                            && repaired.value->repairedContainer == external)
              << " reference_repaired=" << referenceExact << '\n';
    return repaired.ok() && repaired.value->bytes == original
        && repaired.value->repairedContainer == external && referenceExact ? 0 : 1;
}
