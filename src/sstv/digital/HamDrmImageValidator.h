// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "HamDrmMotCodec.h"
#include "HamDrmTypes.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace decodium::sstv::hamdrm {

enum class HamDrmImageFormat : std::uint8_t {
    Jpeg,
    Jpeg2000,
    Png,
    Gif,
    Bmp,
};

struct HamDrmImageInfo final
{
    HamDrmImageFormat format {HamDrmImageFormat::Jpeg};
    std::string mimeType;
    std::uint32_t width {0U};
    std::uint32_t height {0U};
};

// Performs an allocation-free import-boundary check before the bytes are
// passed to Qt or OpenJPEG.  It validates the container signature, extracts
// bounded dimensions and requires consistency with the trusted MOT metadata.
// A successful result is not a replacement for a sandboxed/full decoder.
HamDrmValueResult<HamDrmImageInfo> validateHamDrmImage(
    const HamDrmMotObjectMetadata& metadata,
    const std::uint8_t* data,
    std::size_t size,
    const HamDrmLimits& limits = {});

} // namespace decodium::sstv::hamdrm
