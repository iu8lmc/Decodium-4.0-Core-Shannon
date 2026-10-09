// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "HamDrmTypes.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace decodium::sstv::hamdrm {

enum class HamDrmBsrDialect : std::uint8_t {
    EasyPalCompatible,
    QsstvExtended,
};

struct HamDrmBsrRequest final
{
    std::uint16_t transportId {0U};
    bool headerReceived {true};
    std::uint16_t segmentSize {0U};
    std::vector<std::uint16_t> missingSegments;
    std::optional<std::string> filename;
    std::optional<std::uint32_t> qsstvCompatibilityCode;
};

HamDrmValueResult<std::vector<std::uint8_t>> encodeHamDrmBsr(
    const HamDrmBsrRequest& request,
    HamDrmBsrDialect dialect,
    const HamDrmLimits& limits = {});

HamDrmValueResult<HamDrmBsrRequest> parseHamDrmBsr(
    const std::uint8_t* data,
    std::size_t size,
    const HamDrmLimits& limits = {});

} // namespace decodium::sstv::hamdrm
