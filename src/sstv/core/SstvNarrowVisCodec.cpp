// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvNarrowVisCodec.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace decodium::sstv {
namespace {

double normalizedMatch(double observed,
                       double expected,
                       double tolerance) noexcept
{
    if (!std::isfinite(observed)) {
        return 0.0;
    }
    const double error = std::abs(observed - expected);
    return error > tolerance ? 0.0 : 1.0 - error / tolerance;
}

std::uint32_t packGroups(
    const std::array<std::uint8_t, SstvNarrowVisCodec::GroupCount>& groups)
    noexcept
{
    std::uint32_t packed = 0U;
    for (std::size_t index = 0U; index < groups.size(); ++index) {
        packed |= static_cast<std::uint32_t>(groups[index])
                  << (index * SstvNarrowVisCodec::BitsPerGroup);
    }
    return packed;
}

} // namespace

std::uint8_t SstvNarrowVisCodec::payloadForMode(SstvNarrowVisMode mode)
{
    switch (mode) {
    case SstvNarrowVisMode::Mp73:
        return 0x02U;
    case SstvNarrowVisMode::Mp110:
        return 0x04U;
    case SstvNarrowVisMode::Mp140:
        return 0x05U;
    case SstvNarrowVisMode::Mc110:
        return 0x14U;
    case SstvNarrowVisMode::Mc140:
        return 0x15U;
    case SstvNarrowVisMode::Mc180:
        return 0x16U;
    }
    throw std::invalid_argument("invalid narrow VIS mode");
}

std::optional<SstvNarrowVisMode> SstvNarrowVisCodec::modeForPayload(
    std::uint8_t payload) noexcept
{
    switch (payload) {
    case 0x02U:
        return SstvNarrowVisMode::Mp73;
    case 0x04U:
        return SstvNarrowVisMode::Mp110;
    case 0x05U:
        return SstvNarrowVisMode::Mp140;
    case 0x14U:
        return SstvNarrowVisMode::Mc110;
    case 0x15U:
        return SstvNarrowVisMode::Mc140;
    case 0x16U:
        return SstvNarrowVisMode::Mc180;
    default:
        return std::nullopt;
    }
}

SstvNarrowVisEncodedFrame SstvNarrowVisCodec::encode(
    SstvNarrowVisMode mode)
{
    SstvNarrowVisEncodedFrame frame;
    frame.mode = mode;
    frame.payload = payloadForMode(mode);
    frame.groups = {{FirstPreambleGroup,
                     SecondPreambleGroup,
                     frame.payload,
                     static_cast<std::uint8_t>(
                         SecondPreambleGroup ^ frame.payload)}};
    frame.packedWireValue = packGroups(frame.groups);
    frame.tones[0U] = {OneFrequencyHz, LeaderDuration};
    frame.tones[1U] = {ZeroFrequencyHz, GuardDuration};
    frame.tones[2U] = {OneFrequencyHz, SymbolDuration};

    std::size_t wireIndex = 0U;
    for (const std::uint8_t group : frame.groups) {
        for (std::size_t bit = 0U; bit < BitsPerGroup; ++bit) {
            const bool one = (group & (1U << bit)) != 0U;
            frame.bitsLsbFirst[wireIndex] = one;
            frame.tones[wireIndex + 3U] = {
                one ? OneFrequencyHz : ZeroFrequencyHz,
                SymbolDuration};
            ++wireIndex;
        }
    }
    return frame;
}

