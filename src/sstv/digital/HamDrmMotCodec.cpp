// SPDX-License-Identifier: GPL-3.0-or-later

#include "HamDrmMotCodec.h"

#include "HamDrmCrc.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <utility>

namespace decodium::sstv::hamdrm {
namespace {

constexpr std::uint8_t kExtensionFlag = 0x80U;
constexpr std::uint8_t kCrcFlag = 0x40U;
constexpr std::uint8_t kSessionFlag = 0x20U;
constexpr std::uint8_t kUserAccessFlag = 0x10U;
constexpr std::uint8_t kTransportIdFlag = 0x10U;
constexpr std::size_t kFixedGroupBytesWithoutPayload = 9U;
constexpr std::size_t kCrcBytes = 2U;

std::string lowercaseExtension(const std::string& filename)
{
    const auto dot = filename.find_last_of('.');
    if (dot == std::string::npos || dot + 1U == filename.size()) {
        return {};
    }
    std::string extension = filename.substr(dot + 1U);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char value) {
                       return static_cast<char>(std::tolower(value));
                   });
    return extension;
}

HamDrmStatus contentDescription(const std::string& filename,
                                std::uint8_t& contentType,
                                std::uint16_t& contentSubtype,
                                std::string& mimeType)
{
    const std::string extension = lowercaseExtension(filename);
    contentType = 2U;
    if (extension == "jpg" || extension == "jpeg" || extension == "jfif") {
        contentSubtype = 1U;
        mimeType = "image/jpeg";
    } else if (extension == "jp2") {
        // QSSTV intentionally signals JP2 with the image/JFIF subtype and
        // disambiguates it through ContentName.
        contentSubtype = 1U;
        mimeType = "image/jp2";
    } else if (extension == "png") {
        contentSubtype = 3U;
        mimeType = "image/png";
    } else if (extension == "gif") {
        contentSubtype = 0U;
        mimeType = "image/gif";
    } else if (extension == "bmp") {
        contentSubtype = 2U;
        mimeType = "image/bmp";
    } else if (filename == "bsr.bin") {
        contentSubtype = 1U;
        mimeType = "application/x-hamdrm-bsr";
    } else {
        return HamDrmStatus::failure(HamDrmErrorCode::UnsupportedContent,
                                     "HAMDRM filename has no allowed type");
    }
    return HamDrmStatus::success();
}

HamDrmStatus checkedContentDescription(HamDrmMotObjectMetadata& metadata)
{
    std::uint8_t expectedType = 0U;
    std::uint16_t expectedSubtype = 0U;
    std::string expectedMime;
    const auto status = contentDescription(metadata.filename, expectedType,
                                           expectedSubtype, expectedMime);
    if (!status.ok()) {
        return status;
    }
    const bool unspecified = metadata.contentType == 0U
        && metadata.contentSubtype == 0U && metadata.mimeType.empty();
    if (!unspecified
        && (metadata.contentType != expectedType
            || metadata.contentSubtype != expectedSubtype
            || (!metadata.mimeType.empty()
                && metadata.mimeType != expectedMime))) {
        return HamDrmStatus::failure(
            HamDrmErrorCode::InconsistentObject,
            "MOT content type does not match the safe filename type");
    }
    metadata.contentType = expectedType;
    metadata.contentSubtype = expectedSubtype;
    metadata.mimeType = std::move(expectedMime);
    return HamDrmStatus::success();
}

void appendBigEndian16(std::vector<std::uint8_t>& output,
                       std::uint16_t value)
{
    output.push_back(static_cast<std::uint8_t>(value >> 8U));
    output.push_back(static_cast<std::uint8_t>(value & 0xffU));
}

std::uint16_t readBigEndian16(const std::uint8_t* data) noexcept
{
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(data[0]) << 8U)
        | static_cast<std::uint16_t>(data[1]));
}

HamDrmStatus validateMetadataBounds(const HamDrmMotObjectMetadata& metadata,
                                    const HamDrmLimits& limits)
{
    if (metadata.bodySize == 0U) {
        return HamDrmStatus::failure(HamDrmErrorCode::InvalidArgument,
                                     "empty HAMDRM objects are not supported");
    }
    if (metadata.bodySize > 0x0fffffffU
        || metadata.bodySize > limits.maximumObjectBytes) {
        return HamDrmStatus::failure(HamDrmErrorCode::LimitExceeded,
                                     "MOT body exceeds configured limit");
    }
    return validateHamDrmFilename(metadata.filename, limits);
}

} // namespace

