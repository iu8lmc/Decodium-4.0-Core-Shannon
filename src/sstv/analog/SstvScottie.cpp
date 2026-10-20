// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvScottie.h"

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
constexpr std::uint64_t kPicosecondDenominator =
    static_cast<std::uint64_t>(kPicosecondsPerSecond);
constexpr std::uint64_t kScaledSampleDenominator =
    kPicosecondDenominator * kPpmDenominator;

struct LayoutElement final
{
    SstvScottieRegion region;
    ColourComponent component;
    std::uint64_t startPicoseconds;
    std::uint64_t endPicoseconds;
};

std::uint64_t checkedAdd(std::uint64_t left, std::uint64_t right)
{
    if (left > std::numeric_limits<std::uint64_t>::max() - right) {
        throw std::overflow_error("Scottie integer addition overflow");
    }
    return left + right;
}

std::uint64_t checkedMultiply(std::uint64_t left, std::uint64_t right)
{
    if (left != 0U
        && right > std::numeric_limits<std::uint64_t>::max() / left) {
        throw std::overflow_error("Scottie integer multiplication overflow");
    }
    return left * right;
}

std::uint64_t nonNegative(Picoseconds duration)
{
    if (duration.count < 0) {
        throw std::logic_error("Scottie protocol contains a negative duration");
    }
    return static_cast<std::uint64_t>(duration.count);
}

std::array<LayoutElement, 7U> makeLineLayout(
    const SstvScottieModeSpec& spec)
{
    const std::uint64_t porch = nonNegative(spec.porchDuration);
    const std::uint64_t component = nonNegative(spec.componentDuration);
    const std::uint64_t sync = nonNegative(spec.syncDuration);

    const std::uint64_t greenStart = porch;
    const std::uint64_t greenEnd = checkedAdd(greenStart, component);
    const std::uint64_t bluePorchEnd = checkedAdd(greenEnd, porch);
    const std::uint64_t blueEnd = checkedAdd(bluePorchEnd, component);
    const std::uint64_t syncEnd = checkedAdd(blueEnd, sync);
    const std::uint64_t redPorchEnd = checkedAdd(syncEnd, porch);
    const std::uint64_t redEnd = checkedAdd(redPorchEnd, component);

    if (blueEnd != nonNegative(spec.embeddedSyncOffset)
        || redEnd != nonNegative(spec.lineDuration)) {
        throw std::logic_error("Scottie protocol layout is inconsistent");
    }

    return {{
        {SstvScottieRegion::Porch, ColourComponent::Green,
         0U, greenStart},
        {SstvScottieRegion::Pixel, ColourComponent::Green,
         greenStart, greenEnd},
        {SstvScottieRegion::Porch, ColourComponent::Blue,
         greenEnd, bluePorchEnd},
        {SstvScottieRegion::Pixel, ColourComponent::Blue,
         bluePorchEnd, blueEnd},
        {SstvScottieRegion::Sync, ColourComponent::ModeSpecific,
         blueEnd, syncEnd},
        {SstvScottieRegion::Porch, ColourComponent::Red,
         syncEnd, redPorchEnd},
        {SstvScottieRegion::Pixel, ColourComponent::Red,
         redPorchEnd, redEnd},
    }};
}

std::uint64_t scaledPicosecondsCeiling(std::uint64_t protocolPicoseconds,
                                       std::uint32_t scaleNumerator)
{
    const std::uint64_t whole = protocolPicoseconds / kPpmDenominator;
    const std::uint64_t remainder = protocolPicoseconds % kPpmDenominator;
    std::uint64_t result = checkedMultiply(whole, scaleNumerator);
    const std::uint64_t fractional = checkedMultiply(remainder,
                                                     scaleNumerator);
    result = checkedAdd(
        result,
        checkedAdd(fractional, kPpmDenominator - 1U) / kPpmDenominator);
    return result;
}

double frequencyForVisSymbol(SstvVisSymbol symbol)
{
    switch (symbol) {
    case SstvVisSymbol::Zero:
        return 1'300.0;
    case SstvVisSymbol::One:
        return 1'100.0;
    case SstvVisSymbol::Separator:
        return 1'200.0;
    case SstvVisSymbol::Invalid:
        break;
    }
    throw std::logic_error("invalid Scottie VIS symbol");
}

std::vector<SstvRgbPixel> validatedPixels(const SstvRgbPixel* pixels,
                                          std::size_t count,
                                          SstvScottieMode mode)
{
    if (count != SstvScottieEncoder::pixelCount(mode)) {
        throw std::invalid_argument(
            "Scottie encoder pixel count does not match the selected mode");
    }
    if (pixels == nullptr) {
        throw std::invalid_argument("Scottie encoder pixels must not be null");
    }
    return {pixels, pixels + count};
}

} // namespace

