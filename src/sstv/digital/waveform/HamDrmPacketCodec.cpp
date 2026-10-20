// SPDX-License-Identifier: GPL-3.0-or-later

#include "HamDrmPacketCodec.h"

#include "../channel/HamDrmChannelCrc.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace decodium::sstv::hamdrm::waveform {
namespace {

constexpr std::uint8_t kFirstFlag = 0x80U;
constexpr std::uint8_t kLastFlag = 0x40U;
constexpr std::uint8_t kPaddedPacketIndicator = 0x08U;

HamDrmStatus failure(HamDrmErrorCode code, const char* detail)
{
    return HamDrmStatus::failure(code, detail);
}

bool parametersValid(const HamDrmPacketParameters& parameters) noexcept
{
    return parameters.bodyBytes != 0U
        && parameters.bodyBytes
            <= std::numeric_limits<std::uint16_t>::max()
        && parameters.packetId <= 3U
        && parameters.maximumDataUnitBytes != 0U;
}

std::uint8_t packetHeader(bool first,
                          bool last,
                          bool padded,
                          std::uint8_t packetId,
                          std::uint8_t continuityIndex) noexcept
{
    return static_cast<std::uint8_t>(
        (first ? kFirstFlag : 0U)
        | (last ? kLastFlag : 0U)
        | static_cast<std::uint8_t>(packetId << 4U)
        | (padded ? kPaddedPacketIndicator : 0U)
        | continuityIndex);
}

} // namespace

HamDrmValueResult<HamDrmPacketizedDataUnit> hamDrmPacketizeDataUnit(
    const std::vector<std::uint8_t>& dataUnit,
    const HamDrmPacketParameters& parameters,
    std::uint8_t initialContinuityIndex)
{
    if (!parametersValid(parameters) || initialContinuityIndex > 7U
            || dataUnit.empty()) {
        return {std::nullopt,
                failure(HamDrmErrorCode::InvalidArgument,
                        "invalid HAMDRM packetizer input")};
    }
    if (dataUnit.size() > parameters.maximumDataUnitBytes) {
        return {std::nullopt,
                failure(HamDrmErrorCode::LimitExceeded,
                        "HAMDRM packet data unit exceeds configured limit")};
    }

    HamDrmPacketizedDataUnit result;
    const std::size_t packetCount = (dataUnit.size()
        + parameters.bodyBytes - 1U) / parameters.bodyBytes;
    result.packets.reserve(packetCount);
    std::size_t offset = 0U;
    std::uint8_t continuity = initialContinuityIndex;
    while (offset < dataUnit.size()) {
        const std::size_t remaining = dataUnit.size() - offset;
        const bool padded = remaining < parameters.bodyBytes;
        const bool last = remaining <= parameters.bodyBytes;
        const std::size_t useful = std::min(remaining,
                                            parameters.bodyBytes);
        if (padded && (useful > 255U
                       || useful + 1U > parameters.bodyBytes)) {
            return {std::nullopt,
                    failure(HamDrmErrorCode::UnsupportedProfile,
                            "HAMDRM PPI cannot represent the final packet fragment")};
        }

        std::vector<std::uint8_t> protectedBytes;
        protectedBytes.reserve(1U + parameters.bodyBytes);
        protectedBytes.push_back(packetHeader(offset == 0U, last, padded,
                                              parameters.packetId,
                                              continuity));
        if (padded) {
            protectedBytes.push_back(static_cast<std::uint8_t>(useful));
        }
        protectedBytes.insert(
            protectedBytes.end(),
            dataUnit.begin() + static_cast<std::ptrdiff_t>(offset),
            dataUnit.begin() + static_cast<std::ptrdiff_t>(offset + useful));
        protectedBytes.resize(1U + parameters.bodyBytes, 0U);
        try {
            result.packets.push_back(
                channel::hamDrmAppendChannelCrc16(protectedBytes));
        } catch (const std::exception& error) {
            return {std::nullopt,
                    HamDrmStatus::failure(HamDrmErrorCode::LimitExceeded,
                                          error.what())};
        }
        offset += useful;
        continuity = static_cast<std::uint8_t>((continuity + 1U) & 7U);
    }
    result.nextContinuityIndex = continuity;
    return {std::move(result), HamDrmStatus::success()};
}

