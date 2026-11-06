// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/digital/HamDrmBsrCodec.h"
#include "src/sstv/digital/HamDrmMotCodec.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      std::size_t size);

namespace hamdrm = decodium::sstv::hamdrm;

namespace {

void exerciseMutations(const std::vector<std::uint8_t>& seed)
{
    (void) LLVMFuzzerTestOneInput(seed.data(), seed.size());
    for (std::size_t length = 0U; length < seed.size(); ++length) {
        (void) LLVMFuzzerTestOneInput(seed.data(), length);
    }
    for (std::size_t index = 0U; index < seed.size(); ++index) {
        auto mutation = seed;
        mutation[index] ^= static_cast<std::uint8_t>(
            1U << static_cast<unsigned>(index % 8U));
        (void) LLVMFuzzerTestOneInput(mutation.data(), mutation.size());
    }
}

} // namespace

int main()
{
    hamdrm::HamDrmMotDataGroup group;
    group.kind = hamdrm::HamDrmMotGroupKind::Body;
    group.transportId = 0x1234U;
    group.lastSegment = true;
    group.payload = {0xffU, 0xd8U, 0xffU, 0xd9U};
    const auto encodedGroup = hamdrm::encodeHamDrmMotDataGroup(group);
    if (!encodedGroup.ok()) {
        std::cerr << "could not create MOT fuzz seed\n";
        return 1;
    }
    exerciseMutations(*encodedGroup.value);

    hamdrm::HamDrmMotObjectMetadata metadata;
    metadata.transportId = group.transportId;
    metadata.bodySize = 4U;
    metadata.filename = "fuzz.jpg";
    const auto encodedHeader = hamdrm::encodeHamDrmMotHeader(metadata);
    if (!encodedHeader.ok()) {
        std::cerr << "could not create MOT header fuzz seed\n";
        return 1;
    }
    exerciseMutations(*encodedHeader.value);

    hamdrm::HamDrmBsrRequest bsr;
    bsr.transportId = group.transportId;
    bsr.segmentSize = 82U;
    bsr.missingSegments = {1U, 2U, 3U, 9U};
    const auto encodedBsr = hamdrm::encodeHamDrmBsr(
        bsr, hamdrm::HamDrmBsrDialect::EasyPalCompatible);
    if (!encodedBsr.ok()) {
        std::cerr << "could not create BSR fuzz seed\n";
        return 1;
    }
    exerciseMutations(*encodedBsr.value);

    std::uint32_t randomState = 0x6d2b79f5U;
    std::vector<std::uint8_t> randomBytes(4'096U);
    for (int iteration = 0; iteration < 1'000; ++iteration) {
        randomState = randomState * 1'664'525U + 1'013'904'223U;
        const std::size_t size = randomState % randomBytes.size();
        for (std::size_t index = 0U; index < size; ++index) {
            randomState = randomState * 1'664'525U + 1'013'904'223U;
            randomBytes[index] = static_cast<std::uint8_t>(randomState >> 24U);
        }
        (void) LLVMFuzzerTestOneInput(randomBytes.data(), size);
    }
    std::cout << "HAMDRM deterministic fuzz smoke passed\n";
    return 0;
}
