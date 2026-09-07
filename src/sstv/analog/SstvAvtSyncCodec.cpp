// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvAvtSyncCodec.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace decodium::sstv {
namespace {

constexpr std::uint8_t kMaximumCounter = 31U;

double bitFrequency(bool one, bool narrow) noexcept
{
    if (narrow) {
        return one ? SstvAvtSyncCodec::NarrowOneFrequencyHz
                   : SstvAvtSyncCodec::NarrowZeroFrequencyHz;
    }
    return one ? SstvAvtSyncCodec::NormalOneFrequencyHz
               : SstvAvtSyncCodec::NormalZeroFrequencyHz;
}

double normalizedMatch(double observed,
                       double expected,
                       double tolerance) noexcept
{
    const double error = std::abs(observed - expected);
    return error > tolerance ? 0.0 : 1.0 - error / tolerance;
}

} // namespace

std::uint8_t SstvAvtSyncCodec::modePrefix(SstvAvtMode mode)
{
    switch (mode) {
    case SstvAvtMode::Avt24:
        return 0b010U;
    case SstvAvtMode::Avt90:
        return 0b101U;
    case SstvAvtMode::Avt94:
        return 0b011U;
    }
    throw std::invalid_argument("invalid AVT mode");
}

std::optional<SstvAvtMode> SstvAvtSyncCodec::modeForPrefix(
    std::uint8_t prefix) noexcept
{
    switch (prefix) {
    case 0b010U:
        return SstvAvtMode::Avt24;
    case 0b101U:
        return SstvAvtMode::Avt90;
    case 0b011U:
        return SstvAvtMode::Avt94;
    default:
        return std::nullopt;
    }
}

std::uint8_t SstvAvtSyncCodec::visPayload(SstvAvtMode mode,
                                          SstvAvtVariant variant)
{
    const std::uint8_t variantValue = static_cast<std::uint8_t>(variant);
    if (variantValue > 3U) {
        throw std::invalid_argument("invalid AVT variant");
    }
    std::uint8_t base = 0U;
    switch (mode) {
    case SstvAvtMode::Avt24:
        base = 64U;
        break;
    case SstvAvtMode::Avt90:
        base = 68U;
        break;
    case SstvAvtMode::Avt94:
        base = 72U;
        break;
    default:
        throw std::invalid_argument("invalid AVT mode");
    }
    return static_cast<std::uint8_t>(base + variantValue);
}

std::optional<SstvAvtMode> SstvAvtSyncCodec::modeForVis(
    std::uint8_t payload) noexcept
{
    if (payload >= 64U && payload <= 67U) {
        return SstvAvtMode::Avt24;
    }
    if (payload >= 68U && payload <= 71U) {
        return SstvAvtMode::Avt90;
    }
    if (payload >= 72U && payload <= 75U) {
        return SstvAvtMode::Avt94;
    }
    return std::nullopt;
}

SstvAvtVariant SstvAvtSyncCodec::variantForVis(std::uint8_t payload)
{
    if (!modeForVis(payload).has_value()) {
        throw std::invalid_argument("VIS payload is not a supported AVT mode");
    }
    return static_cast<SstvAvtVariant>(payload & 0x03U);
}

bool SstvAvtSyncCodec::isNarrow(SstvAvtVariant variant) noexcept
{
    return (static_cast<std::uint8_t>(variant) & 0x01U) != 0U;
}

bool SstvAvtSyncCodec::isQrm(SstvAvtVariant variant) noexcept
{
    return (static_cast<std::uint8_t>(variant) & 0x02U) != 0U;
}

SstvAvtSyncFrame SstvAvtSyncCodec::encodeFrame(SstvAvtMode mode,
                                                std::uint8_t counter,
                                                bool narrow)
{
    if (counter > kMaximumCounter) {
        throw std::invalid_argument("AVT countdown counter exceeds five bits");
    }
    SstvAvtSyncFrame frame;
    frame.mode = mode;
    frame.narrow = narrow;
    frame.counter = counter;
    frame.normalWord = static_cast<std::uint8_t>(
        (modePrefix(mode) << 5U) | counter);
    frame.invertedWord = static_cast<std::uint8_t>(~frame.normalWord);
    frame.tones[0] = {StartFrequencyHz, SymbolDuration};

    std::size_t output = 1U;
    for (const std::uint8_t word : {frame.normalWord, frame.invertedWord}) {
        for (int bit = 7; bit >= 0; --bit) {
            const bool one = (word & (1U << static_cast<unsigned>(bit))) != 0U;
            frame.tones[output++] = {bitFrequency(one, narrow),
                                     SymbolDuration};
        }
    }
    return frame;
}

