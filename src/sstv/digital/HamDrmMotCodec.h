// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "HamDrmTypes.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace decodium::sstv::hamdrm {

enum class HamDrmMotGroupKind : std::uint8_t {
    Header = 3U,
    Body = 4U,
};

struct HamDrmMotDataGroup final
{
    HamDrmMotGroupKind kind {HamDrmMotGroupKind::Body};
    std::uint8_t continuityIndex {0U};
    std::uint8_t repetitionIndex {0U};
    std::uint16_t segmentNumber {0U};
    bool lastSegment {false};
    std::uint16_t transportId {0U};
    std::uint8_t segmentRepetitionCount {0U};
    std::vector<std::uint8_t> payload;
};

struct HamDrmMotObjectMetadata final
{
    std::uint16_t transportId {0U};
    std::uint32_t bodySize {0U};
    std::uint16_t headerSize {0U};
    std::uint8_t contentType {0U};
    std::uint16_t contentSubtype {0U};
    std::uint8_t version {0U};
    std::string filename;
    std::string mimeType;
};

struct HamDrmEncodedObject final
{
    HamDrmMotObjectMetadata metadata;
    std::vector<std::vector<std::uint8_t>> headerGroups;
    std::vector<std::vector<std::uint8_t>> bodyGroups;
};

HamDrmStatus validateHamDrmFilename(const std::string& filename,
                                    const HamDrmLimits& limits = {});

HamDrmValueResult<std::vector<std::uint8_t>> encodeHamDrmMotDataGroup(
    const HamDrmMotDataGroup& group,
    const HamDrmLimits& limits = {});

HamDrmValueResult<HamDrmMotDataGroup> parseHamDrmMotDataGroup(
    const std::uint8_t* data,
    std::size_t size,
    const HamDrmLimits& limits = {});

HamDrmValueResult<std::vector<std::uint8_t>> encodeHamDrmMotHeader(
    const HamDrmMotObjectMetadata& metadata,
    const HamDrmLimits& limits = {});

HamDrmValueResult<HamDrmMotObjectMetadata> parseHamDrmMotHeader(
    const std::uint8_t* data,
    std::size_t size,
    std::uint16_t transportId,
    const HamDrmLimits& limits = {});

HamDrmValueResult<HamDrmEncodedObject> encodeHamDrmObject(
    HamDrmMotObjectMetadata metadata,
    const std::vector<std::uint8_t>& body,
    std::size_t bodySegmentBytes,
    const HamDrmLimits& limits = {});

std::uint16_t qsstvCompatibleTransportId(const std::string& filename) noexcept;

} // namespace decodium::sstv::hamdrm
