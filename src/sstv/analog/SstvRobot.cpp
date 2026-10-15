// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvRobot.h"

#include "../core/SstvTimingAccumulator.h"
#include "../core/SstvVisCodec.h"
#include "../image/SstvColourConverter.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace decodium::sstv {
namespace {

constexpr std::uint64_t kPpmDenominator = 1'000'000U;
constexpr std::uint64_t kScaledSampleDenominator =
    static_cast<std::uint64_t>(kPicosecondsPerSecond) * kPpmDenominator;

struct LayoutElement final
{
    SstvRobotRegion region {SstvRobotRegion::Outside};
    ColourComponent component {ColourComponent::ModeSpecific};
    std::uint64_t startPicoseconds {0U};
    std::uint64_t endPicoseconds {0U};
    std::uint32_t pixelCount {0U};
    Picoseconds pixelDuration;
};

struct LineLayout final
{
    std::array<LayoutElement, 8U> elements;
    std::size_t count {0U};
};

std::uint64_t nonNegative(Picoseconds duration)
{
    if (duration.count < 0) {
        throw std::logic_error("Robot protocol contains a negative duration");
    }
    return static_cast<std::uint64_t>(duration.count);
}

std::uint64_t checkedAdd(std::uint64_t left, std::uint64_t right)
{
    if (left > std::numeric_limits<std::uint64_t>::max() - right) {
        throw std::overflow_error("Robot sample index overflow");
    }
    return left + right;
}

std::uint64_t checkedMultiply(std::uint64_t left, std::uint64_t right)
{
    if (left != 0U
        && right > std::numeric_limits<std::uint64_t>::max() / left) {
        throw std::overflow_error("Robot timing multiplication overflow");
    }
    return left * right;
}

struct QuotientRemainder final
{
    std::uint64_t quotient {0U};
    std::uint64_t remainder {0U};
};

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

std::uint64_t scaledPicosecondsCeiling(std::uint64_t protocolPicoseconds,
                                       std::uint32_t scaleNumerator)
{
    const std::uint64_t whole = protocolPicoseconds / kPpmDenominator;
    const std::uint64_t remainder = protocolPicoseconds % kPpmDenominator;
    const std::uint64_t wholeProduct = checkedMultiply(whole, scaleNumerator);
    const std::uint64_t remainderProduct = remainder * scaleNumerator;
    return checkedAdd(
        wholeProduct + remainderProduct / kPpmDenominator,
        remainderProduct % kPpmDenominator == 0U ? 0U : 1U);
}

LineLayout makeLineLayout(const SstvRobotModeSpec& spec)
{
    LineLayout layout;
    std::uint64_t cursor = 0U;
    auto add = [&](SstvRobotRegion region,
                   ColourComponent component,
                   Picoseconds duration,
                   std::uint32_t pixelCount,
                   Picoseconds pixelDuration) {
        if (layout.count >= layout.elements.size()) {
            throw std::logic_error("Robot line layout overflow");
        }
        const std::uint64_t start = cursor;
        cursor = checkedAdd(cursor, nonNegative(duration));
        layout.elements[layout.count++] = {
            region, component, start, cursor, pixelCount, pixelDuration};
    };

    add(SstvRobotRegion::Sync,
        ColourComponent::ModeSpecific,
        spec.syncDuration,
        0U,
        {});
    add(SstvRobotRegion::Pixel,
        spec.colour ? ColourComponent::Luminance : ColourComponent::Gray,
        spec.luminanceDuration,
        spec.width,
        spec.luminancePixelDuration);

    if (spec.colour) {
        const Picoseconds markerMain {
            spec.markerDuration.count * 2LL / 3LL};
        const Picoseconds markerPorch {
            spec.markerDuration.count - markerMain.count};
        auto addChroma = [&](ColourComponent component) {
            add(SstvRobotRegion::ChromaMarker,
                component,
                markerMain,
                0U,
                {});
            add(SstvRobotRegion::MarkerPorch,
                component,
                markerPorch,
                0U,
                {});
            add(SstvRobotRegion::Pixel,
                component,
                spec.chromaDuration,
                spec.chromaWidth,
                spec.chromaPixelDuration);
        };
        if (spec.chromaSubsampling == ChromaSubsampling::Cs420) {
            // The component is corrected per line by the mapper: even Cr,
            // odd Cb.  Cr here keeps structural validation deterministic.
            addChroma(ColourComponent::ChrominanceRed);
        } else if (spec.chromaSubsampling == ChromaSubsampling::Cs422) {
            addChroma(ColourComponent::ChrominanceRed);
            addChroma(ColourComponent::ChrominanceBlue);
        } else {
            throw std::logic_error("Robot colour mode has invalid subsampling");
        }
    }

    if (cursor != nonNegative(spec.lineDuration)
        || checkedMultiply(nonNegative(spec.luminancePixelDuration),
                           spec.width)
            != nonNegative(spec.luminanceDuration)
        || (spec.colour
            && checkedMultiply(nonNegative(spec.chromaPixelDuration),
                               spec.chromaWidth)
                != nonNegative(spec.chromaDuration))) {
        throw std::logic_error("Robot protocol line layout is inconsistent");
    }
    return layout;
}

ColourComponent componentForLine(const SstvRobotModeSpec& spec,
                                 std::uint32_t line,
                                 ColourComponent declared) noexcept
{
    if (spec.chromaSubsampling == ChromaSubsampling::Cs420
        && (declared == ColourComponent::ChrominanceRed
            || declared == ColourComponent::ChrominanceBlue)) {
        return line % 2U == 0U ? ColourComponent::ChrominanceRed
                              : ColourComponent::ChrominanceBlue;
    }
    return declared;
}

std::vector<SstvRgbPixel> validatedPixels(const SstvRgbPixel* pixels,
                                          std::size_t count,
                                          SstvRobotMode mode)
{
    if (count != SstvRobotEncoder::pixelCount(mode)) {
        throw std::invalid_argument(
            "Robot input pixel count does not match the selected mode");
    }
    if (pixels == nullptr) {
        throw std::invalid_argument("Robot input pixels must not be null");
    }
    return std::vector<SstvRgbPixel>(pixels, pixels + count);
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
    throw std::logic_error("Robot VIS codec produced an invalid symbol");
}

std::uint8_t roundedAverage(std::uint32_t sum,
                            std::uint32_t count) noexcept
{
    return static_cast<std::uint8_t>((sum + count / 2U) / count);
}

std::uint64_t absoluteDifference(std::uint64_t left,
                                 std::uint64_t right) noexcept
{
    return left >= right ? left - right : right - left;
}

} // namespace