std::vector<SstvAvtSyncTone> SstvAvtSyncCodec::encodeCountdown(
    SstvAvtMode mode,
    bool narrow)
{
    std::vector<SstvAvtSyncTone> result;
    result.reserve(CountdownToneCount);
    for (std::uint8_t counter = 0U; counter <= kMaximumCounter; ++counter) {
        const SstvAvtSyncFrame frame = encodeFrame(mode, counter, narrow);
        result.insert(result.end(), frame.tones.begin(), frame.tones.end());
    }
    return result;
}

SstvAvtDecodedSyncFrame SstvAvtSyncCodec::decodeFrame(
    const double* frequencyHz,
    std::size_t count,
    bool narrow,
    double toleranceHz)
{
    if (count != TonesPerFrame || frequencyHz == nullptr) {
        throw std::invalid_argument("AVT sync frame must contain 17 tones");
    }
    if (!std::isfinite(toleranceHz) || toleranceHz <= 0.0
        || toleranceHz > 250.0) {
        throw std::invalid_argument("invalid AVT sync tone tolerance");
    }

    SstvAvtDecodedSyncFrame result;
    result.narrow = narrow;
    double confidence = normalizedMatch(
        frequencyHz[0], StartFrequencyHz, toleranceHz);
    if (!std::isfinite(frequencyHz[0]) || confidence <= 0.0) {
        return result;
    }

    std::uint16_t words = 0U;
    for (std::size_t index = 1U; index < TonesPerFrame; ++index) {
        const double observed = frequencyHz[index];
        if (!std::isfinite(observed)) {
            return result;
        }
        const double zero = normalizedMatch(
            observed, bitFrequency(false, narrow), toleranceHz);
        const double one = normalizedMatch(
            observed, bitFrequency(true, narrow), toleranceHz);
        if (zero <= 0.0 && one <= 0.0) {
            return result;
        }
        const bool decodedOne = one > zero;
        confidence = std::min(confidence, std::max(zero, one));
        words = static_cast<std::uint16_t>(
            (words << 1U) | static_cast<std::uint16_t>(decodedOne));
    }

    result.normalWord = static_cast<std::uint8_t>(words >> 8U);
    result.invertedWord = static_cast<std::uint8_t>(words & 0xffU);
    if (result.invertedWord
        != static_cast<std::uint8_t>(~result.normalWord)) {
        return result;
    }
    result.mode = modeForPrefix(
        static_cast<std::uint8_t>(result.normalWord >> 5U));
    if (!result.mode.has_value()) {
        return result;
    }
    result.counter = static_cast<std::uint8_t>(result.normalWord & 0x1fU);
    result.remainingFrames = static_cast<std::uint8_t>(
        kMaximumCounter - result.counter);
    result.remainingDuration = Picoseconds {
        FrameDuration.count * result.remainingFrames};
    result.confidence = confidence;
    result.valid = true;
    return result;
}

SstvAvtDecodedSyncFrame SstvAvtSyncCodec::decodeFrame(
    const std::array<double, TonesPerFrame>& frequencyHz,
    bool narrow,
    double toleranceHz)
{
    return decodeFrame(
        frequencyHz.data(), frequencyHz.size(), narrow, toleranceHz);
}

static_assert(SstvAvtSyncCodec::FrameDuration.count
                  == SstvAvtSyncCodec::SymbolDuration.count
                      * static_cast<std::int64_t>(
                          SstvAvtSyncCodec::TonesPerFrame),
              "AVT frame duration must equal 17 exact symbols");
static_assert(SstvAvtSyncCodec::CountdownDuration.count
                  == SstvAvtSyncCodec::FrameDuration.count
                      * static_cast<std::int64_t>(
                          SstvAvtSyncCodec::FrameCount),
              "AVT countdown duration must equal 32 exact frames");

} // namespace decodium::sstv
