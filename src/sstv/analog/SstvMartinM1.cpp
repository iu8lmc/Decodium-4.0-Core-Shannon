// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvMartinM1.h"

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

struct LayoutElement final
{
    SstvMartinM1Region region;
    ColourComponent component;
    std::uint64_t startPicoseconds;
    std::uint64_t endPicoseconds;
};

std::uint64_t nonNegative(Picoseconds duration)
{
    if (duration.count < 0) {
        throw std::logic_error("Martin protocol contains a negative duration");
    }
    return static_cast<std::uint64_t>(duration.count);
}

struct QuotientRemainder final
{
    std::uint64_t quotient {0U};
    std::uint64_t remainder {0U};
};

// Exact binary long multiplication followed by division.  multiplicand is
// below divisor, and multiplier is deliberately 32-bit.
QuotientRemainder multiplyAndDivide(std::uint64_t multiplicand,
                                    std::uint32_t multiplier,
                                    std::uint64_t divisor)
{
    QuotientRemainder result;
    QuotientRemainder term {0U, multiplicand};
    std::uint32_t bits = multiplier;

    while (bits != 0U) {
        if ((bits & 1U) != 0U) {
            if (result.quotient
                > std::numeric_limits<std::uint64_t>::max()
                    - term.quotient) {
                throw std::overflow_error("Martin M1 rational quotient overflow");
            }
            result.quotient += term.quotient;
            result.remainder += term.remainder;
            if (result.remainder >= divisor) {
                result.remainder -= divisor;
                if (result.quotient
                    == std::numeric_limits<std::uint64_t>::max()) {
                    throw std::overflow_error(
                        "Martin M1 rational quotient overflow");
                }
                ++result.quotient;
            }
        }

        bits >>= 1U;
        if (bits == 0U) {
            break;
        }
        if (term.quotient
            > std::numeric_limits<std::uint64_t>::max() / 2U) {
            throw std::overflow_error("Martin M1 rational quotient overflow");
        }
        term.quotient *= 2U;
        term.remainder *= 2U;
        if (term.remainder >= divisor) {
            term.remainder -= divisor;
            if (term.quotient == std::numeric_limits<std::uint64_t>::max()) {
                throw std::overflow_error("Martin M1 rational quotient overflow");
            }
            ++term.quotient;
        }
    }
    return result;
}

std::uint64_t checkedAdd(std::uint64_t left, std::uint64_t right)
{
    if (left > std::numeric_limits<std::uint64_t>::max() - right) {
        throw std::overflow_error("Martin M1 sample index overflow");
    }
    return left + right;
}

std::uint64_t checkedMultiply(std::uint64_t left, std::uint64_t right)
{
    if (left != 0U
        && right > std::numeric_limits<std::uint64_t>::max() / left) {
        throw std::overflow_error("Martin M1 timing multiplication overflow");
    }
    return left * right;
}

std::array<LayoutElement, 8U> makeLineLayout(
    const SstvMartinModeSpec& spec)
{
    const std::uint64_t sync = nonNegative(spec.syncDuration);
    const std::uint64_t porch = nonNegative(spec.porchDuration);
    const std::uint64_t separator = nonNegative(spec.separatorDuration);
    const std::uint64_t component = nonNegative(spec.componentDuration);

    const std::uint64_t greenStart = checkedAdd(sync, porch);
    const std::uint64_t greenEnd = checkedAdd(greenStart, component);
    const std::uint64_t blueStart = checkedAdd(greenEnd, separator);
    const std::uint64_t blueEnd = checkedAdd(blueStart, component);
    const std::uint64_t redStart = checkedAdd(blueEnd, separator);
    const std::uint64_t redEnd = checkedAdd(redStart, component);
    const std::uint64_t lineEnd = checkedAdd(redEnd, separator);
    if (lineEnd != nonNegative(spec.lineDuration)
        || component
            != checkedMultiply(nonNegative(spec.pixelDuration), spec.width)) {
        throw std::logic_error("Martin protocol layout is inconsistent");
    }
    return {{
        {SstvMartinM1Region::Sync, ColourComponent::ModeSpecific,
         0U, sync},
        {SstvMartinM1Region::Porch, ColourComponent::ModeSpecific,
         sync, greenStart},
        {SstvMartinM1Region::Pixel, ColourComponent::Green,
         greenStart, greenEnd},
        {SstvMartinM1Region::Separator, ColourComponent::ModeSpecific,
         greenEnd, blueStart},
        {SstvMartinM1Region::Pixel, ColourComponent::Blue,
         blueStart, blueEnd},
        {SstvMartinM1Region::Separator, ColourComponent::ModeSpecific,
         blueEnd, redStart},
        {SstvMartinM1Region::Pixel, ColourComponent::Red,
         redStart, redEnd},
        {SstvMartinM1Region::Separator, ColourComponent::ModeSpecific,
         redEnd, lineEnd},
    }};
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
        (remainderProduct % kPpmDenominator) == 0U ? 0U : 1U);
}

