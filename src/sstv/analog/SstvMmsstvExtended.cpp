// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvMmsstvExtended.h"

#include "../core/SstvNarrowVisCodec.h"
#include "../core/SstvTimingAccumulator.h"
#include "../core/SstvVisCodec.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace decodium::sstv {
namespace {

constexpr std::uint64_t kPpmDenominator = 1'000'000U;
constexpr std::uint64_t kScaledSampleDenominator =
    static_cast<std::uint64_t>(kPicosecondsPerSecond) * kPpmDenominator;

struct QuotientRemainder final
{
    std::uint64_t quotient {0U};
    std::uint64_t remainder {0U};
};

struct LayoutElement final
{
    SstvMmsstvRegion region {SstvMmsstvRegion::Outside};
    std::uint8_t componentIndex {0U};
    ColourComponent component {ColourComponent::ModeSpecific};
    std::uint32_t pixelCount {0U};
    std::uint64_t startPicoseconds {0U};
    std::uint64_t endPicoseconds {0U};
};

std::uint64_t checkedAdd(std::uint64_t left, std::uint64_t right)
{
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        throw std::overflow_error("MMSSTV extended integer addition overflow");
    }
    return left + right;
}

std::uint64_t checkedMultiply(std::uint64_t left, std::uint64_t right)
{
    if (left != 0U
        && right > std::numeric_limits<std::uint64_t>::max() / left) {
        throw std::overflow_error(
            "MMSSTV extended integer multiplication overflow");
    }
    return left * right;
}

std::uint64_t nonNegative(Picoseconds duration)
{
    if (duration.count < 0) {
        throw std::logic_error(
            "MMSSTV extended protocol contains a negative duration");
    }
    return static_cast<std::uint64_t>(duration.count);
}

Picoseconds fromMicroseconds(std::uint64_t value)
{
    const std::uint64_t picoseconds = checkedMultiply(
        value, static_cast<std::uint64_t>(kPicosecondsPerMicrosecond));
    if (picoseconds
        > static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max())) {
        throw std::overflow_error("MMSSTV extended duration overflow");
    }
    return Picoseconds {static_cast<std::int64_t>(picoseconds)};
}

QuotientRemainder multiplyAndDivide(std::uint64_t multiplicand,
                                    std::uint32_t multiplier,
                                    std::uint64_t divisor)
{
    QuotientRemainder result;
    QuotientRemainder term {0U, multiplicand};
    std::uint32_t bits = multiplier;
    while (bits != 0U) {
        if ((bits & 1U) != 0U) {
            result.quotient = checkedAdd(result.quotient, term.quotient);
            result.remainder += term.remainder;
            if (result.remainder >= divisor) {
                result.remainder -= divisor;
                result.quotient = checkedAdd(result.quotient, 1U);
            }
        }
        bits >>= 1U;
        if (bits == 0U) {
            break;
        }
        term.quotient = checkedMultiply(term.quotient, 2U);
        term.remainder *= 2U;
        if (term.remainder >= divisor) {
            term.remainder -= divisor;
            term.quotient = checkedAdd(term.quotient, 1U);
        }
    }
    return result;
}

std::uint64_t fractionOf(std::uint64_t duration,
                         std::uint32_t numerator,
                         std::uint32_t denominator)
{
    if (denominator == 0U || numerator > denominator) {
        throw std::logic_error("invalid MMSSTV extended duration fraction");
    }
    return checkedMultiply(duration, numerator) / denominator;
}

std::uint64_t scaledPicosecondsCeiling(std::uint64_t protocolPicoseconds,
                                       std::uint32_t scaleNumerator)
{
    const std::uint64_t whole = protocolPicoseconds / kPpmDenominator;
    const std::uint64_t remainder = protocolPicoseconds % kPpmDenominator;
    const std::uint64_t wholeProduct = checkedMultiply(whole, scaleNumerator);
    const std::uint64_t remainderProduct = remainder * scaleNumerator;
    return checkedAdd(
        checkedAdd(wholeProduct, remainderProduct / kPpmDenominator),
        (remainderProduct % kPpmDenominator) == 0U ? 0U : 1U);
}

std::uint8_t averaged(std::uint8_t first, std::uint8_t second) noexcept
{
    return static_cast<std::uint8_t>(
        (static_cast<std::uint16_t>(first) + second) / 2U);
}

SstvMmsstvModeSpec makeSpec(SstvMmsstvMode mode,
                            const char* stableId,
                            const char* displayName,
                            SstvMmsstvLayout layout,
                            bool narrow,
                            std::uint32_t width,
                            std::uint32_t height,
                            std::uint8_t visWireCode,
                            std::uint64_t syncUs,
                            std::uint64_t porchUs,
                            std::uint64_t primaryUs,
                            std::uint64_t secondaryUs,
                            std::uint64_t holdUs,
                            std::uint64_t scanUs,
                            std::uint64_t imageUs)
{
    const std::uint8_t linesPerScan =
        layout == SstvMmsstvLayout::MpPairedYCbCr ? 2U : 1U;
    if (width == 0U || height == 0U
        || (height % linesPerScan) != 0U) {
        throw std::logic_error("invalid MMSSTV extended geometry");
    }
    const std::uint32_t scans = height / linesPerScan;
    std::uint64_t calculatedScan = checkedAdd(syncUs, porchUs);
    switch (layout) {
    case SstvMmsstvLayout::MpPairedYCbCr:
        calculatedScan = checkedAdd(
            calculatedScan, checkedMultiply(primaryUs, 4U));
        break;
    case SstvMmsstvLayout::MrHorizontal422:
        calculatedScan = checkedAdd(
            calculatedScan,
            checkedAdd(primaryUs,
                       checkedAdd(checkedMultiply(secondaryUs, 2U),
                                  checkedMultiply(holdUs, 3U))));
        break;
    case SstvMmsstvLayout::McSequentialRgb:
        calculatedScan = checkedAdd(
            calculatedScan, checkedMultiply(primaryUs, 3U));
        break;
    }
    if (calculatedScan != scanUs
        || checkedMultiply(scanUs, scans) != imageUs) {
        throw std::logic_error(
            "MMSSTV extended fixture timings are inconsistent");
    }

    const double syncFrequency = narrow ? 1'900.0 : 1'200.0;
    const double porchFrequency = narrow ? 2'044.0 : 1'500.0;
    const double blackFrequency = porchFrequency;
    return {mode,
            stableId,
            displayName,
            layout,
            narrow,
            width,
            height,
            scans,
            linesPerScan,
            visWireCode,
            syncFrequency,
            porchFrequency,
            blackFrequency,
            2'300.0,
            narrow ? SstvMmsstvProtocol::NarrowHeaderDuration
                   : SstvMmsstvProtocol::WideHeaderDuration,
            fromMicroseconds(syncUs),
            fromMicroseconds(porchUs),
            fromMicroseconds(primaryUs),
            fromMicroseconds(secondaryUs),
            fromMicroseconds(holdUs),
            fromMicroseconds(scanUs),
            fromMicroseconds(imageUs)};
}

