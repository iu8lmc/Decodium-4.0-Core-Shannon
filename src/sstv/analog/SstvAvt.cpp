// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvAvt.h"

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

std::uint64_t checkedAdd(std::uint64_t left, std::uint64_t right)
{
    if (left > std::numeric_limits<std::uint64_t>::max() - right) {
        throw std::overflow_error("AVT integer addition overflow");
    }
    return left + right;
}

std::uint64_t checkedMultiply(std::uint64_t left, std::uint64_t right)
{
    if (left != 0U
        && right > std::numeric_limits<std::uint64_t>::max() / left) {
        throw std::overflow_error("AVT integer multiplication overflow");
    }
    return left * right;
}

std::uint64_t nonNegative(Picoseconds duration)
{
    if (duration.count < 0) {
        throw std::logic_error("AVT protocol contains a negative duration");
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
        throw std::logic_error("invalid AVT duration fraction");
    }
    return checkedMultiply(duration, numerator) / denominator;
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
    throw std::logic_error("invalid AVT VIS symbol");
}

std::vector<SstvRgbPixel> validatedPixels(const SstvRgbPixel* pixels,
                                          std::size_t count,
                                          SstvAvtMode mode)
{
    if (count != SstvAvtEncoder::pixelCount(mode)) {
        throw std::invalid_argument(
            "AVT pixel count does not match the selected mode");
    }
    if (pixels == nullptr) {
        throw std::invalid_argument("AVT encoder pixels must not be null");
    }
    return {pixels, pixels + count};
}

ColourComponent componentForIndex(std::uint32_t index)
{
    switch (index) {
    case 0U:
        return ColourComponent::Red;
    case 1U:
        return ColourComponent::Green;
    case 2U:
        return ColourComponent::Blue;
    default:
        throw std::logic_error("invalid AVT component index");
    }
}

} // namespace