HamDrmStatus validateHamDrmFilename(const std::string& filename,
                                    const HamDrmLimits& limits)
{
    if (filename.empty() || filename.size() > limits.maximumFilenameBytes) {
        return HamDrmStatus::failure(HamDrmErrorCode::UnsafeFilename,
                                     "HAMDRM filename length is invalid");
    }
    if (filename == "." || filename == ".." || filename.front() == '.') {
        return HamDrmStatus::failure(HamDrmErrorCode::UnsafeFilename,
                                     "hidden or relative filename rejected");
    }
    for (const char character : filename) {
        const auto value = static_cast<unsigned char>(character);
        if (value < 0x20U || value > 0x7eU || value == '/'
            || value == '\\') {
            return HamDrmStatus::failure(HamDrmErrorCode::UnsafeFilename,
                                         "filename contains unsafe characters");
        }
    }
    return HamDrmStatus::success();
}

HamDrmValueResult<std::vector<std::uint8_t>> encodeHamDrmMotDataGroup(
    const HamDrmMotDataGroup& group,
    const HamDrmLimits& limits)
{
    if (group.continuityIndex > 15U || group.repetitionIndex > 15U
        || group.segmentRepetitionCount > 7U
        || group.segmentNumber >= limits.maximumSegments
        || group.payload.empty()
        || group.payload.size() > limits.maximumSegmentBytes
        || group.payload.size() > 8'191U) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::InvalidArgument,
                                      "MOT data-group field is out of range")};
    }
    const auto kind = static_cast<std::uint8_t>(group.kind);
    if (kind != static_cast<std::uint8_t>(HamDrmMotGroupKind::Header)
        && kind != static_cast<std::uint8_t>(HamDrmMotGroupKind::Body)) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::InvalidArgument,
                                      "unsupported MOT data-group type")};
    }

    std::vector<std::uint8_t> output;
    output.reserve(kFixedGroupBytesWithoutPayload + group.payload.size()
                   + kCrcBytes);
    output.push_back(static_cast<std::uint8_t>(
        kCrcFlag | kSessionFlag | kUserAccessFlag | kind));
    output.push_back(static_cast<std::uint8_t>(
        (group.continuityIndex << 4U) | group.repetitionIndex));
    appendBigEndian16(output, static_cast<std::uint16_t>(
        (group.lastSegment ? 0x8000U : 0U) | group.segmentNumber));
    output.push_back(0x12U); // Transport ID present; two-byte user field.
    appendBigEndian16(output, group.transportId);
    appendBigEndian16(output, static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(group.segmentRepetitionCount) << 13U)
        | static_cast<std::uint16_t>(group.payload.size())));
    output.insert(output.end(), group.payload.begin(), group.payload.end());
    appendBigEndian16(output, hamDrmCrc16X25(output.data(), output.size()));
    return {std::move(output), HamDrmStatus::success()};
}

