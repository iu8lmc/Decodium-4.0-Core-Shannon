// SPDX-License-Identifier: GPL-3.0-or-later

#include "HamDrmMlcCodec.h"

#include "HamDrmChannelCoding.h"
#include "HamDrmInterleaver.h"
#include "../phy/HamDrmQam.h"

#include <algorithm>
#include <exception>
#include <limits>

namespace decodium::sstv::hamdrm::channel {

namespace {

struct PatternRatio final
{
    std::size_t groups;
    std::size_t outputBits;
};

PatternRatio patternRatio(std::size_t patternIndex)
{
    switch (patternIndex) {
    case 0U: return {1U, 4U};
    case 2U: return {1U, 3U};
    case 4U: return {1U, 2U};
    case 6U: return {3U, 5U};
    case 7U: return {2U, 3U};
    case 9U: return {3U, 4U};
    case 10U: return {4U, 5U};
    default: return {0U, 0U};
    }
}

bool binary(const std::vector<std::uint8_t>& bits) noexcept
{
    return std::all_of(bits.begin(), bits.end(),
                       [](std::uint8_t bit) { return bit <= 1U; });
}

HamDrmStatus invalid(HamDrmErrorCode code, const char* detail)
{
    return HamDrmStatus::failure(code, detail);
}

bool profileShapeValid(const HamDrmMlcProfile& profile) noexcept
{
    if (profile.usefulCells == 0U
            || profile.usefulCells > kHamDrmMaximumMscCellsPerFrame
            || profile.levelCount == 0U || profile.levelCount > 3U) {
        return false;
    }
    std::size_t sum = 0U;
    for (std::size_t level = 0U; level < profile.levelCount; ++level) {
        if (profile.inputBitsPerLevel[level] == 0U
                || patternRatio(profile.puncturePatternIndex[level]).groups
                    == 0U) {
            return false;
        }
        sum += profile.inputBitsPerLevel[level];
    }
    return sum == profile.inputBits;
}

} // namespace

HamDrmStatus hamDrmSdcSupportStatus()
{
    return HamDrmStatus::failure(
        HamDrmErrorCode::UnsupportedFeature,
        "SDC is absent from the pinned QSSTV HAMDRM transmit pipeline");
}

HamDrmValueResult<HamDrmMlcProfile> hamDrmMlcProfile(
    std::size_t usefulMscCells,
    HamDrmConstellation constellation,
    HamDrmProtection protection)
{
    if (usefulMscCells < 7U
            || usefulMscCells > kHamDrmMaximumMscCellsPerFrame
            || usefulMscCells > std::numeric_limits<std::size_t>::max() / 2U) {
        return {std::nullopt,
                invalid(HamDrmErrorCode::InvalidArgument,
                        "invalid HAMDRM useful MSC cell count")};
    }
    HamDrmMlcProfile profile;
    profile.constellation = constellation;
    profile.protection = protection;
    profile.usefulCells = usefulMscCells;

    switch (protection) {
    case HamDrmProtection::High:
    case HamDrmProtection::Normal:
        break;
    default:
        return {std::nullopt,
                invalid(HamDrmErrorCode::UnsupportedProfile,
                        "unsupported HAMDRM protection level")};
    }

    switch (constellation) {
    case HamDrmConstellation::Qam4:
        profile.levelCount = 1U;
        profile.puncturePatternIndex = {{6U, 0U, 0U}};
        profile.bitInterleaverT0 = {{21U, 0U, 0U}};
        break;
    case HamDrmConstellation::Qam16:
        profile.levelCount = 2U;
        profile.puncturePatternIndex = protection == HamDrmProtection::High
            ? std::array<std::size_t, 3U> {{2U, 7U, 0U}}
            : std::array<std::size_t, 3U> {{4U, 9U, 0U}};
        profile.bitInterleaverT0 = {{13U, 21U, 0U}};
        break;
    case HamDrmConstellation::Qam64:
        profile.levelCount = 3U;
        profile.puncturePatternIndex = protection == HamDrmProtection::High
            ? std::array<std::size_t, 3U> {{0U, 4U, 9U}}
            : std::array<std::size_t, 3U> {{2U, 7U, 10U}};
        profile.bitInterleaverT0 = {{0U, 13U, 21U}};
        break;
    default:
        return {std::nullopt,
                invalid(HamDrmErrorCode::UnsupportedProfile,
                        "unsupported HAMDRM MSC constellation")};
    }

    const std::size_t encodedWithoutTail = 2U * usefulMscCells - 12U;
    for (std::size_t level = 0U; level < profile.levelCount; ++level) {
        const PatternRatio ratio = patternRatio(
            profile.puncturePatternIndex[level]);
        profile.inputBitsPerLevel[level] = ratio.groups
            * (encodedWithoutTail / ratio.outputBits);
        profile.inputBits += profile.inputBitsPerLevel[level];
    }
    if (!profileShapeValid(profile)
            || profile.inputBits > kHamDrmMaximumChannelBits) {
        return {std::nullopt,
                invalid(HamDrmErrorCode::LimitExceeded,
                        "HAMDRM MLC profile exceeds bounded channel capacity")};
    }
    return {profile, HamDrmStatus::success()};
}

HamDrmValueResult<std::vector<phy::HamDrmComplex>> hamDrmEncodeMscCells(
    const std::vector<std::uint8_t>& bits,
    const HamDrmMlcProfile& profile)
{
    if (!profileShapeValid(profile) || bits.size() != profile.inputBits
            || !binary(bits)) {
        return {std::nullopt,
                invalid(HamDrmErrorCode::InvalidArgument,
                        "invalid HAMDRM MSC encoder input")};
    }
    const auto dispersed = hamDrmEnergyDisperse(bits);
    std::array<std::vector<std::uint8_t>, 3U> levels;
    std::size_t sourceOffset = 0U;
    for (std::size_t level = 0U; level < profile.levelCount; ++level) {
        const std::size_t levelBits = profile.inputBitsPerLevel[level];
        const std::vector<std::uint8_t> partition(
            dispersed.begin() + static_cast<std::ptrdiff_t>(sourceOffset),
            dispersed.begin()
                + static_cast<std::ptrdiff_t>(sourceOffset + levelBits));
        sourceOffset += levelBits;
        const auto encoded = hamDrmConvolutionEncode(
            partition, profile.puncturePatternIndex[level],
            2U * profile.usefulCells, HamDrmPunctureTailMode::Msc);
        if (!encoded.ok()) {
            return {std::nullopt, encoded.status};
        }
        levels[level] = *encoded.value;
        if (profile.bitInterleaverT0[level] != 0U) {
            try {
                levels[level] = hamDrmBitInterleave(
                    levels[level], profile.bitInterleaverT0[level]);
            } catch (const std::exception& error) {
                return {std::nullopt,
                        HamDrmStatus::failure(HamDrmErrorCode::InvalidArgument,
                                              error.what())};
            }
        }
    }

    std::vector<std::uint8_t> qamBits;
    qamBits.reserve(profile.usefulCells * 2U * profile.levelCount);
    for (std::size_t cell = 0U; cell < profile.usefulCells; ++cell) {
        for (std::size_t level = 0U; level < profile.levelCount; ++level) {
            qamBits.push_back(levels[level][2U * cell]);
        }
        for (std::size_t level = 0U; level < profile.levelCount; ++level) {
            qamBits.push_back(levels[level][2U * cell + 1U]);
        }
    }
    try {
        return {phy::hamDrmMapQamBits(qamBits, profile.constellation,
                                     kHamDrmMaximumMscCellsPerFrame),
                HamDrmStatus::success()};
    } catch (const std::exception& error) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::InvalidArgument,
                                      error.what())};
    }
}