SstvScottieModeSpec SstvScottieProtocol::spec(SstvScottieMode mode)
{
    // Clean-room protocol facts were compared only against the already
    // audited revisions recorded in docs/sstv/UPSTREAM_PROVENANCE.md:
    // SlowRX a50a4e2/ca6d7012, Robot36 75146a53, libsstv 193157a9,
    // pySSTV d998fad1 and QSSTV 8c27d6d.  No upstream implementation
    // expression is used here.  Resolutions kept deliberately visible:
    //
    // - S1's 432 us pixels and 1.5/9 ms framing sum to 428.220 ms.  SlowRX's
    //   separate 428.380 ms line field has an unexplained 160 us remainder;
    //   Robot36/libsstv agree with the structural sum used below.
    // - S2 is 320 pixels in four audited paths; pySSTV alone declares 160 and
    //   also shortens its emitted component duration.
    // - QSSTV/pySSTV TX and SlowRX/Robot36 RX begin row zero with green;
    //   libsstv alone emits an additional leading image sync.
    // - S3 and S4 reuse the S1 and S2 line timings respectively, with 128
    //   rows.  Handbook table 4.5 and the independently executed pinned
    //   libsstv encoder agree on 320 transmitted/display columns.  The
    //   Handbook describes S4's effective sampled resolution as 160 columns;
    //   that distinction belongs in the registry and does not halve the wire
    //   mapper or display raster.
    // - DX uses the structurally consistent 1.080 ms pixel from Robot36 and
    //   libsstv.  SlowRX's 1.08053 ms pixel conflicts with its own 1.0503 s
    //   line field.
    //
    // These resolutions permit deterministic implementation tests.  The
    // pinned S3/S4 libsstv landmarks add independent developer evidence, but
    // neither that compact metadata nor the self-generated paths constitute
    // a live-radio or interoperability verification claim.
    switch (mode) {
    case SstvScottieMode::S1:
        return {mode,
                "scottie-s1",
                "Scottie S1",
                Width,
                Height,
                60U,
                SyncDuration,
                PorchDuration,
                Picoseconds {432'000'000LL},
                Picoseconds {138'240'000'000LL},
                Picoseconds {279'480'000'000LL},
                Picoseconds {428'220'000'000LL},
                Picoseconds {109'624'320'000'000LL}};
    case SstvScottieMode::S2:
        return {mode,
                "scottie-s2",
                "Scottie S2",
                Width,
                Height,
                56U,
                SyncDuration,
                PorchDuration,
                Picoseconds {275'200'000LL},
                Picoseconds {88'064'000'000LL},
                Picoseconds {179'128'000'000LL},
                Picoseconds {277'692'000'000LL},
                Picoseconds {71'089'152'000'000LL}};
    case SstvScottieMode::S3:
        return {mode,
                "scottie-s3",
                "Scottie S3",
                Width,
                HalfHeight,
                52U,
                SyncDuration,
                PorchDuration,
                Picoseconds {432'000'000LL},
                Picoseconds {138'240'000'000LL},
                Picoseconds {279'480'000'000LL},
                Picoseconds {428'220'000'000LL},
                Picoseconds {54'812'160'000'000LL}};
    case SstvScottieMode::S4:
        return {mode,
                "scottie-s4",
                "Scottie S4",
                Width,
                HalfHeight,
                48U,
                SyncDuration,
                PorchDuration,
                Picoseconds {275'200'000LL},
                Picoseconds {88'064'000'000LL},
                Picoseconds {179'128'000'000LL},
                Picoseconds {277'692'000'000LL},
                Picoseconds {35'544'576'000'000LL}};
    case SstvScottieMode::DX:
        return {mode,
                "scottie-dx",
                "Scottie DX",
                Width,
                Height,
                76U,
                SyncDuration,
                PorchDuration,
                Picoseconds {1'080'000'000LL},
                Picoseconds {345'600'000'000LL},
                Picoseconds {694'200'000'000LL},
                Picoseconds {1'050'300'000'000LL},
                Picoseconds {268'876'800'000'000LL}};
    }
    throw std::invalid_argument("unknown Scottie mode");
}

double SstvScottieProtocol::frequencyForValue(std::uint8_t value) noexcept
{
    return BlackFrequencyHz
        + (WhiteFrequencyHz - BlackFrequencyHz)
            * static_cast<double>(value) / 255.0;
}

std::uint8_t SstvScottieProtocol::valueForFrequency(
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

SstvScottieMapper::SstvScottieMapper(SstvScottieMapperConfig config)
    : config_(config)
    , spec_(SstvScottieProtocol::spec(config.mode))
{
    if (config.sampleRate < MinimumSampleRate
        || config.sampleRate > MaximumSampleRate) {
        throw std::invalid_argument("unsupported Scottie sample rate");
    }
    if (config.clockErrorPpm < -MaximumAbsoluteClockErrorPpm
        || config.clockErrorPpm > MaximumAbsoluteClockErrorPpm) {
        throw std::invalid_argument("Scottie clock correction is out of range");
    }

    const std::int64_t numerator =
        static_cast<std::int64_t>(kPpmDenominator)
        + config.clockErrorPpm;
    if (numerator <= 0
        || numerator > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("invalid Scottie clock scale");
    }
    clockScaleNumerator_ = static_cast<std::uint32_t>(numerator);

    const std::uint64_t expectedImageDuration = checkedMultiply(
        nonNegative(spec_.lineDuration), spec_.height);
    if (expectedImageDuration != nonNegative(spec_.imageDuration)
        || spec_.width != SstvScottieProtocol::Width
        || (spec_.height != SstvScottieProtocol::Height
            && spec_.height != SstvScottieProtocol::HalfHeight)) {
        throw std::logic_error("Scottie protocol dimensions or duration differ");
    }
    static_cast<void>(makeLineLayout(spec_));
    imageSamples_ = samplesAtProtocolTime(nonNegative(spec_.imageDuration));
}

SstvScottieMapperConfig SstvScottieMapper::config() const noexcept
{
    return config_;
}

SstvScottieModeSpec SstvScottieMapper::modeSpec() const noexcept
{
    return spec_;
}

std::uint64_t SstvScottieMapper::imageSampleCount() const noexcept
{
    return imageSamples_;
}

std::uint64_t SstvScottieMapper::lineStartSample(std::uint32_t line) const
{
    if (line > spec_.height) {
        throw std::out_of_range("Scottie line is outside the image");
    }
    return samplesAtProtocolTime(
        checkedMultiply(line, nonNegative(spec_.lineDuration)));
}

std::uint64_t SstvScottieMapper::lineEndSample(std::uint32_t line) const
{
    if (line >= spec_.height) {
        throw std::out_of_range("Scottie line is outside the image");
    }
    return lineStartSample(line + 1U);
}

std::uint64_t SstvScottieMapper::embeddedSyncStartSample(
    std::uint32_t line) const
{
    if (line >= spec_.height) {
        throw std::out_of_range("Scottie sync line is outside the image");
    }
    return samplesAtProtocolTime(checkedAdd(
        checkedMultiply(line, nonNegative(spec_.lineDuration)),
        nonNegative(spec_.embeddedSyncOffset)));
}

std::uint64_t SstvScottieMapper::samplesAtProtocolTime(
    std::uint64_t picoseconds) const
{
    if (picoseconds > nonNegative(spec_.imageDuration)) {
        throw std::out_of_range("Scottie time is outside the image");
    }

    const std::uint64_t wholeSeconds = picoseconds / kPicosecondDenominator;
    const std::uint64_t partialSecond = picoseconds % kPicosecondDenominator;
    const std::uint64_t fractionProduct = checkedMultiply(
        partialSecond, config_.sampleRate);
    std::uint64_t unscaledSamples = checkedAdd(
        checkedMultiply(wholeSeconds, config_.sampleRate),
        fractionProduct / kPicosecondDenominator);
    const std::uint64_t unscaledRemainder =
        fractionProduct % kPicosecondDenominator;

    const std::uint64_t scaledProduct = checkedMultiply(
        unscaledSamples, clockScaleNumerator_);
    std::uint64_t result = scaledProduct / kPpmDenominator;
    const std::uint64_t residual = checkedAdd(
        checkedMultiply(scaledProduct % kPpmDenominator,
                        kPicosecondDenominator),
        checkedMultiply(unscaledRemainder, clockScaleNumerator_));
    result = checkedAdd(result, residual / kScaledSampleDenominator);
    return result;
}

SstvScottiePosition SstvScottieMapper::makeNonPixelPosition(
    SstvScottieRegion region,
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

SstvScottiePosition SstvScottieMapper::pixelPositionAtSample(
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
            checkedMultiply(middle, nonNegative(spec_.pixelDuration)));
        if (samplesAtProtocolTime(boundary) <= sample) {
            low = middle;
        } else {
            high = middle;
        }
    }

    const std::uint64_t start = checkedAdd(
        componentStartPicoseconds,
        checkedMultiply(low, nonNegative(spec_.pixelDuration)));
    const std::uint64_t end = checkedAdd(
        componentStartPicoseconds,
        checkedMultiply(low + 1U, nonNegative(spec_.pixelDuration)));
    return {SstvScottieRegion::Pixel,
            line,
            component,
            low,
            samplesAtProtocolTime(start),
            samplesAtProtocolTime(end)};
}

SstvScottiePosition SstvScottieMapper::positionAtSample(
    std::uint64_t imageSample) const
{
    if (imageSample >= imageSamples_) {
        return {SstvScottieRegion::Complete,
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
        if (element.region == SstvScottieRegion::Pixel) {
            return pixelPositionAtSample(
                line, element.component, start, imageSample);
        }
        auto result = makeNonPixelPosition(element.region, line, start, end);
        result.component = element.component;
        return result;
    }
    throw std::logic_error("Scottie sample did not map to a line element");
}

SstvScottiePosition SstvScottieMapper::positionAtProtocolTime(
    std::uint64_t protocolPicoseconds) const
{
    const std::uint64_t imageDuration = nonNegative(spec_.imageDuration);
    if (protocolPicoseconds >= imageDuration) {
        return {SstvScottieRegion::Complete,
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
    const auto layout = makeLineLayout(spec_);
    for (const auto& element : layout) {
        if (local >= element.endPicoseconds) {
            continue;
        }
        const std::uint64_t start = checkedAdd(
            lineStart, element.startPicoseconds);
        const std::uint64_t end = checkedAdd(
            lineStart, element.endPicoseconds);
        if (element.region != SstvScottieRegion::Pixel) {
            auto result = makeNonPixelPosition(
                element.region, line, start, end);
            result.component = element.component;
            return result;
        }

        const std::uint64_t pixelOffset =
            (protocolPicoseconds - start)
            / nonNegative(spec_.pixelDuration);
        const std::uint32_t pixel = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(pixelOffset, spec_.width - 1U));
        const std::uint64_t pixelStart = checkedAdd(
            start,
            checkedMultiply(pixel, nonNegative(spec_.pixelDuration)));
        const std::uint64_t pixelEnd = checkedAdd(
            pixelStart, nonNegative(spec_.pixelDuration));
        return {SstvScottieRegion::Pixel,
                line,
                element.component,
                pixel,
                samplesAtProtocolTime(pixelStart),
                samplesAtProtocolTime(pixelEnd)};
    }
    throw std::logic_error("Scottie time did not map to a line element");
}

SstvScottiePosition SstvScottieMapper::positionAtElapsedTime(
    Picoseconds elapsed) const
{
    if (elapsed.count < 0) {
        return {};
    }
    const std::uint64_t elapsedPicoseconds =
        static_cast<std::uint64_t>(elapsed.count);
    const std::uint64_t effectiveEnd = scaledPicosecondsCeiling(
        nonNegative(spec_.imageDuration), clockScaleNumerator_);
    if (elapsedPicoseconds >= effectiveEnd) {
        return {SstvScottieRegion::Complete,
                spec_.height,
                ColourComponent::ModeSpecific,
                0U,
                imageSamples_,
                imageSamples_};
    }

    const std::uint64_t whole = elapsedPicoseconds / clockScaleNumerator_;
    const std::uint64_t remainder = elapsedPicoseconds % clockScaleNumerator_;
    const std::uint64_t protocolTime = checkedAdd(
        checkedMultiply(whole, kPpmDenominator),
        checkedMultiply(remainder, kPpmDenominator)
            / clockScaleNumerator_);
    return positionAtProtocolTime(protocolTime);
}

std::array<SstvScottieEncoder::HeaderSegment,
           SstvScottieEncoder::HeaderSegmentCount>
SstvScottieEncoder::makeHeader(std::uint8_t visPayload)
{
    std::array<HeaderSegment, HeaderSegmentCount> header {};
    header[0] = {1'900.0, Picoseconds {300'000'000'000LL}};
    header[1] = {1'200.0, Picoseconds {10'000'000'000LL}};
    header[2] = {1'900.0, Picoseconds {300'000'000'000LL}};

    const auto vis = SstvVisCodec::encodeStandard(visPayload);
    if (vis.symbols.size() != HeaderSegmentCount - 3U) {
        throw std::logic_error("unexpected Scottie VIS symbol count");
    }
    for (std::size_t index = 0U; index < vis.symbols.size(); ++index) {
        header[index + 3U] = {
            frequencyForVisSymbol(vis.symbols[index]),
            Picoseconds {30'000'000'000LL}};
    }
    return header;
}

std::array<std::uint64_t, SstvScottieEncoder::HeaderSegmentCount + 1U>
SstvScottieEncoder::makeHeaderBoundaries(
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

SstvScottieEncoder::SstvScottieEncoder(
    const SstvRgbPixel* pixels,
    std::size_t count,
    SstvScottieEncoderConfig config)
    : config_(config)
    , spec_(SstvScottieProtocol::spec(config.mode))
    , pixels_(validatedPixels(pixels, count, config.mode))
    , mapper_({config.mode, config.sampleRate, config.clockErrorPpm})
    , generator_(config.sampleRate, config.headroom)
    , header_(makeHeader(spec_.visPayload))
    , headerBoundaries_(makeHeaderBoundaries(config.sampleRate, header_))
{
    if (!std::isfinite(config.level)
        || config.level < 0.0
        || config.level > MaximumLevel) {
        throw std::invalid_argument("Scottie TX level is out of range");
    }
    generator_.validateTone(SstvScottieProtocol::SyncFrequencyHz,
                            config.level);
    generator_.validateTone(SstvScottieProtocol::WhiteFrequencyHz,
                            config.level);
    totalSamples_ = checkedAdd(headerBoundaries_.back(),
                               mapper_.imageSampleCount());
    metrics_.residentImageBytes = pixels_.size() * sizeof(SstvRgbPixel);
}

SstvScottieEncoder::SstvScottieEncoder(
    const std::vector<SstvRgbPixel>& pixels,
    SstvScottieEncoderConfig config)
    : SstvScottieEncoder(pixels.data(), pixels.size(), config)
{
}

std::size_t SstvScottieEncoder::pixelCount(SstvScottieMode mode)
{
    const SstvScottieModeSpec modeSpec = SstvScottieProtocol::spec(mode);
    if (modeSpec.width != 0U
        && modeSpec.height > std::numeric_limits<std::size_t>::max()
            / modeSpec.width) {
        throw std::overflow_error("Scottie encoder pixel count overflow");
    }
    return static_cast<std::size_t>(modeSpec.width) * modeSpec.height;
}

std::size_t SstvScottieEncoder::headerIndexAt(
    std::uint64_t sample) const noexcept
{
    const auto first = headerBoundaries_.begin() + 1;
    const auto found = std::upper_bound(first,
                                        headerBoundaries_.end(),
                                        sample);
    return static_cast<std::size_t>(found - first);
}

double SstvScottieEncoder::imageFrequency(
    const SstvScottiePosition& position) const
{
    switch (position.region) {
    case SstvScottieRegion::Porch:
        return SstvScottieProtocol::PorchFrequencyHz;
    case SstvScottieRegion::Sync:
        return SstvScottieProtocol::SyncFrequencyHz;
    case SstvScottieRegion::Pixel:
        break;
    case SstvScottieRegion::Outside:
    case SstvScottieRegion::Complete:
        throw std::logic_error("Scottie encoder has no tone at this position");
    }

    const std::size_t index = static_cast<std::size_t>(position.line)
        * spec_.width + position.pixel;
    if (index >= pixels_.size()) {
        throw std::logic_error("Scottie mapped pixel is outside the frame");
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
        throw std::logic_error("Scottie mapped an invalid colour component");
    }
    return SstvScottieProtocol::frequencyForValue(value);
}

SstvScottieEncoderPosition SstvScottieEncoder::position() const
{
    SstvScottieEncoderPosition result;
    result.producedSamples = producedSamples_;
    result.totalSamples = totalSamples_;
    if (complete()) {
        result.stage = SstvScottieEncoderStage::Complete;
        return result;
    }
    if (cancelled()) {
        result.stage = SstvScottieEncoderStage::Cancelled;
        return result;
    }
    if (producedSamples_ < headerBoundaries_.back()) {
        result.stage = SstvScottieEncoderStage::Header;
        result.headerSegment = headerIndexAt(producedSamples_);
        result.frequencyHz = header_[result.headerSegment].frequencyHz;
        return result;
    }
    result.stage = SstvScottieEncoderStage::Image;
    result.image = mapper_.positionAtSample(
        producedSamples_ - headerBoundaries_.back());
    result.frequencyHz = imageFrequency(result.image);
    return result;
}

void SstvScottieEncoder::noteTransition(
    SstvScottieEncoderStage stage,
    std::size_t headerIndex,
    const SstvScottiePosition& imagePosition) noexcept
{
    const bool changed = haveLastSegment_
        && (stage != lastStage_
            || (stage == SstvScottieEncoderStage::Header
                && headerIndex != lastHeaderIndex_)
            || (stage == SstvScottieEncoderStage::Image
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
std::size_t SstvScottieEncoder::generate(double frequencyHz,
                                         Sample* output,
                                         std::size_t count)
{
    static_assert(std::is_same_v<Sample, float>
                      || std::is_same_v<Sample, std::int16_t>,
                  "unsupported Scottie encoder sample type");
    if constexpr (std::is_same_v<Sample, float>) {
        return generator_.generateFloat(
            frequencyHz, config_.level, output, count);
    } else {
        return generator_.generatePcm16(
            frequencyHz, config_.level, output, count);
    }
}

template<typename Sample>
std::size_t SstvScottieEncoder::pull(Sample* output,
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
        throw std::length_error("Scottie TX pull exceeds its work bound");
    }
    if (capacity != 0U && output == nullptr) {
        if (metrics_.rejectedInputCalls
            != std::numeric_limits<std::uint64_t>::max()) {
            ++metrics_.rejectedInputCalls;
        }
        throw std::invalid_argument("Scottie TX output must not be null");
    }
    if (capacity == 0U || complete() || cancelled()) {
        return 0U;
    }

    std::size_t produced = 0U;
    while (produced < capacity && !complete() && !cancelled()) {
        const auto current = position();
        std::uint64_t segmentEnd = 0U;
        if (current.stage == SstvScottieEncoderStage::Header) {
            segmentEnd = headerBoundaries_[current.headerSegment + 1U];
        } else if (current.stage == SstvScottieEncoderStage::Image) {
            segmentEnd = checkedAdd(headerBoundaries_.back(),
                                    current.image.segmentEndSample);
        } else {
            break;
        }
        if (segmentEnd <= producedSamples_) {
            throw std::logic_error("Scottie TX segment made no progress");
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

std::size_t SstvScottieEncoder::pullFloat(float* output,
                                          std::size_t capacity)
{
    return pull(output, capacity);
}

std::size_t SstvScottieEncoder::pullPcm16(std::int16_t* output,
                                          std::size_t capacity)
{
    return pull(output, capacity);
}

SstvScottieMode SstvScottieEncoder::mode() const noexcept
{
    return config_.mode;
}

std::uint64_t SstvScottieEncoder::totalSamples() const noexcept
{
    return totalSamples_;
}

std::uint64_t SstvScottieEncoder::producedSamples() const noexcept
{
    return producedSamples_;
}

bool SstvScottieEncoder::complete() const noexcept
{
    return producedSamples_ >= totalSamples_;
}

bool SstvScottieEncoder::cancelled() const noexcept
{
    return generator_.cancelled();
}

SstvScottieEncoderMetrics SstvScottieEncoder::metrics() const noexcept
{
    auto result = metrics_;
    result.tone = generator_.metrics();
    return result;
}

double SstvScottieEncoder::phaseTurns() const noexcept
{
    return generator_.phaseTurns();
}

void SstvScottieEncoder::cancel() noexcept
{
    generator_.cancel();
}

void SstvScottieEncoder::reset() noexcept
{
    generator_.reset();
    producedSamples_ = 0U;
    metrics_ = {};
    metrics_.residentImageBytes = pixels_.size() * sizeof(SstvRgbPixel);
    lastStage_ = SstvScottieEncoderStage::Header;
    lastHeaderIndex_ = 0U;
    lastImageRegion_ = SstvScottieRegion::Outside;
    lastLine_ = 0U;
    lastComponent_ = ColourComponent::ModeSpecific;
    lastPixel_ = 0U;
    haveLastSegment_ = false;
}

void SstvScottieDecoder::validateConfig(
    const SstvScottieDecoderConfig& config)
{
    if (!std::isfinite(config.frequencyOffsetHz)
        || std::abs(config.frequencyOffsetHz)
            > MaximumAbsoluteFrequencyOffsetHz) {
        throw std::invalid_argument("Scottie RX frequency offset is out of range");
    }
    if (!std::isfinite(config.minimumObservationConfidence)
        || config.minimumObservationConfidence < 0.0
        || config.minimumObservationConfidence > 1.0) {
        throw std::invalid_argument("invalid Scottie RX confidence threshold");
    }
    if (config.maximumPendingDirtyEvents == 0U
        || config.maximumPendingDirtyEvents
            > SstvImageFrame::kMaximumDirtyEvents) {
        throw std::invalid_argument("invalid Scottie dirty-event bound");
    }
}

SstvScottieDecoder::SstvScottieDecoder(SstvScottieDecoderConfig config)
    : config_(config)
    , spec_(SstvScottieProtocol::spec(config.mode))
    , mapper_({config.mode, config.sampleRate, config.clockErrorPpm})
    , frame_(std::make_unique<SstvImageFrame>(
          spec_.width,
          spec_.height,
          config.maximumPendingDirtyEvents))
{
    validateConfig(config);
    refreshBufferMetrics();
}

void SstvScottieDecoder::saturatingAdd(std::uint64_t& value,
                                       std::uint64_t increment) noexcept
{
    value = increment > std::numeric_limits<std::uint64_t>::max() - value
        ? std::numeric_limits<std::uint64_t>::max()
        : value + increment;
}

bool SstvScottieDecoder::acceptSync(const SstvScottieLineSync& sync)
{
    if (sync.lineIndex >= spec_.height
        || !std::isfinite(sync.confidence)
        || sync.confidence < 0.0
        || sync.confidence > 1.0) {
        return false;
    }

    std::uint64_t lineStart = 0U;
    std::uint64_t lineEnd = 0U;
    try {
        const std::uint64_t mappedLineStart =
            mapper_.lineStartSample(sync.lineIndex);
        const std::uint64_t mappedLineEnd =
            mapper_.lineEndSample(sync.lineIndex);
        const std::uint64_t mappedSyncStart =
            mapper_.embeddedSyncStartSample(sync.lineIndex);
        const std::uint64_t syncOffset = mappedSyncStart - mappedLineStart;
        if (sync.syncStartSample < syncOffset) {
            return false;
        }
        lineStart = sync.syncStartSample - syncOffset;
        lineEnd = checkedAdd(lineStart, mappedLineEnd - mappedLineStart);
    } catch (const std::exception&) {
        return false;
    }

    const std::size_t index = sync.lineIndex;
    Anchor& existing = anchors_[index];
    if (existing.present) {
        if (existing.syncStartSample == sync.syncStartSample) {
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
            if (anchors_[previous].syncStartSample >= sync.syncStartSample
                || anchors_[previous].lineStartSample >= lineStart) {
                return false;
            }
            break;
        }
    }
    for (std::size_t next = index + 1U; next < anchors_.size(); ++next) {
        if (anchors_[next].present) {
            if (anchors_[next].syncStartSample <= sync.syncStartSample
                || anchors_[next].lineStartSample <= lineStart) {
                return false;
            }
            break;
        }
    }

    if (!existing.present) {
        ++metrics_.storedSyncAnchors;
    }
    existing = {sync.syncStartSample,
                lineStart,
                lineEnd,
                sync.confidence,
                true,
                sync.predicted};
    highestStoredAnchorLine_ = std::max(highestStoredAnchorLine_,
                                       sync.lineIndex);
    return true;
}

std::size_t SstvScottieDecoder::consumeLineSyncs(
    const SstvScottieLineSync* syncs,
    std::size_t count)
{
    if (count > MaximumSyncsPerConsume) {
        saturatingAdd(metrics_.rejectedInputCalls);
        saturatingAdd(metrics_.rejectedOversizeCalls);
        throw std::length_error("Scottie RX sync call exceeds its work bound");
    }
    if (count != 0U && syncs == nullptr) {
        saturatingAdd(metrics_.rejectedInputCalls);
        throw std::invalid_argument("Scottie RX sync input must not be null");
    }
    if (state_ != SstvScottieDecodeState::Receiving) {
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

std::size_t SstvScottieDecoder::consumeLineSyncs(
    const std::vector<SstvScottieLineSync>& syncs)
{
    return consumeLineSyncs(syncs.data(), syncs.size());
}

const SstvScottieDecoder::Anchor* SstvScottieDecoder::anchorFor(
    std::uint64_t sample,
    std::uint32_t& line) const noexcept
{
    const std::uint32_t limit = std::min<std::uint32_t>(
        highestStoredAnchorLine_, spec_.height - 1U);
    for (std::uint32_t cursor = limit + 1U; cursor != 0U; --cursor) {
        const std::uint32_t index = cursor - 1U;
        const auto& candidate = anchors_[index];
        if (!candidate.present
            || sample < candidate.lineStartSample
            || sample >= candidate.lineEndSample) {
            continue;
        }
        line = index;
        return &candidate;
    }
    return nullptr;
}

std::size_t SstvScottieDecoder::accumulatorIndex(
    ColourComponent component,
    std::uint32_t pixel)
{
    if (pixel >= SstvScottieProtocol::Width) {
        throw std::out_of_range("Scottie pixel is outside a scanline");
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
        throw std::invalid_argument("invalid Scottie colour component");
    }
    return componentIndex * SstvScottieProtocol::Width + pixel;
}

SstvImageChannel SstvScottieDecoder::imageChannel(
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
        throw std::invalid_argument("invalid Scottie image channel");
    }
}

bool SstvScottieDecoder::beginLine(std::uint32_t line)
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

void SstvScottieDecoder::accumulate(
    const SstvScottiePosition& position,
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

void SstvScottieDecoder::publishCurrentLine()
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
        for (std::uint32_t pixel = 0U; pixel < spec_.width; ++pixel) {
            const auto& accumulator =
                accumulators_[accumulatorIndex(component, pixel)];
            if (accumulator.count == 0U) {
                continue;
            }
            const auto result = frame_->writeChannel(
                pixel,
                currentLine_,
                channel,
                SstvScottieProtocol::valueForFrequency(
                    accumulator.meanFrequencyHz));
            if (result == SstvImageWriteResult::Cancelled) {
                state_ = SstvScottieDecodeState::Cancelled;
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

void SstvScottieDecoder::clearLineAccumulators() noexcept
{
    accumulators_.fill({});
    nonEmptyAccumulators_ = 0U;
    refreshBufferMetrics();
}

void SstvScottieDecoder::refreshBufferMetrics() noexcept
{
    metrics_.bufferedPixelAccumulators = nonEmptyAccumulators_;
    metrics_.peakBufferedPixelAccumulators = std::max(
        metrics_.peakBufferedPixelAccumulators,
        nonEmptyAccumulators_);
}

std::size_t SstvScottieDecoder::consume(
    const SstvFrequencyObservation* observations,
    std::size_t count)
{
    if (count > MaximumObservationsPerConsume) {
        saturatingAdd(metrics_.rejectedInputCalls);
        saturatingAdd(metrics_.rejectedOversizeCalls);
        throw std::length_error(
            "Scottie RX observation call exceeds its work bound");
    }
    if (count != 0U && observations == nullptr) {
        saturatingAdd(metrics_.rejectedInputCalls);
        throw std::invalid_argument(
            "Scottie RX observations must not be null");
    }
    if (state_ != SstvScottieDecodeState::Receiving) {
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
            || frequency < SstvScottieProtocol::BlackFrequencyHz - 500.0
            || frequency > SstvScottieProtocol::WhiteFrequencyHz + 500.0) {
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
            observation.centreSample - anchor->lineStartSample;
        std::uint64_t mappedSample = 0U;
        try {
            mappedSample = checkedAdd(mapper_.lineStartSample(line),
                                      localSample);
        } catch (const std::exception&) {
            saturatingAdd(metrics_.numericFaults);
            continue;
        }
        const auto position = mapper_.positionAtSample(mappedSample);
        if (position.line != line) {
            saturatingAdd(metrics_.outOfLineObservations);
            continue;
        }
        if (position.region != SstvScottieRegion::Pixel) {
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

std::size_t SstvScottieDecoder::consume(
    const std::vector<SstvFrequencyObservation>& observations)
{
    return consume(observations.data(), observations.size());
}

SstvScottieDecodeState SstvScottieDecoder::finish()
{
    if (state_ != SstvScottieDecodeState::Receiving) {
        return state_;
    }
    publishCurrentLine();
    haveCurrentLine_ = false;
    state_ = frame_->isComplete()
        ? SstvScottieDecodeState::Complete
        : SstvScottieDecodeState::Partial;
    return state_;
}

void SstvScottieDecoder::cancel() noexcept
{
    if (state_ == SstvScottieDecodeState::Receiving) {
        frame_->cancel();
        state_ = SstvScottieDecodeState::Cancelled;
        clearLineAccumulators();
        haveCurrentLine_ = false;
    }
}

void SstvScottieDecoder::reset() noexcept
{
    frame_->reset();
    anchors_.fill({});
    accumulators_.fill({});
    metrics_ = {};
    state_ = SstvScottieDecodeState::Receiving;
    currentLine_ = 0U;
    haveCurrentLine_ = false;
    lastObservationSample_ = 0U;
    haveLastObservation_ = false;
    nonEmptyAccumulators_ = 0U;
    highestStoredAnchorLine_ = 0U;
    refreshBufferMetrics();
}

SstvScottieMode SstvScottieDecoder::mode() const noexcept
{
    return config_.mode;
}

double SstvScottieDecoder::setFrequencyOffsetHz(double offsetHz)
{
    if (!std::isfinite(offsetHz)
        || std::abs(offsetHz) > MaximumAbsoluteFrequencyOffsetHz) {
        throw std::invalid_argument("Scottie RX frequency offset is out of range");
    }
    config_.frequencyOffsetHz = offsetHz;
    return config_.frequencyOffsetHz;
}

double SstvScottieDecoder::frequencyOffsetHz() const noexcept
{
    return config_.frequencyOffsetHz;
}

SstvScottieDecodeState SstvScottieDecoder::state() const noexcept
{
    return state_;
}

const SstvImageFrame& SstvScottieDecoder::imageFrame() const noexcept
{
    return *frame_;
}

SstvImageSnapshot SstvScottieDecoder::snapshot() const
{
    return frame_->snapshot();
}

std::vector<SstvDirtyEvent> SstvScottieDecoder::takeDirtyEvents()
{
    return frame_->takeDirtyEvents();
}

SstvScottieDecoderMetrics SstvScottieDecoder::metrics() const noexcept
{
    return metrics_;
}

} // namespace decodium::sstv