SstvAvtModeSpec SstvAvtProtocol::spec(SstvAvtMode mode)
{
    switch (mode) {
    case SstvAvtMode::Avt24:
        return {mode,
                "avt-24",
                "AVT24",
                128U,
                128U,
                120U,
                64U,
                Picoseconds {62'500'000'000LL},
                Picoseconds {187'500'000'000LL},
                Picoseconds {22'500'000'000'000LL}};
    case SstvAvtMode::Avt90:
        // The Handbook records 256x240 effective resolution. QSSTV and
        // MMSSTV prepare and scan 320 samples across each 125 ms component.
        // Decodium preserves both facts instead of silently choosing one.
        return {mode,
                "avt-90",
                "AVT90",
                320U,
                256U,
                240U,
                68U,
                Picoseconds {125'000'000'000LL},
                Picoseconds {375'000'000'000LL},
                Picoseconds {90'000'000'000'000LL}};
    case SstvAvtMode::Avt94:
        return {mode,
                "avt-94",
                "AVT94",
                320U,
                320U,
                200U,
                72U,
                Picoseconds {156'250'000'000LL},
                Picoseconds {468'750'000'000LL},
                Picoseconds {93'750'000'000'000LL}};
    }
    throw std::invalid_argument("unknown AVT mode");
}

std::optional<SstvAvtMode> SstvAvtProtocol::normalModeForVis(
    std::uint8_t payload) noexcept
{
    switch (payload) {
    case 64U:
        return SstvAvtMode::Avt24;
    case 68U:
        return SstvAvtMode::Avt90;
    case 72U:
        return SstvAvtMode::Avt94;
    default:
        return std::nullopt;
    }
}

double SstvAvtProtocol::frequencyForValue(std::uint8_t value) noexcept
{
    return BlackFrequencyHz
        + (WhiteFrequencyHz - BlackFrequencyHz)
            * static_cast<double>(value) / 255.0;
}

std::uint8_t SstvAvtProtocol::valueForFrequency(double frequencyHz) noexcept
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

std::vector<SstvAvtToneSegment> SstvAvtProtocol::normalHeader(
    SstvAvtMode mode)
{
    const SstvAvtModeSpec modeSpec = spec(mode);
    const SstvVisEncodedFrame vis = SstvVisCodec::encodeStandard(
        modeSpec.visPayload);
    if (vis.symbols.size() != StandardVisSegmentCount - 3U) {
        throw std::logic_error("unexpected AVT standard VIS symbol count");
    }

    std::vector<SstvAvtToneSegment> result;
    result.reserve(HeaderSegmentCount);
    for (std::size_t repeat = 0U; repeat < 3U; ++repeat) {
        static_cast<void>(repeat);
        result.push_back({1'900.0, Picoseconds {300'000'000'000LL}});
        result.push_back({1'200.0, Picoseconds {10'000'000'000LL}});
        result.push_back({1'900.0, Picoseconds {300'000'000'000LL}});
        for (const SstvVisSymbol symbol : vis.symbols) {
            result.push_back(
                {frequencyForVisSymbol(symbol),
                 Picoseconds {30'000'000'000LL}});
        }
    }

    const std::vector<SstvAvtSyncTone> countdown =
        SstvAvtSyncCodec::encodeCountdown(mode, false);
    for (const SstvAvtSyncTone& tone : countdown) {
        result.push_back({tone.frequencyHz, tone.duration});
    }
    if (result.size() != HeaderSegmentCount) {
        throw std::logic_error("AVT header segment count is inconsistent");
    }
    std::uint64_t duration = 0U;
    for (const SstvAvtToneSegment& segment : result) {
        duration = checkedAdd(duration, nonNegative(segment.duration));
    }
    if (duration != nonNegative(HeaderDuration)) {
        throw std::logic_error("AVT header duration is inconsistent");
    }
    return result;
}

SstvAvtMapper::SstvAvtMapper(SstvAvtMapperConfig config)
    : config_(config)
    , spec_(SstvAvtProtocol::spec(config.mode))
{
    if (config.sampleRate < MinimumSampleRate
        || config.sampleRate > MaximumSampleRate) {
        throw std::invalid_argument("unsupported AVT sample rate");
    }
    if (config.clockErrorPpm < -MaximumAbsoluteClockErrorPpm
        || config.clockErrorPpm > MaximumAbsoluteClockErrorPpm) {
        throw std::invalid_argument("AVT clock correction is out of range");
    }
    const std::int64_t scale =
        static_cast<std::int64_t>(kPpmDenominator) + config.clockErrorPpm;
    if (scale <= 0
        || scale > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("invalid AVT clock scale");
    }
    clockScaleNumerator_ = static_cast<std::uint32_t>(scale);
    if (checkedMultiply(nonNegative(spec_.componentDuration), 3U)
            != nonNegative(spec_.lineDuration)
        || checkedMultiply(nonNegative(spec_.lineDuration), spec_.height)
            != nonNegative(spec_.imageDuration)) {
        throw std::logic_error("AVT image duration is inconsistent");
    }
    imageSamples_ = samplesAtProtocolTime(nonNegative(spec_.imageDuration));
}

SstvAvtMapperConfig SstvAvtMapper::config() const noexcept
{
    return config_;
}

SstvAvtModeSpec SstvAvtMapper::modeSpec() const noexcept
{
    return spec_;
}

std::uint64_t SstvAvtMapper::imageSampleCount() const noexcept
{
    return imageSamples_;
}

std::uint64_t SstvAvtMapper::lineStartSample(std::uint32_t line) const
{
    if (line > spec_.height) {
        throw std::out_of_range("AVT line is outside the image");
    }
    return samplesAtProtocolTime(
        checkedMultiply(line, nonNegative(spec_.lineDuration)));
}

std::uint64_t SstvAvtMapper::lineEndSample(std::uint32_t line) const
{
    if (line >= spec_.height) {
        throw std::out_of_range("AVT line is outside the image");
    }
    return lineStartSample(line + 1U);
}

std::uint64_t SstvAvtMapper::samplesAtProtocolTime(
    std::uint64_t picoseconds) const
{
    if (picoseconds > nonNegative(spec_.imageDuration)) {
        throw std::out_of_range("AVT time is outside the image");
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

SstvAvtPosition SstvAvtMapper::makePosition(
    std::uint32_t line,
    ColourComponent component,
    std::uint32_t pixel,
    std::uint64_t startPicoseconds,
    std::uint64_t endPicoseconds) const
{
    return {SstvAvtRegion::Pixel,
            line,
            component,
            pixel,
            samplesAtProtocolTime(startPicoseconds),
            samplesAtProtocolTime(endPicoseconds)};
}

SstvAvtPosition SstvAvtMapper::positionAtSample(
    std::uint64_t imageSample) const
{
    if (imageSample >= imageSamples_) {
        return {SstvAvtRegion::Complete,
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
    const std::uint64_t componentDuration =
        nonNegative(spec_.componentDuration);

    std::uint32_t component = 0U;
    while (component + 1U < 3U
           && imageSample >= samplesAtProtocolTime(checkedAdd(
               lineStart,
               checkedMultiply(component + 1U, componentDuration)))) {
        ++component;
    }
    const std::uint64_t componentStart = checkedAdd(
        lineStart, checkedMultiply(component, componentDuration));

    std::uint32_t pixelLow = 0U;
    std::uint32_t pixelHigh = spec_.width;
    while (pixelLow + 1U < pixelHigh) {
        const std::uint32_t middle =
            pixelLow + (pixelHigh - pixelLow) / 2U;
        const std::uint64_t boundary = checkedAdd(
            componentStart,
            fractionOf(componentDuration, middle, spec_.width));
        if (samplesAtProtocolTime(boundary) <= imageSample) {
            pixelLow = middle;
        } else {
            pixelHigh = middle;
        }
    }
    const std::uint64_t pixelStart = checkedAdd(
        componentStart,
        fractionOf(componentDuration, pixelLow, spec_.width));
    const std::uint64_t pixelEnd = checkedAdd(
        componentStart,
        fractionOf(componentDuration, pixelLow + 1U, spec_.width));
    return makePosition(line,
                        componentForIndex(component),
                        pixelLow,
                        pixelStart,
                        pixelEnd);
}

SstvAvtPosition SstvAvtMapper::positionAtProtocolTime(
    std::uint64_t protocolPicoseconds) const
{
    if (protocolPicoseconds >= nonNegative(spec_.imageDuration)) {
        return {SstvAvtRegion::Complete,
                spec_.height,
                ColourComponent::ModeSpecific,
                0U,
                imageSamples_,
                imageSamples_};
    }
    const std::uint64_t lineDuration = nonNegative(spec_.lineDuration);
    const std::uint64_t componentDuration =
        nonNegative(spec_.componentDuration);
    const std::uint32_t line = static_cast<std::uint32_t>(
        protocolPicoseconds / lineDuration);
    const std::uint64_t lineStart = checkedMultiply(line, lineDuration);
    const std::uint64_t local = protocolPicoseconds - lineStart;
    const std::uint32_t component = static_cast<std::uint32_t>(
        local / componentDuration);
    const std::uint64_t componentStart = checkedAdd(
        lineStart, checkedMultiply(component, componentDuration));
    const std::uint64_t componentLocal = local
        - checkedMultiply(component, componentDuration);
    const std::uint64_t scaledPixel = checkedMultiply(
        componentLocal, spec_.width);
    const std::uint32_t pixel = std::min(
        spec_.width - 1U,
        static_cast<std::uint32_t>(scaledPixel / componentDuration));
    const std::uint64_t pixelStart = checkedAdd(
        componentStart, fractionOf(componentDuration, pixel, spec_.width));
    const std::uint64_t pixelEnd = checkedAdd(
        componentStart,
        fractionOf(componentDuration, pixel + 1U, spec_.width));
    return makePosition(line,
                        componentForIndex(component),
                        pixel,
                        pixelStart,
                        pixelEnd);
}

SstvAvtPosition SstvAvtMapper::positionAtElapsedTime(
    Picoseconds elapsed) const
{
    if (elapsed.count < 0) {
        return {};
    }
    if (elapsed.count == 0) {
        return positionAtProtocolTime(0U);
    }
    const std::uint64_t elapsedPs = static_cast<std::uint64_t>(elapsed.count);
    const std::uint64_t whole = elapsedPs / clockScaleNumerator_;
    const std::uint64_t remainder = elapsedPs % clockScaleNumerator_;
    const std::uint64_t protocolTime = checkedAdd(
        checkedMultiply(whole, kPpmDenominator),
        checkedMultiply(remainder, kPpmDenominator)
            / clockScaleNumerator_);
    return positionAtProtocolTime(protocolTime);
}

std::vector<std::uint64_t> SstvAvtEncoder::makeHeaderBoundaries(
    std::uint32_t sampleRate,
    const std::vector<SstvAvtToneSegment>& header)
{
    SstvTimingAccumulator timing(sampleRate);
    std::vector<std::uint64_t> boundaries(header.size() + 1U, 0U);
    for (std::size_t index = 0U; index < header.size(); ++index) {
        static_cast<void>(timing.samplesFor(header[index].duration));
        boundaries[index + 1U] = timing.totalSamples();
    }
    return boundaries;
}

SstvAvtEncoder::SstvAvtEncoder(const SstvRgbPixel* pixels,
                               std::size_t count,
                               SstvAvtEncoderConfig config)
    : config_(config)
    , spec_(SstvAvtProtocol::spec(config.mode))
    , pixels_(validatedPixels(pixels, count, config.mode))
    , mapper_({config.mode, config.sampleRate, config.clockErrorPpm})
    , generator_(config.sampleRate, config.headroom)
    , header_(SstvAvtProtocol::normalHeader(config.mode))
    , headerBoundaries_(makeHeaderBoundaries(config.sampleRate, header_))
{
    if (!std::isfinite(config.level)
        || config.level < 0.0 || config.level > MaximumLevel) {
        throw std::invalid_argument("AVT TX level is out of range");
    }
    for (const SstvAvtToneSegment& segment : header_) {
        generator_.validateTone(segment.frequencyHz, config.level);
    }
    generator_.validateTone(SstvAvtProtocol::WhiteFrequencyHz,
                            config.level);
    totalSamples_ = checkedAdd(headerBoundaries_.back(),
                               mapper_.imageSampleCount());
    metrics_.residentImageBytes = pixels_.size() * sizeof(SstvRgbPixel);
    metrics_.residentHeaderBytes =
        header_.size() * sizeof(SstvAvtToneSegment)
        + headerBoundaries_.size() * sizeof(std::uint64_t);
}

SstvAvtEncoder::SstvAvtEncoder(const std::vector<SstvRgbPixel>& pixels,
                               SstvAvtEncoderConfig config)
    : SstvAvtEncoder(pixels.data(), pixels.size(), config)
{
}

std::size_t SstvAvtEncoder::pixelCount(SstvAvtMode mode)
{
    const SstvAvtModeSpec modeSpec = SstvAvtProtocol::spec(mode);
    if (modeSpec.width != 0U
        && modeSpec.height > std::numeric_limits<std::size_t>::max()
            / modeSpec.width) {
        throw std::overflow_error("AVT frame pixel count overflow");
    }
    return static_cast<std::size_t>(modeSpec.width) * modeSpec.height;
}

std::size_t SstvAvtEncoder::headerIndexAt(
    std::uint64_t sample) const noexcept
{
    const auto first = headerBoundaries_.begin() + 1;
    const auto found = std::upper_bound(first,
                                        headerBoundaries_.end(),
                                        sample);
    return static_cast<std::size_t>(found - first);
}

double SstvAvtEncoder::imageFrequency(
    const SstvAvtPosition& position) const
{
    if (position.region != SstvAvtRegion::Pixel) {
        throw std::logic_error("AVT encoder has no tone at this position");
    }
    const std::size_t index = static_cast<std::size_t>(position.line)
        * spec_.width + position.pixel;
    if (index >= pixels_.size()) {
        throw std::logic_error("AVT mapped pixel is outside the frame");
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
        throw std::logic_error("AVT mapped an invalid colour component");
    }
    return SstvAvtProtocol::frequencyForValue(value);
}

SstvAvtEncoderPosition SstvAvtEncoder::position() const
{
    SstvAvtEncoderPosition result;
    result.producedSamples = producedSamples_;
    result.totalSamples = totalSamples_;
    if (cancelled()) {
        result.stage = SstvAvtEncoderStage::Cancelled;
        return result;
    }
    if (complete()) {
        result.stage = SstvAvtEncoderStage::Complete;
        return result;
    }
    if (producedSamples_ < headerBoundaries_.back()) {
        result.stage = SstvAvtEncoderStage::Header;
        result.headerSegment = headerIndexAt(producedSamples_);
        result.frequencyHz = header_[result.headerSegment].frequencyHz;
        return result;
    }
    result.stage = SstvAvtEncoderStage::Image;
    result.image = mapper_.positionAtSample(
        producedSamples_ - headerBoundaries_.back());
    result.frequencyHz = imageFrequency(result.image);
    return result;
}

void SstvAvtEncoder::noteTransition(
    const SstvAvtEncoderPosition& position) noexcept
{
    const bool changed = haveLastSegment_
        && (position.stage != lastPosition_.stage
            || position.headerSegment != lastPosition_.headerSegment
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
std::size_t SstvAvtEncoder::generate(double frequencyHz,
                                     Sample* output,
                                     std::size_t count)
{
    static_assert(std::is_same_v<Sample, float>
                      || std::is_same_v<Sample, std::int16_t>,
                  "unsupported AVT encoder sample type");
    if constexpr (std::is_same_v<Sample, float>) {
        return generator_.generateFloat(
            frequencyHz, config_.level, output, count);
    } else {
        return generator_.generatePcm16(
            frequencyHz, config_.level, output, count);
    }
}

template<typename Sample>
std::size_t SstvAvtEncoder::pull(Sample* output, std::size_t capacity)
{
    if (metrics_.pullCalls != std::numeric_limits<std::uint64_t>::max()) {
        ++metrics_.pullCalls;
    }
    if (capacity > MaximumSamplesPerPull) {
        ++metrics_.rejectedInputCalls;
        ++metrics_.rejectedOversizeCalls;
        throw std::length_error("AVT TX pull exceeds its work bound");
    }
    if (capacity != 0U && output == nullptr) {
        ++metrics_.rejectedInputCalls;
        throw std::invalid_argument("AVT TX output must not be null");
    }
    if (capacity == 0U || complete() || cancelled()) {
        return 0U;
    }

    std::size_t produced = 0U;
    while (produced < capacity && !complete() && !cancelled()) {
        const SstvAvtEncoderPosition current = position();
        std::uint64_t segmentEnd = 0U;
        if (current.stage == SstvAvtEncoderStage::Header) {
            segmentEnd = headerBoundaries_[current.headerSegment + 1U];
        } else if (current.stage == SstvAvtEncoderStage::Image) {
            segmentEnd = checkedAdd(headerBoundaries_.back(),
                                    current.image.segmentEndSample);
        } else {
            break;
        }
        if (segmentEnd <= producedSamples_) {
            throw std::logic_error("AVT TX segment made no progress");
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

std::size_t SstvAvtEncoder::pullFloat(float* output, std::size_t capacity)
{
    return pull(output, capacity);
}

std::size_t SstvAvtEncoder::pullPcm16(std::int16_t* output,
                                      std::size_t capacity)
{
    return pull(output, capacity);
}

SstvAvtMode SstvAvtEncoder::mode() const noexcept
{
    return spec_.mode;
}

std::uint64_t SstvAvtEncoder::totalSamples() const noexcept
{
    return totalSamples_;
}

std::uint64_t SstvAvtEncoder::producedSamples() const noexcept
{
    return producedSamples_;
}

std::uint64_t SstvAvtEncoder::headerSamples() const noexcept
{
    return headerBoundaries_.back();
}

bool SstvAvtEncoder::complete() const noexcept
{
    return producedSamples_ >= totalSamples_;
}

bool SstvAvtEncoder::cancelled() const noexcept
{
    return generator_.cancelled();
}

SstvAvtEncoderMetrics SstvAvtEncoder::metrics() const noexcept
{
    SstvAvtEncoderMetrics result = metrics_;
    result.tone = generator_.metrics();
    return result;
}

void SstvAvtEncoder::cancel() noexcept
{
    generator_.cancel();
}

void SstvAvtEncoder::reset() noexcept
{
    generator_.reset();
    producedSamples_ = 0U;
    const std::size_t imageBytes = pixels_.size() * sizeof(SstvRgbPixel);
    const std::size_t headerBytes =
        header_.size() * sizeof(SstvAvtToneSegment)
        + headerBoundaries_.size() * sizeof(std::uint64_t);
    metrics_ = {};
    metrics_.residentImageBytes = imageBytes;
    metrics_.residentHeaderBytes = headerBytes;
    lastPosition_ = {};
    haveLastSegment_ = false;
}

void SstvAvtDecoder::validateConfig(const SstvAvtDecoderConfig& config)
{
    static_cast<void>(SstvAvtProtocol::spec(config.mode));
    if (config.sampleRate < SstvAvtMapper::MinimumSampleRate
        || config.sampleRate > SstvAvtMapper::MaximumSampleRate) {
        throw std::invalid_argument("unsupported AVT decoder sample rate");
    }
    if (config.observationSpanSamples == 0U
        || config.observationSpanSamples > MaximumObservationSpanSamples) {
        throw std::invalid_argument("invalid AVT observation span");
    }
    if (!std::isfinite(config.frequencyOffsetHz)
        || std::abs(config.frequencyOffsetHz)
            > MaximumAbsoluteFrequencyOffsetHz) {
        throw std::invalid_argument(
            "AVT decoder frequency offset is out of range");
    }
    if (!std::isfinite(config.minimumObservationConfidence)
        || config.minimumObservationConfidence < 0.0
        || config.minimumObservationConfidence > 1.0) {
        throw std::invalid_argument(
            "invalid AVT observation confidence");
    }
    if (config.maximumInterpolationGapPixels
        > MaximumInterpolationGapPixels) {
        throw std::invalid_argument("invalid AVT interpolation bound");
    }
    if (config.maximumPendingDirtyEvents == 0U
        || config.maximumPendingDirtyEvents
            > SstvImageFrame::kMaximumDirtyEvents) {
        throw std::invalid_argument("invalid AVT dirty-event bound");
    }
}

SstvAvtDecoder::SstvAvtDecoder(SstvAvtDecoderConfig config)
    : config_(config)
    , spec_(SstvAvtProtocol::spec(config.mode))
    , mapper_({config.mode, config.sampleRate, config.clockErrorPpm})
    , frame_(std::make_unique<SstvImageFrame>(
          spec_.width, spec_.height, config.maximumPendingDirtyEvents))
    , accumulators_(static_cast<std::size_t>(spec_.width) * 3U)
{
    validateConfig(config);
    if (mapper_.imageSampleCount()
        > std::numeric_limits<std::uint64_t>::max()
            - config.imageStartSample) {
        throw std::overflow_error("AVT image sample range overflow");
    }
    imageEndSample_ = config.imageStartSample + mapper_.imageSampleCount();
}

void SstvAvtDecoder::saturatingAdd(std::uint64_t& value,
                                   std::uint64_t increment) noexcept
{
    value = increment > std::numeric_limits<std::uint64_t>::max() - value
        ? std::numeric_limits<std::uint64_t>::max()
        : value + increment;
}

std::size_t SstvAvtDecoder::componentIndex(ColourComponent component)
{
    switch (component) {
    case ColourComponent::Red:
        return 0U;
    case ColourComponent::Green:
        return 1U;
    case ColourComponent::Blue:
        return 2U;
    default:
        throw std::logic_error("invalid AVT decoder component");
    }
}

SstvImageChannel SstvAvtDecoder::imageChannel(ColourComponent component)
{
    switch (component) {
    case ColourComponent::Red:
        return SstvImageChannel::Red;
    case ColourComponent::Green:
        return SstvImageChannel::Green;
    case ColourComponent::Blue:
        return SstvImageChannel::Blue;
    default:
        throw std::logic_error("invalid AVT image component");
    }
}

bool SstvAvtDecoder::beginLine(std::uint32_t line)
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

void SstvAvtDecoder::accumulate(const SstvAvtPosition& position,
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

bool SstvAvtDecoder::reconstructComponent(
    std::size_t base,
    std::vector<std::uint8_t>& values)
{
    const std::size_t width = spec_.width;
    std::vector<double> frequencies(width,
                                    std::numeric_limits<double>::quiet_NaN());
    for (std::size_t pixel = 0U; pixel < width; ++pixel) {
        const PixelAccumulator& accumulator = accumulators_[base + pixel];
        if (accumulator.count > 0U && accumulator.confidenceWeight > 0.0) {
            frequencies[pixel] = accumulator.weightedFrequencyHz
                / accumulator.confidenceWeight;
        }
    }

    const auto firstKnown = std::find_if(
        frequencies.begin(), frequencies.end(), [](double value) {
            return std::isfinite(value);
        });
    if (firstKnown == frequencies.end()) {
        return false;
    }
    const std::size_t first = static_cast<std::size_t>(
        firstKnown - frequencies.begin());
    if (first > config_.maximumInterpolationGapPixels) {
        return false;
    }
    for (std::size_t pixel = 0U; pixel < first; ++pixel) {
        frequencies[pixel] = *firstKnown;
        saturatingAdd(metrics_.interpolatedPixels);
    }

    std::size_t previous = first;
    for (std::size_t pixel = first + 1U; pixel < width; ++pixel) {
        if (!std::isfinite(frequencies[pixel])) {
            continue;
        }
        const std::size_t missing = pixel - previous - 1U;
        if (missing > config_.maximumInterpolationGapPixels) {
            return false;
        }
        const double left = frequencies[previous];
        const double right = frequencies[pixel];
        for (std::size_t gap = 1U; gap <= missing; ++gap) {
            const double ratio = static_cast<double>(gap)
                / static_cast<double>(missing + 1U);
            frequencies[previous + gap] = left + (right - left) * ratio;
            saturatingAdd(metrics_.interpolatedPixels);
        }
        previous = pixel;
    }
    const std::size_t trailing = width - previous - 1U;
    if (trailing > config_.maximumInterpolationGapPixels) {
        return false;
    }
    for (std::size_t gap = 1U; gap <= trailing; ++gap) {
        frequencies[previous + gap] = frequencies[previous];
        saturatingAdd(metrics_.interpolatedPixels);
    }

    values.resize(width);
    for (std::size_t pixel = 0U; pixel < width; ++pixel) {
        values[pixel] = SstvAvtProtocol::valueForFrequency(
            frequencies[pixel] - config_.frequencyOffsetHz);
    }
    return true;
}

void SstvAvtDecoder::publishCurrentLine()
{
    if (!haveCurrentLine_) {
        return;
    }
    std::vector<std::uint8_t> values;
    std::size_t components = 0U;
    for (const ColourComponent component : {
             ColourComponent::Red,
             ColourComponent::Green,
             ColourComponent::Blue}) {
        const std::size_t base = componentIndex(component) * spec_.width;
        if (!reconstructComponent(base, values)) {
            continue;
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

void SstvAvtDecoder::clearAccumulators() noexcept
{
    std::fill(accumulators_.begin(),
              accumulators_.end(),
              PixelAccumulator {});
    nonEmptyAccumulators_ = 0U;
    refreshBufferMetrics();
}

void SstvAvtDecoder::refreshBufferMetrics() noexcept
{
    metrics_.bufferedPixelAccumulators = nonEmptyAccumulators_;
    metrics_.peakBufferedPixelAccumulators = std::max(
        metrics_.peakBufferedPixelAccumulators, nonEmptyAccumulators_);
}

std::size_t SstvAvtDecoder::consume(
    const SstvFrequencyObservation* observations,
    std::size_t count)
{
    if (count > MaximumObservationsPerConsume) {
        saturatingAdd(metrics_.rejectedInputCalls);
        saturatingAdd(metrics_.rejectedOversizeCalls);
        throw std::length_error("AVT decoder consume exceeds its work bound");
    }
    if (count != 0U && observations == nullptr) {
        saturatingAdd(metrics_.rejectedInputCalls);
        throw std::invalid_argument("AVT observations must not be null");
    }
    if (count == 0U || state_ != SstvAvtDecodeState::Receiving) {
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
        const SstvAvtPosition position = mapper_.positionAtSample(
            observation.centreSample - config_.imageStartSample);
        if (!haveCurrentLine_ || position.line != currentLine_) {
            if (!beginLine(position.line)) {
                saturatingAdd(metrics_.invalidObservations);
                continue;
            }
        }
        if (!position.valid()) {
            saturatingAdd(metrics_.invalidObservations);
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

std::size_t SstvAvtDecoder::consume(
    const std::vector<SstvFrequencyObservation>& observations)
{
    return consume(observations.data(), observations.size());
}

SstvAvtDecodeState SstvAvtDecoder::finish()
{
    if (state_ != SstvAvtDecodeState::Receiving) {
        return state_;
    }
    publishCurrentLine();
    state_ = frame_->isComplete()
        ? SstvAvtDecodeState::Complete
        : SstvAvtDecodeState::Partial;
    return state_;
}

void SstvAvtDecoder::cancel() noexcept
{
    if (state_ == SstvAvtDecodeState::Receiving) {
        state_ = SstvAvtDecodeState::Cancelled;
        frame_->cancel();
        haveCurrentLine_ = false;
        clearAccumulators();
    }
}

void SstvAvtDecoder::reset() noexcept
{
    frame_->reset();
    clearAccumulators();
    metrics_ = {};
    state_ = SstvAvtDecodeState::Receiving;
    currentLine_ = 0U;
    haveCurrentLine_ = false;
    lastObservationSample_ = 0U;
    haveLastObservation_ = false;
}

double SstvAvtDecoder::setFrequencyOffsetHz(double offsetHz)
{
    if (!std::isfinite(offsetHz)
        || std::abs(offsetHz) > MaximumAbsoluteFrequencyOffsetHz) {
        throw std::invalid_argument(
            "AVT decoder frequency offset is out of range");
    }
    config_.frequencyOffsetHz = offsetHz;
    return config_.frequencyOffsetHz;
}

double SstvAvtDecoder::frequencyOffsetHz() const noexcept
{
    return config_.frequencyOffsetHz;
}

SstvAvtMode SstvAvtDecoder::mode() const noexcept
{
    return spec_.mode;
}

SstvAvtDecodeState SstvAvtDecoder::state() const noexcept
{
    return state_;
}

std::uint64_t SstvAvtDecoder::imageEndSample() const noexcept
{
    return imageEndSample_;
}

const SstvImageFrame& SstvAvtDecoder::imageFrame() const noexcept
{
    return *frame_;
}

SstvImageSnapshot SstvAvtDecoder::snapshot() const
{
    return frame_->snapshot();
}

std::vector<SstvDirtyEvent> SstvAvtDecoder::takeDirtyEvents()
{
    return frame_->takeDirtyEvents();
}

SstvAvtDecoderMetrics SstvAvtDecoder::metrics() const noexcept
{
    return metrics_;
}

void SstvAvtCountdownDetector::validateConfig(
    const SstvAvtCountdownDetectorConfig& config)
{
    static_cast<void>(SstvAvtProtocol::spec(config.expectedMode));
    if (config.sampleRate < SstvAvtMapper::MinimumSampleRate
        || config.sampleRate > SstvAvtMapper::MaximumSampleRate) {
        throw std::invalid_argument(
            "unsupported AVT countdown sample rate");
    }
    if (config.observationSpanSamples == 0U
        || config.observationSpanSamples
            > SstvAvtDecoder::MaximumObservationSpanSamples) {
        throw std::invalid_argument(
            "invalid AVT countdown observation span");
    }
    if (!std::isfinite(config.frequencyOffsetHz)
        || std::abs(config.frequencyOffsetHz)
            > SstvAvtDecoder::MaximumAbsoluteFrequencyOffsetHz) {
        throw std::invalid_argument(
            "AVT countdown frequency offset is out of range");
    }
    if (!std::isfinite(config.minimumObservationConfidence)
        || config.minimumObservationConfidence < 0.0
        || config.minimumObservationConfidence > 1.0) {
        throw std::invalid_argument(
            "invalid AVT countdown observation confidence");
    }
    if (!std::isfinite(config.toneToleranceHz)
        || config.toneToleranceHz <= 0.0
        || config.toneToleranceHz > 250.0) {
        throw std::invalid_argument(
            "invalid AVT countdown tone tolerance");
    }
}

SstvAvtCountdownDetector::SstvAvtCountdownDetector(
    SstvAvtCountdownDetectorConfig config)
    : config_(config)
{
    validateConfig(config);
    const std::uint64_t searchDuration = checkedAdd(
        nonNegative(SstvAvtSyncCodec::CountdownDuration),
        checkedMultiply(nonNegative(SstvAvtSyncCodec::FrameDuration), 2U));
    const std::uint64_t searchSamples = samplesFor(
        Picoseconds {static_cast<std::int64_t>(searchDuration)});
    searchEndSample_ = checkedAdd(config.searchStartSample, searchSamples);
}

void SstvAvtCountdownDetector::saturatingAdd(
    std::uint64_t& value,
    std::uint64_t increment) noexcept
{
    value = increment > std::numeric_limits<std::uint64_t>::max() - value
        ? std::numeric_limits<std::uint64_t>::max()
        : value + increment;
}

std::uint64_t SstvAvtCountdownDetector::samplesFor(
    Picoseconds duration) const
{
    const std::uint64_t picoseconds = nonNegative(duration);
    const std::uint64_t divisor =
        static_cast<std::uint64_t>(kPicosecondsPerSecond);
    const std::uint64_t whole = checkedMultiply(
        picoseconds / divisor, config_.sampleRate);
    const QuotientRemainder partial = multiplyAndDivide(
        picoseconds % divisor, config_.sampleRate, divisor);
    return checkedAdd(whole, partial.quotient);
}

bool SstvAvtCountdownDetector::isStartTone(
    const SstvFrequencyObservation& observation) const noexcept
{
    return observation.valid()
        && std::isfinite(observation.correctedFrequencyHz)
        && std::isfinite(observation.confidence)
        && observation.confidence >= config_.minimumObservationConfidence
        && std::abs(observation.correctedFrequencyHz
                    - config_.frequencyOffsetHz
                    - SstvAvtSyncCodec::StartFrequencyHz)
            <= config_.toneToleranceHz;
}

void SstvAvtCountdownDetector::startCandidate(
    const SstvFrequencyObservation& observation)
{
    const std::uint64_t halfSpan = config_.observationSpanSamples / 2U;
    candidateStartSample_ = observation.centreSample > halfSpan
        ? observation.centreSample - halfSpan
        : 0U;
    candidateStartSample_ = std::max(candidateStartSample_,
                                     config_.searchStartSample);
    candidateSlotsPresent_.fill(false);
    candidateDistances_.fill(std::numeric_limits<std::uint64_t>::max());
    candidateFrequencies_.fill(0.0);
    candidateConfidences_.fill(0.0);
    candidateSlotsFilled_ = 0U;
    const std::uint64_t halfSymbolPicoseconds =
        nonNegative(SstvAvtSyncCodec::SymbolDuration) / 2U;
    for (std::size_t index = 0U;
         index < SstvAvtSyncCodec::TonesPerFrame;
         ++index) {
        const std::uint64_t odd = checkedAdd(
            checkedMultiply(static_cast<std::uint64_t>(index), 2U), 1U);
        candidateTargets_[index] = checkedAdd(
            candidateStartSample_,
            samplesFor(Picoseconds {static_cast<std::int64_t>(
                checkedMultiply(halfSymbolPicoseconds, odd))}));
    }
    candidateActive_ = true;
    observeCandidate(observation);
    saturatingAdd(metrics_.candidatesStarted);
    metrics_.bufferedObservations = candidateSlotsFilled_;
    metrics_.peakBufferedObservations = std::max(
        metrics_.peakBufferedObservations, candidateSlotsFilled_);
}

void SstvAvtCountdownDetector::observeCandidate(
    const SstvFrequencyObservation& observation) noexcept
{
    for (std::size_t index = 0U;
         index < SstvAvtSyncCodec::TonesPerFrame;
         ++index) {
        const std::uint64_t target = candidateTargets_[index];
        const std::uint64_t distance = observation.centreSample > target
            ? observation.centreSample - target
            : target - observation.centreSample;
        if (distance >= candidateDistances_[index]) {
            continue;
        }
        if (!candidateSlotsPresent_[index]) {
            candidateSlotsPresent_[index] = true;
            ++candidateSlotsFilled_;
        }
        candidateDistances_[index] = distance;
        candidateFrequencies_[index] = observation.correctedFrequencyHz
            - config_.frequencyOffsetHz;
        candidateConfidences_[index] = observation.confidence;
    }
    metrics_.bufferedObservations = candidateSlotsFilled_;
    metrics_.peakBufferedObservations = std::max(
        metrics_.peakBufferedObservations, candidateSlotsFilled_);
}

std::optional<SstvAvtCountdownDetection>
SstvAvtCountdownDetector::tryDecodeCandidate(
    std::uint64_t observationSample)
{
    if (!candidateActive_) {
        return std::nullopt;
    }
    const std::uint64_t minimumFrameSamples = samplesFor(
        SstvAvtSyncCodec::FrameDuration);
    const std::uint64_t minimumFrameEnd = checkedAdd(
        candidateStartSample_, minimumFrameSamples);
    if (observationSample < minimumFrameEnd) {
        return std::nullopt;
    }

    std::array<double, SstvAvtSyncCodec::TonesPerFrame> tones {};
    double confidence = 1.0;
    const std::uint64_t maximumDistance = std::max<std::uint64_t>(
        static_cast<std::uint64_t>(config_.observationSpanSamples) * 2U,
        samplesFor(SstvAvtSyncCodec::SymbolDuration) / 3U);

    for (std::size_t index = 0U;
         index < SstvAvtSyncCodec::TonesPerFrame;
         ++index) {
        if (!candidateSlotsPresent_[index]
            || candidateDistances_[index] > maximumDistance) {
            rejectCandidate();
            return std::nullopt;
        }
        tones[index] = candidateFrequencies_[index];
        confidence = std::min(confidence,
                              candidateConfidences_[index]);
    }

    const SstvAvtDecodedSyncFrame decoded = SstvAvtSyncCodec::decodeFrame(
        tones, false, config_.toneToleranceHz);
    if (!decoded.valid || !decoded.mode.has_value()
        || *decoded.mode != config_.expectedMode) {
        rejectCandidate();
        return std::nullopt;
    }

    SstvAvtCountdownDetection result;
    result.acquired = true;
    result.mode = *decoded.mode;
    result.counter = decoded.counter;
    result.frameStartSample = candidateStartSample_;
    const std::uint64_t beforeFrame = samplesFor(Picoseconds {
        SstvAvtSyncCodec::FrameDuration.count
        * static_cast<std::int64_t>(decoded.counter)});
    const std::uint64_t throughFrame = samplesFor(Picoseconds {
        SstvAvtSyncCodec::FrameDuration.count
        * static_cast<std::int64_t>(decoded.counter + 1U)});
    const std::uint64_t countdownSamples = samplesFor(
        SstvAvtSyncCodec::CountdownDuration);
    if (throughFrame < beforeFrame || countdownSamples < beforeFrame) {
        throw std::logic_error(
            "AVT countdown sample boundaries are inconsistent");
    }
    result.frameEndSample = checkedAdd(
        candidateStartSample_, throughFrame - beforeFrame);
    result.imageStartSample = checkedAdd(
        candidateStartSample_, countdownSamples - beforeFrame);
    result.confidence = std::min(confidence, decoded.confidence);
    detection_ = result;
    state_ = SstvAvtCountdownDetectorState::Acquired;
    candidateSlotsPresent_.fill(false);
    candidateSlotsFilled_ = 0U;
    candidateActive_ = false;
    metrics_.bufferedObservations = 0U;
    saturatingAdd(metrics_.framesDecoded);
    return result;
}

void SstvAvtCountdownDetector::rejectCandidate() noexcept
{
    candidateSlotsPresent_.fill(false);
    candidateSlotsFilled_ = 0U;
    candidateActive_ = false;
    previousWasStartTone_ = false;
    metrics_.bufferedObservations = 0U;
    saturatingAdd(metrics_.framesRejected);
}

std::optional<SstvAvtCountdownDetection> SstvAvtCountdownDetector::consume(
    const SstvFrequencyObservation* observations,
    std::size_t count)
{
    saturatingAdd(metrics_.consumeCalls);
    if (count > MaximumObservationsPerConsume) {
        saturatingAdd(metrics_.rejectedInputCalls);
        saturatingAdd(metrics_.rejectedOversizeCalls);
        throw std::length_error(
            "AVT countdown consume exceeds its work bound");
    }
    if (count != 0U && observations == nullptr) {
        saturatingAdd(metrics_.rejectedInputCalls);
        throw std::invalid_argument(
            "AVT countdown observations must not be null");
    }
    if (state_ != SstvAvtCountdownDetectorState::Searching
        || count == 0U) {
        return detection_;
    }

    for (std::size_t index = 0U; index < count; ++index) {
        const SstvFrequencyObservation& observation = observations[index];
        saturatingAdd(metrics_.observationInputs);
        if (observation.centreSample < config_.searchStartSample) {
            continue;
        }
        if (observation.centreSample > searchEndSample_) {
            state_ = SstvAvtCountdownDetectorState::Exhausted;
            candidateSlotsPresent_.fill(false);
            candidateSlotsFilled_ = 0U;
            candidateActive_ = false;
            metrics_.bufferedObservations = 0U;
            return std::nullopt;
        }
        if (!observation.valid()
            || !std::isfinite(observation.correctedFrequencyHz)
            || !std::isfinite(observation.confidence)
            || observation.confidence < config_.minimumObservationConfidence) {
            saturatingAdd(metrics_.invalidObservations);
            continue;
        }

        const bool startTone = isStartTone(observation);
        if (!candidateActive_ && startTone && !previousWasStartTone_) {
            startCandidate(observation);
        } else if (candidateActive_) {
            observeCandidate(observation);
            const auto decoded = tryDecodeCandidate(
                observation.centreSample);
            if (decoded.has_value()) {
                return decoded;
            }
            // A rejected frame is normally decided on the first sample of
            // the following 1900 Hz start symbol. Reuse that observation so
            // a corrupt frame does not force the detector to skip its
            // immediate successor.
            if (!candidateActive_ && startTone) {
                startCandidate(observation);
            }
        }
        previousWasStartTone_ = startTone;
    }
    return std::nullopt;
}

std::optional<SstvAvtCountdownDetection> SstvAvtCountdownDetector::consume(
    const std::vector<SstvFrequencyObservation>& observations)
{
    return consume(observations.data(), observations.size());
}

SstvAvtCountdownDetectorState SstvAvtCountdownDetector::finish() noexcept
{
    if (state_ == SstvAvtCountdownDetectorState::Searching) {
        state_ = SstvAvtCountdownDetectorState::Exhausted;
        candidateSlotsPresent_.fill(false);
        candidateSlotsFilled_ = 0U;
        candidateActive_ = false;
        metrics_.bufferedObservations = 0U;
    }
    return state_;
}

void SstvAvtCountdownDetector::cancel() noexcept
{
    if (state_ == SstvAvtCountdownDetectorState::Searching) {
        state_ = SstvAvtCountdownDetectorState::Cancelled;
        candidateSlotsPresent_.fill(false);
        candidateSlotsFilled_ = 0U;
        candidateActive_ = false;
        metrics_.bufferedObservations = 0U;
    }
}

SstvAvtCountdownDetectorState SstvAvtCountdownDetector::state() const noexcept
{
    return state_;
}

std::optional<SstvAvtCountdownDetection>
SstvAvtCountdownDetector::detection() const noexcept
{
    return detection_;
}

SstvAvtCountdownDetectorMetrics
SstvAvtCountdownDetector::metrics() const noexcept
{
    return metrics_;
}

static_assert(SstvAvtProtocol::TripleVisDuration.count
                  == SstvAvtProtocol::StandardVisFrameDuration.count * 3LL,
              "AVT triple VIS duration must contain three full headers");
static_assert(SstvAvtProtocol::HeaderDuration.count
                  == SstvAvtProtocol::TripleVisDuration.count
                      + SstvAvtSyncCodec::CountdownDuration.count,
              "AVT header must be triple VIS plus the exact countdown");

} // namespace decodium::sstv
