// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvSequentialRgb.h"

#include "../core/SstvTimingAccumulator.h"
#include "../core/SstvVisCodec.h"

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

struct QuotientRemainder final
{
    std::uint64_t quotient {0U};
    std::uint64_t remainder {0U};
};

struct LayoutElement final
{
    SstvSequentialRgbRegion region {SstvSequentialRgbRegion::Outside};
    ColourComponent component {ColourComponent::ModeSpecific};
    std::uint64_t startPicoseconds {0U};
    std::uint64_t endPicoseconds {0U};
};

std::uint64_t checkedAdd(std::uint64_t left, std::uint64_t right)
{
    if (left > std::numeric_limits<std::uint64_t>::max() - right) {
        throw std::overflow_error("sequential RGB integer addition overflow");
    }
    return left + right;
}

std::uint64_t checkedMultiply(std::uint64_t left, std::uint64_t right)
{
    if (left != 0U
        && right > std::numeric_limits<std::uint64_t>::max() / left) {
        throw std::overflow_error(
            "sequential RGB integer multiplication overflow");
    }
    return left * right;
}

std::uint64_t nonNegative(Picoseconds duration)
{
    if (duration.count < 0) {
        throw std::logic_error(
            "sequential RGB protocol contains a negative duration");
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
        throw std::logic_error("invalid sequential RGB duration fraction");
    }
    return checkedMultiply(duration, numerator) / denominator;
}

SstvSequentialRgbModeSpec makeSpec(
    SstvSequentialRgbMode mode,
    const char* stableId,
    const char* displayName,
    const char* family,
    const char* profile,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t effectiveSampledWidth,
    std::uint8_t visPayload,
    Picoseconds sync,
    std::array<Picoseconds, 4U> gaps,
    Picoseconds component)
{
    std::uint64_t line = nonNegative(sync);
    for (const Picoseconds gap : gaps) {
        line = checkedAdd(line, nonNegative(gap));
    }
    line = checkedAdd(line, checkedMultiply(nonNegative(component), 3U));
    const std::uint64_t image = checkedMultiply(line, height);
    if (line > static_cast<std::uint64_t>(
                   std::numeric_limits<std::int64_t>::max())
        || image > static_cast<std::uint64_t>(
                       std::numeric_limits<std::int64_t>::max())) {
        throw std::overflow_error("sequential RGB protocol duration overflow");
    }
    const std::uint64_t roundedPixel = checkedAdd(
        nonNegative(component), width / 2U) / width;
    return {mode,
            stableId,
            displayName,
            family,
            profile,
            width,
            height,
            effectiveSampledWidth,
            visPayload,
            sync,
            gaps,
            Picoseconds {static_cast<std::int64_t>(roundedPixel)},
            component,
            Picoseconds {static_cast<std::int64_t>(line)},
            Picoseconds {static_cast<std::int64_t>(image)}};
}

std::array<LayoutElement, 8U> makeLineLayout(
    const SstvSequentialRgbModeSpec& spec)
{
    std::array<LayoutElement, 8U> result {};
    std::uint64_t cursor = 0U;
    auto add = [&](std::size_t index,
                   SstvSequentialRgbRegion region,
                   ColourComponent component,
                   std::uint64_t duration) {
        const std::uint64_t end = checkedAdd(cursor, duration);
        result[index] = {region, component, cursor, end};
        cursor = end;
    };
    add(0U, SstvSequentialRgbRegion::Sync,
        ColourComponent::ModeSpecific, nonNegative(spec.syncDuration));
    add(1U, SstvSequentialRgbRegion::Gap,
        ColourComponent::ModeSpecific, nonNegative(spec.gapDurations[0]));
    add(2U, SstvSequentialRgbRegion::Pixel,
        ColourComponent::Red, nonNegative(spec.componentDuration));
    add(3U, SstvSequentialRgbRegion::Gap,
        ColourComponent::ModeSpecific, nonNegative(spec.gapDurations[1]));
    add(4U, SstvSequentialRgbRegion::Pixel,
        ColourComponent::Green, nonNegative(spec.componentDuration));
    add(5U, SstvSequentialRgbRegion::Gap,
        ColourComponent::ModeSpecific, nonNegative(spec.gapDurations[2]));
    add(6U, SstvSequentialRgbRegion::Pixel,
        ColourComponent::Blue, nonNegative(spec.componentDuration));
    add(7U, SstvSequentialRgbRegion::Gap,
        ColourComponent::ModeSpecific, nonNegative(spec.gapDurations[3]));
    if (cursor != nonNegative(spec.lineDuration)) {
        throw std::logic_error("sequential RGB line layout is inconsistent");
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
        checkedAdd(wholeProduct,
                   remainderProduct / kPpmDenominator),
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
    throw std::logic_error("invalid sequential RGB VIS symbol");
}

std::vector<SstvRgbPixel> validatedPixels(
    const SstvRgbPixel* pixels,
    std::size_t count,
    SstvSequentialRgbMode mode)
{
    if (count != SstvSequentialRgbEncoder::pixelCount(mode)) {
        throw std::invalid_argument(
            "sequential RGB pixel count does not match the selected mode");
    }
    if (pixels == nullptr) {
        throw std::invalid_argument(
            "sequential RGB encoder pixels must not be null");
    }
    return {pixels, pixels + count};
}

} // namespace

