// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "HamDrmComplexTransform.h"
#include "../HamDrmTypes.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace decodium::sstv::hamdrm::phy {

constexpr std::size_t kHamDrmMaximumQamCells = 1U << 20U;

struct HamDrmQamDecision final
{
    std::array<std::uint8_t, 6U> bits {};
    std::size_t bitCount {0U};
    double squaredError {0.0};
};

std::size_t hamDrmBitsPerCell(HamDrmConstellation constellation);

HamDrmComplex hamDrmMapQamCell(const std::uint8_t* bits,
                               std::size_t bitCount,
                               HamDrmConstellation constellation);

HamDrmQamDecision hamDrmDemapQamCell(
    HamDrmComplex cell,
    HamDrmConstellation constellation);

std::vector<HamDrmComplex> hamDrmMapQamBits(
    const std::vector<std::uint8_t>& bits,
    HamDrmConstellation constellation,
    std::size_t maximumCells = kHamDrmMaximumQamCells);

std::vector<std::uint8_t> hamDrmDemapQamCells(
    const std::vector<HamDrmComplex>& cells,
    HamDrmConstellation constellation,
    std::size_t maximumCells = kHamDrmMaximumQamCells);

} // namespace decodium::sstv::hamdrm::phy
