// SPDX-License-Identifier: GPL-3.0-or-later

#include "HamDrmImageValidator.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <optional>
#include <utility>

namespace decodium::sstv::hamdrm {
namespace {

std::uint16_t big16(const std::uint8_t* data) noexcept
{
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(data[0]) << 8U) | data[1]);
}

std::uint32_t big32(const std::uint8_t* data) noexcept
{
    return (static_cast<std::uint32_t>(data[0]) << 24U)
        | (static_cast<std::uint32_t>(data[1]) << 16U)
        | (static_cast<std::uint32_t>(data[2]) << 8U)
        | data[3];
}

std::uint64_t big64(const std::uint8_t* data) noexcept
{
    return (static_cast<std::uint64_t>(big32(data)) << 32U)
        | big32(data + 4U);
}

std::uint16_t little16(const std::uint8_t* data) noexcept
{
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(data[0])
        | (static_cast<std::uint16_t>(data[1]) << 8U));
}

std::uint32_t little32(const std::uint8_t* data) noexcept
{
    return static_cast<std::uint32_t>(data[0])
        | (static_cast<std::uint32_t>(data[1]) << 8U)
        | (static_cast<std::uint32_t>(data[2]) << 16U)
        | (static_cast<std::uint32_t>(data[3]) << 24U);
}

bool jpegStartOfFrame(std::uint8_t marker) noexcept
{
    switch (marker) {
    case 0xc0U:
    case 0xc1U:
    case 0xc2U:
    case 0xc3U:
    case 0xc5U:
    case 0xc6U:
    case 0xc7U:
    case 0xc9U:
    case 0xcaU:
    case 0xcbU:
    case 0xcdU:
    case 0xceU:
    case 0xcfU:
        return true;
    default:
        return false;
    }
}

HamDrmValueResult<std::pair<std::uint32_t, std::uint32_t>> jpegDimensions(
    const std::uint8_t* data,
    std::size_t size)
{
    if (size < 4U || data[0] != 0xffU || data[1] != 0xd8U) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                      "JPEG SOI signature is missing")};
    }
    std::size_t offset = 2U;
    while (offset < size) {
        if (data[offset] != 0xffU) {
            return {std::nullopt,
                    HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                          "JPEG marker alignment is invalid")};
        }
        while (offset < size && data[offset] == 0xffU) {
            ++offset;
        }
        if (offset >= size) {
            break;
        }
        const std::uint8_t marker = data[offset++];
        if (marker == 0xd8U || marker == 0x01U
            || (marker >= 0xd0U && marker <= 0xd7U)) {
            continue;
        }
        if (marker == 0xd9U || marker == 0xdaU) {
            break;
        }
        if (size - offset < 2U) {
            return {std::nullopt,
                    HamDrmStatus::failure(HamDrmErrorCode::Truncated,
                                          "JPEG segment length is missing")};
        }
        const std::size_t segmentLength = big16(data + offset);
        if (segmentLength < 2U || segmentLength > size - offset) {
            return {std::nullopt,
                    HamDrmStatus::failure(HamDrmErrorCode::Truncated,
                                          "JPEG segment exceeds object")};
        }
        if (jpegStartOfFrame(marker)) {
            if (segmentLength < 8U) {
                return {std::nullopt,
                        HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                              "JPEG SOF segment is too short")};
            }
            const std::uint32_t height = big16(data + offset + 3U);
            const std::uint32_t width = big16(data + offset + 5U);
            if (width == 0U || height == 0U || data[offset + 7U] == 0U) {
                return {std::nullopt,
                        HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                              "JPEG dimensions are invalid")};
            }
            return {std::make_pair(width, height), HamDrmStatus::success()};
        }
        offset += segmentLength;
    }
    return {std::nullopt,
            HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                  "JPEG has no supported SOF marker")};
}

struct Jp2Box final
{
    std::uint32_t type {0U};
    std::size_t payloadOffset {0U};
    std::size_t endOffset {0U};
};