HamDrmValueResult<HamDrmMotDataGroup> parseHamDrmMotDataGroup(
    const std::uint8_t* data,
    std::size_t size,
    const HamDrmLimits& limits)
{
    if (data == nullptr) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::InvalidArgument,
                                      "null MOT data group")};
    }
    if (size < kFixedGroupBytesWithoutPayload + 1U + kCrcBytes) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::Truncated,
                                      "MOT data group is truncated")};
    }
    const std::uint8_t flags = data[0];
    if ((flags & kExtensionFlag) != 0U
        || (flags & (kCrcFlag | kSessionFlag | kUserAccessFlag))
            != (kCrcFlag | kSessionFlag | kUserAccessFlag)) {
        return {std::nullopt,
                HamDrmStatus::failure(
                    HamDrmErrorCode::UnsupportedFeature,
                    "MOT group is outside the interoperable HAMDRM subset")};
    }
    const std::uint8_t kindValue = flags & 0x0fU;
    if (kindValue != static_cast<std::uint8_t>(HamDrmMotGroupKind::Header)
        && kindValue != static_cast<std::uint8_t>(HamDrmMotGroupKind::Body)) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::UnsupportedFeature,
                                      "unsupported MOT data-group type")};
    }
    if ((data[4] & kTransportIdFlag) == 0U || (data[4] & 0x0fU) != 2U) {
        return {std::nullopt,
                HamDrmStatus::failure(
                    HamDrmErrorCode::UnsupportedFeature,
                    "MOT group must carry a two-byte transport ID")};
    }
    const std::uint16_t segmentField = readBigEndian16(data + 2U);
    const std::uint16_t segmentHeader = readBigEndian16(data + 7U);
    const std::size_t payloadSize = segmentHeader & 0x1fffU;
    const std::size_t expectedSize = kFixedGroupBytesWithoutPayload
        + payloadSize + kCrcBytes;
    if (payloadSize == 0U || payloadSize > limits.maximumSegmentBytes
        || expectedSize != size) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                      "MOT segment length is inconsistent")};
    }
    const std::uint16_t receivedCrc = readBigEndian16(data + size - kCrcBytes);
    const std::uint16_t computedCrc = hamDrmCrc16X25(data, size - kCrcBytes);
    if (receivedCrc != computedCrc) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::CrcMismatch,
                                      "MOT data-group CRC failed")};
    }

    HamDrmMotDataGroup group;
    group.kind = static_cast<HamDrmMotGroupKind>(kindValue);
    group.continuityIndex = static_cast<std::uint8_t>(data[1] >> 4U);
    group.repetitionIndex = data[1] & 0x0fU;
    group.lastSegment = (segmentField & 0x8000U) != 0U;
    group.segmentNumber = segmentField & 0x7fffU;
    group.transportId = readBigEndian16(data + 5U);
    group.segmentRepetitionCount = static_cast<std::uint8_t>(
        segmentHeader >> 13U);
    group.payload.assign(data + kFixedGroupBytesWithoutPayload,
                         data + kFixedGroupBytesWithoutPayload + payloadSize);
    if (group.segmentNumber >= limits.maximumSegments) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::LimitExceeded,
                                      "MOT segment number exceeds limit")};
    }
    return {std::move(group), HamDrmStatus::success()};
}

HamDrmValueResult<std::vector<std::uint8_t>> encodeHamDrmMotHeader(
    const HamDrmMotObjectMetadata& inputMetadata,
    const HamDrmLimits& limits)
{
    HamDrmMotObjectMetadata metadata = inputMetadata;
    if (const auto status = validateMetadataBounds(metadata, limits);
        !status.ok()) {
        return {std::nullopt, status};
    }
    if (const auto status = checkedContentDescription(metadata); !status.ok()) {
        return {std::nullopt, status};
    }

    const std::size_t headerSize = 17U + metadata.filename.size();
    if (headerSize > 0x1fffU || headerSize > limits.maximumHeaderBytes) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::LimitExceeded,
                                      "MOT header exceeds configured limit")};
    }

    std::vector<std::uint8_t> output;
    output.reserve(headerSize);
    output.push_back(static_cast<std::uint8_t>(metadata.bodySize >> 20U));
    output.push_back(static_cast<std::uint8_t>(metadata.bodySize >> 12U));
    output.push_back(static_cast<std::uint8_t>(metadata.bodySize >> 4U));
    output.push_back(static_cast<std::uint8_t>(
        ((metadata.bodySize & 0x0fU) << 4U) | (headerSize >> 9U)));
    output.push_back(static_cast<std::uint8_t>(headerSize >> 1U));
    output.push_back(static_cast<std::uint8_t>(
        ((headerSize & 1U) << 7U)
        | (static_cast<std::size_t>(metadata.contentType) << 1U)
        | (metadata.contentSubtype >> 8U)));
    output.push_back(static_cast<std::uint8_t>(metadata.contentSubtype));

    output.push_back(0x85U); // PLI=2, TriggerTime parameter.
    output.insert(output.end(), 4U, 0U); // Presentation immediately.
    output.push_back(0x46U); // PLI=1, VersionNumber parameter.
    output.push_back(metadata.version);
    output.push_back(0xccU); // PLI=3, ContentName parameter.
    output.push_back(static_cast<std::uint8_t>(metadata.filename.size() + 1U));
    output.push_back(0U); // EBU Latin charset selector and reserved nibble.
    output.insert(output.end(), metadata.filename.begin(), metadata.filename.end());

    if (output.size() != headerSize) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::InconsistentObject,
                                      "internal MOT header size mismatch")};
    }
    return {std::move(output), HamDrmStatus::success()};
}