HamDrmPacketReassembler::HamDrmPacketReassembler(
    HamDrmPacketParameters parameters)
    : parameters_(parameters)
{
    if (!parametersValid(parameters_)) {
        throw std::invalid_argument("invalid HAMDRM packet reassembler parameters");
    }
    pending_.reserve(parameters_.maximumDataUnitBytes);
}

HamDrmValueResult<HamDrmPacketReassemblyResult>
HamDrmPacketReassembler::push(const std::vector<std::uint8_t>& packet)
{
    const std::size_t expectedSize = parameters_.bodyBytes
        + kHamDrmPacketOverheadBytes;
    if (packet.size() != expectedSize) {
        reset();
        return {std::nullopt,
                failure(HamDrmErrorCode::Malformed,
                        "HAMDRM packet has the wrong fixed length")};
    }
    const auto verified = channel::hamDrmVerifyAndStripChannelCrc16(packet);
    if (!verified.ok()) {
        reset();
        return {std::nullopt, verified.status};
    }
    const auto& protectedBytes = *verified.value;
    const std::uint8_t header = protectedBytes[0U];
    const bool first = (header & kFirstFlag) != 0U;
    const bool last = (header & kLastFlag) != 0U;
    const bool padded = (header & kPaddedPacketIndicator) != 0U;
    const std::uint8_t packetId = static_cast<std::uint8_t>(
        (header >> 4U) & 3U);
    const std::uint8_t continuity = header & 7U;
    if (packetId != parameters_.packetId || (padded && !last)) {
        reset();
        return {std::nullopt,
                failure(packetId != parameters_.packetId
                            ? HamDrmErrorCode::TransportMismatch
                            : HamDrmErrorCode::Malformed,
                        packetId != parameters_.packetId
                            ? "HAMDRM packet ID does not match FAC/configuration"
                            : "HAMDRM non-final packet uses PPI")};
    }

    if (first) {
        pending_.clear();
        active_ = true;
    } else if (!active_
               || continuity
                    != static_cast<std::uint8_t>(
                        (lastContinuityIndex_ + 1U) & 7U)) {
        reset();
        return {std::nullopt,
                failure(HamDrmErrorCode::Incomplete,
                        "HAMDRM packet continuity sequence is broken")};
    }
    lastContinuityIndex_ = continuity;

    std::size_t bodyOffset = 1U;
    std::size_t usefulBytes = parameters_.bodyBytes;
    if (padded) {
        usefulBytes = protectedBytes[bodyOffset++];
        if (usefulBytes > parameters_.bodyBytes - 1U) {
            reset();
            return {std::nullopt,
                    failure(HamDrmErrorCode::Malformed,
                            "HAMDRM PPI useful-byte count exceeds packet body")};
        }
        const auto paddingBegin = protectedBytes.begin()
            + static_cast<std::ptrdiff_t>(bodyOffset + usefulBytes);
        if (std::any_of(paddingBegin, protectedBytes.end(),
                        [](std::uint8_t value) { return value != 0U; })) {
            reset();
            return {std::nullopt,
                    failure(HamDrmErrorCode::Malformed,
                            "HAMDRM padded packet contains non-zero fill")};
        }
    }
    if (usefulBytes > parameters_.maximumDataUnitBytes
            || pending_.size()
                > parameters_.maximumDataUnitBytes - usefulBytes) {
        reset();
        return {std::nullopt,
                failure(HamDrmErrorCode::LimitExceeded,
                        "HAMDRM reassembled data unit exceeds configured limit")};
    }
    pending_.insert(
        pending_.end(),
        protectedBytes.begin() + static_cast<std::ptrdiff_t>(bodyOffset),
        protectedBytes.begin()
            + static_cast<std::ptrdiff_t>(bodyOffset + usefulBytes));

    HamDrmPacketReassemblyResult result;
    if (last) {
        if (pending_.empty()) {
            reset();
            return {std::nullopt,
                    failure(HamDrmErrorCode::Malformed,
                            "HAMDRM packet data unit is empty")};
        }
        result.dataUnit = std::move(pending_);
        pending_.clear();
        pending_.reserve(parameters_.maximumDataUnitBytes);
        active_ = false;
    }
    return {std::move(result), HamDrmStatus::success()};
}

void HamDrmPacketReassembler::reset() noexcept
{
    pending_.clear();
    lastContinuityIndex_ = 0U;
    active_ = false;
}

} // namespace decodium::sstv::hamdrm::waveform
