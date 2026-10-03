// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvColourConverter.h"

#include <cstdint>

namespace decodium::sstv {
namespace {

constexpr std::int64_t kCoefficientScale = 1'000'000LL;

std::int64_t divideRounded(std::int64_t numerator) noexcept
{
    if (numerator >= 0) {
        return (numerator + kCoefficientScale / 2LL) / kCoefficientScale;
    }
    return -((-numerator + kCoefficientScale / 2LL) / kCoefficientScale);
}

std::uint8_t clampToByte(std::int64_t value) noexcept
{
    if (value <= 0LL) {
        return 0U;
    }
    if (value >= 255LL) {
        return 255U;
    }
    return static_cast<std::uint8_t>(value);
}

std::int64_t lumaNumerator(SstvRgbPixel pixel) noexcept
{
    return 299'000LL * pixel.red + 587'000LL * pixel.green
        + 114'000LL * pixel.blue;
}

} // namespace

SstvYCbCrPixel SstvColourConverter::rgbToYCbCr(SstvRgbPixel pixel) noexcept
{
    const std::int64_t luminance = divideRounded(lumaNumerator(pixel));
    const std::int64_t chrominanceBlue = 128LL
        + divideRounded(-168'736LL * pixel.red - 331'264LL * pixel.green
                        + 500'000LL * pixel.blue);
    const std::int64_t chrominanceRed = 128LL
        + divideRounded(500'000LL * pixel.red - 418'688LL * pixel.green
                        - 81'312LL * pixel.blue);

    return {clampToByte(luminance),
            clampToByte(chrominanceBlue),
            clampToByte(chrominanceRed)};
}

SstvRgbPixel SstvColourConverter::yCbCrToRgb(SstvYCbCrPixel pixel) noexcept
{
    const std::int64_t luminance = pixel.luminance;
    const std::int64_t chrominanceBlue
        = static_cast<std::int64_t>(pixel.chrominanceBlue) - 128LL;
    const std::int64_t chrominanceRed
        = static_cast<std::int64_t>(pixel.chrominanceRed) - 128LL;

    const std::int64_t red = luminance
        + divideRounded(1'402'000LL * chrominanceRed);
    const std::int64_t green = luminance
        + divideRounded(-344'136LL * chrominanceBlue
                        - 714'136LL * chrominanceRed);
    const std::int64_t blue = luminance
        + divideRounded(1'772'000LL * chrominanceBlue);
    return {clampToByte(red), clampToByte(green), clampToByte(blue)};
}

std::uint8_t SstvColourConverter::rgbToGrayscale(SstvRgbPixel pixel) noexcept
{
    return clampToByte(divideRounded(lumaNumerator(pixel)));
}

SstvRgbPixel SstvColourConverter::grayscaleToRgb(std::uint8_t value) noexcept
{
    return {value, value, value};
}

} // namespace decodium::sstv