HamDrmValueResult<HamDrmMotObjectMetadata> parseHamDrmMotHeader(
    const std::uint8_t* data,
    std::size_t size,
    std::uint16_t transportId,
    const HamDrmLimits& limits)
{
    if (data == nullptr) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::InvalidArgument,
                                      "null MOT header")};
    }
    if (size < 7U) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::Truncated,
                                      "MOT header core is truncated")};
    }
    HamDrmMotObjectMetadata metadata;
    metadata.transportId = transportId;
    metadata.bodySize = (static_cast<std::uint32_t>(data[0]) << 20U)
        | (static_cast<std::uint32_t>(data[1]) << 12U)
        | (static_cast<std::uint32_t>(data[2]) << 4U)
        | (static_cast<std::uint32_t>(data[3]) >> 4U);
    metadata.headerSize = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(data[3] & 0x0fU) << 9U)
        | (static_cast<std::uint16_t>(data[4]) << 1U)
        | (static_cast<std::uint16_t>(data[5]) >> 7U));
    metadata.contentType = static_cast<std::uint8_t>((data[5] >> 1U) & 0x3fU);
    metadata.contentSubtype = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(data[5] & 1U) << 8U) | data[6]);

    if (metadata.headerSize != size || size > limits.maximumHeaderBytes) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                      "MOT header size is inconsistent")};
    }
    if (metadata.bodySize == 0U
        || metadata.bodySize > limits.maximumObjectBytes) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::LimitExceeded,
                                      "MOT body size exceeds configured limit")};
    }

    bool contentNameSeen = false;
    std::size_t offset = 7U;
    while (offset < size) {
        const std::uint8_t parameter = data[offset++];
        const std::uint8_t pli = parameter >> 6U;
        const std::uint8_t parameterId = parameter & 0x3fU;
        std::size_t dataLength = 0U;
        if (pli == 1U) {
            dataLength = 1U;
        } else if (pli == 2U) {
            dataLength = 4U;
        } else if (pli == 3U) {
            if (offset >= size) {
                return {std::nullopt,
                        HamDrmStatus::failure(HamDrmErrorCode::Truncated,
                                              "MOT parameter length missing")};
            }
            const std::uint8_t firstLength = data[offset++];
            if ((firstLength & 0x80U) != 0U) {
                if (offset >= size) {
                    return {std::nullopt,
                            HamDrmStatus::failure(
                                HamDrmErrorCode::Truncated,
                                "extended MOT parameter length missing")};
                }
                dataLength = (static_cast<std::size_t>(firstLength & 0x7fU)
                              << 8U) | data[offset++];
            } else {
                dataLength = firstLength;
            }
        }
        if (dataLength > size - offset) {
            return {std::nullopt,
                    HamDrmStatus::failure(HamDrmErrorCode::Truncated,
                                          "MOT parameter exceeds header")};
        }
        if (parameterId == 6U && dataLength == 1U) {
            metadata.version = data[offset];
        } else if (parameterId == 12U) {
            if (contentNameSeen || dataLength < 2U
                || (data[offset] >> 4U) != 0U) {
                return {std::nullopt,
                        HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                              "invalid MOT ContentName")};
            }
            metadata.filename.assign(
                reinterpret_cast<const char*>(data + offset + 1U),
                dataLength - 1U);
            contentNameSeen = true;
        }
        offset += dataLength;
    }
    if (!contentNameSeen) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                      "MOT ContentName is required")};
    }
    if (const auto status = validateHamDrmFilename(metadata.filename, limits);
        !status.ok()) {
        return {std::nullopt, status};
    }
    if (const auto status = checkedContentDescription(metadata); !status.ok()) {
        return {std::nullopt, status};
    }
    return {std::move(metadata), HamDrmStatus::success()};
}