HamDrmValueResult<HamDrmMscDecodeResult> hamDrmDecodeMscCells(
    const std::vector<phy::HamDrmComplex>& cells,
    const HamDrmMlcProfile& profile)
{
    if (!profileShapeValid(profile) || cells.size() != profile.usefulCells) {
        return {std::nullopt,
                invalid(HamDrmErrorCode::InvalidArgument,
                        "invalid HAMDRM MSC decoder input")};
    }
    std::vector<std::uint8_t> qamBits;
    try {
        qamBits = phy::hamDrmDemapQamCells(
            cells, profile.constellation,
            kHamDrmMaximumMscCellsPerFrame);
    } catch (const std::exception& error) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::InvalidArgument,
                                      error.what())};
    }

    std::array<std::vector<std::uint8_t>, 3U> levels;
    for (std::size_t level = 0U; level < profile.levelCount; ++level) {
        levels[level].resize(2U * profile.usefulCells);
    }
    const std::size_t bitsPerCell = 2U * profile.levelCount;
    for (std::size_t cell = 0U; cell < profile.usefulCells; ++cell) {
        const std::size_t cellOffset = cell * bitsPerCell;
        for (std::size_t level = 0U; level < profile.levelCount; ++level) {
            levels[level][2U * cell] = qamBits[cellOffset + level];
            levels[level][2U * cell + 1U] =
                qamBits[cellOffset + profile.levelCount + level];
        }
    }

    HamDrmMscDecodeResult result;
    std::vector<std::uint8_t> dispersed;
    dispersed.reserve(profile.inputBits);
    for (std::size_t level = 0U; level < profile.levelCount; ++level) {
        if (profile.bitInterleaverT0[level] != 0U) {
            try {
                levels[level] = hamDrmBitDeinterleave(
                    levels[level], profile.bitInterleaverT0[level]);
            } catch (const std::exception& error) {
                return {std::nullopt,
                        HamDrmStatus::failure(HamDrmErrorCode::InvalidArgument,
                                              error.what())};
            }
        }
        const auto decoded = hamDrmViterbiDecode(
            levels[level], profile.inputBitsPerLevel[level],
            profile.puncturePatternIndex[level],
            HamDrmPunctureTailMode::Msc);
        if (!decoded.ok()) {
            return {std::nullopt, decoded.status};
        }
        result.levelPathMetric[level] = decoded.value->pathMetric;
        dispersed.insert(dispersed.end(), decoded.value->bits.begin(),
                         decoded.value->bits.end());
    }
    result.bits = hamDrmEnergyDisperse(dispersed);
    return {std::move(result), HamDrmStatus::success()};
}

} // namespace decodium::sstv::hamdrm::channel