std::vector<LayoutElement> makeLayout(const SstvMmsstvModeSpec& spec)
{
    std::vector<LayoutElement> result;
    result.reserve(9U);
    std::uint64_t cursor = 0U;
    const auto add = [&](SstvMmsstvRegion region,
                         std::uint8_t componentIndex,
                         ColourComponent component,
                         std::uint32_t pixelCount,
                         Picoseconds duration) {
        const std::uint64_t end = checkedAdd(cursor, nonNegative(duration));
        result.push_back(
            {region, componentIndex, component, pixelCount, cursor, end});
        cursor = end;
    };
    add(SstvMmsstvRegion::Sync, 0U, ColourComponent::ModeSpecific, 0U,
        spec.syncDuration);
    add(SstvMmsstvRegion::Porch, 0U, ColourComponent::ModeSpecific, 0U,
        spec.porchDuration);
    switch (spec.layout) {
    case SstvMmsstvLayout::MpPairedYCbCr:
        add(SstvMmsstvRegion::Pixel, 0U, ColourComponent::Luminance,
            spec.width, spec.primaryComponentDuration);
        add(SstvMmsstvRegion::Pixel, 1U, ColourComponent::ChrominanceRed,
            spec.width, spec.primaryComponentDuration);
        add(SstvMmsstvRegion::Pixel, 2U, ColourComponent::ChrominanceBlue,
            spec.width, spec.primaryComponentDuration);
        add(SstvMmsstvRegion::Pixel, 3U, ColourComponent::Luminance,
            spec.width, spec.primaryComponentDuration);
        break;
    case SstvMmsstvLayout::MrHorizontal422:
        add(SstvMmsstvRegion::Pixel, 0U, ColourComponent::Luminance,
            spec.width, spec.primaryComponentDuration);
        add(SstvMmsstvRegion::HoldLast, 0U, ColourComponent::Luminance,
            spec.width, spec.holdLastDuration);
        add(SstvMmsstvRegion::Pixel, 1U, ColourComponent::ChrominanceRed,
            spec.width / 2U, spec.secondaryComponentDuration);
        add(SstvMmsstvRegion::HoldLast, 1U, ColourComponent::ChrominanceRed,
            spec.width / 2U, spec.holdLastDuration);
        add(SstvMmsstvRegion::Pixel, 2U, ColourComponent::ChrominanceBlue,
            spec.width / 2U, spec.secondaryComponentDuration);
        add(SstvMmsstvRegion::HoldLast, 2U, ColourComponent::ChrominanceBlue,
            spec.width / 2U, spec.holdLastDuration);
        break;
    case SstvMmsstvLayout::McSequentialRgb:
        add(SstvMmsstvRegion::Pixel, 0U, ColourComponent::Red,
            spec.width, spec.primaryComponentDuration);
        add(SstvMmsstvRegion::Pixel, 1U, ColourComponent::Green,
            spec.width, spec.primaryComponentDuration);
        add(SstvMmsstvRegion::Pixel, 2U, ColourComponent::Blue,
            spec.width, spec.primaryComponentDuration);
        break;
    }
    if (cursor != nonNegative(spec.scanDuration)) {
        throw std::logic_error("MMSSTV extended scan layout is inconsistent");
    }
    return result;
}

double frequencyForVisSymbol(SstvVisSymbol symbol)
{
    switch (symbol) {
    case SstvVisSymbol::Separator:
        return 1'200.0;
    case SstvVisSymbol::Zero:
        return 1'300.0;
    case SstvVisSymbol::One:
        return 1'100.0;
    case SstvVisSymbol::Invalid:
        break;
    }
    throw std::logic_error("invalid wide extended VIS symbol");
}

SstvNarrowVisMode narrowVisMode(SstvMmsstvMode mode)
{
    switch (mode) {
    case SstvMmsstvMode::Mp73Narrow:
        return SstvNarrowVisMode::Mp73;
    case SstvMmsstvMode::Mp110Narrow:
        return SstvNarrowVisMode::Mp110;
    case SstvMmsstvMode::Mp140Narrow:
        return SstvNarrowVisMode::Mp140;
    case SstvMmsstvMode::Mc110Narrow:
        return SstvNarrowVisMode::Mc110;
    case SstvMmsstvMode::Mc140Narrow:
        return SstvNarrowVisMode::Mc140;
    case SstvMmsstvMode::Mc180Narrow:
        return SstvNarrowVisMode::Mc180;
    default:
        throw std::invalid_argument("mode does not use narrow VIS");
    }
}

} // namespace

