// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/digital/HamDrmImageValidator.h"
#include "src/sstv/digital/HamDrmJpeg2000Codec.h"

#include <cstdint>
#include <iostream>
#include <vector>

namespace hamdrm = decodium::sstv::hamdrm;

namespace {

int failures = 0;

void check(bool condition, const char* expression, int line)
{
    if (!condition) {
        std::cerr << "FAIL line " << line << ": " << expression << '\n';
        ++failures;
    }
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

hamdrm::HamDrmRgbaImage makeImage()
{
    hamdrm::HamDrmRgbaImage image;
    image.width = 32U;
    image.height = 24U;
    image.rgba.resize(static_cast<std::size_t>(image.width) * image.height * 4U);
    for (std::uint32_t y = 0U; y < image.height; ++y) {
        for (std::uint32_t x = 0U; x < image.width; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * image.width + x) * 4U;
            image.rgba[offset] = static_cast<std::uint8_t>((x * 7U + y) & 0xffU);
            image.rgba[offset + 1U] = static_cast<std::uint8_t>(
                (y * 11U + x * 2U) & 0xffU);
            image.rgba[offset + 2U] = static_cast<std::uint8_t>(
                (x * 3U + y * 5U + 17U) & 0xffU);
            image.rgba[offset + 3U] = 255U;
        }
    }
    return image;
}

} // namespace

int main()
{
    const auto source = makeImage();
    const auto encoded = hamdrm::encodeHamDrmJpeg2000Lossless(source);
    CHECK(encoded.ok());
    if (!encoded.ok()) {
        std::cerr << encoded.status.detail << '\n';
        return 1;
    }
    CHECK(encoded.value->size() > 100U);

    hamdrm::HamDrmMotObjectMetadata metadata;
    metadata.filename = "roundtrip.jp2";
    metadata.mimeType = "image/jp2";
    metadata.bodySize = static_cast<std::uint32_t>(encoded.value->size());
    const auto boundary = hamdrm::validateHamDrmImage(
        metadata, encoded.value->data(), encoded.value->size());
    CHECK(boundary.ok());
    CHECK(boundary.ok() && boundary.value->width == source.width);
    CHECK(boundary.ok() && boundary.value->height == source.height);

    const auto decoded = hamdrm::decodeHamDrmJpeg2000(
        encoded.value->data(), encoded.value->size());
    CHECK(decoded.ok());
    CHECK(decoded.ok() && decoded.value->width == source.width);
    CHECK(decoded.ok() && decoded.value->height == source.height);
    CHECK(decoded.ok() && decoded.value->rgba == source.rgba);

    std::vector<std::uint8_t> truncated(
        encoded.value->begin(),
        encoded.value->begin()
            + static_cast<std::ptrdiff_t>(encoded.value->size() / 2U));
    CHECK(!hamdrm::decodeHamDrmJpeg2000(
               truncated.data(), truncated.size()).ok());

    hamdrm::HamDrmLimits tinyOutput;
    tinyOutput.maximumObjectBytes = 64U;
    CHECK(!hamdrm::encodeHamDrmJpeg2000Lossless(source, tinyOutput).ok());

    hamdrm::HamDrmLimits tinyDimensions;
    tinyDimensions.maximumImageDimension = 16U;
    CHECK(!hamdrm::decodeHamDrmJpeg2000(
               encoded.value->data(), encoded.value->size(),
               tinyDimensions).ok());

    if (failures != 0) {
        std::cerr << failures << " OpenJPEG checks failed\n";
        return 1;
    }
    std::cout << "HAMDRM OpenJPEG checks passed\n";
    return 0;
}
