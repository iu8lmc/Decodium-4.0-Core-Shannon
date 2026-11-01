// SPDX-License-Identifier: GPL-3.0-or-later
#include "SstvRadiofax.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace decodium::sstv {

SstvRadiofaxMapper::SstvRadiofaxMapper(SstvFaxConfig config) : config_(config)
{
    if ((config.ioc != SstvFaxIoc::Ioc288 && config.ioc != SstvFaxIoc::Ioc576)
        || (config.linesPerMinute != 60U && config.linesPerMinute != 90U
            && config.linesPerMinute != 120U && config.linesPerMinute != 240U)
        || config.sampleRate < 8'000U || config.sampleRate > 384'000U
        || config.width < 1U || config.width > 4'096U
        || config.height < 1U || config.height > 8'192U
        || (config.centeredPhasingWithWhiteEndLine
            && config.phasing != SstvFaxPhasePattern::WhiteFivePercent)
        || (config.phasing != SstvFaxPhasePattern::Symmetric
            && config.phasing != SstvFaxPhasePattern::WhiteFivePercent)) {
        throw std::invalid_argument("invalid radiofax profile");
    }
    // Require at least one sample per display pixel, preventing ambiguous
    // or zero-length sample segments at low rates / high line speeds.
    if (lineBoundary(1U) < config.width) {
        throw std::invalid_argument("radiofax raster exceeds sampling resolution");
    }
    imageStart_ = static_cast<std::uint64_t>(config.sampleRate) * 35U
        + (config.centeredPhasingWithWhiteEndLine ? lineBoundary(1U) : 0U);
    imageEnd_ = imageStart_ + lineBoundary(config.height);
    stopBlackStart_ = imageEnd_ + static_cast<std::uint64_t>(config.sampleRate) * 5U;
    total_ = stopBlackStart_ + static_cast<std::uint64_t>(config.sampleRate) * 10U;
}

std::uint64_t SstvRadiofaxMapper::lineBoundary(std::uint64_t line) const noexcept
{
    return line * config_.sampleRate * 60U / config_.linesPerMinute;
}

std::uint64_t SstvRadiofaxMapper::lineForOffset(std::uint64_t offset) const noexcept
{
    // Inverse of floor-based cumulative boundaries, including a boundary
    // whose mathematical position lies between two samples.
    return ((offset + 1U) * config_.linesPerMinute - 1U)
        / (static_cast<std::uint64_t>(config_.sampleRate) * 60U);
}

std::uint64_t SstvRadiofaxMapper::lineStartSample(std::uint32_t line) const
{
    if (line > config_.height) throw std::out_of_range("radiofax line");
    return imageStart_ + lineBoundary(line);
}

SstvFaxPosition SstvRadiofaxMapper::positionAtSample(std::uint64_t sample) const noexcept
{
    const std::uint64_t rate = config_.sampleRate;
    if (sample < rate * 5U) {
        const std::uint64_t cycles = config_.ioc == SstvFaxIoc::Ioc576 ? 300U : 675U;
        return {SstvFaxRegion::Start, 0U, 0U, ((sample * cycles * 2U / rate) % 2U) != 0U};
    }
    if (sample < imageStart_) {
        const std::uint64_t offset = sample - rate * 5U;
        if (config_.centeredPhasingWithWhiteEndLine && offset >= rate * 30U) {
            return {SstvFaxRegion::Phasing, 0U, 0U, true};
        }
        const std::uint64_t line = lineForOffset(offset);
        const std::uint64_t start = lineBoundary(line);
        const std::uint64_t end = lineBoundary(line + 1U);
        const std::uint64_t within = offset - start;
        const std::uint64_t fraction = config_.phasing == SstvFaxPhasePattern::Symmetric ? 2U : 20U;
        const std::uint64_t length = end - start;
        const bool white = config_.centeredPhasingWithWhiteEndLine
            ? (within * 40U < length || (length - within) * 40U <= length)
            : within * fraction < length;
        return {SstvFaxRegion::Phasing, static_cast<std::uint32_t>(line), 0U, white};
    }
    if (sample < imageEnd_) {
        const std::uint64_t offset = sample - imageStart_;
        const std::uint64_t line = lineForOffset(offset);
        const std::uint64_t start = lineBoundary(line);
        const std::uint64_t length = lineBoundary(line + 1U) - start;
        const std::uint64_t pixel = (offset - start) * config_.width / length;
        return {SstvFaxRegion::Image, static_cast<std::uint32_t>(line),
                static_cast<std::uint32_t>(pixel), false};
    }
    if (sample < stopBlackStart_) {
        return {SstvFaxRegion::StopAlternating, 0U, 0U,
                (((sample - imageEnd_) * 900U / rate) % 2U) != 0U};
    }
    if (sample < total_) return {SstvFaxRegion::StopBlack, 0U, 0U, false};
    return {};
}