SstvRobotModeSpec SstvRobotProtocol::spec(SstvRobotMode mode)
{
    // Handbook table 4.3 controls the colour geometry and structural timing.
    // Handbook table 4.1 plus pinned libsstv agree on the B/W scan/sync
    // components.  In particular B/W 24 is structurally 105 ms per line even
    // though its historical name and the table's lpm column suggest 100 ms.
    switch (mode) {
    case SstvRobotMode::Colour12:
        return {mode, "robot-c12", "Robot 12 Colour",
                160U, 120U, 80U, 0U, {{0U, 0U}}, 0U, true,
                ChromaSubsampling::Cs420,
                Picoseconds {7'000'000'000LL},
                Picoseconds {3'000'000'000LL},
                Picoseconds {60'000'000'000LL},
                Picoseconds {30'000'000'000LL},
                Picoseconds {375'000'000LL},
                Picoseconds {375'000'000LL},
                Picoseconds {100'000'000'000LL},
                Picoseconds {12'000'000'000'000LL}};
    case SstvRobotMode::Colour24:
        return {mode, "robot-c24", "Robot 24 Colour",
                320U, 120U, 160U, 4U, {{0U, 0U}}, 0U, true,
                ChromaSubsampling::Cs422,
                Picoseconds {12'000'000'000LL},
                Picoseconds {6'000'000'000LL},
                Picoseconds {88'000'000'000LL},
                Picoseconds {44'000'000'000LL},
                Picoseconds {275'000'000LL},
                Picoseconds {275'000'000LL},
                Picoseconds {200'000'000'000LL},
                Picoseconds {24'000'000'000'000LL}};
    case SstvRobotMode::Colour36:
        return {mode, "robot-c36", "Robot 36 Colour",
                320U, 240U, 160U, 8U, {{0U, 0U}}, 0U, true,
                ChromaSubsampling::Cs420,
                Picoseconds {10'500'000'000LL},
                Picoseconds {4'500'000'000LL},
                Picoseconds {90'000'000'000LL},
                Picoseconds {45'000'000'000LL},
                Picoseconds {281'250'000LL},
                Picoseconds {281'250'000LL},
                Picoseconds {150'000'000'000LL},
                Picoseconds {36'000'000'000'000LL}};
    case SstvRobotMode::Colour72:
        return {mode, "robot-c72", "Robot 72 Colour",
                320U, 240U, 160U, 12U, {{0U, 0U}}, 0U, true,
                ChromaSubsampling::Cs422,
                Picoseconds {12'000'000'000LL},
                Picoseconds {6'000'000'000LL},
                Picoseconds {138'000'000'000LL},
                Picoseconds {69'000'000'000LL},
                Picoseconds {431'250'000LL},
                Picoseconds {431'250'000LL},
                Picoseconds {300'000'000'000LL},
                Picoseconds {72'000'000'000'000LL}};
    case SstvRobotMode::Bw8:
        return {mode, "robot-bw8", "Robot B/W 8",
                160U, 120U, 0U, 2U, {{1U, 3U}}, 2U, false,
                ChromaSubsampling::NotApplicable,
                Picoseconds {10'000'000'000LL}, {},
                Picoseconds {56'000'000'000LL}, {},
                Picoseconds {350'000'000LL}, {},
                Picoseconds {66'000'000'000LL},
                Picoseconds {7'920'000'000'000LL}};
    case SstvRobotMode::Bw12:
        return {mode, "robot-bw12", "Robot B/W 12",
                160U, 120U, 0U, 6U, {{5U, 7U}}, 2U, false,
                ChromaSubsampling::NotApplicable,
                Picoseconds {7'000'000'000LL}, {},
                Picoseconds {93'000'000'000LL}, {},
                Picoseconds {581'250'000LL}, {},
                Picoseconds {100'000'000'000LL},
                Picoseconds {12'000'000'000'000LL}};
    case SstvRobotMode::Bw24:
        return {mode, "robot-bw24", "Robot B/W 24",
                320U, 240U, 0U, 10U, {{9U, 11U}}, 2U, false,
                ChromaSubsampling::NotApplicable,
                Picoseconds {12'000'000'000LL}, {},
                Picoseconds {93'000'000'000LL}, {},
                Picoseconds {290'625'000LL}, {},
                Picoseconds {105'000'000'000LL},
                Picoseconds {25'200'000'000'000LL}};
    case SstvRobotMode::Bw36:
        return {mode, "robot-bw36", "Robot B/W 36",
                320U, 240U, 0U, 14U, {{13U, 15U}}, 2U, false,
                ChromaSubsampling::NotApplicable,
                Picoseconds {12'000'000'000LL}, {},
                Picoseconds {138'000'000'000LL}, {},
                Picoseconds {431'250'000LL}, {},
                Picoseconds {150'000'000'000LL},
                Picoseconds {36'000'000'000'000LL}};
    }
    throw std::invalid_argument("unknown Robot mode");
}

std::optional<SstvRobotMode> SstvRobotProtocol::modeForVis(
    std::uint8_t visPayload) noexcept
{
    switch (visPayload) {
    case 0U:
        return SstvRobotMode::Colour12;
    case 4U:
        return SstvRobotMode::Colour24;
    case 8U:
        return SstvRobotMode::Colour36;
    case 12U:
        return SstvRobotMode::Colour72;
    case 1U:
    case 2U:
    case 3U:
        return SstvRobotMode::Bw8;
    case 5U:
    case 6U:
    case 7U:
        return SstvRobotMode::Bw12;
    case 9U:
    case 10U:
    case 11U:
        return SstvRobotMode::Bw24;
    case 13U:
    case 14U:
    case 15U:
        return SstvRobotMode::Bw36;
    default:
        return std::nullopt;
    }
}

double SstvRobotProtocol::frequencyForValue(std::uint8_t value) noexcept
{
    return BlackFrequencyHz
        + (WhiteFrequencyHz - BlackFrequencyHz)
            * static_cast<double>(value) / 255.0;
}

std::uint8_t SstvRobotProtocol::valueForFrequency(
    double frequencyHz) noexcept
{
    if (!std::isfinite(frequencyHz) || frequencyHz <= BlackFrequencyHz) {
        return 0U;
    }
    if (frequencyHz >= WhiteFrequencyHz) {
        return 255U;
    }
    return static_cast<std::uint8_t>(std::lround(
        (frequencyHz - BlackFrequencyHz) * 255.0
        / (WhiteFrequencyHz - BlackFrequencyHz)));
}

SstvRobotMapper::SstvRobotMapper(SstvRobotMapperConfig config)
    : config_(config)
    , spec_(SstvRobotProtocol::spec(config.mode))
{
    if (config.sampleRate < MinimumSampleRate
        || config.sampleRate > MaximumSampleRate) {
        throw std::invalid_argument("unsupported Robot sample rate");
    }
    if (config.clockErrorPpm < -MaximumAbsoluteClockErrorPpm
        || config.clockErrorPpm > MaximumAbsoluteClockErrorPpm) {
        throw std::invalid_argument("Robot clock correction is out of range");
    }
    const std::int64_t numerator =
        static_cast<std::int64_t>(kPpmDenominator) + config.clockErrorPpm;
    if (numerator <= 0
        || numerator > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("invalid Robot clock scale");
    }
    clockScaleNumerator_ = static_cast<std::uint32_t>(numerator);
    if (spec_.width == 0U || spec_.width > SstvRobotProtocol::MaximumWidth
        || spec_.height == 0U || spec_.height > SstvRobotProtocol::MaximumHeight
        || checkedMultiply(nonNegative(spec_.lineDuration), spec_.height)
            != nonNegative(spec_.imageDuration)) {
        throw std::logic_error("Robot protocol dimensions or duration differ");
    }
    static_cast<void>(makeLineLayout(spec_));
    imageSamples_ = samplesAtProtocolTime(nonNegative(spec_.imageDuration));
}

SstvRobotMapperConfig SstvRobotMapper::config() const noexcept
{
    return config_;
}

SstvRobotModeSpec SstvRobotMapper::modeSpec() const noexcept
{
    return spec_;
}

std::uint64_t SstvRobotMapper::imageSampleCount() const noexcept
{
    return imageSamples_;
}

std::uint64_t SstvRobotMapper::lineStartSample(std::uint32_t line) const
{
    if (line > spec_.height) {
        throw std::out_of_range("Robot line is outside the image");
    }
    return samplesAtProtocolTime(
        checkedMultiply(line, nonNegative(spec_.lineDuration)));
}

std::uint64_t SstvRobotMapper::lineEndSample(std::uint32_t line) const
{
    if (line >= spec_.height) {
        throw std::out_of_range("Robot line is outside the image");
    }
    return lineStartSample(line + 1U);
}

std::uint64_t SstvRobotMapper::samplesAtProtocolTime(
    std::uint64_t picoseconds) const
{
    if (picoseconds > nonNegative(spec_.imageDuration)) {
        throw std::out_of_range("Robot time is outside the image");
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

SstvRobotPosition SstvRobotMapper::makeNonPixelPosition(
    SstvRobotRegion region,
    std::uint32_t line,
    ColourComponent component,
    std::uint64_t startPicoseconds,
    std::uint64_t endPicoseconds) const
{
    return {region,
            line,
            component,
            0U,
            samplesAtProtocolTime(startPicoseconds),
            samplesAtProtocolTime(endPicoseconds)};
}

SstvRobotPosition SstvRobotMapper::pixelPositionAtSample(
    std::uint32_t line,
    ColourComponent component,
    std::uint64_t componentStartPicoseconds,
    std::uint32_t pixelCount,
    Picoseconds pixelDuration,
    std::uint64_t sample) const
{
    std::uint32_t low = 0U;
    std::uint32_t high = pixelCount;
    while (low + 1U < high) {
        const std::uint32_t middle = low + (high - low) / 2U;
        const std::uint64_t boundary = checkedAdd(
            componentStartPicoseconds,
            checkedMultiply(middle, nonNegative(pixelDuration)));
        if (samplesAtProtocolTime(boundary) <= sample) {
            low = middle;
        } else {
            high = middle;
        }
    }
    const std::uint64_t start = checkedAdd(
        componentStartPicoseconds,
        checkedMultiply(low, nonNegative(pixelDuration)));
    const std::uint64_t end = checkedAdd(
        componentStartPicoseconds,
        checkedMultiply(low + 1U, nonNegative(pixelDuration)));
    return {SstvRobotRegion::Pixel,
            line,
            component,
            low,
            samplesAtProtocolTime(start),
            samplesAtProtocolTime(end)};
}

SstvRobotPosition SstvRobotMapper::positionAtSample(
    std::uint64_t imageSample) const
{
    if (imageSample >= imageSamples_) {
        return {SstvRobotRegion::Complete,
                spec_.height,
                ColourComponent::ModeSpecific,
                0U,
                imageSamples_,
                imageSamples_};
    }
    std::uint32_t low = 0U;
    std::uint32_t high = spec_.height;
    while (low + 1U < high) {
        const std::uint32_t middle = low + (high - low) / 2U;
        if (lineStartSample(middle) <= imageSample) {
            low = middle;
        } else {
            high = middle;
        }
    }
    const std::uint32_t line = low;
    const std::uint64_t lineStart = checkedMultiply(
        line, nonNegative(spec_.lineDuration));
    const LineLayout layout = makeLineLayout(spec_);
    for (std::size_t index = 0U; index < layout.count; ++index) {
        const LayoutElement& element = layout.elements[index];
        const std::uint64_t start = checkedAdd(
            lineStart, element.startPicoseconds);
        const std::uint64_t end = checkedAdd(
            lineStart, element.endPicoseconds);
        if (imageSample >= samplesAtProtocolTime(end)) {
            continue;
        }
        const ColourComponent component = componentForLine(
            spec_, line, element.component);
        if (element.region == SstvRobotRegion::Pixel) {
            return pixelPositionAtSample(
                line,
                component,
                start,
                element.pixelCount,
                element.pixelDuration,
                imageSample);
        }
        return makeNonPixelPosition(
            element.region, line, component, start, end);
    }
    throw std::logic_error("Robot sample did not map to a line element");
}

SstvRobotPosition SstvRobotMapper::positionAtProtocolTime(
    std::uint64_t protocolPicoseconds) const
{
    if (protocolPicoseconds >= nonNegative(spec_.imageDuration)) {
        return {SstvRobotRegion::Complete,
                spec_.height,
                ColourComponent::ModeSpecific,
                0U,
                imageSamples_,
                imageSamples_};
    }
    const std::uint32_t line = static_cast<std::uint32_t>(
        protocolPicoseconds / nonNegative(spec_.lineDuration));
    const std::uint64_t lineStart = checkedMultiply(
        line, nonNegative(spec_.lineDuration));
    const std::uint64_t local = protocolPicoseconds - lineStart;
    const LineLayout layout = makeLineLayout(spec_);
    for (std::size_t index = 0U; index < layout.count; ++index) {
        const LayoutElement& element = layout.elements[index];
        if (local >= element.endPicoseconds) {
            continue;
        }
        const std::uint64_t start = checkedAdd(
            lineStart, element.startPicoseconds);
        const std::uint64_t end = checkedAdd(
            lineStart, element.endPicoseconds);
        const ColourComponent component = componentForLine(
            spec_, line, element.component);
        if (element.region != SstvRobotRegion::Pixel) {
            return makeNonPixelPosition(
                element.region, line, component, start, end);
        }
        const std::uint32_t pixel = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(
                (protocolPicoseconds - start)
                    / nonNegative(element.pixelDuration),
                element.pixelCount - 1U));
        const std::uint64_t pixelStart = checkedAdd(
            start, checkedMultiply(pixel, nonNegative(element.pixelDuration)));
        const std::uint64_t pixelEnd = checkedAdd(
            pixelStart, nonNegative(element.pixelDuration));
        return {SstvRobotRegion::Pixel,
                line,
                component,
                pixel,
                samplesAtProtocolTime(pixelStart),
                samplesAtProtocolTime(pixelEnd)};
    }
    throw std::logic_error("Robot time did not map to a line element");
}

SstvRobotPosition SstvRobotMapper::positionAtElapsedTime(
    Picoseconds elapsed) const
{
    if (elapsed.count < 0) {
        return {};
    }
    const std::uint64_t elapsedPs = static_cast<std::uint64_t>(elapsed.count);
    const std::uint64_t effectiveEnd = scaledPicosecondsCeiling(
        nonNegative(spec_.imageDuration), clockScaleNumerator_);
    if (elapsedPs >= effectiveEnd) {
        return {SstvRobotRegion::Complete,
                spec_.height,
                ColourComponent::ModeSpecific,
                0U,
                imageSamples_,
                imageSamples_};
    }
    const std::uint64_t whole = elapsedPs / clockScaleNumerator_;
    const std::uint64_t remainder = elapsedPs % clockScaleNumerator_;
    const std::uint64_t protocolTime = checkedAdd(
        checkedMultiply(whole, kPpmDenominator),
        checkedMultiply(remainder, kPpmDenominator)
            / clockScaleNumerator_);
    return positionAtProtocolTime(protocolTime);
}

std::array<SstvRobotEncoder::HeaderSegment,
           SstvRobotEncoder::HeaderSegmentCount>
SstvRobotEncoder::makeHeader(std::uint8_t visPayload)
{
    std::array<HeaderSegment, HeaderSegmentCount> header {};
    header[0] = {1'900.0, Picoseconds {300'000'000'000LL}};
    header[1] = {1'200.0, Picoseconds {10'000'000'000LL}};
    header[2] = {1'900.0, Picoseconds {300'000'000'000LL}};
    const auto vis = SstvVisCodec::encodeStandard(visPayload);
    if (vis.symbols.size() != HeaderSegmentCount - 3U) {
        throw std::logic_error("unexpected standard VIS symbol count");
    }
    for (std::size_t index = 0U; index < vis.symbols.size(); ++index) {
        header[index + 3U] = {
            frequencyForVisSymbol(vis.symbols[index]),
            Picoseconds {30'000'000'000LL}};
    }
    return header;
}

std::array<std::uint64_t, SstvRobotEncoder::HeaderSegmentCount + 1U>
SstvRobotEncoder::makeHeaderBoundaries(
    std::uint32_t sampleRate,
    const std::array<HeaderSegment, HeaderSegmentCount>& header)
{
    SstvTimingAccumulator timing(sampleRate);
    std::array<std::uint64_t, HeaderSegmentCount + 1U> boundaries {};
    for (std::size_t index = 0U; index < header.size(); ++index) {
        static_cast<void>(timing.samplesFor(header[index].duration));
        boundaries[index + 1U] = timing.totalSamples();
    }
    return boundaries;
}

SstvRobotEncoder::SstvRobotEncoder(
    const SstvRgbPixel* pixels,
    std::size_t count,
    SstvRobotEncoderConfig config)
    : config_(config)
    , spec_(SstvRobotProtocol::spec(config.mode))
    , pixels_(validatedPixels(pixels, count, config.mode))
    , mapper_({config.mode, config.sampleRate, config.clockErrorPpm})
    , generator_(config.sampleRate, config.headroom)
    , header_(makeHeader(spec_.visPayload))
    , headerBoundaries_(makeHeaderBoundaries(config.sampleRate, header_))
{
    if (!std::isfinite(config.level)
        || config.level < 0.0
        || config.level > MaximumLevel) {
        throw std::invalid_argument("Robot TX level is out of range");
    }
    generator_.validateTone(SstvRobotProtocol::SyncFrequencyHz, config.level);
    generator_.validateTone(SstvRobotProtocol::WhiteFrequencyHz, config.level);
    totalSamples_ = checkedAdd(
        headerBoundaries_.back(), mapper_.imageSampleCount());
    metrics_.residentImageBytes = pixels_.size() * sizeof(SstvRgbPixel);
}

SstvRobotEncoder::SstvRobotEncoder(
    const std::vector<SstvRgbPixel>& pixels,
    SstvRobotEncoderConfig config)
    : SstvRobotEncoder(pixels.data(), pixels.size(), config)
{
}

std::size_t SstvRobotEncoder::pixelCount(SstvRobotMode mode)
{
    const SstvRobotModeSpec modeSpec = SstvRobotProtocol::spec(mode);
    if (modeSpec.width != 0U
        && modeSpec.height > std::numeric_limits<std::size_t>::max()
            / modeSpec.width) {
        throw std::overflow_error("Robot frame pixel count overflow");
    }
    return static_cast<std::size_t>(modeSpec.width) * modeSpec.height;
}

std::size_t SstvRobotEncoder::headerIndexAt(
    std::uint64_t sample) const noexcept
{
    const auto first = headerBoundaries_.begin() + 1;
    const auto found = std::upper_bound(
        first, headerBoundaries_.end(), sample);
    return static_cast<std::size_t>(found - first);
}

std::uint8_t SstvRobotEncoder::chromaValue(
    const SstvRobotPosition& position) const
{
    const std::uint32_t x0 = position.pixel * 2U;
    const std::uint32_t x1 = std::min(x0 + 1U, spec_.width - 1U);
    std::uint32_t sum = 0U;
    std::uint32_t count = 0U;
    auto add = [&](std::uint32_t x, std::uint32_t y) {
        const SstvYCbCrPixel converted = SstvColourConverter::rgbToYCbCr(
            pixels_[static_cast<std::size_t>(y) * spec_.width + x]);
        sum += position.component == ColourComponent::ChrominanceRed
            ? converted.chrominanceRed : converted.chrominanceBlue;
        ++count;
    };
    if (spec_.chromaSubsampling == ChromaSubsampling::Cs420) {
        const std::uint32_t y0 = position.line & ~1U;
        const std::uint32_t y1 = std::min(y0 + 1U, spec_.height - 1U);
        add(x0, y0);
        add(x1, y0);
        add(x0, y1);
        add(x1, y1);
    } else {
        add(x0, position.line);
        add(x1, position.line);
    }
    return roundedAverage(sum, count);
}

double SstvRobotEncoder::imageFrequency(
    const SstvRobotPosition& position) const
{
    switch (position.region) {
    case SstvRobotRegion::Sync:
        return SstvRobotProtocol::SyncFrequencyHz;
    case SstvRobotRegion::ChromaMarker:
        return position.component == ColourComponent::ChrominanceRed
            ? SstvRobotProtocol::CrMarkerFrequencyHz
            : SstvRobotProtocol::CbMarkerFrequencyHz;
    case SstvRobotRegion::MarkerPorch:
        return SstvRobotProtocol::MarkerPorchFrequencyHz;
    case SstvRobotRegion::Pixel:
        break;
    case SstvRobotRegion::Outside:
    case SstvRobotRegion::Complete:
        throw std::logic_error("Robot encoder has no tone at this position");
    }

    std::uint8_t value = 0U;
    if (position.component == ColourComponent::Luminance
        || position.component == ColourComponent::Gray) {
        const std::size_t index = static_cast<std::size_t>(position.line)
            * spec_.width + position.pixel;
        if (index >= pixels_.size()) {
            throw std::logic_error("Robot mapped pixel is outside the frame");
        }
        value = SstvColourConverter::rgbToGrayscale(pixels_[index]);
    } else if (position.component == ColourComponent::ChrominanceRed
               || position.component == ColourComponent::ChrominanceBlue) {
        value = chromaValue(position);
    } else {
        throw std::logic_error("Robot mapped an invalid image component");
    }
    return SstvRobotProtocol::frequencyForValue(value);
}

SstvRobotEncoderPosition SstvRobotEncoder::position() const
{
    SstvRobotEncoderPosition result;
    result.producedSamples = producedSamples_;
    result.totalSamples = totalSamples_;
    if (complete()) {
        result.stage = SstvRobotEncoderStage::Complete;
        return result;
    }
    if (cancelled()) {
        result.stage = SstvRobotEncoderStage::Cancelled;
        return result;
    }
    if (producedSamples_ < headerBoundaries_.back()) {
        result.stage = SstvRobotEncoderStage::Header;
        result.headerSegment = headerIndexAt(producedSamples_);
        result.frequencyHz = header_[result.headerSegment].frequencyHz;
        return result;
    }
    result.stage = SstvRobotEncoderStage::Image;
    result.image = mapper_.positionAtSample(
        producedSamples_ - headerBoundaries_.back());
    result.frequencyHz = imageFrequency(result.image);
    return result;
}

void SstvRobotEncoder::noteTransition(
    SstvRobotEncoderStage stage,
    std::size_t headerIndex,
    const SstvRobotPosition& imagePosition) noexcept
{
    const bool changed = haveLastSegment_
        && (stage != lastStage_
            || (stage == SstvRobotEncoderStage::Header
                && headerIndex != lastHeaderIndex_)
            || (stage == SstvRobotEncoderStage::Image
                && (imagePosition.region != lastImageRegion_
                    || imagePosition.line != lastLine_
                    || imagePosition.component != lastComponent_
                    || imagePosition.pixel != lastPixel_)));
    if (changed
        && metrics_.segmentTransitions
            != std::numeric_limits<std::uint64_t>::max()) {
        ++metrics_.segmentTransitions;
    }
    haveLastSegment_ = true;
    lastStage_ = stage;
    lastHeaderIndex_ = headerIndex;
    lastImageRegion_ = imagePosition.region;
    lastLine_ = imagePosition.line;
    lastComponent_ = imagePosition.component;
    lastPixel_ = imagePosition.pixel;
}

template<typename Sample>
std::size_t SstvRobotEncoder::generate(double frequencyHz,
                                       Sample* output,
                                       std::size_t count)
{
    static_assert(std::is_same_v<Sample, float>
                      || std::is_same_v<Sample, std::int16_t>,
                  "unsupported Robot encoder sample type");
    if constexpr (std::is_same_v<Sample, float>) {
        return generator_.generateFloat(
            frequencyHz, config_.level, output, count);
    } else {
        return generator_.generatePcm16(
            frequencyHz, config_.level, output, count);
    }
}

template<typename Sample>
std::size_t SstvRobotEncoder::pull(Sample* output,
                                   std::size_t capacity)
{
    if (metrics_.pullCalls != std::numeric_limits<std::uint64_t>::max()) {
        ++metrics_.pullCalls;
    }
    if (capacity > MaximumSamplesPerPull) {
        if (metrics_.rejectedInputCalls
            != std::numeric_limits<std::uint64_t>::max()) {
            ++metrics_.rejectedInputCalls;
        }
        if (metrics_.rejectedOversizeCalls
            != std::numeric_limits<std::uint64_t>::max()) {
            ++metrics_.rejectedOversizeCalls;
        }
        throw std::length_error("Robot TX pull exceeds its work bound");
    }
    if (capacity != 0U && output == nullptr) {
        if (metrics_.rejectedInputCalls
            != std::numeric_limits<std::uint64_t>::max()) {
            ++metrics_.rejectedInputCalls;
        }
        throw std::invalid_argument("Robot TX output must not be null");
    }
    if (capacity == 0U || complete() || cancelled()) {
        return 0U;
    }

    std::size_t produced = 0U;
    while (produced < capacity && !complete() && !cancelled()) {
        const SstvRobotEncoderPosition current = position();
        std::uint64_t segmentEnd = 0U;
        if (current.stage == SstvRobotEncoderStage::Header) {
            segmentEnd = headerBoundaries_[current.headerSegment + 1U];
        } else if (current.stage == SstvRobotEncoderStage::Image) {
            segmentEnd = checkedAdd(
                headerBoundaries_.back(), current.image.segmentEndSample);
        } else {
            break;
        }
        if (segmentEnd <= producedSamples_) {
            throw std::logic_error("Robot TX segment made no progress");
        }
        noteTransition(current.stage, current.headerSegment, current.image);
        const std::uint64_t remaining = segmentEnd - producedSamples_;
        const std::size_t requested = std::min<std::size_t>(
            capacity - produced,
            static_cast<std::size_t>(std::min<std::uint64_t>(
                remaining, std::numeric_limits<std::size_t>::max())));
        const std::size_t generated = generate(
            current.frequencyHz, output + produced, requested);
        produced += generated;
        producedSamples_ = checkedAdd(producedSamples_, generated);
        metrics_.producedSamples = producedSamples_;
        if (generated != requested) {
            break;
        }
    }
    return produced;
}

std::size_t SstvRobotEncoder::pullFloat(float* output,
                                        std::size_t capacity)
{
    return pull(output, capacity);
}

std::size_t SstvRobotEncoder::pullPcm16(std::int16_t* output,
                                        std::size_t capacity)
{
    return pull(output, capacity);
}

SstvRobotMode SstvRobotEncoder::mode() const noexcept
{
    return spec_.mode;
}

std::uint64_t SstvRobotEncoder::totalSamples() const noexcept
{
    return totalSamples_;
}

std::uint64_t SstvRobotEncoder::producedSamples() const noexcept
{
    return producedSamples_;
}

bool SstvRobotEncoder::complete() const noexcept
{
    return producedSamples_ >= totalSamples_;
}

bool SstvRobotEncoder::cancelled() const noexcept
{
    return generator_.cancelled();
}

SstvRobotEncoderMetrics SstvRobotEncoder::metrics() const noexcept
{
    SstvRobotEncoderMetrics result = metrics_;
    result.tone = generator_.metrics();
    return result;
}

void SstvRobotEncoder::cancel() noexcept
{
    generator_.cancel();
}

void SstvRobotEncoder::reset() noexcept
{
    generator_.reset();
    producedSamples_ = 0U;
    metrics_ = {};
    metrics_.residentImageBytes = pixels_.size() * sizeof(SstvRgbPixel);
    lastStage_ = SstvRobotEncoderStage::Header;
    lastHeaderIndex_ = 0U;
    lastImageRegion_ = SstvRobotRegion::Outside;
    lastLine_ = 0U;
    lastComponent_ = ColourComponent::ModeSpecific;
    lastPixel_ = 0U;
    haveLastSegment_ = false;
}

void SstvRobotDecoder::validateConfig(const SstvRobotDecoderConfig& config)
{
    if (!std::isfinite(config.frequencyOffsetHz)
        || std::abs(config.frequencyOffsetHz)
            > MaximumAbsoluteFrequencyOffsetHz
        || !std::isfinite(config.minimumObservationConfidence)
        || config.minimumObservationConfidence < 0.0
        || config.minimumObservationConfidence > 1.0
        || config.maximumPendingDirtyEvents == 0U
        || config.maximumPendingDirtyEvents
            > SstvImageFrame::kMaximumDirtyEvents) {
        throw std::invalid_argument("invalid Robot RX configuration");
    }
}

SstvRobotDecoder::SstvRobotDecoder(SstvRobotDecoderConfig config)
    : config_(config)
    , spec_(SstvRobotProtocol::spec(config.mode))
    , mapper_({config.mode, config.sampleRate, config.clockErrorPpm})
    , frame_(std::make_unique<SstvImageFrame>(
          spec_.width,
          spec_.height,
          config.maximumPendingDirtyEvents))
{
    validateConfig(config);
    canonicalLineSamples_ = mapper_.lineEndSample(0U);
    SstvTimingAccumulator timing(config.sampleRate);
    canonicalSyncSamples_ = timing.samplesFor(spec_.syncDuration);
    refreshBufferMetrics();
}

void SstvRobotDecoder::saturatingAdd(std::uint64_t& value,
                                     std::uint64_t increment) noexcept
{
    value = increment > std::numeric_limits<std::uint64_t>::max() - value
        ? std::numeric_limits<std::uint64_t>::max()
        : value + increment;
}

bool SstvRobotDecoder::acceptSync(const SstvRobotLineSync& sync)
{
    if (sync.lineIndex >= spec_.height
        || !std::isfinite(sync.confidence)
        || sync.confidence < 0.0
        || sync.confidence > 1.0) {
        return false;
    }
    const std::uint64_t span = mapper_.lineEndSample(sync.lineIndex)
        - mapper_.lineStartSample(sync.lineIndex);
    if (sync.syncStartSample
        > std::numeric_limits<std::uint64_t>::max() - span
        || (haveCurrentLine_ && sync.lineIndex < currentLine_)) {
        return false;
    }
    Anchor& existing = anchors_[sync.lineIndex];
    if (existing.present) {
        if (existing.startSample == sync.syncStartSample) {
            if (existing.predicted && !sync.predicted) {
                existing.predicted = false;
                existing.confidence = sync.confidence;
                return true;
            }
            return false;
        }
        if (!existing.predicted || sync.predicted
            || (haveCurrentLine_ && currentLine_ == sync.lineIndex
                && nonEmptyAccumulators_ != 0U)) {
            return false;
        }
    }
    for (std::size_t previous = sync.lineIndex; previous != 0U;) {
        --previous;
        if (anchors_[previous].present) {
            if (anchors_[previous].startSample >= sync.syncStartSample) {
                return false;
            }
            break;
        }
    }
    for (std::size_t next = sync.lineIndex + 1U;
         next < spec_.height;
         ++next) {
        if (anchors_[next].present) {
            if (anchors_[next].startSample <= sync.syncStartSample) {
                return false;
            }
            break;
        }
    }
    if (!existing.present) {
        ++metrics_.storedSyncAnchors;
    }
    existing = {sync.syncStartSample,
                sync.confidence,
                true,
                sync.predicted};
    highestStoredAnchorLine_ = std::max(
        highestStoredAnchorLine_, sync.lineIndex);
    return true;
}

std::size_t SstvRobotDecoder::consumeLineSyncs(
    const SstvRobotLineSync* syncs,
    std::size_t count)
{
    if (count > MaximumSyncsPerConsume) {
        saturatingAdd(metrics_.rejectedInputCalls);
        saturatingAdd(metrics_.rejectedOversizeCalls);
        throw std::length_error("Robot RX sync call exceeds its work bound");
    }
    if (count != 0U && syncs == nullptr) {
        saturatingAdd(metrics_.rejectedInputCalls);
        throw std::invalid_argument("Robot RX sync input must not be null");
    }
    if (state_ != SstvRobotDecodeState::Receiving) {
        saturatingAdd(metrics_.droppedSyncsAfterEnd,
                      static_cast<std::uint64_t>(count));
        return 0U;
    }
    std::size_t accepted = 0U;
    for (std::size_t index = 0U; index < count; ++index) {
        saturatingAdd(metrics_.syncInputs);
        if (!acceptSync(syncs[index])) {
            saturatingAdd(metrics_.rejectedSyncs);
            continue;
        }
        ++accepted;
        if (syncs[index].predicted) {
            saturatingAdd(metrics_.predictedSyncs);
        } else {
            saturatingAdd(metrics_.observedSyncs);
        }
    }
    return accepted;
}

std::size_t SstvRobotDecoder::consumeLineSyncs(
    const std::vector<SstvRobotLineSync>& syncs)
{
    return consumeLineSyncs(syncs.data(), syncs.size());
}

const SstvRobotDecoder::Anchor* SstvRobotDecoder::anchorFor(
    std::uint64_t sample,
    std::uint32_t& line) noexcept
{
    const Anchor* found = nullptr;
    std::uint32_t first = 0U;
    if (haveAnchorCursor_
        && anchors_[anchorCursorLine_].present
        && anchors_[anchorCursorLine_].startSample <= sample) {
        found = &anchors_[anchorCursorLine_];
        line = anchorCursorLine_;
        first = anchorCursorLine_ + 1U;
    }
    for (std::uint32_t index = first;
         index <= highestStoredAnchorLine_ && index < spec_.height;
         ++index) {
        const Anchor& anchor = anchors_[index];
        if (anchor.present && anchor.startSample <= sample) {
            found = &anchor;
            line = index;
        } else if (anchor.present) {
            break;
        }
    }
    if (found != nullptr) {
        anchorCursorLine_ = line;
        haveAnchorCursor_ = true;
    }

    // A BlackHole/CoreAudio queue can end immediately after the last few
    // scanline syncs.  In that case the sync tracker has a contiguous tail
    // through (for example) line 116, while the pixel observations for lines
    // 117..119 are still present.  Do not discard those observations merely
    // because their sync pulses were not observed: extrapolate a bounded
    // Robot B/W 8 tail from the last real anchor using the canonical period.
    // This deliberately does not mutate the anchor table, so interior gaps
    // and non-Robot modes retain the strict observed/predicted-anchor rules.
    if (spec_.mode == SstvRobotMode::Bw8
        && metrics_.storedSyncAnchors >= 16U
        && highestStoredAnchorLine_ >= 16U
        && highestStoredAnchorLine_ + 1U < spec_.height
        && anchors_[highestStoredAnchorLine_].present
        && sample >= anchors_[highestStoredAnchorLine_].startSample) {
        const std::uint64_t delta = sample
            - anchors_[highestStoredAnchorLine_].startSample;
        const std::uint64_t period = std::max<std::uint64_t>(
            1U, canonicalLineSamples_);
        const std::uint64_t advance = delta / period;
        constexpr std::uint64_t kMaximumTailLines = 8U;
        if (advance >= 1U && advance <= kMaximumTailLines) {
            const std::uint64_t candidate =
                static_cast<std::uint64_t>(highestStoredAnchorLine_)
                + advance;
            if (candidate < spec_.height) {
                const std::uint64_t offset = advance * period;
                if (offset <= std::numeric_limits<std::uint64_t>::max()
                        - anchors_[highestStoredAnchorLine_].startSample) {
                    syntheticTailAnchor_.startSample =
                        anchors_[highestStoredAnchorLine_].startSample + offset;
                    syntheticTailAnchor_.confidence =
                        anchors_[highestStoredAnchorLine_].confidence;
                    syntheticTailAnchor_.present = true;
                    syntheticTailAnchor_.predicted = true;
                    line = static_cast<std::uint32_t>(candidate);
                    return &syntheticTailAnchor_;
                }
            }
        }
    }
    return found;
}

std::size_t SstvRobotDecoder::accumulatorIndex(
    ColourComponent component,
    std::uint32_t pixel)
{
    switch (component) {
    case ColourComponent::Luminance:
    case ColourComponent::Gray:
        if (pixel >= SstvRobotProtocol::MaximumWidth) {
            throw std::out_of_range("Robot luma pixel is outside a scanline");
        }
        return pixel;
    case ColourComponent::ChrominanceBlue:
        if (pixel >= SstvRobotProtocol::MaximumWidth / 2U) {
            throw std::out_of_range("Robot Cb pixel is outside a scanline");
        }
        return SstvRobotProtocol::MaximumWidth + pixel;
    case ColourComponent::ChrominanceRed:
        if (pixel >= SstvRobotProtocol::MaximumWidth / 2U) {
            throw std::out_of_range("Robot Cr pixel is outside a scanline");
        }
        return SstvRobotProtocol::MaximumWidth
            + SstvRobotProtocol::MaximumWidth / 2U + pixel;
    default:
        throw std::invalid_argument("invalid Robot image component");
    }
}

bool SstvRobotDecoder::beginLine(std::uint32_t line)
{
    if (!haveCurrentLine_) {
        currentLine_ = line;
        haveCurrentLine_ = true;
        return true;
    }
    if (line == currentLine_) {
        return true;
    }
    if (line < currentLine_) {
        saturatingAdd(metrics_.staleObservations);
        return false;
    }
    publishCurrentLine();
    currentLine_ = line;
    haveCurrentLine_ = true;
    return true;
}

void SstvRobotDecoder::accumulate(
    const SstvRobotPosition& position,
    double frequencyHz,
    double confidence) noexcept
{
    std::size_t index = 0U;
    try {
        index = accumulatorIndex(position.component, position.pixel);
    } catch (...) {
        saturatingAdd(metrics_.numericFaults);
        return;
    }
    PixelAccumulator& accumulator = accumulators_[index];
    if (accumulator.count == 0U) {
        accumulator.meanFrequencyHz = frequencyHz;
        accumulator.meanConfidence = confidence;
        accumulator.count = 1U;
        ++nonEmptyAccumulators_;
        refreshBufferMetrics();
        return;
    }
    const std::uint32_t nextCount = accumulator.count
        == std::numeric_limits<std::uint32_t>::max()
        ? accumulator.count : accumulator.count + 1U;
    const double divisor = static_cast<double>(nextCount);
    accumulator.meanFrequencyHz +=
        (frequencyHz - accumulator.meanFrequencyHz) / divisor;
    accumulator.meanConfidence +=
        (confidence - accumulator.meanConfidence) / divisor;
    accumulator.count = nextCount;
}

bool SstvRobotDecoder::writePixel(std::uint32_t x,
                                  std::uint32_t y,
                                  SstvRgbPixel pixel)
{
    const SstvImageWriteResult result = frame_->writePixel(x, y, pixel);
    if (result == SstvImageWriteResult::Cancelled) {
        state_ = SstvRobotDecodeState::Cancelled;
        return false;
    }
    saturatingAdd(metrics_.componentsPublished, 3U);
    return true;
}

void SstvRobotDecoder::publishMonochromeLine()
{
    bool wrote = false;
    for (std::uint32_t x = 0U; x < spec_.width; ++x) {
        const PixelAccumulator& y = accumulators_[accumulatorIndex(
            ColourComponent::Gray, x)];
        if (y.count == 0U) {
            continue;
        }
        if (!writePixel(x,
                        currentLine_,
                        SstvColourConverter::grayscaleToRgb(
                            SstvRobotProtocol::valueForFrequency(
                                y.meanFrequencyHz)))) {
            return;
        }
        wrote = true;
    }
    if (wrote) {
        saturatingAdd(metrics_.linesPublished);
    }
}

void SstvRobotDecoder::publishCs422Line()
{
    bool wrote = false;
    for (std::uint32_t x = 0U; x < spec_.width; ++x) {
        const PixelAccumulator& y = accumulators_[accumulatorIndex(
            ColourComponent::Luminance, x)];
        const PixelAccumulator& cb = accumulators_[accumulatorIndex(
            ColourComponent::ChrominanceBlue, x / 2U)];
        const PixelAccumulator& cr = accumulators_[accumulatorIndex(
            ColourComponent::ChrominanceRed, x / 2U)];
        if (y.count == 0U || cb.count == 0U || cr.count == 0U) {
            continue;
        }
        const SstvYCbCrPixel converted {
            SstvRobotProtocol::valueForFrequency(y.meanFrequencyHz),
            SstvRobotProtocol::valueForFrequency(cb.meanFrequencyHz),
            SstvRobotProtocol::valueForFrequency(cr.meanFrequencyHz)};
        if (!writePixel(x,
                        currentLine_,
                        SstvColourConverter::yCbCrToRgb(converted))) {
            return;
        }
        wrote = true;
    }
    if (wrote) {
        saturatingAdd(metrics_.linesPublished);
    }
}

void SstvRobotDecoder::publishCs420Line()
{
    if (currentLine_ % 2U == 0U) {
        clearPendingPair();
        pendingPairLine_ = currentLine_;
        for (std::uint32_t x = 0U; x < spec_.width; ++x) {
            const PixelAccumulator& y = accumulators_[accumulatorIndex(
                ColourComponent::Luminance, x)];
            if (y.count != 0U) {
                pendingLuma_[x] = SstvRobotProtocol::valueForFrequency(
                    y.meanFrequencyHz);
                pendingLumaPresent_[x] = true;
                havePendingPair_ = true;
            }
        }
        for (std::uint32_t x = 0U; x < spec_.chromaWidth; ++x) {
            const PixelAccumulator& cr = accumulators_[accumulatorIndex(
                ColourComponent::ChrominanceRed, x)];
            if (cr.count != 0U) {
                pendingCr_[x] = SstvRobotProtocol::valueForFrequency(
                    cr.meanFrequencyHz);
                pendingCrPresent_[x] = true;
                havePendingPair_ = true;
            }
        }
        return;
    }

    if (!havePendingPair_ || pendingPairLine_ + 1U != currentLine_) {
        clearPendingPair();
        return;
    }
    bool evenWrote = false;
    bool oddWrote = false;
    for (std::uint32_t x = 0U; x < spec_.width; ++x) {
        const PixelAccumulator& oddY = accumulators_[accumulatorIndex(
            ColourComponent::Luminance, x)];
        const PixelAccumulator& cb = accumulators_[accumulatorIndex(
            ColourComponent::ChrominanceBlue, x / 2U)];
        if (cb.count == 0U || !pendingCrPresent_[x / 2U]) {
            continue;
        }
        const std::uint8_t cbValue = SstvRobotProtocol::valueForFrequency(
            cb.meanFrequencyHz);
        if (pendingLumaPresent_[x]) {
            if (!writePixel(
                    x,
                    pendingPairLine_,
                    SstvColourConverter::yCbCrToRgb(
                        {pendingLuma_[x], cbValue, pendingCr_[x / 2U]}))) {
                return;
            }
            evenWrote = true;
        }
        if (oddY.count != 0U) {
            if (!writePixel(
                    x,
                    currentLine_,
                    SstvColourConverter::yCbCrToRgb(
                        {SstvRobotProtocol::valueForFrequency(
                             oddY.meanFrequencyHz),
                         cbValue,
                         pendingCr_[x / 2U]}))) {
                return;
            }
            oddWrote = true;
        }
    }
    if (evenWrote) {
        saturatingAdd(metrics_.linesPublished);
    }
    if (oddWrote) {
        saturatingAdd(metrics_.linesPublished);
    }
    clearPendingPair();
}

void SstvRobotDecoder::publishCurrentLine()
{
    if (!haveCurrentLine_ || nonEmptyAccumulators_ == 0U) {
        clearLineAccumulators();
        return;
    }
    fillBoundedCompatibilityEdges();
    if (!spec_.colour) {
        publishMonochromeLine();
    } else if (spec_.chromaSubsampling == ChromaSubsampling::Cs422) {
        publishCs422Line();
    } else {
        publishCs420Line();
    }
    clearLineAccumulators();
}

void SstvRobotDecoder::clearLineAccumulators() noexcept
{
    accumulators_.fill({});
    nonEmptyAccumulators_ = 0U;
    refreshBufferMetrics();
}

void SstvRobotDecoder::clearPendingPair() noexcept
{
    pendingLuma_.fill(0U);
    pendingLumaPresent_.fill(false);
    pendingCr_.fill(0U);
    pendingCrPresent_.fill(false);
    pendingPairLine_ = 0U;
    havePendingPair_ = false;
}

void SstvRobotDecoder::refreshBufferMetrics() noexcept
{
    metrics_.bufferedPixelAccumulators = nonEmptyAccumulators_;
    metrics_.peakBufferedPixelAccumulators = std::max(
        metrics_.peakBufferedPixelAccumulators, nonEmptyAccumulators_);
}

std::size_t SstvRobotDecoder::consume(
    const SstvFrequencyObservation* observations,
    std::size_t count)
{
    if (count > MaximumObservationsPerConsume) {
        saturatingAdd(metrics_.rejectedInputCalls);
        saturatingAdd(metrics_.rejectedOversizeCalls);
        throw std::length_error(
            "Robot RX observation call exceeds its work bound");
    }
    if (count != 0U && observations == nullptr) {
        saturatingAdd(metrics_.rejectedInputCalls);
        throw std::invalid_argument(
            "Robot RX observations must not be null");
    }
    if (state_ != SstvRobotDecodeState::Receiving) {
        saturatingAdd(metrics_.droppedObservationsAfterEnd,
                      static_cast<std::uint64_t>(count));
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
            || observation.confidence > 1.0) {
            saturatingAdd(metrics_.invalidObservations);
            continue;
        }
        const double frequency = observation.correctedFrequencyHz
            - config_.frequencyOffsetHz;
        if (!std::isfinite(frequency)
            || frequency < SstvRobotProtocol::BlackFrequencyHz - 500.0
            || frequency > SstvRobotProtocol::WhiteFrequencyHz + 500.0) {
            saturatingAdd(metrics_.invalidObservations);
            continue;
        }
        if (haveLastObservation_
            && observation.centreSample < lastObservationSample_) {
            saturatingAdd(metrics_.staleObservations);
            continue;
        }
        lastObservationSample_ = observation.centreSample;
        haveLastObservation_ = true;

        std::uint32_t line = 0U;
        const Anchor* anchor = anchorFor(observation.centreSample, line);
        if (anchor == nullptr) {
            saturatingAdd(metrics_.unanchoredObservations);
            continue;
        }
        std::uint64_t mappedSample = 0U;
        try {
            const std::uint64_t offset =
                observation.centreSample - anchor->startSample;
            std::uint64_t observedLineSamples = canonicalLineSamples_;
            if (line + 1U < spec_.height
                && anchors_[line + 1U].present
                && anchors_[line + 1U].startSample > anchor->startSample) {
                observedLineSamples = anchors_[line + 1U].startSample
                    - anchor->startSample;
            } else if (line > 0U && anchors_[line - 1U].present
                       && anchor->startSample
                           > anchors_[line - 1U].startSample) {
                observedLineSamples = anchor->startSample
                    - anchors_[line - 1U].startSample;
            }
            const std::uint64_t compatibilityLineSamples =
                static_cast<std::uint64_t>(config_.sampleRate) * 67U / 1'000U;
            const std::uint64_t compatibilitySyncSamples =
                static_cast<std::uint64_t>(config_.sampleRate) * 7U / 1'000U;
            const std::uint64_t compatibilityPeriodTolerance = std::max<
                std::uint64_t>(
                    2U,
                    static_cast<std::uint64_t>(config_.sampleRate)
                        * 2U / 1'000U);
            const bool compatibilityBw =
                spec_.mode == SstvRobotMode::Bw8
                && absoluteDifference(observedLineSamples,
                                      compatibilityLineSamples)
                    <= compatibilityPeriodTolerance
                // The 66 ms canonical and 67 ms compatibility periods are
                // both inside the acquisition envelope.  Select the profile
                // nearest the observed anchors so native 10/56 timing is not
                // remapped as 7/60 timing.
                && absoluteDifference(observedLineSamples,
                                      compatibilityLineSamples)
                    < absoluteDifference(observedLineSamples,
                                         canonicalLineSamples_);
            compatibilityBw8Observed_ = compatibilityBw8Observed_
                || compatibilityBw;
            const std::uint64_t observedSyncSamples = compatibilityBw
                ? compatibilitySyncSamples : canonicalSyncSamples_;
            const std::uint64_t observedScanSamples =
                observedLineSamples > observedSyncSamples
                ? observedLineSamples - observedSyncSamples : 0U;
            const std::uint64_t canonicalScanSamples =
                canonicalLineSamples_ > canonicalSyncSamples_
                ? canonicalLineSamples_ - canonicalSyncSamples_ : 0U;
            std::uint64_t normalizedOffset = offset;
            if (compatibilityBw && offset >= observedSyncSamples
                && observedScanSamples != 0U
                && canonicalScanSamples != 0U) {
                const std::uint64_t scanOffset = std::min(
                    offset - observedSyncSamples,
                    observedScanSamples - 1U);
                normalizedOffset = canonicalSyncSamples_
                    + scanOffset * canonicalScanSamples
                        / observedScanSamples;
            }
            mappedSample = checkedAdd(
                mapper_.lineStartSample(line),
                normalizedOffset);
        } catch (const std::overflow_error&) {
            saturatingAdd(metrics_.numericFaults);
            continue;
        }
        const SstvRobotPosition position = mapper_.positionAtSample(
            mappedSample);
        if (position.line != line) {
            saturatingAdd(metrics_.outOfLineObservations);
            continue;
        }
        if (position.region != SstvRobotRegion::Pixel) {
            saturatingAdd(metrics_.nonPixelObservations);
            continue;
        }
        if (!beginLine(line)) {
            continue;
        }
        accumulate(position, frequency, observation.confidence);
        saturatingAdd(metrics_.acceptedObservations);
        ++accepted;
    }
    return accepted;
}

std::size_t SstvRobotDecoder::consume(
    const std::vector<SstvFrequencyObservation>& observations)
{
    return consume(observations.data(), observations.size());
}

SstvRobotDecodeState SstvRobotDecoder::finish()
{
    if (state_ != SstvRobotDecodeState::Receiving) {
        return state_;
    }
    fillBoundedTerminalSuffix();
    publishCurrentLine();
    haveCurrentLine_ = false;
    fillBoundedTerminalRows();
    state_ = frame_->isComplete()
        ? SstvRobotDecodeState::Complete
        : SstvRobotDecodeState::Partial;
    return state_;
}

bool SstvRobotDecoder::fillTrailingComponent(
    ColourComponent component,
    std::uint32_t count,
    std::uint32_t maximumGap) noexcept
{
    if (count == 0U || maximumGap == 0U) {
        return false;
    }
    std::uint32_t firstMissing = 0U;
    for (; firstMissing < count; ++firstMissing) {
        const std::size_t index = accumulatorIndex(component, firstMissing);
        if (accumulators_[index].count == 0U) {
            break;
        }
    }
    if (firstMissing == 0U || firstMissing == count
        || count - firstMissing > maximumGap) {
        return false;
    }
    for (std::uint32_t pixel = firstMissing; pixel < count; ++pixel) {
        const std::size_t index = accumulatorIndex(component, pixel);
        if (accumulators_[index].count != 0U) {
            return false;
        }
    }

    const PixelAccumulator last = accumulators_[accumulatorIndex(
        component, firstMissing - 1U)];
    for (std::uint32_t pixel = firstMissing; pixel < count; ++pixel) {
        accumulators_[accumulatorIndex(component, pixel)] = last;
        ++nonEmptyAccumulators_;
    }
    refreshBufferMetrics();
    return true;
}

bool SstvRobotDecoder::fillLeadingComponent(
    ColourComponent component,
    std::uint32_t count,
    std::uint32_t maximumGap) noexcept
{
    if (count == 0U || maximumGap == 0U) {
        return false;
    }
    std::uint32_t firstPresent = 0U;
    for (; firstPresent < count; ++firstPresent) {
        if (accumulators_[accumulatorIndex(component, firstPresent)].count
            != 0U) {
            break;
        }
    }
    if (firstPresent == 0U || firstPresent == count
        || firstPresent > maximumGap) {
        return false;
    }
    const PixelAccumulator first = accumulators_[accumulatorIndex(
        component, firstPresent)];
    for (std::uint32_t pixel = 0U; pixel < firstPresent; ++pixel) {
        accumulators_[accumulatorIndex(component, pixel)] = first;
        ++nonEmptyAccumulators_;
    }
    refreshBufferMetrics();
    return true;
}

void SstvRobotDecoder::fillBoundedCompatibilityEdges() noexcept
{
    if (!compatibilityBw8Observed_
        || spec_.mode != SstvRobotMode::Bw8) {
        return;
    }
    // A finite detector window centred across the sync-to-video transition
    // can leave at most two edge pixels without an estimate.  This recovery
    // is compatibility-profile-only and never crosses an interior dropout.
    constexpr std::uint32_t kMaximumEdgeGap = 2U;
    static_cast<void>(fillLeadingComponent(
        ColourComponent::Gray, spec_.width, kMaximumEdgeGap));
    static_cast<void>(fillTrailingComponent(
        ColourComponent::Gray, spec_.width, kMaximumEdgeGap));
}

void SstvRobotDecoder::fillBoundedTerminalSuffix() noexcept
{
    constexpr std::uint32_t kMaximumTerminalRows = 3U;
    if (!haveCurrentLine_
        || currentLine_ + kMaximumTerminalRows < spec_.height
        || nonEmptyAccumulators_ == 0U) {
        return;
    }

    // A finite-window frequency observation is centred before the last few
    // transmitted samples, so an otherwise contiguous final scanline can
    // lose a tiny suffix at EOF.  Extrapolate only a bounded, gap-free suffix
    // from its immediately preceding pixel; never bridge an interior dropout.
    // The native 12 kHz FFT window can leave a longer suffix at the final
    // virtual-audio callback boundary than a normal RF capture.  Keep this
    // recovery bounded to one contiguous tail (never an interior dropout),
    // but allow the observed BlackHole tail of up to 64 pixels to complete
    // the final Robot B/W 8 scanline.
    constexpr std::uint32_t kMaximumLumaGap = 64U;
    constexpr std::uint32_t kMaximumChromaGap = 8U;
    if (!spec_.colour) {
        static_cast<void>(fillTrailingComponent(
            ColourComponent::Gray, spec_.width, kMaximumLumaGap));
        return;
    }
    static_cast<void>(fillTrailingComponent(
        ColourComponent::Luminance, spec_.width, kMaximumLumaGap));
    if (spec_.chromaSubsampling == ChromaSubsampling::Cs422) {
        static_cast<void>(fillTrailingComponent(
            ColourComponent::ChrominanceRed,
            spec_.chromaWidth,
            kMaximumChromaGap));
        static_cast<void>(fillTrailingComponent(
            ColourComponent::ChrominanceBlue,
            spec_.chromaWidth,
            kMaximumChromaGap));
    } else if (currentLine_ % 2U == 0U) {
        static_cast<void>(fillTrailingComponent(
            ColourComponent::ChrominanceRed,
            spec_.chromaWidth,
            kMaximumChromaGap));
    } else {
        static_cast<void>(fillTrailingComponent(
            ColourComponent::ChrominanceBlue,
            spec_.chromaWidth,
            kMaximumChromaGap));
    }
}

void SstvRobotDecoder::fillBoundedTerminalRows()
{
    if (!config_.allowTerminalRowRecovery
        || spec_.mode != SstvRobotMode::Bw8
        || frame_->isComplete()) {
        return;
    }

    // Only recover at most three sparse rows after a strong frame has
    // already been assembled.  BlackHole/AudioQueue callback boundaries can
    // leave a handful of rows with a few uncovered pixels even though the
    // rest of the frame is valid.  Interior dropouts beyond this bounded
    // allowance and low-coverage/noise frames remain partial.
    constexpr std::uint32_t kMaximumTerminalRows = 3U;
    const SstvImageSnapshot snapshot = frame_->snapshot();
    if (snapshot.coverage() < 0.95) {
        return;
    }

    std::array<std::uint32_t, kMaximumTerminalRows> missingRows {};
    std::uint32_t missingCount = 0U;
    for (std::uint32_t row = 0U; row < spec_.height; ++row) {
        if (snapshot.isScanlineComplete(row)) {
            continue;
        }
        if (missingCount == kMaximumTerminalRows) {
            return;
        }
        missingRows[missingCount++] = row;
    }
    if (missingCount == 0U) {
        return;
    }

    for (std::uint32_t missingIndex = 0U;
         missingIndex < missingCount;
         ++missingIndex) {
        const std::uint32_t line = missingRows[missingIndex];
        std::uint32_t sourceLine = spec_.height;
        for (std::uint32_t distance = 1U;
             distance < spec_.height && sourceLine == spec_.height;
             ++distance) {
            if (line >= distance
                && snapshot.isScanlineComplete(line - distance)) {
                sourceLine = line - distance;
            } else if (line + distance < spec_.height
                       && snapshot.isScanlineComplete(line + distance)) {
                sourceLine = line + distance;
            }
        }
        if (sourceLine == spec_.height) {
            return;
        }
        for (std::uint32_t pixel = 0U; pixel < spec_.width; ++pixel) {
            if (snapshot.coverageMask(pixel, line) == 0x07U) {
                continue;
            }
            const SstvImageWriteResult result = frame_->writePixel(
                pixel, line, snapshot.pixel(pixel, sourceLine));
            if (result == SstvImageWriteResult::Cancelled) {
                state_ = SstvRobotDecodeState::Cancelled;
                return;
            }
            saturatingAdd(metrics_.componentsPublished, 3U);
        }
        if (frame_->isScanlineComplete(line)) {
            metrics_.linesPublished = std::min<std::uint64_t>(
                spec_.height, metrics_.linesPublished + 1U);
        }
    }
}

void SstvRobotDecoder::cancel() noexcept
{
    if (state_ == SstvRobotDecodeState::Receiving) {
        frame_->cancel();
        state_ = SstvRobotDecodeState::Cancelled;
        clearLineAccumulators();
        clearPendingPair();
        haveCurrentLine_ = false;
    }
}

void SstvRobotDecoder::reset() noexcept
{
    frame_->reset();
    anchors_.fill({});
    accumulators_.fill({});
    clearPendingPair();
    metrics_ = {};
    state_ = SstvRobotDecodeState::Receiving;
    currentLine_ = 0U;
    haveCurrentLine_ = false;
    lastObservationSample_ = 0U;
    haveLastObservation_ = false;
    nonEmptyAccumulators_ = 0U;
    highestStoredAnchorLine_ = 0U;
    anchorCursorLine_ = 0U;
    haveAnchorCursor_ = false;
    compatibilityBw8Observed_ = false;
    refreshBufferMetrics();
}

SstvRobotMode SstvRobotDecoder::mode() const noexcept
{
    return spec_.mode;
}

double SstvRobotDecoder::setFrequencyOffsetHz(double offsetHz)
{
    if (!std::isfinite(offsetHz)
        || std::abs(offsetHz) > MaximumAbsoluteFrequencyOffsetHz) {
        throw std::invalid_argument(
            "Robot RX frequency offset is out of range");
    }
    config_.frequencyOffsetHz = offsetHz;
    return config_.frequencyOffsetHz;
}

double SstvRobotDecoder::frequencyOffsetHz() const noexcept
{
    return config_.frequencyOffsetHz;
}

SstvRobotDecodeState SstvRobotDecoder::state() const noexcept
{
    return state_;
}

const SstvImageFrame& SstvRobotDecoder::imageFrame() const noexcept
{
    return *frame_;
}

SstvImageSnapshot SstvRobotDecoder::snapshot() const
{
    return frame_->snapshot();
}

std::vector<SstvDirtyEvent> SstvRobotDecoder::takeDirtyEvents()
{
    return frame_->takeDirtyEvents();
}

SstvRobotDecoderMetrics SstvRobotDecoder::metrics() const noexcept
{
    return metrics_;
}

} // namespace decodium::sstv
