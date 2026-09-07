// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/digital/HamDrmBsrCodec.h"
#include "src/sstv/digital/HamDrmImageValidator.h"
#include "src/sstv/digital/HamDrmMotCodec.h"

#include <cstddef>
#include <cstdint>

using namespace decodium::sstv::hamdrm;

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      std::size_t size)
{
    constexpr std::size_t maximumInputBytes = 256U * 1024U;
    if (data == nullptr || size > maximumInputBytes) {
        return 0;
    }

    (void) parseHamDrmMotDataGroup(data, size);
    (void) parseHamDrmBsr(data, size);
    const std::uint16_t transportId = size >= 2U
        ? static_cast<std::uint16_t>(
              (static_cast<std::uint16_t>(data[0]) << 8U) | data[1])
        : 0U;
    const auto header = parseHamDrmMotHeader(data, size, transportId);
    if (header.ok() && header.value->bodySize == size) {
        (void) validateHamDrmImage(*header.value, data, size);
    }

    // Exercise every bounded image-container parser without trusting fuzzer
    // bytes as filesystem names or allocation sizes.
    HamDrmMotObjectMetadata metadata;
    metadata.bodySize = static_cast<std::uint32_t>(size);
    switch (size == 0U ? 0U : data[0] % 5U) {
    case 0U:
        metadata.filename = "fuzz.jpg";
        metadata.mimeType = "image/jpeg";
        break;
    case 1U:
        metadata.filename = "fuzz.jp2";
        metadata.mimeType = "image/jp2";
        break;
    case 2U:
        metadata.filename = "fuzz.png";
        metadata.mimeType = "image/png";
        break;
    case 3U:
        metadata.filename = "fuzz.gif";
        metadata.mimeType = "image/gif";
        break;
    default:
        metadata.filename = "fuzz.bmp";
        metadata.mimeType = "image/bmp";
        break;
    }
    (void) validateHamDrmImage(metadata, data, size);
    return 0;
}