SstvMmsstvModeSpec SstvMmsstvProtocol::spec(SstvMmsstvMode mode)
{
    using Layout = SstvMmsstvLayout;
    switch (mode) {
    case SstvMmsstvMode::Mp73:
        return makeSpec(mode, "mp-73", "MP73", Layout::MpPairedYCbCr,
                        false, 320U, 256U, 0x25U, 9'000U, 1'000U,
                        140'000U, 140'000U, 0U, 570'000U, 72'960'000U);
    case SstvMmsstvMode::Mp115:
        return makeSpec(mode, "mp-115", "MP115", Layout::MpPairedYCbCr,
                        false, 320U, 256U, 0x29U, 9'000U, 1'000U,
                        223'000U, 223'000U, 0U, 902'000U, 115'456'000U);
    case SstvMmsstvMode::Mp140:
        return makeSpec(mode, "mp-140", "MP140", Layout::MpPairedYCbCr,
                        false, 320U, 256U, 0x2aU, 9'000U, 1'000U,
                        270'000U, 270'000U, 0U, 1'090'000U, 139'520'000U);
    case SstvMmsstvMode::Mp175:
        return makeSpec(mode, "mp-175", "MP175", Layout::MpPairedYCbCr,
                        false, 320U, 256U, 0x2cU, 9'000U, 1'000U,
                        340'000U, 340'000U, 0U, 1'370'000U, 175'360'000U);
    case SstvMmsstvMode::Mr73:
        return makeSpec(mode, "mr-73", "MR73", Layout::MrHorizontal422,
                        false, 320U, 256U, 0x45U, 9'000U, 1'000U,
                        138'000U, 69'000U, 100U, 286'300U, 73'292'800U);
    case SstvMmsstvMode::Mr90:
        return makeSpec(mode, "mr-90", "MR90", Layout::MrHorizontal422,
                        false, 320U, 256U, 0x46U, 9'000U, 1'000U,
                        171'000U, 85'500U, 100U, 352'300U, 90'188'800U);
    case SstvMmsstvMode::Mr115:
        return makeSpec(mode, "mr-115", "MR115", Layout::MrHorizontal422,
                        false, 320U, 256U, 0x49U, 9'000U, 1'000U,
                        220'000U, 110'000U, 100U, 450'300U, 115'276'800U);
    case SstvMmsstvMode::Mr140:
        return makeSpec(mode, "mr-140", "MR140", Layout::MrHorizontal422,
                        false, 320U, 256U, 0x4aU, 9'000U, 1'000U,
                        269'000U, 134'500U, 100U, 548'300U, 140'364'800U);
    case SstvMmsstvMode::Mr175:
        // 0x4c is selected from the original MMSSTV transmitter, mode.txt
        // and Handbook.  QSSTV's duplicate 0x4a table row is a documented
        // catalogue typo, not an on-air alias accepted by this codec.
        return makeSpec(mode, "mr-175", "MR175", Layout::MrHorizontal422,
                        false, 320U, 256U, 0x4cU, 9'000U, 1'000U,
                        337'000U, 168'500U, 100U, 684'300U, 175'180'800U);
    case SstvMmsstvMode::Ml180:
        return makeSpec(mode, "ml-180", "ML180", Layout::MrHorizontal422,
                        false, 640U, 496U, 0x85U, 9'000U, 1'000U,
                        176'500U, 88'250U, 100U, 363'300U, 180'196'800U);
    case SstvMmsstvMode::Ml240:
        return makeSpec(mode, "ml-240", "ML240", Layout::MrHorizontal422,
                        false, 640U, 496U, 0x86U, 9'000U, 1'000U,
                        236'500U, 118'250U, 100U, 483'300U, 239'716'800U);
    case SstvMmsstvMode::Ml280:
        return makeSpec(mode, "ml-280", "ML280", Layout::MrHorizontal422,
                        false, 640U, 496U, 0x89U, 9'000U, 1'000U,
                        277'500U, 138'750U, 100U, 565'300U, 280'388'800U);
    case SstvMmsstvMode::Ml320:
        return makeSpec(mode, "ml-320", "ML320", Layout::MrHorizontal422,
                        false, 640U, 496U, 0x8aU, 9'000U, 1'000U,
                        317'500U, 158'750U, 100U, 645'300U, 320'068'800U);
    case SstvMmsstvMode::Mp73Narrow:
        return makeSpec(mode, "mp-73-narrow", "MP73-Narrow",
                        Layout::MpPairedYCbCr, true, 320U, 256U, 0x02U,
                        9'000U, 1'000U, 140'000U, 140'000U, 0U,
                        570'000U, 72'960'000U);
    case SstvMmsstvMode::Mp110Narrow:
        return makeSpec(mode, "mp-110-narrow", "MP110-Narrow",
                        Layout::MpPairedYCbCr, true, 320U, 256U, 0x04U,
                        9'000U, 1'000U, 212'000U, 212'000U, 0U,
                        858'000U, 109'824'000U);
    case SstvMmsstvMode::Mp140Narrow:
        return makeSpec(mode, "mp-140-narrow", "MP140-Narrow",
                        Layout::MpPairedYCbCr, true, 320U, 256U, 0x05U,
                        9'000U, 1'000U, 270'000U, 270'000U, 0U,
                        1'090'000U, 139'520'000U);
    case SstvMmsstvMode::Mc110Narrow:
        // MMSSTV executable behaviour (140 ms/component) resolves the
        // conflicting 143 ms mode.txt prose and is cross-checked by QSSTV's
        // 428.52734375 ms quantised line.
        return makeSpec(mode, "mc-110-narrow", "MC110-Narrow",
                        Layout::McSequentialRgb, true, 320U, 256U, 0x14U,
                        8'000U, 500U, 140'000U, 140'000U, 0U,
                        428'500U, 109'696'000U);
    case SstvMmsstvMode::Mc140Narrow:
        return makeSpec(mode, "mc-140-narrow", "MC140-Narrow",
                        Layout::McSequentialRgb, true, 320U, 256U, 0x15U,
                        8'000U, 500U, 180'000U, 180'000U, 0U,
                        548'500U, 140'416'000U);
    case SstvMmsstvMode::Mc180Narrow:
        return makeSpec(mode, "mc-180-narrow", "MC180-Narrow",
                        Layout::McSequentialRgb, true, 320U, 256U, 0x16U,
                        8'000U, 500U, 232'000U, 232'000U, 0U,
                        704'500U, 180'352'000U);
    }
    throw std::invalid_argument("unknown MMSSTV extended mode");
}

std::optional<SstvMmsstvMode> SstvMmsstvProtocol::modeForExtendedRaw(
    std::uint8_t rawOctet) noexcept
{
    switch (rawOctet) {
    case 0x25U: return SstvMmsstvMode::Mp73;
    case 0x29U: return SstvMmsstvMode::Mp115;
    case 0x2aU: return SstvMmsstvMode::Mp140;
    case 0x2cU: return SstvMmsstvMode::Mp175;
    case 0x45U: return SstvMmsstvMode::Mr73;
    case 0x46U: return SstvMmsstvMode::Mr90;
    case 0x49U: return SstvMmsstvMode::Mr115;
    case 0x4aU: return SstvMmsstvMode::Mr140;
    case 0x4cU: return SstvMmsstvMode::Mr175;
    case 0x85U: return SstvMmsstvMode::Ml180;
    case 0x86U: return SstvMmsstvMode::Ml240;
    case 0x89U: return SstvMmsstvMode::Ml280;
    case 0x8aU: return SstvMmsstvMode::Ml320;
    default: return std::nullopt;
    }
}

std::optional<SstvMmsstvMode> SstvMmsstvProtocol::modeForNarrowPayload(
    std::uint8_t payload) noexcept
{
    switch (payload) {
    case 0x02U: return SstvMmsstvMode::Mp73Narrow;
    case 0x04U: return SstvMmsstvMode::Mp110Narrow;
    case 0x05U: return SstvMmsstvMode::Mp140Narrow;
    case 0x14U: return SstvMmsstvMode::Mc110Narrow;
    case 0x15U: return SstvMmsstvMode::Mc140Narrow;
    case 0x16U: return SstvMmsstvMode::Mc180Narrow;
    default: return std::nullopt;
    }
}

double SstvMmsstvProtocol::frequencyForValue(
    const SstvMmsstvModeSpec& modeSpec,
    std::uint8_t value) noexcept
{
    return modeSpec.blackFrequencyHz
        + (modeSpec.whiteFrequencyHz - modeSpec.blackFrequencyHz)
            * static_cast<double>(value) / 255.0;
}

std::uint8_t SstvMmsstvProtocol::valueForFrequency(
    const SstvMmsstvModeSpec& modeSpec,
    double frequencyHz) noexcept
{
    if (!std::isfinite(frequencyHz)
        || frequencyHz <= modeSpec.blackFrequencyHz) {
        return 0U;
    }
    if (frequencyHz >= modeSpec.whiteFrequencyHz) {
        return 255U;
    }
    return static_cast<std::uint8_t>(std::lround(
        (frequencyHz - modeSpec.blackFrequencyHz) * 255.0
        / (modeSpec.whiteFrequencyHz - modeSpec.blackFrequencyHz)));
}

SstvMmsstvMapper::SstvMmsstvMapper(SstvMmsstvMapperConfig config)
    : config_(config)
    , spec_(SstvMmsstvProtocol::spec(config.mode))
{
    if (config.sampleRate < MinimumSampleRate
        || config.sampleRate > MaximumSampleRate) {
        throw std::invalid_argument(
            "unsupported MMSSTV extended sample rate");
    }
    if (config.clockErrorPpm < -MaximumAbsoluteClockErrorPpm
        || config.clockErrorPpm > MaximumAbsoluteClockErrorPpm) {
        throw std::invalid_argument(
            "MMSSTV extended clock correction is out of range");
    }
    const std::int64_t scale = static_cast<std::int64_t>(kPpmDenominator)
        + config.clockErrorPpm;
    if (scale <= 0
        || scale > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("invalid MMSSTV extended clock scale");
    }
    clockScaleNumerator_ = static_cast<std::uint32_t>(scale);
    if (checkedMultiply(nonNegative(spec_.scanDuration), spec_.scanCount)
        != nonNegative(spec_.imageDuration)) {
        throw std::logic_error(
            "MMSSTV extended image duration is inconsistent");
    }
    static_cast<void>(makeLayout(spec_));
    imageSamples_ = samplesAtProtocolTime(nonNegative(spec_.imageDuration));
}

SstvMmsstvMapperConfig SstvMmsstvMapper::config() const noexcept
{
    return config_;
}

SstvMmsstvModeSpec SstvMmsstvMapper::modeSpec() const noexcept
{
    return spec_;
}

std::uint64_t SstvMmsstvMapper::imageSampleCount() const noexcept
{
    return imageSamples_;
}

std::uint64_t SstvMmsstvMapper::scanStartSample(std::uint32_t scan) const
{
    if (scan > spec_.scanCount) {
        throw std::out_of_range("MMSSTV scan is outside the image");
    }
    return samplesAtProtocolTime(checkedMultiply(
        scan, nonNegative(spec_.scanDuration)));
}

std::uint64_t SstvMmsstvMapper::scanEndSample(std::uint32_t scan) const
{
    if (scan >= spec_.scanCount) {
        throw std::out_of_range("MMSSTV scan is outside the image");
    }
    return scanStartSample(scan + 1U);
}

std::uint64_t SstvMmsstvMapper::samplesAtProtocolTime(
    std::uint64_t picoseconds) const
{
    if (picoseconds > nonNegative(spec_.imageDuration)) {
        throw std::out_of_range("MMSSTV time is outside the image");
    }
    const std::uint64_t divisor =
        static_cast<std::uint64_t>(kPicosecondsPerSecond);
    const std::uint64_t wholeSeconds = picoseconds / divisor;
    const std::uint64_t partialSecond = picoseconds % divisor;
    std::uint64_t unscaledSamples = checkedMultiply(
        wholeSeconds, config_.sampleRate);
    const QuotientRemainder fraction = multiplyAndDivide(
        partialSecond, config_.sampleRate, divisor);
    unscaledSamples = checkedAdd(unscaledSamples, fraction.quotient);

    const std::uint64_t scaledWhole = checkedMultiply(
        unscaledSamples, clockScaleNumerator_);
    std::uint64_t result = scaledWhole / kPpmDenominator;
    const std::uint64_t wholeRemainder = scaledWhole % kPpmDenominator;
    const std::uint64_t residual = checkedAdd(
        checkedMultiply(wholeRemainder,
                        static_cast<std::uint64_t>(kPicosecondsPerSecond)),
        checkedMultiply(fraction.remainder, clockScaleNumerator_));
    result = checkedAdd(result, residual / kScaledSampleDenominator);
    return result;
}

SstvMmsstvPosition SstvMmsstvMapper::makePosition(
    SstvMmsstvRegion region,
    std::uint32_t scan,
    std::uint8_t componentIndex,
    ColourComponent component,
    std::uint32_t pixel,
    std::uint32_t transmittedPixelCount,
    std::uint64_t startPicoseconds,
    std::uint64_t endPicoseconds) const
{
    return {region,
            scan,
            scan * spec_.linesPerScan,
            componentIndex,
            component,
            pixel,
            transmittedPixelCount,
            samplesAtProtocolTime(startPicoseconds),
            samplesAtProtocolTime(endPicoseconds)};
}

SstvMmsstvPosition SstvMmsstvMapper::positionAtSample(
    std::uint64_t imageSample) const
{
    if (imageSample >= imageSamples_) {
        return {SstvMmsstvRegion::Complete,
                spec_.scanCount,
                spec_.height,
                0U,
                ColourComponent::ModeSpecific,
                0U,
                0U,
                imageSamples_,
                imageSamples_};
    }
    std::uint32_t low = 0U;
    std::uint32_t high = spec_.scanCount;
    while (low + 1U < high) {
        const std::uint32_t middle = low + (high - low) / 2U;
        if (scanStartSample(middle) <= imageSample) {
            low = middle;
        } else {
            high = middle;
        }
    }
    const std::uint32_t scan = low;
    const std::uint64_t scanStart = checkedMultiply(
        scan, nonNegative(spec_.scanDuration));
    for (const LayoutElement& element : makeLayout(spec_)) {
        const std::uint64_t start = checkedAdd(
            scanStart, element.startPicoseconds);
        const std::uint64_t end = checkedAdd(
            scanStart, element.endPicoseconds);
        if (start == end || imageSample >= samplesAtProtocolTime(end)) {
            continue;
        }
        if (element.region != SstvMmsstvRegion::Pixel) {
            const std::uint32_t terminalPixel =
                element.pixelCount == 0U ? 0U : element.pixelCount - 1U;
            return makePosition(element.region,
                                scan,
                                element.componentIndex,
                                element.component,
                                terminalPixel,
                                element.pixelCount,
                                start,
                                end);
        }
        std::uint32_t pixelLow = 0U;
        std::uint32_t pixelHigh = element.pixelCount;
        const std::uint64_t duration = end - start;
        while (pixelLow + 1U < pixelHigh) {
            const std::uint32_t middle =
                pixelLow + (pixelHigh - pixelLow) / 2U;
            const std::uint64_t boundary = checkedAdd(
                start, fractionOf(duration, middle, element.pixelCount));
            if (samplesAtProtocolTime(boundary) <= imageSample) {
                pixelLow = middle;
            } else {
                pixelHigh = middle;
            }
        }
        const std::uint64_t pixelStart = checkedAdd(
            start, fractionOf(duration, pixelLow, element.pixelCount));
        const std::uint64_t pixelEnd = checkedAdd(
            start,
            fractionOf(duration, pixelLow + 1U, element.pixelCount));
        return makePosition(SstvMmsstvRegion::Pixel,
                            scan,
                            element.componentIndex,
                            element.component,
                            pixelLow,
                            element.pixelCount,
                            pixelStart,
                            pixelEnd);
    }
    throw std::logic_error("MMSSTV sample did not map to a scan element");
}

SstvMmsstvPosition SstvMmsstvMapper::positionAtProtocolTime(
    std::uint64_t protocolPicoseconds) const
{
    if (protocolPicoseconds >= nonNegative(spec_.imageDuration)) {
        return positionAtSample(imageSamples_);
    }
    const std::uint64_t scanDuration = nonNegative(spec_.scanDuration);
    const std::uint32_t scan = static_cast<std::uint32_t>(
        protocolPicoseconds / scanDuration);
    const std::uint64_t scanStart = checkedMultiply(scan, scanDuration);
    const std::uint64_t local = protocolPicoseconds - scanStart;
    for (const LayoutElement& element : makeLayout(spec_)) {
        if (element.startPicoseconds == element.endPicoseconds
            || local >= element.endPicoseconds) {
            continue;
        }
        const std::uint64_t start = checkedAdd(
            scanStart, element.startPicoseconds);
        const std::uint64_t end = checkedAdd(
            scanStart, element.endPicoseconds);
        if (element.region != SstvMmsstvRegion::Pixel) {
            const std::uint32_t terminalPixel =
                element.pixelCount == 0U ? 0U : element.pixelCount - 1U;
            return makePosition(element.region,
                                scan,
                                element.componentIndex,
                                element.component,
                                terminalPixel,
                                element.pixelCount,
                                start,
                                end);
        }
        const std::uint64_t duration = end - start;
        const std::uint64_t offset = protocolPicoseconds - start;
        const std::uint32_t pixel = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(
                checkedMultiply(offset, element.pixelCount) / duration,
                element.pixelCount - 1U));
        const std::uint64_t pixelStart = checkedAdd(
            start, fractionOf(duration, pixel, element.pixelCount));
        const std::uint64_t pixelEnd = checkedAdd(
            start, fractionOf(duration, pixel + 1U, element.pixelCount));
        return makePosition(SstvMmsstvRegion::Pixel,
                            scan,
                            element.componentIndex,
                            element.component,
                            pixel,
                            element.pixelCount,
                            pixelStart,
                            pixelEnd);
    }
    throw std::logic_error("MMSSTV time did not map to a scan element");
}

SstvMmsstvPosition SstvMmsstvMapper::positionAtElapsedTime(
    Picoseconds elapsed) const
{
    if (elapsed.count < 0) {
        return {};
    }
    const std::uint64_t elapsedPs = static_cast<std::uint64_t>(elapsed.count);
    const std::uint64_t effectiveEnd = scaledPicosecondsCeiling(
        nonNegative(spec_.imageDuration), clockScaleNumerator_);
    if (elapsedPs >= effectiveEnd) {
        return positionAtSample(imageSamples_);
    }
    const std::uint64_t whole = elapsedPs / clockScaleNumerator_;
    const std::uint64_t remainder = elapsedPs % clockScaleNumerator_;
    const std::uint64_t protocolTime = checkedAdd(
        checkedMultiply(whole, kPpmDenominator),
        checkedMultiply(remainder, kPpmDenominator)
            / clockScaleNumerator_);
    return positionAtProtocolTime(protocolTime);
}

std::vector<SstvMmsstvEncoder::HeaderSegment>
SstvMmsstvEncoder::makeHeader(const SstvMmsstvModeSpec& spec)
{
    std::vector<HeaderSegment> header;
    if (spec.narrow) {
        const SstvNarrowVisEncodedFrame frame =
            SstvNarrowVisCodec::encode(narrowVisMode(spec.mode));
        header.reserve(frame.tones.size());
        for (const SstvNarrowVisTone& tone : frame.tones) {
            header.push_back({tone.frequencyHz, tone.duration});
        }
    } else {
        header.reserve(21U);
        header.push_back({1'900.0, Picoseconds {300'000'000'000LL}});
        header.push_back({1'200.0, Picoseconds {10'000'000'000LL}});
        header.push_back({1'900.0, Picoseconds {300'000'000'000LL}});
        const std::uint8_t payload =
            static_cast<std::uint8_t>(spec.visWireCode & 0x7fU);
        const SstvVisEncodedFrame vis = SstvVisCodec::encodeExtended(payload);
        if (!vis.extension.has_value()
            || vis.extension->rawOctet != spec.visWireCode
            || vis.symbols.size() != 18U) {
            throw std::logic_error(
                "MMSSTV wide extension parity does not match fixture");
        }
        for (const SstvVisSymbol symbol : vis.symbols) {
            header.push_back(
                {frequencyForVisSymbol(symbol),
                 Picoseconds {30'000'000'000LL}});
        }
    }
    return header;
}

std::vector<std::uint64_t> SstvMmsstvEncoder::makeHeaderBoundaries(
    std::uint32_t sampleRate,
    const std::vector<HeaderSegment>& header)
{
    SstvTimingAccumulator timing(sampleRate);
    std::vector<std::uint64_t> boundaries(header.size() + 1U, 0U);
    for (std::size_t index = 0U; index < header.size(); ++index) {
        static_cast<void>(timing.samplesFor(header[index].duration));
        boundaries[index + 1U] = timing.totalSamples();
    }
    return boundaries;
}

SstvMmsstvEncoder::SstvMmsstvEncoder(
    const SstvRgbPixel* pixels,
    std::size_t count,
    SstvMmsstvEncoderConfig config)
    : config_(config)
    , spec_(SstvMmsstvProtocol::spec(config.mode))
    , mapper_({config.mode, config.sampleRate, config.clockErrorPpm})
    , generator_(config.sampleRate, config.headroom)
    , header_(makeHeader(spec_))
    , headerBoundaries_(makeHeaderBoundaries(config.sampleRate, header_))
{
    if (count != pixelCount(config.mode)) {
        throw std::invalid_argument(
            "MMSSTV pixel count does not match selected mode");
    }
    if (count != 0U && pixels == nullptr) {
        throw std::invalid_argument("MMSSTV encoder pixels must not be null");
    }
    if (!std::isfinite(config.level)
        || config.level < 0.0 || config.level > MaximumLevel) {
        throw std::invalid_argument("MMSSTV TX level is out of range");
    }
    rgbPixels_.assign(pixels, pixels + count);
    if (spec_.layout != SstvMmsstvLayout::McSequentialRgb) {
        yCbCrPixels_.reserve(count);
        std::transform(rgbPixels_.cbegin(), rgbPixels_.cend(),
                       std::back_inserter(yCbCrPixels_),
                       [](SstvRgbPixel pixel) {
                           return SstvColourConverter::rgbToYCbCr(pixel);
                       });
    }
    generator_.validateTone(spec_.syncFrequencyHz, config.level);
    generator_.validateTone(spec_.whiteFrequencyHz, config.level);
    totalSamples_ = checkedAdd(headerBoundaries_.back(),
                               mapper_.imageSampleCount());
    metrics_.residentImageBytes = rgbPixels_.size() * sizeof(SstvRgbPixel)
        + yCbCrPixels_.size() * sizeof(SstvYCbCrPixel);
}

SstvMmsstvEncoder::SstvMmsstvEncoder(
    const std::vector<SstvRgbPixel>& pixels,
    SstvMmsstvEncoderConfig config)
    : SstvMmsstvEncoder(pixels.data(), pixels.size(), config)
{
}

std::size_t SstvMmsstvEncoder::pixelCount(SstvMmsstvMode mode)
{
    const SstvMmsstvModeSpec modeSpec = SstvMmsstvProtocol::spec(mode);
    if (modeSpec.width != 0U
        && modeSpec.height > std::numeric_limits<std::size_t>::max()
            / modeSpec.width) {
        throw std::overflow_error("MMSSTV frame pixel count overflow");
    }
    return static_cast<std::size_t>(modeSpec.width) * modeSpec.height;
}

std::size_t SstvMmsstvEncoder::headerIndexAt(
    std::uint64_t sample) const noexcept
{
    const auto first = headerBoundaries_.cbegin() + 1;
    const auto found = std::upper_bound(
        first, headerBoundaries_.cend(), sample);
    return static_cast<std::size_t>(found - first);
}

double SstvMmsstvEncoder::imageFrequency(
    const SstvMmsstvPosition& position) const
{
    switch (position.region) {
    case SstvMmsstvRegion::Sync:
        return spec_.syncFrequencyHz;
    case SstvMmsstvRegion::Porch:
        return spec_.porchFrequencyHz;
    case SstvMmsstvRegion::Pixel:
    case SstvMmsstvRegion::HoldLast:
        break;
    case SstvMmsstvRegion::Outside:
    case SstvMmsstvRegion::Complete:
        throw std::logic_error("MMSSTV encoder has no tone at this position");
    }

    const std::uint32_t firstLine = position.firstDestinationLine;
    std::uint8_t value = 0U;
    if (spec_.layout == SstvMmsstvLayout::MpPairedYCbCr) {
        const std::uint32_t secondLine = firstLine + 1U;
        if (secondLine >= spec_.height || position.pixel >= spec_.width) {
            throw std::logic_error("MMSSTV MP pixel is outside the frame");
        }
        const std::size_t firstIndex =
            static_cast<std::size_t>(firstLine) * spec_.width + position.pixel;
        const std::size_t secondIndex =
            static_cast<std::size_t>(secondLine) * spec_.width + position.pixel;
        switch (position.componentIndex) {
        case 0U: value = yCbCrPixels_[firstIndex].luminance; break;
        case 1U:
            value = averaged(yCbCrPixels_[firstIndex].chrominanceRed,
                             yCbCrPixels_[secondIndex].chrominanceRed);
            break;
        case 2U:
            value = averaged(yCbCrPixels_[firstIndex].chrominanceBlue,
                             yCbCrPixels_[secondIndex].chrominanceBlue);
            break;
        case 3U: value = yCbCrPixels_[secondIndex].luminance; break;
        default:
            throw std::logic_error("invalid MMSSTV MP component");
        }
    } else if (spec_.layout == SstvMmsstvLayout::MrHorizontal422) {
        if (firstLine >= spec_.height) {
            throw std::logic_error("MMSSTV MR line is outside the frame");
        }
        if (position.componentIndex == 0U) {
            if (position.pixel >= spec_.width) {
                throw std::logic_error("MMSSTV MR luma is outside the frame");
            }
            const std::size_t index =
                static_cast<std::size_t>(firstLine) * spec_.width
                + position.pixel;
            value = yCbCrPixels_[index].luminance;
        } else {
            const std::uint32_t firstX = position.pixel * 2U;
            const std::uint32_t secondX = firstX + 1U;
            if (secondX >= spec_.width) {
                throw std::logic_error("MMSSTV MR chroma is outside the frame");
            }
            const std::size_t firstIndex =
                static_cast<std::size_t>(firstLine) * spec_.width + firstX;
            const std::size_t secondIndex = firstIndex + 1U;
            if (position.componentIndex == 1U) {
                value = averaged(yCbCrPixels_[firstIndex].chrominanceRed,
                                 yCbCrPixels_[secondIndex].chrominanceRed);
            } else if (position.componentIndex == 2U) {
                value = averaged(yCbCrPixels_[firstIndex].chrominanceBlue,
                                 yCbCrPixels_[secondIndex].chrominanceBlue);
            } else {
                throw std::logic_error("invalid MMSSTV MR component");
            }
        }
    } else {
        if (firstLine >= spec_.height || position.pixel >= spec_.width) {
            throw std::logic_error("MMSSTV MC pixel is outside the frame");
        }
        const SstvRgbPixel pixel = rgbPixels_[
            static_cast<std::size_t>(firstLine) * spec_.width
            + position.pixel];
        switch (position.componentIndex) {
        case 0U: value = pixel.red; break;
        case 1U: value = pixel.green; break;
        case 2U: value = pixel.blue; break;
        default:
            throw std::logic_error("invalid MMSSTV MC component");
        }
    }
    return SstvMmsstvProtocol::frequencyForValue(spec_, value);
}

void SstvMmsstvEncoder::noteTransition(
    SstvMmsstvEncoderStage stage,
    std::size_t headerSegment,
    const SstvMmsstvPosition& image) noexcept
{
    const bool changed = haveLastSegment_
        && (stage != lastStage_
            || headerSegment != lastHeaderSegment_
            || image.region != lastImagePosition_.region
            || image.scan != lastImagePosition_.scan
            || image.componentIndex != lastImagePosition_.componentIndex
            || image.pixel != lastImagePosition_.pixel);
    if (changed
        && metrics_.segmentTransitions
            != std::numeric_limits<std::uint64_t>::max()) {
        ++metrics_.segmentTransitions;
    }
    lastStage_ = stage;
    lastHeaderSegment_ = headerSegment;
    lastImagePosition_ = image;
    haveLastSegment_ = true;
}

template<typename Sample>
std::size_t SstvMmsstvEncoder::generate(double frequencyHz,
                                        Sample* output,
                                        std::size_t count)
{
    static_assert(std::is_same_v<Sample, float>
                      || std::is_same_v<Sample, std::int16_t>,
                  "unsupported MMSSTV encoder sample type");
    if constexpr (std::is_same_v<Sample, float>) {
        return generator_.generateFloat(
            frequencyHz, config_.level, output, count);
    } else {
        return generator_.generatePcm16(
            frequencyHz, config_.level, output, count);
    }
}

template<typename Sample>
std::size_t SstvMmsstvEncoder::pull(Sample* output, std::size_t capacity)
{
    if (metrics_.pullCalls != std::numeric_limits<std::uint64_t>::max()) {
        ++metrics_.pullCalls;
    }
    if (capacity > MaximumSamplesPerPull) {
        ++metrics_.rejectedInputCalls;
        ++metrics_.rejectedOversizeCalls;
        throw std::length_error("MMSSTV TX pull exceeds its work bound");
    }
    if (capacity != 0U && output == nullptr) {
        ++metrics_.rejectedInputCalls;
        throw std::invalid_argument("MMSSTV TX output must not be null");
    }
    if (capacity == 0U || complete() || cancelled()) {
        return 0U;
    }

    std::size_t produced = 0U;
    while (produced < capacity && !complete() && !cancelled()) {
        SstvMmsstvEncoderStage stage = SstvMmsstvEncoderStage::Image;
        std::size_t headerSegment = 0U;
        SstvMmsstvPosition image;
        double frequency = 0.0;
        std::uint64_t segmentEnd = 0U;
        if (producedSamples_ < headerBoundaries_.back()) {
            stage = SstvMmsstvEncoderStage::Header;
            headerSegment = headerIndexAt(producedSamples_);
            frequency = header_[headerSegment].frequencyHz;
            segmentEnd = headerBoundaries_[headerSegment + 1U];
        } else {
            image = mapper_.positionAtSample(
                producedSamples_ - headerBoundaries_.back());
            frequency = imageFrequency(image);
            segmentEnd = checkedAdd(
                headerBoundaries_.back(), image.segmentEndSample);
        }
        if (segmentEnd <= producedSamples_) {
            throw std::logic_error("MMSSTV TX segment made no progress");
        }
        noteTransition(stage, headerSegment, image);
        const std::uint64_t remaining = segmentEnd - producedSamples_;
        const std::size_t requested = std::min<std::size_t>(
            capacity - produced,
            static_cast<std::size_t>(std::min<std::uint64_t>(
                remaining, std::numeric_limits<std::size_t>::max())));
        const std::size_t generated = generate(
            frequency, output + produced, requested);
        produced += generated;
        producedSamples_ = checkedAdd(producedSamples_, generated);
        metrics_.producedSamples = producedSamples_;
        if (generated != requested) {
            break;
        }
    }
    return produced;
}

std::size_t SstvMmsstvEncoder::pullFloat(float* output,
                                         std::size_t capacity)
{
    return pull(output, capacity);
}

std::size_t SstvMmsstvEncoder::pullPcm16(std::int16_t* output,
                                         std::size_t capacity)
{
    return pull(output, capacity);
}

SstvMmsstvMode SstvMmsstvEncoder::mode() const noexcept
{
    return spec_.mode;
}

std::uint64_t SstvMmsstvEncoder::totalSamples() const noexcept
{
    return totalSamples_;
}

std::uint64_t SstvMmsstvEncoder::producedSamples() const noexcept
{
    return producedSamples_;
}

std::uint64_t SstvMmsstvEncoder::headerSamples() const noexcept
{
    return headerBoundaries_.back();
}

bool SstvMmsstvEncoder::complete() const noexcept
{
    return producedSamples_ >= totalSamples_;
}

bool SstvMmsstvEncoder::cancelled() const noexcept
{
    return generator_.cancelled();
}

SstvMmsstvEncoderMetrics SstvMmsstvEncoder::metrics() const noexcept
{
    SstvMmsstvEncoderMetrics result = metrics_;
    result.tone = generator_.metrics();
    return result;
}

void SstvMmsstvEncoder::cancel() noexcept
{
    generator_.cancel();
}

void SstvMmsstvEncoder::reset() noexcept
{
    generator_.reset();
    producedSamples_ = 0U;
    metrics_ = {};
    metrics_.residentImageBytes = rgbPixels_.size() * sizeof(SstvRgbPixel)
        + yCbCrPixels_.size() * sizeof(SstvYCbCrPixel);
    lastStage_ = SstvMmsstvEncoderStage::Header;
    lastHeaderSegment_ = 0U;
    lastImagePosition_ = {};
    haveLastSegment_ = false;
}

void SstvMmsstvDecoder::validateConfig(
    const SstvMmsstvDecoderConfig& config)
{
    static_cast<void>(SstvMmsstvProtocol::spec(config.mode));
    if (config.sampleRate < SstvMmsstvMapper::MinimumSampleRate
        || config.sampleRate > SstvMmsstvMapper::MaximumSampleRate) {
        throw std::invalid_argument(
            "unsupported MMSSTV decoder sample rate");
    }
    if (!std::isfinite(config.frequencyOffsetHz)
        || std::abs(config.frequencyOffsetHz)
            > MaximumAbsoluteFrequencyOffsetHz) {
        throw std::invalid_argument(
            "MMSSTV decoder frequency offset is out of range");
    }
    if (!std::isfinite(config.minimumObservationConfidence)
        || config.minimumObservationConfidence < 0.0
        || config.minimumObservationConfidence > 1.0) {
        throw std::invalid_argument(
            "invalid MMSSTV observation confidence");
    }
    if (config.maximumPendingDirtyEvents == 0U
        || config.maximumPendingDirtyEvents
            > SstvImageFrame::kMaximumDirtyEvents) {
        throw std::invalid_argument("invalid MMSSTV dirty-event bound");
    }
}

SstvMmsstvDecoder::SstvMmsstvDecoder(SstvMmsstvDecoderConfig config)
    : config_(config)
    , spec_(SstvMmsstvProtocol::spec(config.mode))
    , mapper_({config.mode, config.sampleRate, config.clockErrorPpm})
    , frame_(std::make_unique<SstvImageFrame>(
          spec_.width, spec_.height, config.maximumPendingDirtyEvents))
    , accumulators_(static_cast<std::size_t>(spec_.width) * 4U)
{
    validateConfig(config);
    if (mapper_.imageSampleCount()
        > std::numeric_limits<std::uint64_t>::max()
            - config.imageStartSample) {
        throw std::overflow_error("MMSSTV image sample range overflow");
    }
    imageEndSample_ = config.imageStartSample + mapper_.imageSampleCount();
}

void SstvMmsstvDecoder::saturatingAdd(std::uint64_t& value,
                                      std::uint64_t increment) noexcept
{
    value = increment > std::numeric_limits<std::uint64_t>::max() - value
        ? std::numeric_limits<std::uint64_t>::max()
        : value + increment;
}

bool SstvMmsstvDecoder::beginScan(std::uint32_t scan)
{
    if (scan >= spec_.scanCount) {
        return false;
    }
    if (haveCurrentScan_) {
        if (scan < currentScan_) {
            return false;
        }
        if (scan == currentScan_) {
            return true;
        }
        publishCurrentScan();
    }
    currentScan_ = scan;
    haveCurrentScan_ = true;
    clearAccumulators();
    return true;
}

void SstvMmsstvDecoder::accumulate(
    const SstvMmsstvPosition& position,
    double frequencyHz,
    double confidence) noexcept
{
    const std::size_t index =
        static_cast<std::size_t>(position.componentIndex) * spec_.width
        + position.pixel;
    if (position.componentIndex >= 4U || index >= accumulators_.size()) {
        saturatingAdd(metrics_.invalidObservations);
        return;
    }
    PixelAccumulator& accumulator = accumulators_[index];
    if (accumulator.count == 0U) {
        ++nonEmptyAccumulators_;
        refreshBufferMetrics();
    }
    if (accumulator.count != std::numeric_limits<std::uint32_t>::max()) {
        ++accumulator.count;
    }
    accumulator.weightedFrequencyHz += frequencyHz * confidence;
    accumulator.confidenceWeight += confidence;
}

std::optional<std::uint8_t> SstvMmsstvDecoder::accumulatedValue(
    std::uint8_t component,
    std::uint32_t pixel) const noexcept
{
    const std::size_t index = static_cast<std::size_t>(component)
        * spec_.width + pixel;
    if (component >= 4U || index >= accumulators_.size()) {
        return std::nullopt;
    }
    const PixelAccumulator& accumulator = accumulators_[index];
    if (accumulator.count == 0U || accumulator.confidenceWeight <= 0.0) {
        return std::nullopt;
    }
    return SstvMmsstvProtocol::valueForFrequency(
        spec_,
        accumulator.weightedFrequencyHz / accumulator.confidenceWeight
            - config_.frequencyOffsetHz);
}

void SstvMmsstvDecoder::publishCurrentScan()
{
    if (!haveCurrentScan_) {
        return;
    }
    const std::uint32_t firstLine = currentScan_ * spec_.linesPerScan;
    const std::uint32_t lineCount = spec_.linesPerScan;
    std::vector<std::vector<SstvRgbPixel>> rows(
        lineCount, std::vector<SstvRgbPixel>(spec_.width));
    std::vector<std::vector<bool>> present(
        lineCount, std::vector<bool>(spec_.width, false));

    for (std::uint32_t x = 0U; x < spec_.width; ++x) {
        if (spec_.layout == SstvMmsstvLayout::MpPairedYCbCr) {
            const auto y0 = accumulatedValue(0U, x);
            const auto cr = accumulatedValue(1U, x);
            const auto cb = accumulatedValue(2U, x);
            const auto y1 = accumulatedValue(3U, x);
            if (y0 && cr && cb) {
                rows[0U][x] = SstvColourConverter::yCbCrToRgb(
                    {*y0, *cb, *cr});
                present[0U][x] = true;
            }
            if (y1 && cr && cb) {
                rows[1U][x] = SstvColourConverter::yCbCrToRgb(
                    {*y1, *cb, *cr});
                present[1U][x] = true;
            }
        } else if (spec_.layout == SstvMmsstvLayout::MrHorizontal422) {
            const auto y = accumulatedValue(0U, x);
            const auto cr = accumulatedValue(1U, x / 2U);
            const auto cb = accumulatedValue(2U, x / 2U);
            if (y && cr && cb) {
                rows[0U][x] = SstvColourConverter::yCbCrToRgb(
                    {*y, *cb, *cr});
                present[0U][x] = true;
            }
        } else {
            const auto red = accumulatedValue(0U, x);
            const auto green = accumulatedValue(1U, x);
            const auto blue = accumulatedValue(2U, x);
            if (red && green && blue) {
                rows[0U][x] = {*red, *green, *blue};
                present[0U][x] = true;
            }
        }
    }

    bool allLinesComplete = true;
    for (std::uint32_t line = 0U; line < lineCount; ++line) {
        const bool lineComplete = std::all_of(
            present[line].cbegin(), present[line].cend(),
            [](bool value) { return value; });
        allLinesComplete = allLinesComplete && lineComplete;
        std::uint32_t publishedPixels = 0U;
        if (lineComplete) {
            if (frame_->writeScanline(firstLine + line, rows[line])
                == SstvImageWriteResult::Cancelled) {
                state_ = SstvMmsstvDecodeState::Cancelled;
                break;
            }
            publishedPixels = spec_.width;
        } else {
            for (std::uint32_t x = 0U; x < spec_.width; ++x) {
                if (!present[line][x]) {
                    continue;
                }
                if (frame_->writePixel(x,
                                       firstLine + line,
                                       rows[line][x])
                    == SstvImageWriteResult::Cancelled) {
                    state_ = SstvMmsstvDecodeState::Cancelled;
                    break;
                }
                ++publishedPixels;
            }
        }
        if (publishedPixels == spec_.width) {
            saturatingAdd(metrics_.linesPublished);
        }
        saturatingAdd(metrics_.componentsPublished,
                      static_cast<std::uint64_t>(publishedPixels) * 3U);
    }
    if (allLinesComplete
        && state_ != SstvMmsstvDecodeState::Cancelled) {
        saturatingAdd(metrics_.scansPublished);
    }
    haveCurrentScan_ = false;
    clearAccumulators();
}

void SstvMmsstvDecoder::clearAccumulators() noexcept
{
    std::fill(accumulators_.begin(), accumulators_.end(), PixelAccumulator {});
    nonEmptyAccumulators_ = 0U;
    refreshBufferMetrics();
}

void SstvMmsstvDecoder::refreshBufferMetrics() noexcept
{
    metrics_.bufferedPixelAccumulators = nonEmptyAccumulators_;
    metrics_.peakBufferedPixelAccumulators = std::max(
        metrics_.peakBufferedPixelAccumulators, nonEmptyAccumulators_);
}

std::size_t SstvMmsstvDecoder::consume(
    const SstvFrequencyObservation* observations,
    std::size_t count)
{
    if (count > MaximumObservationsPerConsume) {
        saturatingAdd(metrics_.rejectedInputCalls);
        saturatingAdd(metrics_.rejectedOversizeCalls);
        throw std::length_error(
            "MMSSTV decoder consume exceeds its work bound");
    }
    if (count != 0U && observations == nullptr) {
        saturatingAdd(metrics_.rejectedInputCalls);
        throw std::invalid_argument("MMSSTV observations must not be null");
    }
    if (count == 0U || state_ != SstvMmsstvDecodeState::Receiving) {
        return 0U;
    }
    std::size_t accepted = 0U;
    for (std::size_t index = 0U; index < count; ++index) {
        const SstvFrequencyObservation& observation = observations[index];
        saturatingAdd(metrics_.observationInputs);
        if (!observation.valid()
            || !std::isfinite(observation.correctedFrequencyHz)
            || !std::isfinite(observation.confidence)
            || observation.confidence < config_.minimumObservationConfidence
            || (haveLastObservation_
                && observation.centreSample < lastObservationSample_)) {
            saturatingAdd(metrics_.invalidObservations);
            continue;
        }
        lastObservationSample_ = observation.centreSample;
        haveLastObservation_ = true;
        if (observation.centreSample < config_.imageStartSample) {
            saturatingAdd(metrics_.observationsBeforeImage);
            continue;
        }
        if (observation.centreSample >= imageEndSample_) {
            saturatingAdd(metrics_.observationsAfterImage);
            static_cast<void>(finish());
            break;
        }
        const SstvMmsstvPosition position = mapper_.positionAtSample(
            observation.centreSample - config_.imageStartSample);
        if (!beginScan(position.scan)) {
            saturatingAdd(metrics_.invalidObservations);
            continue;
        }
        if (position.region == SstvMmsstvRegion::Sync) {
            const double corrected = observation.correctedFrequencyHz
                - config_.frequencyOffsetHz;
            if (std::abs(corrected - spec_.syncFrequencyHz) <= 100.0
                && (!haveObservedSyncScan_
                    || lastObservedSyncScan_ != position.scan)) {
                lastObservedSyncScan_ = position.scan;
                haveObservedSyncScan_ = true;
                saturatingAdd(metrics_.observedScanSyncs);
            }
            saturatingAdd(metrics_.nonPixelObservations);
            continue;
        }
        if (position.region != SstvMmsstvRegion::Pixel) {
            saturatingAdd(metrics_.nonPixelObservations);
            continue;
        }
        accumulate(position,
                   observation.correctedFrequencyHz,
                   observation.confidence);
        saturatingAdd(metrics_.acceptedObservations);
        ++accepted;
    }
    return accepted;
}

std::size_t SstvMmsstvDecoder::consume(
    const std::vector<SstvFrequencyObservation>& observations)
{
    return consume(observations.data(), observations.size());
}

SstvMmsstvDecodeState SstvMmsstvDecoder::finish()
{
    if (state_ != SstvMmsstvDecodeState::Receiving) {
        return state_;
    }
    publishCurrentScan();
    if (state_ == SstvMmsstvDecodeState::Cancelled) {
        return state_;
    }
    state_ = frame_->isComplete()
        ? SstvMmsstvDecodeState::Complete
        : SstvMmsstvDecodeState::Partial;
    return state_;
}

void SstvMmsstvDecoder::cancel() noexcept
{
    if (state_ == SstvMmsstvDecodeState::Receiving) {
        state_ = SstvMmsstvDecodeState::Cancelled;
        frame_->cancel();
        haveCurrentScan_ = false;
        clearAccumulators();
    }
}

void SstvMmsstvDecoder::reset() noexcept
{
    frame_->reset();
    clearAccumulators();
    metrics_ = {};
    state_ = SstvMmsstvDecodeState::Receiving;
    currentScan_ = 0U;
    haveCurrentScan_ = false;
    lastObservationSample_ = 0U;
    haveLastObservation_ = false;
    lastObservedSyncScan_ = 0U;
    haveObservedSyncScan_ = false;
}

double SstvMmsstvDecoder::setFrequencyOffsetHz(double offsetHz)
{
    if (!std::isfinite(offsetHz)
        || std::abs(offsetHz) > MaximumAbsoluteFrequencyOffsetHz) {
        throw std::invalid_argument(
            "MMSSTV decoder frequency offset is out of range");
    }
    config_.frequencyOffsetHz = offsetHz;
    return config_.frequencyOffsetHz;
}

double SstvMmsstvDecoder::frequencyOffsetHz() const noexcept
{
    return config_.frequencyOffsetHz;
}

SstvMmsstvMode SstvMmsstvDecoder::mode() const noexcept
{
    return spec_.mode;
}

SstvMmsstvDecodeState SstvMmsstvDecoder::state() const noexcept
{
    return state_;
}

std::uint64_t SstvMmsstvDecoder::imageEndSample() const noexcept
{
    return imageEndSample_;
}

const SstvImageFrame& SstvMmsstvDecoder::imageFrame() const noexcept
{
    return *frame_;
}

SstvImageSnapshot SstvMmsstvDecoder::snapshot() const
{
    return frame_->snapshot();
}

std::vector<SstvDirtyEvent> SstvMmsstvDecoder::takeDirtyEvents()
{
    return frame_->takeDirtyEvents();
}

SstvMmsstvDecoderMetrics SstvMmsstvDecoder::metrics() const noexcept
{
    return metrics_;
}

} // namespace decodium::sstv