HamDrmValueResult<Jp2Box> jp2BoxAt(const std::uint8_t* data,
                                   std::size_t containerEnd,
                                   std::size_t offset)
{
    if (offset > containerEnd || containerEnd - offset < 8U) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::Truncated,
                                      "JPEG2000 box header is truncated")};
    }
    const std::uint32_t shortLength = big32(data + offset);
    const std::uint32_t type = big32(data + offset + 4U);
    std::size_t headerBytes = 8U;
    std::uint64_t boxBytes = shortLength;
    if (shortLength == 1U) {
        if (containerEnd - offset < 16U) {
            return {std::nullopt,
                    HamDrmStatus::failure(HamDrmErrorCode::Truncated,
                                          "JPEG2000 XLBox is truncated")};
        }
        headerBytes = 16U;
        boxBytes = big64(data + offset + 8U);
    } else if (shortLength == 0U) {
        boxBytes = containerEnd - offset;
    }
    if (boxBytes < headerBytes
        || boxBytes > static_cast<std::uint64_t>(containerEnd - offset)) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                      "JPEG2000 box length is invalid")};
    }
    return {Jp2Box {type, offset + headerBytes,
                    offset + static_cast<std::size_t>(boxBytes)},
            HamDrmStatus::success()};
}

HamDrmValueResult<std::pair<std::uint32_t, std::uint32_t>> jp2Dimensions(
    const std::uint8_t* data,
    std::size_t size)
{
    constexpr std::array<std::uint8_t, 12> signature {
        0x00U, 0x00U, 0x00U, 0x0cU, 0x6aU, 0x50U,
        0x20U, 0x20U, 0x0dU, 0x0aU, 0x87U, 0x0aU,
    };
    if (size < signature.size()
        || !std::equal(signature.begin(), signature.end(), data)) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                      "JPEG2000 JP2 signature is missing")};
    }
    constexpr std::uint32_t jp2h = 0x6a703268U;
    constexpr std::uint32_t ihdr = 0x69686472U;
    std::size_t offset = signature.size();
    while (offset < size) {
        auto top = jp2BoxAt(data, size, offset);
        if (!top.ok()) {
            return {std::nullopt, top.status};
        }
        if (top.value->type == jp2h) {
            std::size_t childOffset = top.value->payloadOffset;
            while (childOffset < top.value->endOffset) {
                auto child = jp2BoxAt(data, top.value->endOffset, childOffset);
                if (!child.ok()) {
                    return {std::nullopt, child.status};
                }
                if (child.value->type == ihdr) {
                    if (child.value->endOffset - child.value->payloadOffset
                        < 14U) {
                        return {std::nullopt,
                                HamDrmStatus::failure(
                                    HamDrmErrorCode::Truncated,
                                    "JPEG2000 image header is truncated")};
                    }
                    const std::uint32_t height = big32(
                        data + child.value->payloadOffset);
                    const std::uint32_t width = big32(
                        data + child.value->payloadOffset + 4U);
                    const std::uint16_t components = big16(
                        data + child.value->payloadOffset + 8U);
                    if (width == 0U || height == 0U || components == 0U) {
                        return {std::nullopt,
                                HamDrmStatus::failure(
                                    HamDrmErrorCode::Malformed,
                                    "JPEG2000 dimensions are invalid")};
                    }
                    return {std::make_pair(width, height),
                            HamDrmStatus::success()};
                }
                childOffset = child.value->endOffset;
            }
        }
        offset = top.value->endOffset;
    }
    return {std::nullopt,
            HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                  "JPEG2000 JP2 image header is missing")};
}

HamDrmStatus validateDimensions(std::uint32_t width,
                                std::uint32_t height,
                                const HamDrmLimits& limits)
{
    if (width == 0U || height == 0U || width > limits.maximumImageDimension
        || height > limits.maximumImageDimension
        || static_cast<std::uint64_t>(width) * height
            > limits.maximumImagePixels) {
        return HamDrmStatus::failure(HamDrmErrorCode::LimitExceeded,
                                     "image dimensions exceed HAMDRM policy");
    }
    return HamDrmStatus::success();
}

} // namespace