std::uint16_t qsstvCompatibleTransportId(const std::string& filename) noexcept
{
    std::uint8_t xored = 0U;
    std::uint8_t added = 0U;
    for (std::size_t index = 0U; index < filename.size(); ++index) {
        const auto value = static_cast<std::uint8_t>(filename[index]);
        xored ^= value;
        added = static_cast<std::uint8_t>(added + value);
        added ^= static_cast<std::uint8_t>(index);
    }
    std::uint16_t transportId = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(added) << 8U) | xored);
    if (transportId <= 2U) {
        transportId = static_cast<std::uint16_t>(
            transportId + static_cast<std::uint16_t>(filename.size()));
    }
    return transportId;
}

HamDrmValueResult<HamDrmEncodedObject> encodeHamDrmObject(
    HamDrmMotObjectMetadata metadata,
    const std::vector<std::uint8_t>& body,
    std::size_t bodySegmentBytes,
    const HamDrmLimits& limits)
{
    if (body.empty() || body.size() > limits.maximumObjectBytes
        || body.size() > 0x0fffffffU || bodySegmentBytes == 0U
        || bodySegmentBytes > limits.maximumSegmentBytes) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::LimitExceeded,
                                      "HAMDRM object or segment size invalid")};
    }
    if (metadata.bodySize != 0U && metadata.bodySize != body.size()) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::InconsistentObject,
                                      "MOT body size does not match payload")};
    }
    metadata.bodySize = static_cast<std::uint32_t>(body.size());
    if (metadata.transportId == 0U) {
        metadata.transportId = qsstvCompatibleTransportId(metadata.filename);
    }
    auto encodedHeader = encodeHamDrmMotHeader(metadata, limits);
    if (!encodedHeader.ok()) {
        return {std::nullopt, encodedHeader.status};
    }
    metadata.headerSize = static_cast<std::uint16_t>(encodedHeader.value->size());
    if (const auto status = checkedContentDescription(metadata); !status.ok()) {
        return {std::nullopt, status};
    }

    const std::size_t bodySegments =
        (body.size() + bodySegmentBytes - 1U) / bodySegmentBytes;
    if (bodySegments == 0U || bodySegments > limits.maximumSegments) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::LimitExceeded,
                                      "HAMDRM object has too many segments")};
    }

    HamDrmEncodedObject output;
    output.metadata = metadata;
    HamDrmMotDataGroup headerGroup;
    headerGroup.kind = HamDrmMotGroupKind::Header;
    headerGroup.lastSegment = true;
    headerGroup.transportId = metadata.transportId;
    headerGroup.payload = std::move(*encodedHeader.value);
    auto encodedHeaderGroup = encodeHamDrmMotDataGroup(headerGroup, limits);
    if (!encodedHeaderGroup.ok()) {
        return {std::nullopt, encodedHeaderGroup.status};
    }
    output.headerGroups.push_back(std::move(*encodedHeaderGroup.value));
    output.bodyGroups.reserve(bodySegments);

    for (std::size_t segment = 0U; segment < bodySegments; ++segment) {
        const std::size_t begin = segment * bodySegmentBytes;
        const std::size_t count = std::min(bodySegmentBytes,
                                           body.size() - begin);
        HamDrmMotDataGroup bodyGroup;
        bodyGroup.kind = HamDrmMotGroupKind::Body;
        bodyGroup.continuityIndex = static_cast<std::uint8_t>(segment % 16U);
        bodyGroup.segmentNumber = static_cast<std::uint16_t>(segment);
        bodyGroup.lastSegment = segment + 1U == bodySegments;
        bodyGroup.transportId = metadata.transportId;
        bodyGroup.payload.assign(body.begin() + static_cast<std::ptrdiff_t>(begin),
                                 body.begin() + static_cast<std::ptrdiff_t>(begin + count));
        auto encoded = encodeHamDrmMotDataGroup(bodyGroup, limits);
        if (!encoded.ok()) {
            return {std::nullopt, encoded.status};
        }
        output.bodyGroups.push_back(std::move(*encoded.value));
    }
    return {std::move(output), HamDrmStatus::success()};
}

} // namespace decodium::sstv::hamdrm
