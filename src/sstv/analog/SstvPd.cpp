// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvPd.h"

#include "../core/SstvTimingAccumulator.h"
#include "../core/SstvVisCodec.h"

#include <algorithm>
#include <cmath>
#include <iterator>
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
    SstvPdRegion region {SstvPdRegion::Outside};
    std::uint8_t scanIndex {0U};
    ColourComponent component {ColourComponent::ModeSpecific};
    std::uint64_t startPicoseconds {0U};
    std::uint64_t endPicoseconds {0U};
};

std::uint64_t checkedAdd(std::uint64_t left, std::uint64_t right)
{
    if (left > std::numeric_limits<std::uint64_t>::max() - right) {
        throw std::overflow_error("PD integer addition overflow");
    }
    return left + right;
}

std::uint64_t checkedMultiply(std::uint64_t left, std::uint64_t right)
{
    if (left != 0U
        && right > std::numeric_limits<std::uint64_t>::max() / left) {
        throw std::overflow_error("PD integer multiplication overflow");
    }
    return left * right;
}

std::uint64_t nonNegative(Picoseconds duration)
{
    if (duration.count < 0) {
        throw std::logic_error("PD protocol contains a negative duration");
    }
    return static_cast<std::uint64_t>(duration.count);
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
        throw std::logic_error("invalid PD duration fraction");
    }
    return checkedMultiply(duration, numerator) / denominator;
}

