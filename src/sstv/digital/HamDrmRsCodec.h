// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "HamDrmTypes.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace decodium::sstv::hamdrm {

struct HamDrmRsDecoded final {
    std::string extension;
    std::vector<std::uint8_t> bytes;
    std::vector<std::uint8_t> repairedContainer;
};

// EasyPal/QSSTV .rs1-.rs4 container. Erasure positions are byte offsets
// in the interleaved container, not RS-codeword indices.
HamDrmValueResult<std::vector<std::uint8_t>> encodeHamDrmRs(
    const std::vector<std::uint8_t>& image, const std::string& extension,
    unsigned level, const HamDrmLimits& limits = {});
HamDrmValueResult<HamDrmRsDecoded> decodeHamDrmRs(
    const std::vector<std::uint8_t>& container, unsigned level,
    const std::vector<std::size_t>& erasures = {}, const HamDrmLimits& limits = {});

} // namespace decodium::sstv::hamdrm