SstvSequentialRgbModeSpec SstvSequentialRgbProtocol::spec(
    SstvSequentialRgbMode mode)
{
    // Clean-room protocol facts are resolved from the pinned sources listed
    // in docs/sstv/UPSTREAM_PROVENANCE.md.  In particular, this table keeps
    // effective resolution separate from the transmitted/display raster.
    switch (mode) {
    case SstvSequentialRgbMode::WraaseSc2_60:
        // QSSTV's RX-side 61.5435 s / 256-row profile: 5 ms sync, four
        // 1 ms black gaps and three equal scans.  Its own TX-side fields omit
        // those gaps and lengthen the scans.  This explicitly named RX
        // compatibility profile is not presented as equivalent to QSSTV TX;
        // the Handbook's rounded 58/117/58 ms 2:4:2 row is not blended in.
        return makeSpec(
            mode, "wraase-sc2-60", "Wraase SC2-60", "Wraase",
            "QSSTV 8c27d6d RX compatibility profile; TX/vector absent",
            320U, 256U, 256U, 59U,
            Picoseconds {5'000'000'000LL},
            {Picoseconds {1'000'000'000LL},
             Picoseconds {1'000'000'000LL},
             Picoseconds {1'000'000'000LL},
             Picoseconds {1'000'000'000LL}},
            Picoseconds {77'134'765'625LL});
    case SstvSequentialRgbMode::WraaseSc2_120:
        // The executable pySSTV profile is within 72 ppm of SlowRX/QSSTV's
        // complete line and is the only pinned TX path observed by both.
        return makeSpec(
            mode, "wraase-sc2-120", "Wraase SC2-120", "Wraase",
            "pySSTV d998fad compatibility profile; equal RGB scans",
            320U, 256U, 320U, 63U,
            Picoseconds {5'522'500'000LL},
            {Picoseconds {1'000'000'000LL},
             Picoseconds {500'000'000LL},
             Picoseconds {500'000'000LL},
             Picoseconds {0LL}},
            Picoseconds {156'000'000'000LL});
    case SstvSequentialRgbMode::WraaseSc2_180:
        return makeSpec(
            mode, "wraase-sc2-180", "Wraase SC2-180", "Wraase",
            "pySSTV d998fad and SlowRX ca6d7012 line profile",
            320U, 256U, 512U, 55U,
            Picoseconds {5'522'500'000LL},
            {Picoseconds {500'000'000LL},
             Picoseconds {0LL},
             Picoseconds {0LL},
             Picoseconds {0LL}},
            Picoseconds {235'000'000'000LL});
    case SstvSequentialRgbMode::PasokonP3:
        return makeSpec(
            mode, "pasokon-p3", "Pasokon P3", "Pasokon",
            "4800 Hz Pasokon time-unit profile",
            640U, 496U, 320U, 113U,
            Picoseconds {5'208'333'333LL},
            {Picoseconds {1'041'666'667LL},
             Picoseconds {1'041'666'667LL},
             Picoseconds {1'041'666'667LL},
             Picoseconds {1'041'666'667LL}},
            Picoseconds {133'333'333'333LL});
    case SstvSequentialRgbMode::PasokonP5:
        return makeSpec(
            mode, "pasokon-p5", "Pasokon P5", "Pasokon",
            "3200 Hz Pasokon time-unit profile",
            640U, 496U, 640U, 114U,
            Picoseconds {7'812'500'000LL},
            {Picoseconds {1'562'500'000LL},
             Picoseconds {1'562'500'000LL},
             Picoseconds {1'562'500'000LL},
             Picoseconds {1'562'500'000LL}},
            Picoseconds {200'000'000'000LL});
    case SstvSequentialRgbMode::PasokonP7:
        return makeSpec(
            mode, "pasokon-p7", "Pasokon P7", "Pasokon",
            "2400 Hz Pasokon time-unit profile",
            640U, 496U, 640U, 115U,
            Picoseconds {10'416'666'667LL},
            {Picoseconds {2'083'333'333LL},
             Picoseconds {2'083'333'333LL},
             Picoseconds {2'083'333'333LL},
             Picoseconds {2'083'333'333LL}},
            Picoseconds {266'666'666'667LL});
    }
    throw std::invalid_argument("unknown sequential RGB mode");
}

std::optional<SstvSequentialRgbMode> SstvSequentialRgbProtocol::modeForVis(
    std::uint8_t visPayload) noexcept
{
    switch (visPayload) {
    case 59U:
        return SstvSequentialRgbMode::WraaseSc2_60;
    case 63U:
        return SstvSequentialRgbMode::WraaseSc2_120;
    case 55U:
        return SstvSequentialRgbMode::WraaseSc2_180;
    case 113U:
        return SstvSequentialRgbMode::PasokonP3;
    case 114U:
        return SstvSequentialRgbMode::PasokonP5;
    case 115U:
        return SstvSequentialRgbMode::PasokonP7;
    default:
        return std::nullopt;
    }
}

double SstvSequentialRgbProtocol::frequencyForValue(
    std::uint8_t value) noexcept
{
    return BlackFrequencyHz
        + (WhiteFrequencyHz - BlackFrequencyHz)
            * static_cast<double>(value) / 255.0;
}

std::uint8_t SstvSequentialRgbProtocol::valueForFrequency(
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

SstvSequentialRgbMapper::SstvSequentialRgbMapper(
    SstvSequentialRgbMapperConfig config)
    : config_(config)
    , spec_(SstvSequentialRgbProtocol::spec(config.mode))
{
    if (config.sampleRate < MinimumSampleRate
        || config.sampleRate > MaximumSampleRate) {
        throw std::invalid_argument(
            "unsupported sequential RGB sample rate");
    }
    if (config.clockErrorPpm < -MaximumAbsoluteClockErrorPpm
        || config.clockErrorPpm > MaximumAbsoluteClockErrorPpm) {
        throw std::invalid_argument(
            "sequential RGB clock correction is out of range");
    }
    const std::int64_t scale =
        static_cast<std::int64_t>(kPpmDenominator)
        + config.clockErrorPpm;
    if (scale <= 0
        || scale > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("invalid sequential RGB clock scale");
    }
    clockScaleNumerator_ = static_cast<std::uint32_t>(scale);
    if (checkedMultiply(nonNegative(spec_.lineDuration), spec_.height)
        != nonNegative(spec_.imageDuration)) {
        throw std::logic_error(
            "sequential RGB image duration is inconsistent");
    }
    static_cast<void>(makeLineLayout(spec_));
    imageSamples_ = samplesAtProtocolTime(nonNegative(spec_.imageDuration));
}

SstvSequentialRgbMapperConfig SstvSequentialRgbMapper::config() const noexcept
{
    return config_;
}

SstvSequentialRgbModeSpec SstvSequentialRgbMapper::modeSpec() const noexcept
{
    return spec_;
}

std::uint64_t SstvSequentialRgbMapper::imageSampleCount() const noexcept
{
    return imageSamples_;
}

std::uint64_t SstvSequentialRgbMapper::lineStartSample(
    std::uint32_t line) const
{
    if (line > spec_.height) {
        throw std::out_of_range(
            "sequential RGB line is outside the image");
    }
    return samplesAtProtocolTime(
        checkedMultiply(line, nonNegative(spec_.lineDuration)));
}

std::uint64_t SstvSequentialRgbMapper::lineEndSample(
    std::uint32_t line) const
{
    if (line >= spec_.height) {
        throw std::out_of_range(
            "sequential RGB line is outside the image");
    }
    return lineStartSample(line + 1U);
}

std::uint64_t SstvSequentialRgbMapper::samplesAtProtocolTime(
    std::uint64_t picoseconds) const
{
    if (picoseconds > nonNegative(spec_.imageDuration)) {
        throw std::out_of_range(
            "sequential RGB time is outside the image");
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
    const std::uint64_t wholeRemainder =
        scaledWhole % kPpmDenominator;
    const std::uint64_t residual = checkedAdd(
        checkedMultiply(wholeRemainder,
                        static_cast<std::uint64_t>(kPicosecondsPerSecond)),
        checkedMultiply(fraction.remainder, clockScaleNumerator_));
    result = checkedAdd(result, residual / kScaledSampleDenominator);
    return result;
}

SstvSequentialRgbPosition SstvSequentialRgbMapper::makePosition(
    SstvSequentialRgbRegion region,
    std::uint32_t line,
    ColourComponent component,
    std::uint32_t pixel,
    std::uint64_t startPicoseconds,
    std::uint64_t endPicoseconds) const
{
    return {region,
            line,
            component,
            pixel,
            samplesAtProtocolTime(startPicoseconds),
            samplesAtProtocolTime(endPicoseconds)};
}

SstvSequentialRgbPosition SstvSequentialRgbMapper::positionAtSample(
    std::uint64_t imageSample) const
{
    if (imageSample >= imageSamples_) {
        return {SstvSequentialRgbRegion::Complete,
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
    for (const LayoutElement& element : makeLineLayout(spec_)) {
        const std::uint64_t start = checkedAdd(
            lineStart, element.startPicoseconds);
        const std::uint64_t end = checkedAdd(
            lineStart, element.endPicoseconds);
        if (start == end || imageSample >= samplesAtProtocolTime(end)) {
            continue;
        }
        if (element.region != SstvSequentialRgbRegion::Pixel) {
            return makePosition(element.region, line, element.component, 0U,
                                start, end);
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
        return makePosition(SstvSequentialRgbRegion::Pixel,
                            line,
                            element.component,
                            pixelLow,
                            pixelStart,
                            pixelEnd);
    }
    throw std::logic_error(
        "sequential RGB sample did not map to a line element");
}

SstvSequentialRgbPosition SstvSequentialRgbMapper::positionAtProtocolTime(
    std::uint64_t protocolPicoseconds) const
{
    if (protocolPicoseconds >= nonNegative(spec_.imageDuration)) {
        return {SstvSequentialRgbRegion::Complete,
                spec_.height,
                ColourComponent::ModeSpecific,
                0U,
                imageSamples_,
                imageSamples_};
    }
    const std::uint64_t lineDuration = nonNegative(spec_.lineDuration);
    const std::uint32_t line = static_cast<std::uint32_t>(
        protocolPicoseconds / lineDuration);
    const std::uint64_t lineStart = checkedMultiply(line, lineDuration);
    const std::uint64_t local = protocolPicoseconds - lineStart;
    for (const LayoutElement& element : makeLineLayout(spec_)) {
        if (element.startPicoseconds == element.endPicoseconds
            || local >= element.endPicoseconds) {
            continue;
        }
        const std::uint64_t start = checkedAdd(
            lineStart, element.startPicoseconds);
        const std::uint64_t end = checkedAdd(
            lineStart, element.endPicoseconds);
        if (element.region != SstvSequentialRgbRegion::Pixel) {
            return makePosition(element.region, line, element.component, 0U,
                                start, end);
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
        return makePosition(SstvSequentialRgbRegion::Pixel,
                            line,
                            element.component,
                            pixel,
                            pixelStart,
                            pixelEnd);
    }
    throw std::logic_error(
        "sequential RGB time did not map to a line element");
}

SstvSequentialRgbPosition SstvSequentialRgbMapper::positionAtElapsedTime(
    Picoseconds elapsed) const
{
    if (elapsed.count < 0) {
        return {};
    }
    const std::uint64_t elapsedPs = static_cast<std::uint64_t>(elapsed.count);
    const std::uint64_t effectiveEnd = scaledPicosecondsCeiling(
        nonNegative(spec_.imageDuration), clockScaleNumerator_);
    if (elapsedPs >= effectiveEnd) {
        return {SstvSequentialRgbRegion::Complete,
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

std::array<SstvSequentialRgbEncoder::HeaderSegment,
           SstvSequentialRgbEncoder::HeaderSegmentCount>
SstvSequentialRgbEncoder::makeHeader(std::uint8_t visPayload)
{
    std::array<HeaderSegment, HeaderSegmentCount> header {};
    header[0] = {1'900.0, Picoseconds {300'000'000'000LL}};
    header[1] = {1'200.0, Picoseconds {10'000'000'000LL}};
    header[2] = {1'900.0, Picoseconds {300'000'000'000LL}};
    const SstvVisEncodedFrame vis = SstvVisCodec::encodeStandard(visPayload);
    if (vis.symbols.size() != HeaderSegmentCount - 3U) {
        throw std::logic_error(
            "unexpected sequential RGB standard VIS symbol count");
    }
    for (std::size_t index = 0U; index < vis.symbols.size(); ++index) {
        header[index + 3U] = {
            frequencyForVisSymbol(vis.symbols[index]),
            Picoseconds {30'000'000'000LL}};
    }
    return header;
}

std::array<std::uint64_t, SstvSequentialRgbEncoder::HeaderSegmentCount + 1U>
SstvSequentialRgbEncoder::makeHeaderBoundaries(
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

SstvSequentialRgbEncoder::SstvSequentialRgbEncoder(
    const SstvRgbPixel* pixels,
    std::size_t count,
    SstvSequentialRgbEncoderConfig config)
    : config_(config)
    , spec_(SstvSequentialRgbProtocol::spec(config.mode))
    , pixels_(validatedPixels(pixels, count, config.mode))
    , mapper_({config.mode, config.sampleRate, config.clockErrorPpm})
    , generator_(config.sampleRate, config.headroom)
    , header_(makeHeader(spec_.visPayload))
    , headerBoundaries_(makeHeaderBoundaries(config.sampleRate, header_))
{
    if (!std::isfinite(config.level)
        || config.level < 0.0 || config.level > MaximumLevel) {
        throw std::invalid_argument(
            "sequential RGB TX level is out of range");
    }
    generator_.validateTone(SstvSequentialRgbProtocol::SyncFrequencyHz,
                            config.level);
    generator_.validateTone(SstvSequentialRgbProtocol::WhiteFrequencyHz,
                            config.level);
    totalSamples_ = checkedAdd(headerBoundaries_.back(),
                               mapper_.imageSampleCount());
    metrics_.residentImageBytes = pixels_.size() * sizeof(SstvRgbPixel);
}

SstvSequentialRgbEncoder::SstvSequentialRgbEncoder(
    const std::vector<SstvRgbPixel>& pixels,
    SstvSequentialRgbEncoderConfig config)
    : SstvSequentialRgbEncoder(pixels.data(), pixels.size(), config)
{
}

std::size_t SstvSequentialRgbEncoder::pixelCount(
    SstvSequentialRgbMode mode)
{
    const SstvSequentialRgbModeSpec modeSpec =
        SstvSequentialRgbProtocol::spec(mode);
    if (modeSpec.width != 0U
        && modeSpec.height > std::numeric_limits<std::size_t>::max()
            / modeSpec.width) {
        throw std::overflow_error(
            "sequential RGB frame pixel count overflow");
    }
    return static_cast<std::size_t>(modeSpec.width) * modeSpec.height;
}

std::size_t SstvSequentialRgbEncoder::headerIndexAt(
    std::uint64_t sample) const noexcept
{
    const auto first = headerBoundaries_.begin() + 1;
    const auto found = std::upper_bound(first,
                                        headerBoundaries_.end(),
                                        sample);
    return static_cast<std::size_t>(found - first);
}

double SstvSequentialRgbEncoder::imageFrequency(
    const SstvSequentialRgbPosition& position) const
{
    switch (position.region) {
    case SstvSequentialRgbRegion::Sync:
        return SstvSequentialRgbProtocol::SyncFrequencyHz;
    case SstvSequentialRgbRegion::Gap:
        return SstvSequentialRgbProtocol::GapFrequencyHz;
    case SstvSequentialRgbRegion::Pixel:
        break;
    case SstvSequentialRgbRegion::Outside:
    case SstvSequentialRgbRegion::Complete:
        throw std::logic_error(
            "sequential RGB encoder has no tone at this position");
    }
    const std::size_t index = static_cast<std::size_t>(position.line)
        * spec_.width + position.pixel;
    if (index >= pixels_.size()) {
        throw std::logic_error(
            "sequential RGB mapped pixel is outside the frame");
    }
    const SstvRgbPixel& pixel = pixels_[index];
    std::uint8_t value = 0U;
    switch (position.component) {
    case ColourComponent::Red:
        value = pixel.red;
        break;
    case ColourComponent::Green:
        value = pixel.green;
        break;
    case ColourComponent::Blue:
        value = pixel.blue;
        break;
    default:
        throw std::logic_error(
            "sequential RGB mapped an invalid colour component");
    }
    return SstvSequentialRgbProtocol::frequencyForValue(value);
}

SstvSequentialRgbEncoderPosition SstvSequentialRgbEncoder::position() const
{
    SstvSequentialRgbEncoderPosition result;
    result.producedSamples = producedSamples_;
    result.totalSamples = totalSamples_;
    if (complete()) {
        result.stage = SstvSequentialRgbEncoderStage::Complete;
        return result;
    }
    if (cancelled()) {
        result.stage = SstvSequentialRgbEncoderStage::Cancelled;
        return result;
    }
    if (producedSamples_ < headerBoundaries_.back()) {
        result.stage = SstvSequentialRgbEncoderStage::Header;
        result.headerSegment = headerIndexAt(producedSamples_);
        result.frequencyHz = header_[result.headerSegment].frequencyHz;
        return result;
    }
    result.stage = SstvSequentialRgbEncoderStage::Image;
    result.image = mapper_.positionAtSample(
        producedSamples_ - headerBoundaries_.back());
    result.frequencyHz = imageFrequency(result.image);
    return result;
}

void SstvSequentialRgbEncoder::noteTransition(
    const SstvSequentialRgbEncoderPosition& position) noexcept
{
    const bool changed = haveLastSegment_
        && (position.stage != lastPosition_.stage
            || position.headerSegment != lastPosition_.headerSegment
            || position.image.region != lastPosition_.image.region
            || position.image.line != lastPosition_.image.line
            || position.image.component != lastPosition_.image.component
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
std::size_t SstvSequentialRgbEncoder::generate(double frequencyHz,
                                                Sample* output,
                                                std::size_t count)
{
    static_assert(std::is_same_v<Sample, float>
                      || std::is_same_v<Sample, std::int16_t>,
                  "unsupported sequential RGB encoder sample type");
    if constexpr (std::is_same_v<Sample, float>) {
        return generator_.generateFloat(
            frequencyHz, config_.level, output, count);
    } else {
        return generator_.generatePcm16(
            frequencyHz, config_.level, output, count);
    }
}

template<typename Sample>
std::size_t SstvSequentialRgbEncoder::pull(Sample* output,
                                            std::size_t capacity)
{
    if (metrics_.pullCalls != std::numeric_limits<std::uint64_t>::max()) {
        ++metrics_.pullCalls;
    }
    if (capacity > MaximumSamplesPerPull) {
        ++metrics_.rejectedInputCalls;
        ++metrics_.rejectedOversizeCalls;
        throw std::length_error(
            "sequential RGB TX pull exceeds its work bound");
    }
    if (capacity != 0U && output == nullptr) {
        ++metrics_.rejectedInputCalls;
        throw std::invalid_argument(
            "sequential RGB TX output must not be null");
    }
    if (capacity == 0U || complete() || cancelled()) {
        return 0U;
    }
    std::size_t produced = 0U;
    while (produced < capacity && !complete() && !cancelled()) {
        const SstvSequentialRgbEncoderPosition current = position();
        std::uint64_t segmentEnd = 0U;
        if (current.stage == SstvSequentialRgbEncoderStage::Header) {
            segmentEnd = headerBoundaries_[current.headerSegment + 1U];
        } else if (current.stage == SstvSequentialRgbEncoderStage::Image) {
            segmentEnd = checkedAdd(headerBoundaries_.back(),
                                    current.image.segmentEndSample);
        } else {
            break;
        }
        if (segmentEnd <= producedSamples_) {
            throw std::logic_error(
                "sequential RGB TX segment made no progress");
        }
        noteTransition(current);
        const std::uint64_t remaining = segmentEnd - producedSamples_;
        const std::size_t requested = std::min<std::size_t>(
            capacity - produced,
            static_cast<std::size_t>(std::min<std::uint64_t>(
                remaining,
                std::numeric_limits<std::size_t>::max())));
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

std::size_t SstvSequentialRgbEncoder::pullFloat(float* output,
                                                 std::size_t capacity)
{
    return pull(output, capacity);
}

std::size_t SstvSequentialRgbEncoder::pullPcm16(std::int16_t* output,
                                                 std::size_t capacity)
{
    return pull(output, capacity);
}

SstvSequentialRgbMode SstvSequentialRgbEncoder::mode() const noexcept
{
    return spec_.mode;
}

std::uint64_t SstvSequentialRgbEncoder::totalSamples() const noexcept
{
    return totalSamples_;
}

std::uint64_t SstvSequentialRgbEncoder::producedSamples() const noexcept
{
    return producedSamples_;
}

bool SstvSequentialRgbEncoder::complete() const noexcept
{
    return producedSamples_ >= totalSamples_;
}

bool SstvSequentialRgbEncoder::cancelled() const noexcept
{
    return generator_.cancelled();
}

SstvSequentialRgbEncoderMetrics SstvSequentialRgbEncoder::metrics() const noexcept
{
    SstvSequentialRgbEncoderMetrics result = metrics_;
    result.tone = generator_.metrics();
    return result;
}

void SstvSequentialRgbEncoder::cancel() noexcept
{
    generator_.cancel();
}

void SstvSequentialRgbEncoder::reset() noexcept
{
    generator_.reset();
    producedSamples_ = 0U;
    metrics_ = {};
    metrics_.residentImageBytes = pixels_.size() * sizeof(SstvRgbPixel);
    lastPosition_ = {};
    haveLastSegment_ = false;
}

void SstvSequentialRgbDecoder::validateConfig(
    const SstvSequentialRgbDecoderConfig& config)
{
    static_cast<void>(SstvSequentialRgbProtocol::spec(config.mode));
    if (config.sampleRate < SstvSequentialRgbMapper::MinimumSampleRate
        || config.sampleRate > SstvSequentialRgbMapper::MaximumSampleRate) {
        throw std::invalid_argument(
            "unsupported sequential RGB decoder sample rate");
    }
    if (!std::isfinite(config.frequencyOffsetHz)
        || std::abs(config.frequencyOffsetHz)
            > MaximumAbsoluteFrequencyOffsetHz) {
        throw std::invalid_argument(
            "sequential RGB decoder frequency offset is out of range");
    }
    if (!std::isfinite(config.minimumObservationConfidence)
        || config.minimumObservationConfidence < 0.0
        || config.minimumObservationConfidence > 1.0) {
        throw std::invalid_argument(
            "invalid sequential RGB observation confidence");
    }
    if (config.maximumPendingDirtyEvents == 0U
        || config.maximumPendingDirtyEvents
            > SstvImageFrame::kMaximumDirtyEvents) {
        throw std::invalid_argument(
            "invalid sequential RGB dirty-event bound");
    }
}

SstvSequentialRgbDecoder::SstvSequentialRgbDecoder(
    SstvSequentialRgbDecoderConfig config)
    : config_(config)
    , spec_(SstvSequentialRgbProtocol::spec(config.mode))
    , mapper_({config.mode, config.sampleRate, config.clockErrorPpm})
    , frame_(std::make_unique<SstvImageFrame>(
          spec_.width, spec_.height, config.maximumPendingDirtyEvents))
    , accumulators_(static_cast<std::size_t>(spec_.width) * 3U)
{
    validateConfig(config);
    if (mapper_.imageSampleCount()
        > std::numeric_limits<std::uint64_t>::max()
            - config.imageStartSample) {
        throw std::overflow_error(
            "sequential RGB image sample range overflow");
    }
    imageEndSample_ = config.imageStartSample + mapper_.imageSampleCount();
}

void SstvSequentialRgbDecoder::saturatingAdd(std::uint64_t& value,
                                              std::uint64_t increment) noexcept
{
    value = increment > std::numeric_limits<std::uint64_t>::max() - value
        ? std::numeric_limits<std::uint64_t>::max()
        : value + increment;
}

std::size_t SstvSequentialRgbDecoder::componentIndex(
    ColourComponent component)
{
    switch (component) {
    case ColourComponent::Red:
        return 0U;
    case ColourComponent::Green:
        return 1U;
    case ColourComponent::Blue:
        return 2U;
    default:
        throw std::logic_error(
            "invalid sequential RGB decoder component");
    }
}

SstvImageChannel SstvSequentialRgbDecoder::imageChannel(
    ColourComponent component)
{
    switch (component) {
    case ColourComponent::Red:
        return SstvImageChannel::Red;
    case ColourComponent::Green:
        return SstvImageChannel::Green;
    case ColourComponent::Blue:
        return SstvImageChannel::Blue;
    default:
        throw std::logic_error(
            "invalid sequential RGB image component");
    }
}

bool SstvSequentialRgbDecoder::beginLine(std::uint32_t line)
{
    if (line >= spec_.height) {
        return false;
    }
    if (haveCurrentLine_) {
        publishCurrentLine();
    }
    currentLine_ = line;
    haveCurrentLine_ = true;
    clearAccumulators();
    return true;
}

void SstvSequentialRgbDecoder::accumulate(
    const SstvSequentialRgbPosition& position,
    double frequencyHz,
    double confidence) noexcept
{
    const std::size_t index = componentIndex(position.component) * spec_.width
        + position.pixel;
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

void SstvSequentialRgbDecoder::publishCurrentLine()
{
    if (!haveCurrentLine_) {
        return;
    }
    std::vector<std::uint8_t> values(spec_.width);
    std::size_t components = 0U;
    for (const ColourComponent component : {
             ColourComponent::Red,
             ColourComponent::Green,
             ColourComponent::Blue}) {
        const std::size_t base = componentIndex(component) * spec_.width;
        const bool completeComponent = std::all_of(
            accumulators_.begin() + static_cast<std::ptrdiff_t>(base),
            accumulators_.begin()
                + static_cast<std::ptrdiff_t>(base + spec_.width),
            [](const PixelAccumulator& accumulator) {
                return accumulator.count > 0U
                    && accumulator.confidenceWeight > 0.0;
            });
        if (!completeComponent) {
            continue;
        }
        for (std::uint32_t pixel = 0U; pixel < spec_.width; ++pixel) {
            const PixelAccumulator& accumulator =
                accumulators_[base + pixel];
            const double frequency = accumulator.weightedFrequencyHz
                / accumulator.confidenceWeight;
            values[pixel] = SstvSequentialRgbProtocol::valueForFrequency(
                frequency - config_.frequencyOffsetHz);
        }
        static_cast<void>(frame_->writeChannelScanline(
            currentLine_, imageChannel(component), values));
        ++components;
        saturatingAdd(metrics_.componentsPublished);
    }
    if (components == 3U) {
        saturatingAdd(metrics_.linesPublished);
    }
    haveCurrentLine_ = false;
    clearAccumulators();
}

void SstvSequentialRgbDecoder::clearAccumulators() noexcept
{
    std::fill(accumulators_.begin(), accumulators_.end(), PixelAccumulator {});
    nonEmptyAccumulators_ = 0U;
    refreshBufferMetrics();
}

void SstvSequentialRgbDecoder::refreshBufferMetrics() noexcept
{
    metrics_.bufferedPixelAccumulators = nonEmptyAccumulators_;
    metrics_.peakBufferedPixelAccumulators = std::max(
        metrics_.peakBufferedPixelAccumulators, nonEmptyAccumulators_);
}

std::size_t SstvSequentialRgbDecoder::consume(
    const SstvFrequencyObservation* observations,
    std::size_t count)
{
    if (count > MaximumObservationsPerConsume) {
        saturatingAdd(metrics_.rejectedInputCalls);
        saturatingAdd(metrics_.rejectedOversizeCalls);
        throw std::length_error(
            "sequential RGB decoder consume exceeds its work bound");
    }
    if (count != 0U && observations == nullptr) {
        saturatingAdd(metrics_.rejectedInputCalls);
        throw std::invalid_argument(
            "sequential RGB observations must not be null");
    }
    if (count == 0U || state_ != SstvSequentialRgbDecodeState::Receiving) {
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
        const SstvSequentialRgbPosition position = mapper_.positionAtSample(
            observation.centreSample - config_.imageStartSample);
        if (!haveCurrentLine_ || position.line != currentLine_) {
            if (!beginLine(position.line)) {
                saturatingAdd(metrics_.invalidObservations);
                continue;
            }
        }
        if (position.region == SstvSequentialRgbRegion::Sync) {
            const double corrected = observation.correctedFrequencyHz
                - config_.frequencyOffsetHz;
            if (std::abs(corrected
                         - SstvSequentialRgbProtocol::SyncFrequencyHz)
                    <= 100.0
                && (!haveObservedSyncLine_
                    || lastObservedSyncLine_ != position.line)) {
                lastObservedSyncLine_ = position.line;
                haveObservedSyncLine_ = true;
                saturatingAdd(metrics_.observedLineSyncs);
            }
            saturatingAdd(metrics_.nonPixelObservations);
            continue;
        }
        if (position.region != SstvSequentialRgbRegion::Pixel) {
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

std::size_t SstvSequentialRgbDecoder::consume(
    const std::vector<SstvFrequencyObservation>& observations)
{
    return consume(observations.data(), observations.size());
}

SstvSequentialRgbDecodeState SstvSequentialRgbDecoder::finish()
{
    if (state_ != SstvSequentialRgbDecodeState::Receiving) {
        return state_;
    }
    publishCurrentLine();
    state_ = frame_->isComplete()
        ? SstvSequentialRgbDecodeState::Complete
        : SstvSequentialRgbDecodeState::Partial;
    return state_;
}

void SstvSequentialRgbDecoder::cancel() noexcept
{
    if (state_ == SstvSequentialRgbDecodeState::Receiving) {
        state_ = SstvSequentialRgbDecodeState::Cancelled;
        frame_->cancel();
        haveCurrentLine_ = false;
        clearAccumulators();
    }
}

void SstvSequentialRgbDecoder::reset() noexcept
{
    frame_->reset();
    clearAccumulators();
    metrics_ = {};
    state_ = SstvSequentialRgbDecodeState::Receiving;
    currentLine_ = 0U;
    haveCurrentLine_ = false;
    lastObservationSample_ = 0U;
    haveLastObservation_ = false;
    lastObservedSyncLine_ = 0U;
    haveObservedSyncLine_ = false;
}

double SstvSequentialRgbDecoder::setFrequencyOffsetHz(double offsetHz)
{
    if (!std::isfinite(offsetHz)
        || std::abs(offsetHz) > MaximumAbsoluteFrequencyOffsetHz) {
        throw std::invalid_argument(
            "sequential RGB decoder frequency offset is out of range");
    }
    config_.frequencyOffsetHz = offsetHz;
    return config_.frequencyOffsetHz;
}

double SstvSequentialRgbDecoder::frequencyOffsetHz() const noexcept
{
    return config_.frequencyOffsetHz;
}

SstvSequentialRgbMode SstvSequentialRgbDecoder::mode() const noexcept
{
    return spec_.mode;
}

SstvSequentialRgbDecodeState SstvSequentialRgbDecoder::state() const noexcept
{
    return state_;
}

std::uint64_t SstvSequentialRgbDecoder::imageEndSample() const noexcept
{
    return imageEndSample_;
}

const SstvImageFrame& SstvSequentialRgbDecoder::imageFrame() const noexcept
{
    return *frame_;
}

SstvImageSnapshot SstvSequentialRgbDecoder::snapshot() const
{
    return frame_->snapshot();
}

std::vector<SstvDirtyEvent> SstvSequentialRgbDecoder::takeDirtyEvents()
{
    return frame_->takeDirtyEvents();
}

SstvSequentialRgbDecoderMetrics SstvSequentialRgbDecoder::metrics() const noexcept
{
    return metrics_;
}

} // namespace decodium::sstv
