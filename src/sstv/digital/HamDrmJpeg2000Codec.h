// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "HamDrmTypes.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace decodium::sstv::hamdrm {

struct HamDrmRgbaImage final
{
    std::uint32_t width {0U};
    std::uint32_t height {0U};
    std::vector<std::uint8_t> rgba;
};

HamDrmValueResult<HamDrmRgbaImage> decodeHamDrmJpeg2000(
    const std::uint8_t* data,
    std::size_t size,
    const HamDrmLimits& limits = {});

HamDrmValueResult<std::vector<std::uint8_t>> encodeHamDrmJpeg2000Lossless(
    const HamDrmRgbaImage& image,
    const HamDrmLimits& limits = {});

} // namespace decodium::sstv::hamdrm