SstvPdModeSpec makeSpec(SstvPdMode mode,
                        const char* stableId,
                        const char* displayName,
                        std::uint32_t width,
                        std::uint32_t height,
                        std::uint8_t visPayload,
                        Picoseconds pixelDuration)
{
    constexpr Picoseconds sync {20'000'000'000LL};
    constexpr Picoseconds porch {2'080'000'000LL};
    if (width == 0U || height == 0U || (height % 2U) != 0U) {
        throw std::logic_error("PD mode geometry must contain row pairs");
    }
    const std::uint64_t component = checkedMultiply(
        nonNegative(pixelDuration), width);
    const std::uint64_t pair = checkedAdd(
        checkedAdd(nonNegative(sync), nonNegative(porch)),
        checkedMultiply(component, 4U));
    const std::uint64_t image = checkedMultiply(pair, height / 2U);
    if (component > static_cast<std::uint64_t>(
                        std::numeric_limits<std::int64_t>::max())
        || pair > static_cast<std::uint64_t>(
                       std::numeric_limits<std::int64_t>::max())
        || image > static_cast<std::uint64_t>(
                        std::numeric_limits<std::int64_t>::max())) {
        throw std::overflow_error("PD duration overflow");
    }
    return {mode,
            stableId,
            displayName,
            width,
            height,
            visPayload,
            sync,
            porch,
            pixelDuration,
            Picoseconds {static_cast<std::int64_t>(component)},
            Picoseconds {static_cast<std::int64_t>(pair)},
            Picoseconds {static_cast<std::int64_t>(image)}};
}

std::array<LayoutElement, 6U> makePairLayout(const SstvPdModeSpec& spec)
{
    std::array<LayoutElement, 6U> result {};
    std::uint64_t cursor = 0U;
    auto add = [&](std::size_t index,
                   SstvPdRegion region,
                   std::uint8_t scanIndex,
                   ColourComponent component,
                   std::uint64_t duration) {
        const std::uint64_t end = checkedAdd(cursor, duration);
        result[index] = {region, scanIndex, component, cursor, end};
        cursor = end;
    };
    add(0U, SstvPdRegion::Sync, 0U, ColourComponent::ModeSpecific,
        nonNegative(spec.syncDuration));
    add(1U, SstvPdRegion::Porch, 0U, ColourComponent::ModeSpecific,
        nonNegative(spec.porchDuration));
    add(2U, SstvPdRegion::Pixel, 0U, ColourComponent::Luminance,
        nonNegative(spec.componentDuration));
    add(3U, SstvPdRegion::Pixel, 1U, ColourComponent::ChrominanceRed,
        nonNegative(spec.componentDuration));
    add(4U, SstvPdRegion::Pixel, 2U, ColourComponent::ChrominanceBlue,
        nonNegative(spec.componentDuration));
    add(5U, SstvPdRegion::Pixel, 3U, ColourComponent::Luminance,
        nonNegative(spec.componentDuration));
    if (cursor != nonNegative(spec.linePairDuration)) {
        throw std::logic_error("PD pair layout is inconsistent");
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
        checkedAdd(wholeProduct, remainderProduct / kPpmDenominator),
        (remainderProduct % kPpmDenominator) == 0U ? 0U : 1U);
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
    throw std::logic_error("invalid PD VIS symbol");
}

std::uint8_t averaged(std::uint8_t first, std::uint8_t second) noexcept
{
    // Both audited native encoders use integer pair averaging.  Retaining the
    // floor is deterministic and avoids silently selecting a third rounding
    // convention for half-step chroma values.
    return static_cast<std::uint8_t>(
        (static_cast<std::uint16_t>(first) + second) / 2U);
}

} // namespace

SstvPdModeSpec SstvPdProtocol::spec(SstvPdMode mode)
{
    switch (mode) {
    case SstvPdMode::Pd50:
        return makeSpec(mode, "pd-50", "PD50", 320U, 256U, 93U,
                        Picoseconds {286'000'000LL});
    case SstvPdMode::Pd90:
        return makeSpec(mode, "pd-90", "PD90", 320U, 256U, 99U,
                        Picoseconds {532'000'000LL});
    case SstvPdMode::Pd120:
        return makeSpec(mode, "pd-120", "PD120", 640U, 496U, 95U,
                        Picoseconds {190'000'000LL});
    case SstvPdMode::Pd160:
        return makeSpec(mode, "pd-160", "PD160", 512U, 400U, 98U,
                        Picoseconds {382'000'000LL});
    case SstvPdMode::Pd180:
        return makeSpec(mode, "pd-180", "PD180", 640U, 496U, 96U,
                        Picoseconds {286'000'000LL});
    case SstvPdMode::Pd240:
        return makeSpec(mode, "pd-240", "PD240", 640U, 496U, 97U,
                        Picoseconds {382'000'000LL});
    case SstvPdMode::Pd290:
        return makeSpec(mode, "pd-290", "PD290", 800U, 616U, 94U,
                        Picoseconds {286'000'000LL});
    }
    throw std::invalid_argument("unknown PD mode");
}

std::optional<SstvPdMode> SstvPdProtocol::modeForVis(
    std::uint8_t visPayload) noexcept
{
    switch (visPayload) {
    case 93U:
        return SstvPdMode::Pd50;
    case 99U:
        return SstvPdMode::Pd90;
    case 95U:
        return SstvPdMode::Pd120;
    case 98U:
        return SstvPdMode::Pd160;
    case 96U:
        return SstvPdMode::Pd180;
    case 97U:
        return SstvPdMode::Pd240;
    case 94U:
        return SstvPdMode::Pd290;
    default:
        return std::nullopt;
    }
}

double SstvPdProtocol::frequencyForValue(std::uint8_t value) noexcept
{
    return BlackFrequencyHz
        + (WhiteFrequencyHz - BlackFrequencyHz)
            * static_cast<double>(value) / 255.0;
}

std::uint8_t SstvPdProtocol::valueForFrequency(double frequencyHz) noexcept
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

SstvPdMapper::SstvPdMapper(SstvPdMapperConfig config)
    : config_(config)
    , spec_(SstvPdProtocol::spec(config.mode))
{
    if (config.sampleRate < MinimumSampleRate
        || config.sampleRate > MaximumSampleRate) {
        throw std::invalid_argument("unsupported PD sample rate");
    }
    if (config.clockErrorPpm < -MaximumAbsoluteClockErrorPpm
        || config.clockErrorPpm > MaximumAbsoluteClockErrorPpm) {
        throw std::invalid_argument("PD clock correction is out of range");
    }
    const std::int64_t scale = static_cast<std::int64_t>(kPpmDenominator)
        + config.clockErrorPpm;
    if (scale <= 0
        || scale > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("invalid PD clock scale");
    }
    clockScaleNumerator_ = static_cast<std::uint32_t>(scale);
    if (checkedMultiply(nonNegative(spec_.linePairDuration), linePairCount())
        != nonNegative(spec_.imageDuration)) {
        throw std::logic_error("PD image duration is inconsistent");
    }
    static_cast<void>(makePairLayout(spec_));
    imageSamples_ = samplesAtProtocolTime(nonNegative(spec_.imageDuration));
}

SstvPdMapperConfig SstvPdMapper::config() const noexcept
{
    return config_;
}

SstvPdModeSpec SstvPdMapper::modeSpec() const noexcept
{
    return spec_;
}

std::uint32_t SstvPdMapper::linePairCount() const noexcept
{
    return spec_.height / 2U;
}

std::uint64_t SstvPdMapper::imageSampleCount() const noexcept
{
    return imageSamples_;
}

std::uint64_t SstvPdMapper::linePairStartSample(
    std::uint32_t linePair) const
{
    if (linePair > linePairCount()) {
        throw std::out_of_range("PD line pair is outside the image");
    }
    return samplesAtProtocolTime(checkedMultiply(
        linePair, nonNegative(spec_.linePairDuration)));
}

std::uint64_t SstvPdMapper::linePairEndSample(
    std::uint32_t linePair) const
{
    if (linePair >= linePairCount()) {
        throw std::out_of_range("PD line pair is outside the image");
    }
    return linePairStartSample(linePair + 1U);
}

std::uint64_t SstvPdMapper::samplesAtProtocolTime(
    std::uint64_t picoseconds) const
{
    if (picoseconds > nonNegative(spec_.imageDuration)) {
        throw std::out_of_range("PD time is outside the image");
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

SstvPdPosition SstvPdMapper::makePosition(
    SstvPdRegion region,
    std::uint32_t linePair,
    std::uint8_t scanIndex,
    ColourComponent component,
    std::uint32_t pixel,
    std::uint64_t startPicoseconds,
    std::uint64_t endPicoseconds) const
{
    return {region,
            linePair,
            linePair * 2U,
            scanIndex,
            component,
            pixel,
            samplesAtProtocolTime(startPicoseconds),
            samplesAtProtocolTime(endPicoseconds)};
}

SstvPdPosition SstvPdMapper::positionAtSample(
    std::uint64_t imageSample) const
{
    if (imageSample >= imageSamples_) {
        return {SstvPdRegion::Complete,
                linePairCount(),
                spec_.height,
                0U,
                ColourComponent::ModeSpecific,
                0U,
                imageSamples_,
                imageSamples_};
    }
    std::uint32_t low = 0U;
    std::uint32_t high = linePairCount();
    while (low + 1U < high) {
        const std::uint32_t middle = low + (high - low) / 2U;
        if (linePairStartSample(middle) <= imageSample) {
            low = middle;
        } else {
            high = middle;
        }
    }
    const std::uint32_t pair = low;
    const std::uint64_t pairStart = checkedMultiply(
        pair, nonNegative(spec_.linePairDuration));
    for (const LayoutElement& element : makePairLayout(spec_)) {
        const std::uint64_t start = checkedAdd(
            pairStart, element.startPicoseconds);
        const std::uint64_t end = checkedAdd(
            pairStart, element.endPicoseconds);
        if (start == end || imageSample >= samplesAtProtocolTime(end)) {
            continue;
        }
        if (element.region != SstvPdRegion::Pixel) {
            return makePosition(element.region,
                                pair,
                                element.scanIndex,
                                element.component,
                                0U,
                                start,
                                end);
        }
        std::uint32_t pixelLow = 0U;
        std::uint32_t pixelHigh = spec_.width;
        const std::uint64_t component = nonNegative(spec_.componentDuration);
        while (pixelLow + 1U < pixelHigh) {
            const std::uint32_t middle =
                pixelLow + (pixelHigh - pixelLow) / 2U;
            const std::uint64_t boundary = checkedAdd(
                start, fractionOf(component, middle, spec_.width));
            if (samplesAtProtocolTime(boundary) <= imageSample) {
                pixelLow = middle;
            } else {
                pixelHigh = middle;
            }
        }
        const std::uint64_t pixelStart = checkedAdd(
            start, fractionOf(component, pixelLow, spec_.width));
        const std::uint64_t pixelEnd = checkedAdd(
            start, fractionOf(component, pixelLow + 1U, spec_.width));
        return makePosition(SstvPdRegion::Pixel,
                            pair,
                            element.scanIndex,
                            element.component,
                            pixelLow,
                            pixelStart,
                            pixelEnd);
    }
    throw std::logic_error("PD sample did not map to a pair element");
}

SstvPdPosition SstvPdMapper::positionAtProtocolTime(
    std::uint64_t protocolPicoseconds) const
{
    if (protocolPicoseconds >= nonNegative(spec_.imageDuration)) {
        return {SstvPdRegion::Complete,
                linePairCount(),
                spec_.height,
                0U,
                ColourComponent::ModeSpecific,
                0U,
                imageSamples_,
                imageSamples_};
    }
    const std::uint64_t pairDuration = nonNegative(spec_.linePairDuration);
    const std::uint32_t pair = static_cast<std::uint32_t>(
        protocolPicoseconds / pairDuration);
    const std::uint64_t pairStart = checkedMultiply(pair, pairDuration);
    const std::uint64_t local = protocolPicoseconds - pairStart;
    for (const LayoutElement& element : makePairLayout(spec_)) {
        if (element.startPicoseconds == element.endPicoseconds
            || local >= element.endPicoseconds) {
            continue;
        }
        const std::uint64_t start = checkedAdd(
            pairStart, element.startPicoseconds);
        const std::uint64_t end = checkedAdd(
            pairStart, element.endPicoseconds);
        if (element.region != SstvPdRegion::Pixel) {
            return makePosition(element.region,
                                pair,
                                element.scanIndex,
                                element.component,
                                0U,
                                start,
                                end);
        }
        const std::uint64_t component = nonNegative(spec_.componentDuration);
        const std::uint64_t offset = protocolPicoseconds - start;
        const std::uint32_t pixel = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(
                checkedMultiply(offset, spec_.width) / component,
                spec_.width - 1U));
        const std::uint64_t pixelStart = checkedAdd(
            start, fractionOf(component, pixel, spec_.width));
        const std::uint64_t pixelEnd = checkedAdd(
            start, fractionOf(component, pixel + 1U, spec_.width));
        return makePosition(SstvPdRegion::Pixel,
                            pair,
                            element.scanIndex,
                            element.component,
                            pixel,
                            pixelStart,
                            pixelEnd);
    }
    throw std::logic_error("PD time did not map to a pair element");
}

SstvPdPosition SstvPdMapper::positionAtElapsedTime(Picoseconds elapsed) const
{
    if (elapsed.count < 0) {
        return {};
    }
    const std::uint64_t elapsedPs = static_cast<std::uint64_t>(elapsed.count);
    const std::uint64_t effectiveEnd = scaledPicosecondsCeiling(
        nonNegative(spec_.imageDuration), clockScaleNumerator_);
    if (elapsedPs >= effectiveEnd) {
        return {SstvPdRegion::Complete,
                linePairCount(),
                spec_.height,
                0U,
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

std::array<SstvPdEncoder::HeaderSegment, SstvPdEncoder::HeaderSegmentCount>
SstvPdEncoder::makeHeader(std::uint8_t visPayload)
{
    std::array<HeaderSegment, HeaderSegmentCount> header {};
    header[0] = {1'900.0, Picoseconds {300'000'000'000LL}};
    header[1] = {1'200.0, Picoseconds {10'000'000'000LL}};
    header[2] = {1'900.0, Picoseconds {300'000'000'000LL}};
    const SstvVisEncodedFrame vis = SstvVisCodec::encodeStandard(visPayload);
    if (vis.symbols.size() != HeaderSegmentCount - 3U) {
        throw std::logic_error("unexpected PD standard VIS symbol count");
    }
    for (std::size_t index = 0U; index < vis.symbols.size(); ++index) {
        header[index + 3U] = {
            frequencyForVisSymbol(vis.symbols[index]),
            Picoseconds {30'000'000'000LL}};
    }
    return header;
}

std::array<std::uint64_t, SstvPdEncoder::HeaderSegmentCount + 1U>
SstvPdEncoder::makeHeaderBoundaries(
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

std::vector<SstvYCbCrPixel> SstvPdEncoder::convertPixels(
    const SstvRgbPixel* pixels,
    std::size_t count,
    SstvPdMode mode)
{
    if (count != pixelCount(mode)) {
        throw std::invalid_argument(
            "PD pixel count does not match the selected mode");
    }
    if (pixels == nullptr) {
        throw std::invalid_argument("PD encoder pixels must not be null");
    }
    std::vector<SstvYCbCrPixel> converted;
    converted.reserve(count);
    std::transform(pixels,
                   pixels + count,
                   std::back_inserter(converted),
                   [](SstvRgbPixel pixel) {
                       return SstvColourConverter::rgbToYCbCr(pixel);
                   });
    return converted;
}

SstvPdEncoder::SstvPdEncoder(const SstvRgbPixel* pixels,
                             std::size_t count,
                             SstvPdEncoderConfig config)
    : config_(config)
    , spec_(SstvPdProtocol::spec(config.mode))
    , pixels_(convertPixels(pixels, count, config.mode))
    , mapper_({config.mode, config.sampleRate, config.clockErrorPpm})
    , generator_(config.sampleRate, config.headroom)
    , header_(makeHeader(spec_.visPayload))
    , headerBoundaries_(makeHeaderBoundaries(config.sampleRate, header_))
{
    if (!std::isfinite(config.level)
        || config.level < 0.0 || config.level > MaximumLevel) {
        throw std::invalid_argument("PD TX level is out of range");
    }
    generator_.validateTone(SstvPdProtocol::SyncFrequencyHz, config.level);
    generator_.validateTone(SstvPdProtocol::WhiteFrequencyHz, config.level);
    totalSamples_ = checkedAdd(headerBoundaries_.back(),
                               mapper_.imageSampleCount());
    metrics_.residentImageBytes = pixels_.size() * sizeof(SstvYCbCrPixel);
}

SstvPdEncoder::SstvPdEncoder(const std::vector<SstvRgbPixel>& pixels,
                             SstvPdEncoderConfig config)
    : SstvPdEncoder(pixels.data(), pixels.size(), config)
{
}

std::size_t SstvPdEncoder::pixelCount(SstvPdMode mode)
{
    const SstvPdModeSpec modeSpec = SstvPdProtocol::spec(mode);
    if (modeSpec.width != 0U
        && modeSpec.height > std::numeric_limits<std::size_t>::max()
            / modeSpec.width) {
        throw std::overflow_error("PD frame pixel count overflow");
    }
    return static_cast<std::size_t>(modeSpec.width) * modeSpec.height;
}

std::size_t SstvPdEncoder::headerIndexAt(std::uint64_t sample) const noexcept
{
    const auto first = headerBoundaries_.begin() + 1;
    const auto found = std::upper_bound(first, headerBoundaries_.end(), sample);
    return static_cast<std::size_t>(found - first);
}

double SstvPdEncoder::imageFrequency(const SstvPdPosition& position) const
{
    switch (position.region) {
    case SstvPdRegion::Sync:
        return SstvPdProtocol::SyncFrequencyHz;
    case SstvPdRegion::Porch:
        return SstvPdProtocol::PorchFrequencyHz;
    case SstvPdRegion::Pixel:
        break;
    case SstvPdRegion::Outside:
    case SstvPdRegion::Complete:
        throw std::logic_error("PD encoder has no tone at this position");
    }
    const std::uint32_t evenLine = position.firstDestinationLine;
    const std::uint32_t oddLine = evenLine + 1U;
    if (oddLine >= spec_.height || position.pixel >= spec_.width) {
        throw std::logic_error("PD mapped pixel is outside the frame");
    }
    const std::size_t evenIndex = static_cast<std::size_t>(evenLine)
        * spec_.width + position.pixel;
    const std::size_t oddIndex = static_cast<std::size_t>(oddLine)
        * spec_.width + position.pixel;
    if (evenIndex >= pixels_.size() || oddIndex >= pixels_.size()) {
        throw std::logic_error("PD mapped pair is outside image storage");
    }
    std::uint8_t value = 0U;
    switch (position.scanIndex) {
    case 0U:
        value = pixels_[evenIndex].luminance;
        break;
    case 1U:
        value = averaged(pixels_[evenIndex].chrominanceRed,
                         pixels_[oddIndex].chrominanceRed);
        break;
    case 2U:
        value = averaged(pixels_[evenIndex].chrominanceBlue,
                         pixels_[oddIndex].chrominanceBlue);
        break;
    case 3U:
        value = pixels_[oddIndex].luminance;
        break;
    default:
        throw std::logic_error("PD mapped an invalid scan index");
    }
    return SstvPdProtocol::frequencyForValue(value);
}

SstvPdEncoderPosition SstvPdEncoder::position() const
{
    SstvPdEncoderPosition result;
    result.producedSamples = producedSamples_;
    result.totalSamples = totalSamples_;
    if (complete()) {
        result.stage = SstvPdEncoderStage::Complete;
        return result;
    }
    if (cancelled()) {
        result.stage = SstvPdEncoderStage::Cancelled;
        return result;
    }
    if (producedSamples_ < headerBoundaries_.back()) {
        result.stage = SstvPdEncoderStage::Header;
        result.headerSegment = headerIndexAt(producedSamples_);
        result.frequencyHz = header_[result.headerSegment].frequencyHz;
        return result;
    }
    result.stage = SstvPdEncoderStage::Image;
    result.image = mapper_.positionAtSample(
        producedSamples_ - headerBoundaries_.back());
    result.frequencyHz = imageFrequency(result.image);
    return result;
}

void SstvPdEncoder::noteTransition(
    const SstvPdEncoderPosition& position) noexcept
{
    const bool changed = haveLastSegment_
        && (position.stage != lastPosition_.stage
            || position.headerSegment != lastPosition_.headerSegment
            || position.image.region != lastPosition_.image.region
            || position.image.linePair != lastPosition_.image.linePair
            || position.image.scanIndex != lastPosition_.image.scanIndex
            || position.image.pixel != lastPosition_.image.pixel);
    if (changed
        && metrics_.segmentTransitions
            != std::numeric_limits<std::uint64_t>::max()) {
        ++metrics_.segmentTransitions;
    }
    lastPosition_ = position;
    haveLastSegment_ = true;
}

template<typename Sample>
std::size_t SstvPdEncoder::generate(double frequencyHz,
                                    Sample* output,
                                    std::size_t count)
{
    static_assert(std::is_same_v<Sample, float>
                      || std::is_same_v<Sample, std::int16_t>,
                  "unsupported PD encoder sample type");
    if constexpr (std::is_same_v<Sample, float>) {
        return generator_.generateFloat(
            frequencyHz, config_.level, output, count);
    } else {
        return generator_.generatePcm16(
            frequencyHz, config_.level, output, count);
    }
}

template<typename Sample>
std::size_t SstvPdEncoder::pull(Sample* output, std::size_t capacity)
{
    if (metrics_.pullCalls != std::numeric_limits<std::uint64_t>::max()) {
        ++metrics_.pullCalls;
    }
    if (capacity > MaximumSamplesPerPull) {
        ++metrics_.rejectedInputCalls;
        ++metrics_.rejectedOversizeCalls;
        throw std::length_error("PD TX pull exceeds its work bound");
    }
    if (capacity != 0U && output == nullptr) {
        ++metrics_.rejectedInputCalls;
        throw std::invalid_argument("PD TX output must not be null");
    }
    if (capacity == 0U || complete() || cancelled()) {
        return 0U;
    }
    std::size_t produced = 0U;
    while (produced < capacity && !complete() && !cancelled()) {
        const SstvPdEncoderPosition current = position();
        std::uint64_t segmentEnd = 0U;
        if (current.stage == SstvPdEncoderStage::Header) {
            segmentEnd = headerBoundaries_[current.headerSegment + 1U];
        } else if (current.stage == SstvPdEncoderStage::Image) {
            segmentEnd = checkedAdd(headerBoundaries_.back(),
                                    current.image.segmentEndSample);
        } else {
            break;
        }
        if (segmentEnd <= producedSamples_) {
            throw std::logic_error("PD TX segment made no progress");
        }
        noteTransition(current);
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

std::size_t SstvPdEncoder::pullFloat(float* output, std::size_t capacity)
{
    return pull(output, capacity);
}

std::size_t SstvPdEncoder::pullPcm16(std::int16_t* output,
                                     std::size_t capacity)
{
    return pull(output, capacity);
}

SstvPdMode SstvPdEncoder::mode() const noexcept
{
    return spec_.mode;
}

std::uint64_t SstvPdEncoder::totalSamples() const noexcept
{
    return totalSamples_;
}

std::uint64_t SstvPdEncoder::producedSamples() const noexcept
{
    return producedSamples_;
}

bool SstvPdEncoder::complete() const noexcept
{
    return producedSamples_ >= totalSamples_;
}

bool SstvPdEncoder::cancelled() const noexcept
{
    return generator_.cancelled();
}

SstvPdEncoderMetrics SstvPdEncoder::metrics() const noexcept
{
    SstvPdEncoderMetrics result = metrics_;
    result.tone = generator_.metrics();
    return result;
}

void SstvPdEncoder::cancel() noexcept
{
    generator_.cancel();
}

void SstvPdEncoder::reset() noexcept
{
    generator_.reset();
    producedSamples_ = 0U;
    metrics_ = {};
    metrics_.residentImageBytes = pixels_.size() * sizeof(SstvYCbCrPixel);
    lastPosition_ = {};
    haveLastSegment_ = false;
}

void SstvPdDecoder::validateConfig(const SstvPdDecoderConfig& config)
{
    static_cast<void>(SstvPdProtocol::spec(config.mode));
    if (config.sampleRate < SstvPdMapper::MinimumSampleRate
        || config.sampleRate > SstvPdMapper::MaximumSampleRate) {
        throw std::invalid_argument("unsupported PD decoder sample rate");
    }
    if (!std::isfinite(config.frequencyOffsetHz)
        || std::abs(config.frequencyOffsetHz)
            > MaximumAbsoluteFrequencyOffsetHz) {
        throw std::invalid_argument(
            "PD decoder frequency offset is out of range");
    }
    if (!std::isfinite(config.minimumObservationConfidence)
        || config.minimumObservationConfidence < 0.0
        || config.minimumObservationConfidence > 1.0) {
        throw std::invalid_argument("invalid PD observation confidence");
    }
    if (config.maximumPendingDirtyEvents == 0U
        || config.maximumPendingDirtyEvents
            > SstvImageFrame::kMaximumDirtyEvents) {
        throw std::invalid_argument("invalid PD dirty-event bound");
    }
}

SstvPdDecoder::SstvPdDecoder(SstvPdDecoderConfig config)
    : config_(config)
    , spec_(SstvPdProtocol::spec(config.mode))
    , mapper_({config.mode, config.sampleRate, config.clockErrorPpm})
    , frame_(std::make_unique<SstvImageFrame>(
          spec_.width, spec_.height, config.maximumPendingDirtyEvents))
    , accumulators_(static_cast<std::size_t>(spec_.width) * 4U)
{
    validateConfig(config);
    if (mapper_.imageSampleCount()
        > std::numeric_limits<std::uint64_t>::max()
            - config.imageStartSample) {
        throw std::overflow_error("PD image sample range overflow");
    }
    imageEndSample_ = config.imageStartSample + mapper_.imageSampleCount();
}

void SstvPdDecoder::saturatingAdd(std::uint64_t& value,
                                  std::uint64_t increment) noexcept
{
    value = increment > std::numeric_limits<std::uint64_t>::max() - value
        ? std::numeric_limits<std::uint64_t>::max()
        : value + increment;
}

bool SstvPdDecoder::beginLinePair(std::uint32_t linePair)
{
    if (linePair >= mapper_.linePairCount()) {
        return false;
    }
    if (haveCurrentLinePair_) {
        if (linePair < currentLinePair_) {
            return false;
        }
        if (linePair == currentLinePair_) {
            return true;
        }
        publishCurrentLinePair();
    }
    currentLinePair_ = linePair;
    haveCurrentLinePair_ = true;
    clearAccumulators();
    return true;
}

void SstvPdDecoder::accumulate(const SstvPdPosition& position,
                               double frequencyHz,
                               double confidence) noexcept
{
    const std::size_t index = static_cast<std::size_t>(position.scanIndex)
        * spec_.width + position.pixel;
    if (position.scanIndex >= 4U || index >= accumulators_.size()) {
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

void SstvPdDecoder::publishCurrentLinePair()
{
    if (!haveCurrentLinePair_) {
        return;
    }
    const std::uint32_t evenLine = currentLinePair_ * 2U;
    const std::uint32_t oddLine = evenLine + 1U;
    std::vector<SstvRgbPixel> evenRow(spec_.width);
    std::vector<SstvRgbPixel> oddRow(spec_.width);
    std::vector<bool> present(spec_.width, false);
    std::uint32_t evenPixels = 0U;
    std::uint32_t oddPixels = 0U;
    for (std::uint32_t x = 0U; x < spec_.width; ++x) {
        std::array<std::uint8_t, 4U> values {};
        bool complete = true;
        for (std::uint8_t scan = 0U; scan < 4U; ++scan) {
            const PixelAccumulator& accumulator = accumulators_[
                static_cast<std::size_t>(scan) * spec_.width + x];
            if (accumulator.count == 0U
                || accumulator.confidenceWeight <= 0.0) {
                complete = false;
                break;
            }
            values[scan] = SstvPdProtocol::valueForFrequency(
                accumulator.weightedFrequencyHz
                    / accumulator.confidenceWeight
                - config_.frequencyOffsetHz);
        }
        if (!complete) {
            continue;
        }
        evenRow[x] = SstvColourConverter::yCbCrToRgb(
            {values[0], values[2], values[1]});
        oddRow[x] = SstvColourConverter::yCbCrToRgb(
            {values[3], values[2], values[1]});
        present[x] = true;
    }
    const bool completePair = std::all_of(
        present.cbegin(), present.cend(), [](bool value) { return value; });
    if (completePair) {
        if (frame_->writeScanline(evenLine, evenRow)
                == SstvImageWriteResult::Cancelled
            || frame_->writeScanline(oddLine, oddRow)
                == SstvImageWriteResult::Cancelled) {
            state_ = SstvPdDecodeState::Cancelled;
        } else {
            evenPixels = spec_.width;
            oddPixels = spec_.width;
            saturatingAdd(metrics_.componentsPublished,
                          static_cast<std::uint64_t>(spec_.width) * 6U);
        }
    } else {
        for (std::uint32_t x = 0U; x < spec_.width; ++x) {
            if (!present[x]) {
                continue;
            }
            if (frame_->writePixel(x, evenLine, evenRow[x])
                    == SstvImageWriteResult::Cancelled
                || frame_->writePixel(x, oddLine, oddRow[x])
                    == SstvImageWriteResult::Cancelled) {
                state_ = SstvPdDecodeState::Cancelled;
                break;
            }
            ++evenPixels;
            ++oddPixels;
            saturatingAdd(metrics_.componentsPublished, 6U);
        }
    }
    if (evenPixels == spec_.width) {
        saturatingAdd(metrics_.linesPublished);
    }
    if (oddPixels == spec_.width) {
        saturatingAdd(metrics_.linesPublished);
    }
    if (evenPixels == spec_.width && oddPixels == spec_.width) {
        saturatingAdd(metrics_.linePairsPublished);
    }
    haveCurrentLinePair_ = false;
    clearAccumulators();
}

void SstvPdDecoder::clearAccumulators() noexcept
{
    std::fill(accumulators_.begin(), accumulators_.end(), PixelAccumulator {});
    nonEmptyAccumulators_ = 0U;
    refreshBufferMetrics();
}

void SstvPdDecoder::refreshBufferMetrics() noexcept
{
    metrics_.bufferedPixelAccumulators = nonEmptyAccumulators_;
    metrics_.peakBufferedPixelAccumulators = std::max(
        metrics_.peakBufferedPixelAccumulators, nonEmptyAccumulators_);
}

std::size_t SstvPdDecoder::consume(
    const SstvFrequencyObservation* observations,
    std::size_t count)
{
    if (count > MaximumObservationsPerConsume) {
        saturatingAdd(metrics_.rejectedInputCalls);
        saturatingAdd(metrics_.rejectedOversizeCalls);
        throw std::length_error("PD decoder consume exceeds its work bound");
    }
    if (count != 0U && observations == nullptr) {
        saturatingAdd(metrics_.rejectedInputCalls);
        throw std::invalid_argument("PD observations must not be null");
    }
    if (count == 0U || state_ != SstvPdDecodeState::Receiving) {
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
        const SstvPdPosition position = mapper_.positionAtSample(
            observation.centreSample - config_.imageStartSample);
        if (!beginLinePair(position.linePair)) {
            saturatingAdd(metrics_.invalidObservations);
            continue;
        }
        if (position.region == SstvPdRegion::Sync) {
            const double corrected = observation.correctedFrequencyHz
                - config_.frequencyOffsetHz;
            if (std::abs(corrected - SstvPdProtocol::SyncFrequencyHz)
                    <= 100.0
                && (!haveObservedSyncPair_
                    || lastObservedSyncPair_ != position.linePair)) {
                lastObservedSyncPair_ = position.linePair;
                haveObservedSyncPair_ = true;
                saturatingAdd(metrics_.observedPairSyncs);
            }
            saturatingAdd(metrics_.nonPixelObservations);
            continue;
        }
        if (position.region != SstvPdRegion::Pixel) {
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

std::size_t SstvPdDecoder::consume(
    const std::vector<SstvFrequencyObservation>& observations)
{
    return consume(observations.data(), observations.size());
}

SstvPdDecodeState SstvPdDecoder::finish()
{
    if (state_ != SstvPdDecodeState::Receiving) {
        return state_;
    }
    publishCurrentLinePair();
    if (state_ == SstvPdDecodeState::Cancelled) {
        return state_;
    }
    state_ = frame_->isComplete()
        ? SstvPdDecodeState::Complete : SstvPdDecodeState::Partial;
    return state_;
}

void SstvPdDecoder::cancel() noexcept
{
    if (state_ == SstvPdDecodeState::Receiving) {
        state_ = SstvPdDecodeState::Cancelled;
        frame_->cancel();
        haveCurrentLinePair_ = false;
        clearAccumulators();
    }
}

void SstvPdDecoder::reset() noexcept
{
    frame_->reset();
    clearAccumulators();
    metrics_ = {};
    state_ = SstvPdDecodeState::Receiving;
    currentLinePair_ = 0U;
    haveCurrentLinePair_ = false;
    lastObservationSample_ = 0U;
    haveLastObservation_ = false;
    lastObservedSyncPair_ = 0U;
    haveObservedSyncPair_ = false;
}

double SstvPdDecoder::setFrequencyOffsetHz(double offsetHz)
{
    if (!std::isfinite(offsetHz)
        || std::abs(offsetHz) > MaximumAbsoluteFrequencyOffsetHz) {
        throw std::invalid_argument(
            "PD decoder frequency offset is out of range");
    }
    config_.frequencyOffsetHz = offsetHz;
    return config_.frequencyOffsetHz;
}

double SstvPdDecoder::frequencyOffsetHz() const noexcept
{
    return config_.frequencyOffsetHz;
}

SstvPdMode SstvPdDecoder::mode() const noexcept
{
    return spec_.mode;
}

SstvPdDecodeState SstvPdDecoder::state() const noexcept
{
    return state_;
}

std::uint64_t SstvPdDecoder::imageEndSample() const noexcept
{
    return imageEndSample_;
}

const SstvImageFrame& SstvPdDecoder::imageFrame() const noexcept
{
    return *frame_;
}

SstvImageSnapshot SstvPdDecoder::snapshot() const
{
    return frame_->snapshot();
}

std::vector<SstvDirtyEvent> SstvPdDecoder::takeDirtyEvents()
{
    return frame_->takeDirtyEvents();
}

SstvPdDecoderMetrics SstvPdDecoder::metrics() const noexcept
{
    return metrics_;
}

} // namespace decodium::sstv