SstvNarrowVisDecodeResult SstvNarrowVisCodec::decode(
    const double* frequencyHz,
    std::size_t count,
    double toleranceHz)
{
    if (frequencyHz == nullptr || count != ToneCount) {
        throw std::invalid_argument(
            "narrow VIS frame must contain exactly 27 tones");
    }
    // The two data tones are only 200 Hz apart.  Overlapping acceptance
    // windows would turn a midpoint observation into an arbitrary bit.
    if (!std::isfinite(toleranceHz) || toleranceHz <= 0.0
        || toleranceHz >= 100.0) {
        throw std::invalid_argument("invalid narrow VIS tone tolerance");
    }

    SstvNarrowVisDecodeResult result;
    double confidence = 1.0;
    const std::array<double, 3U> header {{
        OneFrequencyHz, ZeroFrequencyHz, OneFrequencyHz}};
    for (std::size_t index = 0U; index < header.size(); ++index) {
        const double match = normalizedMatch(
            frequencyHz[index], header[index], toleranceHz);
        if (match <= 0.0) {
            result.error = SstvNarrowVisError::InvalidHeader;
            return result;
        }
        confidence = std::min(confidence, match);
    }

    for (std::size_t wireIndex = 0U;
         wireIndex < DataBitCount;
         ++wireIndex) {
        const double observed = frequencyHz[wireIndex + 3U];
        const double one = normalizedMatch(
            observed, OneFrequencyHz, toleranceHz);
        const double zero = normalizedMatch(
            observed, ZeroFrequencyHz, toleranceHz);
        if ((one <= 0.0 && zero <= 0.0) || one == zero) {
            result.error = SstvNarrowVisError::InvalidSymbol;
            return result;
        }
        const bool decodedOne = one > zero;
        result.bitsLsbFirst[wireIndex] = decodedOne;
        confidence = std::min(confidence, std::max(one, zero));
        const std::size_t group = wireIndex / BitsPerGroup;
        const std::size_t bit = wireIndex % BitsPerGroup;
        if (decodedOne) {
            result.groups[group] = static_cast<std::uint8_t>(
                result.groups[group] | (1U << bit));
        }
    }
    result.packedWireValue = packGroups(result.groups);
    result.payload = result.groups[2U];
    result.confidence = confidence;

    if (result.groups[0U] != FirstPreambleGroup
        || result.groups[1U] != SecondPreambleGroup) {
        result.error = SstvNarrowVisError::InvalidPreamble;
        return result;
    }
    if (result.groups[3U]
        != static_cast<std::uint8_t>(SecondPreambleGroup ^ result.payload)) {
        result.error = SstvNarrowVisError::ComplementMismatch;
        return result;
    }
    result.mode = modeForPayload(result.payload);
    if (!result.mode.has_value()) {
        result.error = SstvNarrowVisError::UnknownMode;
        return result;
    }
    result.error = SstvNarrowVisError::None;
    result.valid = true;
    return result;
}

SstvNarrowVisDecodeResult SstvNarrowVisCodec::decode(
    const std::array<double, ToneCount>& frequencyHz,
    double toleranceHz)
{
    return decode(frequencyHz.data(), frequencyHz.size(), toleranceHz);
}

const char* SstvNarrowVisCodec::errorName(
    SstvNarrowVisError error) noexcept
{
    switch (error) {
    case SstvNarrowVisError::None:
        return "none";
    case SstvNarrowVisError::InvalidHeader:
        return "invalid-header";
    case SstvNarrowVisError::InvalidSymbol:
        return "invalid-symbol";
    case SstvNarrowVisError::InvalidPreamble:
        return "invalid-preamble";
    case SstvNarrowVisError::ComplementMismatch:
        return "complement-mismatch";
    case SstvNarrowVisError::UnknownMode:
        return "unknown-mode";
    }
    return "unknown";
}

static_assert(SstvNarrowVisCodec::FrameDuration.count
                  == SstvNarrowVisCodec::LeaderDuration.count
                      + SstvNarrowVisCodec::GuardDuration.count
                      + SstvNarrowVisCodec::SymbolDuration.count
                          * static_cast<std::int64_t>(
                              SstvNarrowVisCodec::DataBitCount + 1U),
              "narrow VIS frame duration must include header and 24 bits");

} // namespace decodium::sstv