std::vector<SstvRgbPixel> validatedPixels(const SstvRgbPixel* pixels,
                                          std::size_t count,
                                          SstvMartinMode mode)
{
    if (count != SstvMartinM1Encoder::pixelCount(mode)) {
        throw std::invalid_argument(
            "Martin input pixel count does not match the selected mode");
    }
    if (pixels == nullptr) {
        throw std::invalid_argument("Martin input pixels must not be null");
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
    throw std::logic_error("Martin M1 VIS codec produced an invalid symbol");
}

} // namespace

SstvMartinModeSpec SstvMartinM1Protocol::spec(SstvMartinMode mode)
{
    // Canonical values follow SSTV Handbook table 4.4 and its VIS mode list.
    // The exact pinned libsstv 193157a9 descriptor independently agrees on
    // 320 transmitted/display columns, 256/128 rows, 4.862 ms sync, 0.572 ms
    // gaps, and 457.6/228.8 us pixels.  M2/M4 retain the 320-column wire
    // raster while exposing their 160-column effective sampled resolution.
    switch (mode) {
    case SstvMartinMode::M1:
        return {mode,
                "martin-m1",
                "Martin M1",
                Width,
                Height,
                Width,
                VisCode,
                SyncDuration,
                PorchDuration,
                SeparatorDuration,
                PixelDuration,
                ComponentDuration,
                LineDuration,
                ImageDuration};
    case SstvMartinMode::M2:
        return {mode,
                "martin-m2",
                "Martin M2",
                Width,
                Height,
                160U,
                40U,
                SyncDuration,
                PorchDuration,
                SeparatorDuration,
                Picoseconds {228'800'000LL},
                Picoseconds {73'216'000'000LL},
                Picoseconds {226'798'000'000LL},
                Picoseconds {58'060'288'000'000LL}};
    case SstvMartinMode::M3:
        return {mode,
                "martin-m3",
                "Martin M3",
                Width,
                128U,
                Width,
                36U,
                SyncDuration,
                PorchDuration,
                SeparatorDuration,
                PixelDuration,
                ComponentDuration,
                LineDuration,
                Picoseconds {57'145'088'000'000LL}};
    case SstvMartinMode::M4:
        return {mode,
                "martin-m4",
                "Martin M4",
                Width,
                128U,
                160U,
                32U,
                SyncDuration,
                PorchDuration,
                SeparatorDuration,
                Picoseconds {228'800'000LL},
                Picoseconds {73'216'000'000LL},
                Picoseconds {226'798'000'000LL},
                Picoseconds {29'030'144'000'000LL}};
    }
    throw std::invalid_argument("unknown Martin mode");
}

double SstvMartinM1Protocol::frequencyForValue(std::uint8_t value) noexcept
{
    return BlackFrequencyHz
        + (WhiteFrequencyHz - BlackFrequencyHz)
            * static_cast<double>(value) / 255.0;
}

std::uint8_t SstvMartinM1Protocol::valueForFrequency(
    double frequencyHz) noexcept
{
    if (!std::isfinite(frequencyHz) || frequencyHz <= BlackFrequencyHz) {
        return 0U;
    }
    if (frequencyHz >= WhiteFrequencyHz) {
        return 255U;
    }
    const double scaled = (frequencyHz - BlackFrequencyHz) * 255.0
        / (WhiteFrequencyHz - BlackFrequencyHz);
    return static_cast<std::uint8_t>(std::lround(scaled));
}

SstvMartinM1Mapper::SstvMartinM1Mapper(SstvMartinM1MapperConfig config)
    : config_(config)
    , spec_(SstvMartinM1Protocol::spec(config.mode))
{
    if (config.sampleRate < MinimumSampleRate
        || config.sampleRate > MaximumSampleRate) {
        throw std::invalid_argument("unsupported Martin sample rate");
    }
    if (config.clockErrorPpm < -MaximumAbsoluteClockErrorPpm
        || config.clockErrorPpm > MaximumAbsoluteClockErrorPpm) {
        throw std::invalid_argument("Martin clock correction is out of range");
    }

    const std::int64_t numerator =
        static_cast<std::int64_t>(kPpmDenominator)
        + config.clockErrorPpm;
    if (numerator <= 0
        || numerator > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("invalid Martin clock scale");
    }
    clockScaleNumerator_ = static_cast<std::uint32_t>(numerator);
    const std::uint64_t expectedImageDuration = checkedMultiply(
        nonNegative(spec_.lineDuration), spec_.height);
    if (expectedImageDuration != nonNegative(spec_.imageDuration)
        || spec_.width != SstvMartinM1Protocol::Width
        || (spec_.height != SstvMartinM1Protocol::Height
            && spec_.height != 128U)) {
        throw std::logic_error("Martin protocol dimensions or duration differ");
    }
    static_cast<void>(makeLineLayout(spec_));
    imageSamples_ = samplesAtProtocolTime(nonNegative(spec_.imageDuration));
}

SstvMartinM1MapperConfig SstvMartinM1Mapper::config() const noexcept
{
    return config_;
}

SstvMartinModeSpec SstvMartinM1Mapper::modeSpec() const noexcept
{
    return spec_;
}

std::uint64_t SstvMartinM1Mapper::imageSampleCount() const noexcept
{
    return imageSamples_;
}

std::uint64_t SstvMartinM1Mapper::lineStartSample(std::uint32_t line) const
{
    if (line > spec_.height) {
        throw std::out_of_range("Martin line is outside the image");
    }
    return samplesAtProtocolTime(
        checkedMultiply(line, nonNegative(spec_.lineDuration)));
}

std::uint64_t SstvMartinM1Mapper::lineEndSample(std::uint32_t line) const
{
    if (line >= spec_.height) {
        throw std::out_of_range("Martin line is outside the image");
    }
    return lineStartSample(line + 1U);
}

std::uint64_t SstvMartinM1Mapper::samplesAtProtocolTime(
    std::uint64_t picoseconds) const
{
    if (picoseconds
        > static_cast<std::uint64_t>(
            spec_.imageDuration.count)) {
        throw std::out_of_range("Martin time is outside the image");
    }

    const std::uint64_t divisor =
        static_cast<std::uint64_t>(kPicosecondsPerSecond);
    const std::uint64_t wholeSeconds = picoseconds / divisor;
    const std::uint64_t partialSecond = picoseconds % divisor;
    std::uint64_t unscaledSamples = checkedMultiply(
        wholeSeconds, config_.sampleRate);
    const auto fraction = multiplyAndDivide(
        partialSecond, config_.sampleRate, divisor);
    unscaledSamples = checkedAdd(unscaledSamples, fraction.quotient);

    const std::uint64_t scaledWhole = checkedMultiply(
        unscaledSamples, clockScaleNumerator_);
    std::uint64_t result = scaledWhole / kPpmDenominator;
    const std::uint64_t wholeRemainder =
        scaledWhole % kPpmDenominator;

    // This is the exact residual numerator over 1e18.  Its validated maximum
    // is below 2.1e18, comfortably inside uint64_t.
    const std::uint64_t residual = checkedAdd(
        checkedMultiply(wholeRemainder,
                        static_cast<std::uint64_t>(kPicosecondsPerSecond)),
        checkedMultiply(fraction.remainder, clockScaleNumerator_));
    result = checkedAdd(result, residual / kScaledSampleDenominator);
    return result;
}

SstvMartinM1Position SstvMartinM1Mapper::makeNonPixelPosition(
    SstvMartinM1Region region,
    std::uint32_t line,
    std::uint64_t startPicoseconds,
    std::uint64_t endPicoseconds) const
{
    return {region,
            line,
            ColourComponent::ModeSpecific,
            0U,
            samplesAtProtocolTime(startPicoseconds),
            samplesAtProtocolTime(endPicoseconds)};
}

SstvMartinM1Position SstvMartinM1Mapper::pixelPositionAtSample(
    std::uint32_t line,
    ColourComponent component,
    std::uint64_t componentStartPicoseconds,
    std::uint64_t sample) const
{
    std::uint32_t low = 0U;
    std::uint32_t high = spec_.width;
    while (low + 1U < high) {
        const std::uint32_t middle = low + (high - low) / 2U;
        const std::uint64_t boundary = checkedAdd(
            componentStartPicoseconds,
            checkedMultiply(middle, static_cast<std::uint64_t>(
                                        spec_.pixelDuration.count)));
        if (samplesAtProtocolTime(boundary) <= sample) {
            low = middle;
        } else {
            high = middle;
        }
    }

    const std::uint64_t start = checkedAdd(
        componentStartPicoseconds,
        checkedMultiply(low, static_cast<std::uint64_t>(
                                 spec_.pixelDuration.count)));
    const std::uint64_t end = checkedAdd(
        componentStartPicoseconds,
        checkedMultiply(low + 1U, static_cast<std::uint64_t>(
                                      spec_.pixelDuration.count)));
    return {SstvMartinM1Region::Pixel,
            line,
            component,
            low,
            samplesAtProtocolTime(start),
            samplesAtProtocolTime(end)};
}

SstvMartinM1Position SstvMartinM1Mapper::positionAtSample(
    std::uint64_t imageSample) const
{
    if (imageSample >= imageSamples_) {
        return {SstvMartinM1Region::Complete,
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
    const auto layout = makeLineLayout(spec_);
    for (const auto& element : layout) {
        const std::uint64_t start = checkedAdd(
            lineStart, element.startPicoseconds);
        const std::uint64_t end = checkedAdd(
            lineStart, element.endPicoseconds);
        if (imageSample >= samplesAtProtocolTime(end)) {
            continue;
        }
        if (element.region == SstvMartinM1Region::Pixel) {
            return pixelPositionAtSample(
                line, element.component, start, imageSample);
        }
        return makeNonPixelPosition(element.region, line, start, end);
    }
    throw std::logic_error("Martin M1 sample did not map to a line element");
}

SstvMartinM1Position SstvMartinM1Mapper::positionAtProtocolTime(
    std::uint64_t protocolPicoseconds) const
{
    const std::uint64_t imageDuration = static_cast<std::uint64_t>(
        spec_.imageDuration.count);
    if (protocolPicoseconds >= imageDuration) {
        return {SstvMartinM1Region::Complete,
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
    const auto layout = makeLineLayout(spec_);
    for (const auto& element : layout) {
        if (local >= element.endPicoseconds) {
            continue;
        }
        const std::uint64_t start = checkedAdd(
            lineStart, element.startPicoseconds);
        const std::uint64_t end = checkedAdd(
            lineStart, element.endPicoseconds);
        if (element.region != SstvMartinM1Region::Pixel) {
            return makeNonPixelPosition(element.region, line, start, end);
        }

        const std::uint64_t pixelOffset =
            (protocolPicoseconds - start)
            / static_cast<std::uint64_t>(
                spec_.pixelDuration.count);
        const std::uint32_t pixel = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(pixelOffset,
                                    spec_.width - 1U));
        const std::uint64_t pixelStart = start
            + checkedMultiply(pixel, static_cast<std::uint64_t>(
                                         spec_.pixelDuration.count));
        const std::uint64_t pixelEnd = pixelStart
            + static_cast<std::uint64_t>(
                spec_.pixelDuration.count);
        return {SstvMartinM1Region::Pixel,
                line,
                element.component,
                pixel,
                samplesAtProtocolTime(pixelStart),
                samplesAtProtocolTime(pixelEnd)};
    }
    throw std::logic_error("Martin M1 time did not map to a line element");
}

SstvMartinM1Position SstvMartinM1Mapper::positionAtElapsedTime(
    Picoseconds elapsed) const
{
    if (elapsed.count < 0) {
        return {};
    }
    const std::uint64_t elapsedPs = static_cast<std::uint64_t>(elapsed.count);
    const std::uint64_t effectiveImageEnd = scaledPicosecondsCeiling(
        static_cast<std::uint64_t>(
            spec_.imageDuration.count),
        clockScaleNumerator_);
    if (elapsedPs >= effectiveImageEnd) {
        return {SstvMartinM1Region::Complete,
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

std::array<SstvMartinM1Encoder::HeaderSegment,
           SstvMartinM1Encoder::HeaderSegmentCount>
SstvMartinM1Encoder::makeHeader(std::uint8_t visPayload)
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

std::array<std::uint64_t, SstvMartinM1Encoder::HeaderSegmentCount + 1U>
SstvMartinM1Encoder::makeHeaderBoundaries(
    std::uint32_t sampleRate,
    const std::array<HeaderSegment, HeaderSegmentCount>& header)
{
    SstvTimingAccumulator timing(sampleRate);
    std::array<std::uint64_t, HeaderSegmentCount + 1U> boundaries {};
    for (std::size_t index = 0U; index < header.size(); ++index) {
        timing.samplesFor(header[index].duration);
        boundaries[index + 1U] = timing.totalSamples();
    }
    return boundaries;
}

SstvMartinM1Encoder::SstvMartinM1Encoder(
    const SstvRgbPixel* pixels,
    std::size_t count,
    SstvMartinM1EncoderConfig config)
    : config_(config)
    , spec_(SstvMartinM1Protocol::spec(config.mode))
    , pixels_(validatedPixels(pixels, count, config.mode))
    , mapper_({config.sampleRate, config.clockErrorPpm, config.mode})
    , generator_(config.sampleRate, config.headroom)
    , header_(makeHeader(spec_.visPayload))
    , headerBoundaries_(makeHeaderBoundaries(config.sampleRate, header_))
{
    if (!std::isfinite(config.level)
        || config.level < 0.0
        || config.level > MaximumLevel) {
        throw std::invalid_argument("Martin TX level is out of range");
    }
    generator_.validateTone(SstvMartinM1Protocol::SyncFrequencyHz,
                            config.level);
    generator_.validateTone(SstvMartinM1Protocol::WhiteFrequencyHz,
                            config.level);
    totalSamples_ = checkedAdd(headerBoundaries_.back(),
                               mapper_.imageSampleCount());
    metrics_.residentImageBytes = pixels_.size() * sizeof(SstvRgbPixel);
}

std::size_t SstvMartinM1Encoder::pixelCount(SstvMartinMode mode)
{
    const SstvMartinModeSpec modeSpec = SstvMartinM1Protocol::spec(mode);
    if (modeSpec.width != 0U
        && modeSpec.height > std::numeric_limits<std::size_t>::max()
            / modeSpec.width) {
        throw std::overflow_error("Martin frame pixel count overflow");
    }
    return static_cast<std::size_t>(modeSpec.width) * modeSpec.height;
}

SstvMartinM1Encoder::SstvMartinM1Encoder(
    const std::vector<SstvRgbPixel>& pixels,
    SstvMartinM1EncoderConfig config)
    : SstvMartinM1Encoder(pixels.data(), pixels.size(), config)
{
}

std::size_t SstvMartinM1Encoder::headerIndexAt(
    std::uint64_t sample) const noexcept
{
    const auto first = headerBoundaries_.begin() + 1;
    const auto found = std::upper_bound(first,
                                        headerBoundaries_.end(),
                                        sample);
    return static_cast<std::size_t>(found - first);
}

double SstvMartinM1Encoder::imageFrequency(
    const SstvMartinM1Position& position) const
{
    switch (position.region) {
    case SstvMartinM1Region::Sync:
        return SstvMartinM1Protocol::SyncFrequencyHz;
    case SstvMartinM1Region::Porch:
    case SstvMartinM1Region::Separator:
        return SstvMartinM1Protocol::SeparatorFrequencyHz;
    case SstvMartinM1Region::Pixel:
        break;
    case SstvMartinM1Region::Outside:
    case SstvMartinM1Region::Complete:
        throw std::logic_error("Martin M1 encoder has no tone at this position");
    }

    const std::size_t index = static_cast<std::size_t>(position.line)
        * spec_.width + position.pixel;
    if (index >= pixels_.size()) {
        throw std::logic_error("Martin mapped pixel is outside the frame");
    }
    const auto& pixel = pixels_[index];
    std::uint8_t value = 0U;
    switch (position.component) {
    case ColourComponent::Green:
        value = pixel.green;
        break;
    case ColourComponent::Blue:
        value = pixel.blue;
        break;
    case ColourComponent::Red:
        value = pixel.red;
        break;
    default:
        throw std::logic_error("Martin M1 mapped an invalid colour component");
    }
    return SstvMartinM1Protocol::frequencyForValue(value);
}

SstvMartinM1EncoderPosition SstvMartinM1Encoder::position() const
{
    SstvMartinM1EncoderPosition result;
    result.producedSamples = producedSamples_;
    result.totalSamples = totalSamples_;
    if (complete()) {
        result.stage = SstvMartinM1EncoderStage::Complete;
        return result;
    }
    if (cancelled()) {
        result.stage = SstvMartinM1EncoderStage::Cancelled;
        return result;
    }
    if (producedSamples_ < headerBoundaries_.back()) {
        result.stage = SstvMartinM1EncoderStage::Header;
        result.headerSegment = headerIndexAt(producedSamples_);
        result.frequencyHz = header_[result.headerSegment].frequencyHz;
        return result;
    }
    result.stage = SstvMartinM1EncoderStage::Image;
    result.image = mapper_.positionAtSample(
        producedSamples_ - headerBoundaries_.back());
    result.frequencyHz = imageFrequency(result.image);
    return result;
}

void SstvMartinM1Encoder::noteTransition(
    SstvMartinM1EncoderStage stage,
    std::size_t headerIndex,
    const SstvMartinM1Position& imagePosition) noexcept
{
    const bool changed = haveLastSegment_
        && (stage != lastStage_
            || (stage == SstvMartinM1EncoderStage::Header
                && headerIndex != lastHeaderIndex_)
            || (stage == SstvMartinM1EncoderStage::Image
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
std::size_t SstvMartinM1Encoder::generate(double frequencyHz,
                                          Sample* output,
                                          std::size_t count)
{
    static_assert(std::is_same_v<Sample, float>
                      || std::is_same_v<Sample, std::int16_t>,
                  "unsupported Martin M1 encoder sample type");
    if constexpr (std::is_same_v<Sample, float>) {
        return generator_.generateFloat(
            frequencyHz, config_.level, output, count);
    } else {
        return generator_.generatePcm16(
            frequencyHz, config_.level, output, count);
    }
}

template<typename Sample>
std::size_t SstvMartinM1Encoder::pull(Sample* output,
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
        throw std::length_error("Martin M1 TX pull exceeds its work bound");
    }
    if (capacity != 0U && output == nullptr) {
        if (metrics_.rejectedInputCalls
            != std::numeric_limits<std::uint64_t>::max()) {
            ++metrics_.rejectedInputCalls;
        }
        throw std::invalid_argument("Martin M1 TX output must not be null");
    }
    if (capacity == 0U || complete() || cancelled()) {
        return 0U;
    }

    std::size_t produced = 0U;
    while (produced < capacity && !complete() && !cancelled()) {
        const auto current = position();
        std::uint64_t segmentEnd = 0U;
        if (current.stage == SstvMartinM1EncoderStage::Header) {
            segmentEnd = headerBoundaries_[current.headerSegment + 1U];
        } else if (current.stage == SstvMartinM1EncoderStage::Image) {
            segmentEnd = checkedAdd(headerBoundaries_.back(),
                                    current.image.segmentEndSample);
        } else {
            break;
        }
        if (segmentEnd <= producedSamples_) {
            throw std::logic_error("Martin M1 TX segment made no progress");
        }

        noteTransition(current.stage,
                       current.headerSegment,
                       current.image);
        const std::uint64_t segmentRemaining = segmentEnd - producedSamples_;
        const std::size_t requested = std::min<std::size_t>(
            capacity - produced,
            static_cast<std::size_t>(std::min<std::uint64_t>(
                segmentRemaining,
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

std::size_t SstvMartinM1Encoder::pullFloat(float* output,
                                           std::size_t capacity)
{
    return pull(output, capacity);
}

std::size_t SstvMartinM1Encoder::pullPcm16(std::int16_t* output,
                                           std::size_t capacity)
{
    return pull(output, capacity);
}

std::uint64_t SstvMartinM1Encoder::totalSamples() const noexcept
{
    return totalSamples_;
}

SstvMartinMode SstvMartinM1Encoder::mode() const noexcept
{
    return spec_.mode;
}

std::uint64_t SstvMartinM1Encoder::producedSamples() const noexcept
{
    return producedSamples_;
}

bool SstvMartinM1Encoder::complete() const noexcept
{
    return producedSamples_ >= totalSamples_;
}

bool SstvMartinM1Encoder::cancelled() const noexcept
{
    return generator_.cancelled();
}

SstvMartinM1EncoderMetrics SstvMartinM1Encoder::metrics() const noexcept
{
    auto result = metrics_;
    result.tone = generator_.metrics();
    return result;
}

void SstvMartinM1Encoder::cancel() noexcept
{
    generator_.cancel();
}

void SstvMartinM1Encoder::reset() noexcept
{
    generator_.reset();
    producedSamples_ = 0U;
    metrics_ = {};
    metrics_.residentImageBytes = pixels_.size() * sizeof(SstvRgbPixel);
    lastStage_ = SstvMartinM1EncoderStage::Header;
    lastHeaderIndex_ = 0U;
    lastImageRegion_ = SstvMartinM1Region::Outside;
    lastLine_ = 0U;
    lastComponent_ = ColourComponent::ModeSpecific;
    lastPixel_ = 0U;
    haveLastSegment_ = false;
}

void SstvMartinM1Decoder::validateConfig(
    const SstvMartinM1DecoderConfig& config)
{
    if (!std::isfinite(config.frequencyOffsetHz)
        || std::abs(config.frequencyOffsetHz)
            > MaximumAbsoluteFrequencyOffsetHz) {
        throw std::invalid_argument("Martin M1 RX frequency offset is out of range");
    }
    if (!std::isfinite(config.minimumObservationConfidence)
        || config.minimumObservationConfidence < 0.0
        || config.minimumObservationConfidence > 1.0) {
        throw std::invalid_argument("invalid Martin M1 RX confidence threshold");
    }
    if (config.maximumPendingDirtyEvents == 0U
        || config.maximumPendingDirtyEvents
            > SstvImageFrame::kMaximumDirtyEvents) {
        throw std::invalid_argument("invalid Martin M1 dirty-event bound");
    }
}

SstvMartinM1Decoder::SstvMartinM1Decoder(SstvMartinM1DecoderConfig config)
    : config_(config)
    , spec_(SstvMartinM1Protocol::spec(config.mode))
    , mapper_({config.sampleRate, config.clockErrorPpm, config.mode})
    , frame_(std::make_unique<SstvImageFrame>(
          spec_.width,
          spec_.height,
          config.maximumPendingDirtyEvents))
{
    validateConfig(config);
    refreshBufferMetrics();
}

void SstvMartinM1Decoder::saturatingAdd(std::uint64_t& value,
                                        std::uint64_t increment) noexcept
{
    value = increment > std::numeric_limits<std::uint64_t>::max() - value
        ? std::numeric_limits<std::uint64_t>::max()
        : value + increment;
}

bool SstvMartinM1Decoder::acceptSync(const SstvMartinM1LineSync& sync)
{
    if (sync.lineIndex >= spec_.height
        || !std::isfinite(sync.confidence)
        || sync.confidence < 0.0
        || sync.confidence > 1.0) {
        return false;
    }
    const std::uint64_t lineStart = mapper_.lineStartSample(sync.lineIndex);
    const std::uint64_t lineEnd = mapper_.lineEndSample(sync.lineIndex);
    const std::uint64_t lineSpan = lineEnd - lineStart;
    if (sync.syncStartSample
        > std::numeric_limits<std::uint64_t>::max() - lineSpan) {
        return false;
    }
    // A sync pulse is only classified after its exit debounce.  With a chunk
    // boundary inside the pulse, the decoder may already have seen a handful
    // of non-pixel observations whose centres follow syncStartSample.  A new
    // forward anchor is still safe at that point: porch/video has not begun
    // and anchorFor() will pick it up on the next observation.  Reject only an
    // anchor for a line that image assembly has actually passed, or a changed
    // anchor for a line whose pixels are already accumulating.
    if (haveCurrentLine_ && sync.lineIndex < currentLine_) {
        return false;
    }
    if (haveCurrentLine_ && sync.lineIndex == currentLine_
        && nonEmptyAccumulators_ != 0U
        && (!anchors_[sync.lineIndex].present
            || anchors_[sync.lineIndex].startSample
                != sync.syncStartSample)) {
        return false;
    }

    const std::size_t index = sync.lineIndex;
    Anchor& existing = anchors_[index];
    if (existing.present) {
        if (existing.startSample == sync.syncStartSample) {
            if (existing.predicted && !sync.predicted) {
                existing.predicted = false;
                existing.confidence = sync.confidence;
                return true;
            }
            return false;
        }
        if (haveCurrentLine_ && currentLine_ == sync.lineIndex
            && nonEmptyAccumulators_ != 0U) {
            return false;
        }
        if (!existing.predicted || sync.predicted) {
            return false;
        }
    }

    for (std::size_t previous = index; previous != 0U;) {
        --previous;
        if (anchors_[previous].present) {
            if (anchors_[previous].startSample >= sync.syncStartSample) {
                return false;
            }
            break;
        }
    }
    for (std::size_t next = index + 1U; next < anchors_.size(); ++next) {
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
    highestStoredAnchorLine_ = std::max(highestStoredAnchorLine_,
                                       sync.lineIndex);
    return true;
}

std::size_t SstvMartinM1Decoder::consumeLineSyncs(
    const SstvMartinM1LineSync* syncs,
    std::size_t count)
{
    if (count > MaximumSyncsPerConsume) {
        saturatingAdd(metrics_.rejectedInputCalls);
        saturatingAdd(metrics_.rejectedOversizeCalls);
        throw std::length_error("Martin M1 RX sync call exceeds its work bound");
    }
    if (count != 0U && syncs == nullptr) {
        saturatingAdd(metrics_.rejectedInputCalls);
        throw std::invalid_argument("Martin M1 RX sync input must not be null");
    }
    if (state_ != SstvMartinM1DecodeState::Receiving) {
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

std::size_t SstvMartinM1Decoder::consumeLineSyncs(
    const std::vector<SstvMartinM1LineSync>& syncs)
{
    return consumeLineSyncs(syncs.data(), syncs.size());
}

const SstvMartinM1Decoder::Anchor* SstvMartinM1Decoder::anchorFor(
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
         index <= highestStoredAnchorLine_ && index < anchors_.size();
         ++index) {
        const auto& anchor = anchors_[index];
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
    return found;
}

std::size_t SstvMartinM1Decoder::accumulatorIndex(
    ColourComponent component,
    std::uint32_t pixel)
{
    if (pixel >= SstvMartinM1Protocol::Width) {
        throw std::out_of_range("Martin M1 pixel is outside a scanline");
    }
    std::size_t componentIndex = 0U;
    switch (component) {
    case ColourComponent::Green:
        componentIndex = 0U;
        break;
    case ColourComponent::Blue:
        componentIndex = 1U;
        break;
    case ColourComponent::Red:
        componentIndex = 2U;
        break;
    default:
        throw std::invalid_argument("invalid Martin M1 colour component");
    }
    return componentIndex * SstvMartinM1Protocol::Width + pixel;
}

SstvImageChannel SstvMartinM1Decoder::imageChannel(
    ColourComponent component)
{
    switch (component) {
    case ColourComponent::Green:
        return SstvImageChannel::Green;
    case ColourComponent::Blue:
        return SstvImageChannel::Blue;
    case ColourComponent::Red:
        return SstvImageChannel::Red;
    default:
        throw std::invalid_argument("invalid Martin M1 image channel");
    }
}

bool SstvMartinM1Decoder::beginLine(std::uint32_t line)
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

void SstvMartinM1Decoder::accumulate(
    const SstvMartinM1Position& position,
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
        ? accumulator.count
        : accumulator.count + 1U;
    const double divisor = static_cast<double>(nextCount);
    accumulator.meanFrequencyHz +=
        (frequencyHz - accumulator.meanFrequencyHz) / divisor;
    accumulator.meanConfidence +=
        (confidence - accumulator.meanConfidence) / divisor;
    accumulator.count = nextCount;
}

void SstvMartinM1Decoder::publishCurrentLine()
{
    if (!haveCurrentLine_ || nonEmptyAccumulators_ == 0U) {
        clearLineAccumulators();
        return;
    }

    bool wrote = false;
    for (const auto component : {ColourComponent::Green,
                                 ColourComponent::Blue,
                                 ColourComponent::Red}) {
        const SstvImageChannel channel = imageChannel(component);
        for (std::uint32_t pixel = 0U;
             pixel < spec_.width;
             ++pixel) {
            const auto& accumulator =
                accumulators_[accumulatorIndex(component, pixel)];
            if (accumulator.count == 0U) {
                continue;
            }
            const auto result = frame_->writeChannel(
                pixel,
                currentLine_,
                channel,
                SstvMartinM1Protocol::valueForFrequency(
                    accumulator.meanFrequencyHz));
            if (result == SstvImageWriteResult::Cancelled) {
                state_ = SstvMartinM1DecodeState::Cancelled;
                clearLineAccumulators();
                return;
            }
            wrote = true;
            saturatingAdd(metrics_.componentsPublished);
        }
    }
    if (wrote) {
        saturatingAdd(metrics_.linesPublished);
    }
    clearLineAccumulators();
}

void SstvMartinM1Decoder::clearLineAccumulators() noexcept
{
    accumulators_.fill({});
    nonEmptyAccumulators_ = 0U;
    refreshBufferMetrics();
}

void SstvMartinM1Decoder::refreshBufferMetrics() noexcept
{
    metrics_.bufferedPixelAccumulators = nonEmptyAccumulators_;
    metrics_.peakBufferedPixelAccumulators = std::max(
        metrics_.peakBufferedPixelAccumulators,
        nonEmptyAccumulators_);
}

std::size_t SstvMartinM1Decoder::consume(
    const SstvFrequencyObservation* observations,
    std::size_t count)
{
    if (count > MaximumObservationsPerConsume) {
        saturatingAdd(metrics_.rejectedInputCalls);
        saturatingAdd(metrics_.rejectedOversizeCalls);
        throw std::length_error(
            "Martin M1 RX observation call exceeds its work bound");
    }
    if (count != 0U && observations == nullptr) {
        saturatingAdd(metrics_.rejectedInputCalls);
        throw std::invalid_argument(
            "Martin M1 RX observations must not be null");
    }
    if (state_ != SstvMartinM1DecodeState::Receiving) {
        saturatingAdd(metrics_.droppedObservationsAfterEnd,
                      static_cast<std::uint64_t>(count));
        return 0U;
    }

    std::size_t accepted = 0U;
    for (std::size_t index = 0U; index < count; ++index) {
        const auto& observation = observations[index];
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
            || frequency < SstvMartinM1Protocol::BlackFrequencyHz - 500.0
            || frequency > SstvMartinM1Protocol::WhiteFrequencyHz + 500.0) {
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
        const std::uint64_t localSample =
            observation.centreSample - anchor->startSample;
        std::uint64_t mappedSample = 0U;
        try {
            mappedSample = checkedAdd(mapper_.lineStartSample(line),
                                      localSample);
        } catch (const std::overflow_error&) {
            saturatingAdd(metrics_.numericFaults);
            continue;
        }
        const auto position = mapper_.positionAtSample(mappedSample);
        if (position.line != line) {
            saturatingAdd(metrics_.outOfLineObservations);
            continue;
        }
        if (position.region != SstvMartinM1Region::Pixel) {
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

std::size_t SstvMartinM1Decoder::consume(
    const std::vector<SstvFrequencyObservation>& observations)
{
    return consume(observations.data(), observations.size());
}

SstvMartinM1DecodeState SstvMartinM1Decoder::finish()
{
    if (state_ != SstvMartinM1DecodeState::Receiving) {
        return state_;
    }
    publishCurrentLine();
    haveCurrentLine_ = false;
    state_ = frame_->isComplete()
        ? SstvMartinM1DecodeState::Complete
        : SstvMartinM1DecodeState::Partial;
    return state_;
}

void SstvMartinM1Decoder::cancel() noexcept
{
    if (state_ == SstvMartinM1DecodeState::Receiving) {
        frame_->cancel();
        state_ = SstvMartinM1DecodeState::Cancelled;
        clearLineAccumulators();
        haveCurrentLine_ = false;
    }
}

void SstvMartinM1Decoder::reset() noexcept
{
    frame_->reset();
    anchors_.fill({});
    accumulators_.fill({});
    metrics_ = {};
    state_ = SstvMartinM1DecodeState::Receiving;
    currentLine_ = 0U;
    haveCurrentLine_ = false;
    lastObservationSample_ = 0U;
    haveLastObservation_ = false;
    nonEmptyAccumulators_ = 0U;
    highestStoredAnchorLine_ = 0U;
    anchorCursorLine_ = 0U;
    haveAnchorCursor_ = false;
    refreshBufferMetrics();
}

double SstvMartinM1Decoder::setFrequencyOffsetHz(double offsetHz)
{
    if (!std::isfinite(offsetHz)
        || std::abs(offsetHz) > MaximumAbsoluteFrequencyOffsetHz) {
        throw std::invalid_argument("Martin M1 RX frequency offset is out of range");
    }
    config_.frequencyOffsetHz = offsetHz;
    return config_.frequencyOffsetHz;
}

double SstvMartinM1Decoder::frequencyOffsetHz() const noexcept
{
    return config_.frequencyOffsetHz;
}

SstvMartinMode SstvMartinM1Decoder::mode() const noexcept
{
    return spec_.mode;
}

SstvMartinM1DecodeState SstvMartinM1Decoder::state() const noexcept
{
    return state_;
}

const SstvImageFrame& SstvMartinM1Decoder::imageFrame() const noexcept
{
    return *frame_;
}

SstvImageSnapshot SstvMartinM1Decoder::snapshot() const
{
    return frame_->snapshot();
}

std::vector<SstvDirtyEvent> SstvMartinM1Decoder::takeDirtyEvents()
{
    return frame_->takeDirtyEvents();
}

SstvMartinM1DecoderMetrics SstvMartinM1Decoder::metrics() const noexcept
{
    return metrics_;
}

} // namespace decodium::sstv
