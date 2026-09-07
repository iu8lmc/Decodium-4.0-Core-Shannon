// SPDX-License-Identifier: GPL-3.0-or-later

#include "HamDrmFacCodec.h"

#include "HamDrmChannelCoding.h"
#include "HamDrmChannelCrc.h"
#include "HamDrmInterleaver.h"
#include "../phy/HamDrmQam.h"

#include <algorithm>
#include <array>
#include <exception>

namespace decodium::sstv::hamdrm::channel {

namespace {

constexpr std::size_t kFacEncodedBits = 90U;
constexpr std::size_t kFacPattern = 6U;

void writeBits(std::vector<std::uint8_t>& bits,
               std::size_t& position,
               std::uint32_t value,
               std::size_t width)
{
    for (std::size_t offset = 0U; offset < width; ++offset) {
        const std::size_t shift = width - offset - 1U;
        bits[position++] = static_cast<std::uint8_t>((value >> shift) & 1U);
    }
}

std::uint32_t readBits(const std::vector<std::uint8_t>& bits,
                       std::size_t& position,
                       std::size_t width) noexcept
{
    std::uint32_t value = 0U;
    for (std::size_t offset = 0U; offset < width; ++offset) {
        value = (value << 1U) | bits[position++];
    }
    return value;
}

std::array<std::uint8_t, 5U> facDataBytes(
    const std::vector<std::uint8_t>& bits)
{
    std::array<std::uint8_t, 5U> bytes {};
    for (std::size_t byte = 0U; byte < bytes.size(); ++byte) {
        for (std::size_t bit = 0U; bit < 8U; ++bit) {
            bytes[byte] = static_cast<std::uint8_t>(
                (bytes[byte] << 1U) | bits[byte * 8U + bit]);
        }
    }
    return bytes;
}

HamDrmStatus invalid(HamDrmErrorCode code, const char* detail)
{
    return HamDrmStatus::failure(code, detail);
}

bool binary(const std::vector<std::uint8_t>& bits) noexcept
{
    return std::all_of(bits.begin(), bits.end(),
                       [](std::uint8_t bit) { return bit <= 1U; });
}

} // namespace

HamDrmValueResult<std::vector<std::uint8_t>> hamDrmEncodeFacPayloadBits(
    const HamDrmFacParameters& parameters)
{
    if (parameters.frameIdentity > 2U || parameters.packetId > 3U
            || parameters.callsign.size() > 9U) {
        return {std::nullopt,
                invalid(HamDrmErrorCode::InvalidArgument,
                        "invalid HAMDRM FAC field")};
    }
    std::uint32_t identity = 0U;
    switch (parameters.frameIdentity) {
    case 0U: identity = 3U; break;
    case 1U: identity = 1U; break;
    case 2U: identity = 2U; break;
    default: break;
    }
    std::uint32_t bandwidth = 0U;
    switch (parameters.occupiedBandwidth) {
    case HamDrmOccupiedBandwidth::Hz2300: bandwidth = 0U; break;
    case HamDrmOccupiedBandwidth::Hz2500: bandwidth = 1U; break;
    default:
        return {std::nullopt,
                invalid(HamDrmErrorCode::UnsupportedProfile,
                        "unsupported HAMDRM FAC bandwidth")};
    }
    std::uint32_t interleaver = 0U;
    switch (parameters.interleaver) {
    case HamDrmInterleaver::Long: interleaver = 0U; break;
    case HamDrmInterleaver::Short: interleaver = 1U; break;
    default:
        return {std::nullopt,
                invalid(HamDrmErrorCode::UnsupportedProfile,
                        "unsupported HAMDRM FAC interleaver")};
    }
    std::uint32_t mscMode = 0U;
    std::uint32_t extendedMscMode = 0U;
    switch (parameters.mscConstellation) {
    case HamDrmConstellation::Qam4:
        mscMode = 1U;
        extendedMscMode = 1U;
        break;
    case HamDrmConstellation::Qam16:
        mscMode = 1U;
        break;
    case HamDrmConstellation::Qam64:
        break;
    default:
        return {std::nullopt,
                invalid(HamDrmErrorCode::UnsupportedProfile,
                        "unsupported HAMDRM FAC MSC constellation")};
    }
    std::uint32_t protection = 0U;
    switch (parameters.protection) {
    case HamDrmProtection::High: protection = 0U; break;
    case HamDrmProtection::Normal: protection = 1U; break;
    default:
        return {std::nullopt,
                invalid(HamDrmErrorCode::UnsupportedProfile,
                        "unsupported HAMDRM FAC protection")};
    }

    std::vector<std::uint8_t> bits(kHamDrmFacPayloadBits, 0U);
    std::size_t position = 0U;
    writeBits(bits, position, identity, 2U);
    writeBits(bits, position, bandwidth, 1U);
    writeBits(bits, position, interleaver, 1U);
    writeBits(bits, position, mscMode, 1U);
    writeBits(bits, position, protection, 1U);
    writeBits(bits, position, 1U, 1U); // data service in HAMDRM SSTV
    writeBits(bits, position, parameters.packetId, 2U);
    writeBits(bits, position, extendedMscMode, 1U);
    for (std::size_t index = parameters.frameIdentity * 3U;
         index < parameters.frameIdentity * 3U + 3U; ++index) {
        const std::uint8_t character = index < parameters.callsign.size()
            ? static_cast<std::uint8_t>(parameters.callsign[index]) & 0x7FU
            : 0U;
        writeBits(bits, position, character, 7U);
    }
    // Bits 31..39 are the zero reserved field in the pinned amateur format.
    position = 40U;
    const auto bytes = facDataBytes(bits);
    writeBits(bits, position,
              hamDrmChannelCrc8(bytes.data(), bytes.size()), 8U);
    return {std::move(bits), HamDrmStatus::success()};
}

HamDrmValueResult<HamDrmFacDecodeResult> hamDrmDecodeFacPayloadBits(
    const std::vector<std::uint8_t>& payloadBits,
    std::size_t correctedBitMetric)
{
    if (payloadBits.size() != kHamDrmFacPayloadBits || !binary(payloadBits)) {
        return {std::nullopt,
                invalid(HamDrmErrorCode::Malformed,
                        "invalid HAMDRM FAC payload bits")};
    }
    const auto bytes = facDataBytes(payloadBits);
    std::size_t crcPosition = 40U;
    const auto receivedCrc = static_cast<std::uint8_t>(
        readBits(payloadBits, crcPosition, 8U));
    if (hamDrmChannelCrc8(bytes.data(), bytes.size()) != receivedCrc) {
        return {std::nullopt,
                invalid(HamDrmErrorCode::CrcMismatch,
                        "HAMDRM FAC CRC-8 mismatch")};
    }
    if (std::any_of(payloadBits.begin() + 31, payloadBits.begin() + 40,
                    [](std::uint8_t bit) { return bit != 0U; })) {
        return {std::nullopt,
                invalid(HamDrmErrorCode::Malformed,
                        "HAMDRM FAC reserved bits are non-zero")};
    }

    HamDrmFacDecodeResult result;
    result.correctedBitMetric = correctedBitMetric;
    std::size_t position = 0U;
    const std::uint32_t identity = readBits(payloadBits, position, 2U);
    switch (identity) {
    case 3U: result.parameters.frameIdentity = 0U; break;
    case 1U: result.parameters.frameIdentity = 1U; break;
    case 2U: result.parameters.frameIdentity = 2U; break;
    default:
        return {std::nullopt,
                invalid(HamDrmErrorCode::Malformed,
                        "invalid HAMDRM FAC frame identity")};
    }
    result.parameters.occupiedBandwidth = readBits(payloadBits, position, 1U)
        == 0U ? HamDrmOccupiedBandwidth::Hz2300
              : HamDrmOccupiedBandwidth::Hz2500;
    result.parameters.interleaver = readBits(payloadBits, position, 1U) == 0U
        ? HamDrmInterleaver::Long : HamDrmInterleaver::Short;
    const std::uint32_t mscMode = readBits(payloadBits, position, 1U);
    result.parameters.protection = readBits(payloadBits, position, 1U) == 0U
        ? HamDrmProtection::High : HamDrmProtection::Normal;
    const std::uint32_t dataService = readBits(payloadBits, position, 1U);
    result.parameters.packetId = static_cast<std::uint8_t>(
        readBits(payloadBits, position, 2U));
    const std::uint32_t extended = readBits(payloadBits, position, 1U);
    if (dataService != 1U || (mscMode == 0U && extended != 0U)) {
        return {std::nullopt,
                invalid(HamDrmErrorCode::UnsupportedFeature,
                        "FAC does not describe the pinned HAMDRM data subset")};
    }
    result.parameters.mscConstellation = mscMode == 0U
        ? HamDrmConstellation::Qam64
        : (extended == 1U ? HamDrmConstellation::Qam4
                          : HamDrmConstellation::Qam16);
    for (std::size_t index = 0U; index < 3U; ++index) {
        const char character = static_cast<char>(readBits(payloadBits,
                                                          position, 7U));
        if (character != '\0') {
            result.callsignFragment.push_back(character);
        }
    }
    result.parameters.callsign = result.callsignFragment;
    return {std::move(result), HamDrmStatus::success()};
}

HamDrmValueResult<std::vector<phy::HamDrmComplex>> hamDrmEncodeFacCells(
    const HamDrmFacParameters& parameters)
{
    const auto payload = hamDrmEncodeFacPayloadBits(parameters);
    if (!payload.ok()) {
        return {std::nullopt, payload.status};
    }
    const auto dispersed = hamDrmEnergyDisperse(*payload.value);
    const auto encoded = hamDrmConvolutionEncode(
        dispersed, kFacPattern, kFacEncodedBits,
        HamDrmPunctureTailMode::Fac);
    if (!encoded.ok()) {
        return {std::nullopt, encoded.status};
    }
    try {
        const auto interleaved = hamDrmBitInterleave(*encoded.value, 21U);
        return {phy::hamDrmMapQamBits(interleaved,
                                     HamDrmConstellation::Qam4),
                HamDrmStatus::success()};
    } catch (const std::exception& error) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::InvalidArgument,
                                      error.what())};
    }
}

HamDrmValueResult<HamDrmFacDecodeResult> hamDrmDecodeFacCells(
    const std::vector<phy::HamDrmComplex>& cells)
{
    if (cells.size() != kHamDrmFacCells) {
        return {std::nullopt,
                invalid(HamDrmErrorCode::InvalidArgument,
                        "HAMDRM FAC must contain 45 cells")};
    }
    try {
        const auto mappedBits = phy::hamDrmDemapQamCells(
            cells, HamDrmConstellation::Qam4);
        const auto deinterleaved = hamDrmBitDeinterleave(mappedBits, 21U);
        const auto decoded = hamDrmViterbiDecode(
            deinterleaved, kHamDrmFacPayloadBits, kFacPattern,
            HamDrmPunctureTailMode::Fac);
        if (!decoded.ok()) {
            return {std::nullopt, decoded.status};
        }
        const auto payload = hamDrmEnergyDisperse(decoded.value->bits);
        return hamDrmDecodeFacPayloadBits(payload,
                                          decoded.value->pathMetric);
    } catch (const std::exception& error) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::InvalidArgument,
                                      error.what())};
    }
}

} // namespace decodium::sstv::hamdrm::channel
