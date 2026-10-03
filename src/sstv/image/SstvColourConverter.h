// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace decodium::sstv {

struct SstvRgbPixel final
{
    std::uint8_t red {0U};
    std::uint8_t green {0U};
    std::uint8_t blue {0U};
};

constexpr bool operator==(const SstvRgbPixel& left,
                          const SstvRgbPixel& right) noexcept
{
    return left.red == right.red && left.green == right.green
        && left.blue == right.blue;
}

constexpr bool operator!=(const SstvRgbPixel& left,
                          const SstvRgbPixel& right) noexcept
{
    return !(left == right);
}

struct SstvYCbCrPixel final
{
    std::uint8_t luminance {0U};
    std::uint8_t chrominanceBlue {128U};
    std::uint8_t chrominanceRed {128U};
};

constexpr bool operator==(const SstvYCbCrPixel& left,
                          const SstvYCbCrPixel& right) noexcept
{
    return left.luminance == right.luminance
        && left.chrominanceBlue == right.chrominanceBlue
        && left.chrominanceRed == right.chrominanceRed;
}

constexpr bool operator!=(const SstvYCbCrPixel& left,
                          const SstvYCbCrPixel& right) noexcept
{
    return !(left == right);
}

// Mode-independent colour primitives for the native SSTV pipeline.
//
// RGB and YCbCr are full-range unsigned 8-bit values.  Y uses the BT.601 luma
// weights (0.299, 0.587, 0.114); Cb/Cr use the corresponding JPEG/full-range
// offsets with neutral chroma at 128.  Integer fixed-point arithmetic and
// nearest rounding make results identical across supported compilers.  The
// result is clamped to [0, 255].  A mode requiring different analogue colour
// scaling must describe and implement that mapping separately rather than
// silently changing this conversion.
class SstvColourConverter final
{
public:
    SstvColourConverter() = delete;

    static SstvYCbCrPixel rgbToYCbCr(SstvRgbPixel pixel) noexcept;
    static SstvRgbPixel yCbCrToRgb(SstvYCbCrPixel pixel) noexcept;

    // Grayscale is the same full-range BT.601 luma value used above.  Expanding
    // grayscale produces a neutral RGB triplet exactly.
    static std::uint8_t rgbToGrayscale(SstvRgbPixel pixel) noexcept;
    static SstvRgbPixel grayscaleToRgb(std::uint8_t value) noexcept;
};

} // namespace decodium::sstv
