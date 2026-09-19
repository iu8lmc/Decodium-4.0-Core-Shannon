// SPDX-License-Identifier: GPL-3.0-or-later

#include "HamDrmQam.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace decodium::sstv::hamdrm::phy {

namespace {

double normalizedLevel(std::size_t index, std::size_t dimensionBits)
{
    switch (dimensionBits) {
    case 1U: {
        constexpr int levels[] {1, -1};
        return static_cast<double>(levels[index]) / std::sqrt(2.0);
    }
    case 2U: {
        constexpr int levels[] {3, -1, 1, -3};
        return static_cast<double>(levels[index]) / std::sqrt(10.0);
    }
    case 3U: {
        constexpr int levels[] {7, -1, 3, -5, 5, -3, 1, -7};
        return static_cast<double>(levels[index]) / std::sqrt(42.0);
    }
    default:
        throw std::invalid_argument("unsupported HAMDRM QAM dimension");
    }
}

std::size_t bitsToIndex(const std::uint8_t* bits, std::size_t bitCount)
{
    std::size_t index = 0U;
    for (std::size_t position = 0U; position < bitCount; ++position) {
        if (bits[position] > 1U) {
            throw std::invalid_argument("HAMDRM QAM bit is not binary");
        }
        index = (index << 1U) | static_cast<std::size_t>(bits[position]);
    }
    return index;
}

void indexToBits(std::size_t index,
                 std::size_t bitCount,
                 std::uint8_t* output)
{
    for (std::size_t position = 0U; position < bitCount; ++position) {
        const std::size_t shift = bitCount - position - 1U;
        output[position] = static_cast<std::uint8_t>((index >> shift) & 1U);
    }
}

} // namespace

std::size_t hamDrmBitsPerCell(HamDrmConstellation constellation)
{
    switch (constellation) {
    case HamDrmConstellation::Qam4:
        return 2U;
    case HamDrmConstellation::Qam16:
        return 4U;
    case HamDrmConstellation::Qam64:
        return 6U;
    }
    throw std::invalid_argument("unsupported HAMDRM constellation");
}

HamDrmComplex hamDrmMapQamCell(const std::uint8_t* bits,
                               std::size_t bitCount,
                               HamDrmConstellation constellation)
{
    const std::size_t expectedBits = hamDrmBitsPerCell(constellation);
    if (bits == nullptr || bitCount != expectedBits) {
        throw std::invalid_argument("invalid HAMDRM QAM cell bit span");
    }

    const std::size_t dimensionBits = expectedBits / 2U;
    const std::size_t inPhaseIndex = bitsToIndex(bits, dimensionBits);
    const std::size_t quadratureIndex = bitsToIndex(bits + dimensionBits,
                                                    dimensionBits);
    return {normalizedLevel(inPhaseIndex, dimensionBits),
            normalizedLevel(quadratureIndex, dimensionBits)};
}

HamDrmQamDecision hamDrmDemapQamCell(
    HamDrmComplex cell,
    HamDrmConstellation constellation)
{
    if (!std::isfinite(cell.real()) || !std::isfinite(cell.imag())) {
        throw std::invalid_argument("non-finite HAMDRM QAM cell");
    }

    HamDrmQamDecision decision;
    decision.bitCount = hamDrmBitsPerCell(constellation);
    decision.squaredError = std::numeric_limits<double>::infinity();
    const std::size_t pointCount = 1U << decision.bitCount;
    std::array<std::uint8_t, 6U> candidateBits {};
    for (std::size_t point = 0U; point < pointCount; ++point) {
        indexToBits(point, decision.bitCount, candidateBits.data());
        const HamDrmComplex candidate = hamDrmMapQamCell(
            candidateBits.data(), decision.bitCount, constellation);
        const double error = std::norm(cell - candidate);
        if (error < decision.squaredError) {
            decision.bits = candidateBits;
            decision.squaredError = error;
        }
    }
    return decision;
}

std::vector<HamDrmComplex> hamDrmMapQamBits(
    const std::vector<std::uint8_t>& bits,
    HamDrmConstellation constellation,
    std::size_t maximumCells)
{
    const std::size_t bitsPerCell = hamDrmBitsPerCell(constellation);
    if ((bits.size() % bitsPerCell) != 0U) {
        throw std::invalid_argument("HAMDRM QAM bits do not fill whole cells");
    }
    const std::size_t cellCount = bits.size() / bitsPerCell;
    const std::size_t effectiveMaximum = std::min(
        maximumCells, kHamDrmMaximumQamCells);
    if (cellCount > effectiveMaximum) {
        throw std::length_error("HAMDRM QAM cell count exceeds bound");
    }

    std::vector<HamDrmComplex> cells;
    cells.reserve(cellCount);
    for (std::size_t offset = 0U; offset < bits.size();
         offset += bitsPerCell) {
        cells.push_back(hamDrmMapQamCell(bits.data() + offset,
                                         bitsPerCell,
                                         constellation));
    }
    return cells;
}

std::vector<std::uint8_t> hamDrmDemapQamCells(
    const std::vector<HamDrmComplex>& cells,
    HamDrmConstellation constellation,
    std::size_t maximumCells)
{
    const std::size_t effectiveMaximum = std::min(
        maximumCells, kHamDrmMaximumQamCells);
    if (cells.size() > effectiveMaximum) {
        throw std::length_error("HAMDRM QAM cell count exceeds bound");
    }
    const std::size_t bitsPerCell = hamDrmBitsPerCell(constellation);
    std::vector<std::uint8_t> bits;
    bits.reserve(cells.size() * bitsPerCell);
    for (const auto& cell : cells) {
        const auto decision = hamDrmDemapQamCell(cell, constellation);
        bits.insert(bits.end(), decision.bits.begin(),
                    decision.bits.begin()
                        + static_cast<std::ptrdiff_t>(decision.bitCount));
    }
    return bits;
}

} // namespace decodium::sstv::hamdrm::phy