std::uint64_t SstvRadiofaxMapper::imageStartSample() const noexcept { return imageStart_; }
std::uint64_t SstvRadiofaxMapper::imageEndSample() const noexcept { return imageEnd_; }
std::uint64_t SstvRadiofaxMapper::totalSamples() const noexcept { return total_; }

double SstvRadiofaxMapper::frequencyForValue(std::uint8_t value) noexcept
{
    return 1500.0 + 800.0 * static_cast<double>(value) / 255.0;
}

std::uint8_t SstvRadiofaxMapper::valueForFrequency(double frequency) noexcept
{
    if (!std::isfinite(frequency) || frequency <= 1500.0) return 0U;
    if (frequency >= 2300.0) return 255U;
    return static_cast<std::uint8_t>(std::lround((frequency - 1500.0) * 255.0 / 800.0));
}

SstvRadiofaxEncoder::SstvRadiofaxEncoder(SstvFaxConfig config,
                                         std::vector<std::uint8_t> pixels)
    : config_(config), mapper_(config), pixels_(std::move(pixels)),
      generator_(config.sampleRate)
{
    if (pixels_.size() != static_cast<std::size_t>(config.width) * config.height) {
        throw std::invalid_argument("radiofax image size mismatch");
    }
}

double SstvRadiofaxEncoder::frequencyAt(std::uint64_t sample) const noexcept
{
    const auto position = mapper_.positionAtSample(sample);
    if (position.region == SstvFaxRegion::Image) {
        return SstvRadiofaxMapper::frequencyForValue(
            pixels_[static_cast<std::size_t>(position.line) * config_.width + position.pixel]);
    }
    return position.white ? 2300.0 : 1500.0;
}

std::size_t SstvRadiofaxEncoder::pullPcm16(std::int16_t* output, std::size_t capacity)
{
    if (capacity > 262'144U) throw std::length_error("radiofax pull too large");
    if (capacity && !output) throw std::invalid_argument("radiofax output is null");
    std::size_t written = 0U;
    while (written < capacity && !complete() && !cancelled()) {
        const double frequency = frequencyAt(produced_);
        std::size_t count = 1U;
        while (count < capacity - written && produced_ + count < mapper_.totalSamples()
               && frequencyAt(produced_ + count) == frequency) {
            ++count;
        }
        const auto generated = generator_.generatePcm16(frequency, 1.0, output + written, count);
        written += generated;
        produced_ += generated;
        if (generated != count) break;
    }
    return written;
}

bool SstvRadiofaxEncoder::complete() const noexcept { return produced_ >= mapper_.totalSamples(); }
bool SstvRadiofaxEncoder::cancelled() const noexcept { return generator_.cancelled(); }
void SstvRadiofaxEncoder::cancel() noexcept { generator_.cancel(); }
void SstvRadiofaxEncoder::reset() noexcept { generator_.reset(); produced_ = 0U; }
std::uint64_t SstvRadiofaxEncoder::totalSamples() const noexcept { return mapper_.totalSamples(); }
std::uint64_t SstvRadiofaxEncoder::producedSamples() const noexcept { return produced_; }
const SstvRadiofaxMapper& SstvRadiofaxEncoder::mapper() const noexcept { return mapper_; }

} // namespace decodium::sstv
