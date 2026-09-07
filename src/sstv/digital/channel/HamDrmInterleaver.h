// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../phy/HamDrmComplexTransform.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace decodium::sstv::hamdrm::channel {

constexpr std::size_t kHamDrmMaximumInterleaverElements = 1U << 20U;

std::vector<std::size_t> hamDrmBlockInterleaverPermutation(
    std::size_t frameSize,
    std::size_t t0);
std::vector<std::uint8_t> hamDrmBitInterleave(
    const std::vector<std::uint8_t>& input,
    std::size_t t0);
std::vector<std::uint8_t> hamDrmBitDeinterleave(
    const std::vector<std::uint8_t>& input,
    std::size_t t0);

class HamDrmSymbolInterleaverEncoder final
{
public:
    HamDrmSymbolInterleaverEncoder(std::size_t frameSize,
                                  std::size_t depth);

    std::vector<phy::HamDrmComplex> push(
        const std::vector<phy::HamDrmComplex>& frame);
    void reset();

private:
    std::size_t frameSize_;
    std::size_t depth_;
    std::vector<std::size_t> permutation_;
    std::vector<std::vector<phy::HamDrmComplex>> memory_;
    std::vector<std::size_t> currentIndex_;
};

class HamDrmSymbolInterleaverDecoder final
{
public:
    HamDrmSymbolInterleaverDecoder(std::size_t frameSize,
                                  std::size_t depth);

    // Long interleaving needs depth received frames before the first complete
    // source frame is returned.  Startup cells belonging to negative logical
    // frame indices are deliberately discarded.
    std::optional<std::vector<phy::HamDrmComplex>> push(
        const std::vector<phy::HamDrmComplex>& transmittedFrame);
    void reset();

private:
    std::size_t frameSize_;
    std::size_t depth_;
    std::size_t receivedFrames_ {0U};
    std::vector<std::size_t> permutation_;
    std::vector<std::vector<phy::HamDrmComplex>> pending_;
};

} // namespace decodium::sstv::hamdrm::channel