HamDrmValueResult<HamDrmImageInfo> validateHamDrmImage(
    const HamDrmMotObjectMetadata& metadata,
    const std::uint8_t* data,
    std::size_t size,
    const HamDrmLimits& limits)
{
    if (data == nullptr || size == 0U || size != metadata.bodySize) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::InconsistentObject,
                                      "image bytes do not match MOT body size")};
    }
    if (size > limits.maximumObjectBytes) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::LimitExceeded,
                                      "image object exceeds HAMDRM policy")};
    }

    HamDrmImageInfo info;
    HamDrmValueResult<std::pair<std::uint32_t, std::uint32_t>> dimensions;
    if (metadata.mimeType == "image/jpeg") {
        info.format = HamDrmImageFormat::Jpeg;
        dimensions = jpegDimensions(data, size);
    } else if (metadata.mimeType == "image/jp2") {
        info.format = HamDrmImageFormat::Jpeg2000;
        dimensions = jp2Dimensions(data, size);
    } else if (metadata.mimeType == "image/png") {
        constexpr std::array<std::uint8_t, 8> signature {
            0x89U, 0x50U, 0x4eU, 0x47U, 0x0dU, 0x0aU, 0x1aU, 0x0aU,
        };
        if (size < 33U || !std::equal(signature.begin(), signature.end(), data)
            || big32(data + 8U) != 13U
            || std::memcmp(data + 12U, "IHDR", 4U) != 0) {
            return {std::nullopt,
                    HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                          "PNG signature or IHDR is invalid")};
        }
        info.format = HamDrmImageFormat::Png;
        dimensions = {std::make_pair(big32(data + 16U), big32(data + 20U)),
                      HamDrmStatus::success()};
    } else if (metadata.mimeType == "image/gif") {
        if (size < 10U
            || (std::memcmp(data, "GIF87a", 6U) != 0
                && std::memcmp(data, "GIF89a", 6U) != 0)) {
            return {std::nullopt,
                    HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                          "GIF signature is invalid")};
        }
        info.format = HamDrmImageFormat::Gif;
        dimensions = {std::make_pair(
                          static_cast<std::uint32_t>(little16(data + 6U)),
                          static_cast<std::uint32_t>(little16(data + 8U))),
                      HamDrmStatus::success()};
    } else if (metadata.mimeType == "image/bmp") {
        if (size < 26U || data[0] != 'B' || data[1] != 'M') {
            return {std::nullopt,
                    HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                          "BMP signature is invalid")};
        }
        const std::uint32_t dibSize = little32(data + 14U);
        std::uint32_t width = 0U;
        std::uint32_t height = 0U;
        if (dibSize == 12U) {
            width = little16(data + 18U);
            height = little16(data + 20U);
        } else if (dibSize >= 40U && size >= 26U) {
            const std::int32_t signedWidth = static_cast<std::int32_t>(
                little32(data + 18U));
            const std::int32_t signedHeight = static_cast<std::int32_t>(
                little32(data + 22U));
            if (signedWidth <= 0 || signedHeight == 0
                || signedHeight == std::numeric_limits<std::int32_t>::min()) {
                return {std::nullopt,
                        HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                              "BMP dimensions are invalid")};
            }
            width = static_cast<std::uint32_t>(signedWidth);
            height = static_cast<std::uint32_t>(
                signedHeight < 0 ? -signedHeight : signedHeight);
        } else {
            return {std::nullopt,
                    HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                          "BMP DIB header is unsupported")};
        }
        info.format = HamDrmImageFormat::Bmp;
        dimensions = {std::make_pair(width, height), HamDrmStatus::success()};
    } else {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::UnsupportedContent,
                                      "MOT object is not an allowed image")};
    }
    if (!dimensions.ok()) {
        return {std::nullopt, dimensions.status};
    }
    info.mimeType = metadata.mimeType;
    info.width = dimensions.value->first;
    info.height = dimensions.value->second;
    if (const auto status = validateDimensions(info.width, info.height, limits);
        !status.ok()) {
        return {std::nullopt, status};
    }
    return {std::move(info), HamDrmStatus::success()};
}

} // namespace decodium::sstv::hamdrm
