// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvNarrowVisDetector.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace decodium::sstv {
namespace {

constexpr std::uint64_t kLeaderDurationUs = 300'000U;
constexpr std::uint64_t kGuardDurationUs = 100'000U;
constexpr std::uint64_t kSymbolDurationUs = 22'000U;

std::uint64_t cappedEnd(std::uint64_t start,
                        std::uint64_t duration) noexcept
{
    return duration > std::numeric_limits<std::uint64_t>::max() - start
        ? std::numeric_limits<std::uint64_t>::max()
        : start + duration;
}

} // namespace

void SstvNarrowVisDetector::validateConfig(
    const SstvNarrowVisDetectorConfig& config)
{
    if (!std::isfinite(config.frequencyToleranceHz)
        || config.frequencyToleranceHz <= 0.0
        || config.frequencyToleranceHz >= 100.0
        || !std::isfinite(config.durationTolerance)
        || config.durationTolerance < 0.05
        || config.durationTolerance > 0.50
        || !std::isfinite(config.minimumConfidence)
        || config.minimumConfidence < 0.0
        || config.minimumConfidence > 1.0
        || config.maximumGapUs > kSymbolDurationUs) {
        throw std::invalid_argument("invalid narrow VIS detector config");
    }
}

SstvNarrowVisDetector::SstvNarrowVisDetector(
    SstvNarrowVisDetectorConfig config)
    : config_(config)
{
    validateConfig(config_);
}

void SstvNarrowVisDetector::saturatingAdd(
    std::uint64_t& value,
    std::uint64_t increment) noexcept
{
    value = increment > std::numeric_limits<std::uint64_t>::max() - value
        ? std::numeric_limits<std::uint64_t>::max()
        : value + increment;
}

SstvNarrowVisDetector::Tone SstvNarrowVisDetector::classify(
    double frequencyHz) const noexcept
{
    if (!std::isfinite(frequencyHz)) {
        return Tone::Unknown;
    }
    const double one = std::abs(
        frequencyHz - SstvNarrowVisCodec::OneFrequencyHz);
    const double zero = std::abs(
        frequencyHz - SstvNarrowVisCodec::ZeroFrequencyHz);
    if (one <= config_.frequencyToleranceHz && one < zero) {
        return Tone::One;
    }
    if (zero <= config_.frequencyToleranceHz && zero < one) {
        return Tone::Zero;
    }
    return Tone::Unknown;
}

bool SstvNarrowVisDetector::durationMatches(
    std::uint64_t actual,
    std::uint64_t nominal) const noexcept
{
    const long double tolerance = static_cast<long double>(nominal)
        * config_.durationTolerance;
    return std::abs(static_cast<long double>(actual)
                    - static_cast<long double>(nominal))
        <= tolerance;
}

std::optional<std::size_t> SstvNarrowVisDetector::symbolRunLength(
    std::uint64_t durationUs) const noexcept
{
    const long double ratio = static_cast<long double>(durationUs)
        / static_cast<long double>(kSymbolDurationUs);
    const long double rounded = std::round(ratio);
    if (!std::isfinite(ratio) || rounded < 1.0L
        || rounded > static_cast<long double>(SymbolCount)) {
        return std::nullopt;
    }
    const auto count = static_cast<std::size_t>(rounded);
    const std::uint64_t nominal =
        static_cast<std::uint64_t>(count) * kSymbolDurationUs;
    return durationMatches(durationUs, nominal)
        ? std::optional<std::size_t>(count) : std::nullopt;
}

void SstvNarrowVisDetector::resetFrame() noexcept
{
    state_ = SstvNarrowVisDetectorState::SearchingLeader;
    frameStartedAtUs_ = 0U;
    expectedNextUs_ = 0U;
    symbols_.clear();
    confidence_ = 1.0;
    offsetTimeSum_ = 0.0L;
    offsetDurationUs_ = 0U;
}

void SstvNarrowVisDetector::reject(
    std::vector<SstvNarrowVisDetection>& output,
    std::uint64_t endedAtUs)
{
    SstvNarrowVisDetection result;
    result.status = SstvNarrowVisDetectionStatus::Rejected;
    result.frameStartedAtUs = frameStartedAtUs_;
    result.frameEndedAtUs = endedAtUs;
    result.confidence = confidence_;
    output.push_back(std::move(result));
    saturatingAdd(metrics_.framesRejected);
    resetFrame();
}

void SstvNarrowVisDetector::complete(
    std::vector<SstvNarrowVisDetection>& output,
    std::uint64_t endedAtUs)
{
    std::array<double, SstvNarrowVisCodec::ToneCount> frequencies {};
    frequencies[0U] = SstvNarrowVisCodec::OneFrequencyHz;
    frequencies[1U] = SstvNarrowVisCodec::ZeroFrequencyHz;
    frequencies[2U] = SstvNarrowVisCodec::OneFrequencyHz;
    for (std::size_t bit = 0U; bit < SstvNarrowVisCodec::DataBitCount;
         ++bit) {
        frequencies[bit + 3U] = symbols_[bit + 1U]
            ? SstvNarrowVisCodec::OneFrequencyHz
            : SstvNarrowVisCodec::ZeroFrequencyHz;
    }

    SstvNarrowVisDetection result;
    result.codecResult = SstvNarrowVisCodec::decode(frequencies);
    result.status = result.codecResult.valid
        ? SstvNarrowVisDetectionStatus::Decoded
        : SstvNarrowVisDetectionStatus::Rejected;
    result.frameStartedAtUs = frameStartedAtUs_;
    result.frameEndedAtUs = endedAtUs;
    result.confidence = std::min(confidence_, result.codecResult.confidence);
    result.codecResult.confidence = result.confidence;
    result.estimatedFrequencyOffsetHz = offsetDurationUs_ == 0U
        ? 0.0
        : static_cast<double>(
            offsetTimeSum_ / static_cast<long double>(offsetDurationUs_));
    if (result.valid()) {
        saturatingAdd(metrics_.framesDecoded);
    } else {
        saturatingAdd(metrics_.framesRejected);
    }
    output.push_back(std::move(result));
    resetFrame();
}

std::vector<SstvNarrowVisDetection> SstvNarrowVisDetector::consume(
    const SstvNarrowVisToneEvent* events,
    std::size_t count)
{
    if (count > MaximumEventsPerConsume) {
        throw std::length_error("narrow VIS event batch exceeds bound");
    }
    if (count != 0U && events == nullptr) {
        throw std::invalid_argument("narrow VIS events must not be null");
    }
    if (state_ == SstvNarrowVisDetectorState::Cancelled) {
        return {};
    }

    std::vector<SstvNarrowVisDetection> output;
    output.reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
        const SstvNarrowVisToneEvent& event = events[index];
        saturatingAdd(metrics_.eventsConsumed);
        const std::uint64_t end = cappedEnd(
            event.startTimeUs, event.durationUs);
        if (event.durationUs == 0U || end <= event.startTimeUs
            || !std::isfinite(event.frequencyHz)
            || !std::isfinite(event.confidence)
            || event.confidence < 0.0 || event.confidence > 1.0
            || (haveTimeline_ && event.startTimeUs < lastEventEndUs_)) {
            saturatingAdd(metrics_.invalidInputs);
            SstvNarrowVisDetection invalid;
            invalid.status = SstvNarrowVisDetectionStatus::InvalidInput;
            invalid.frameStartedAtUs = frameStartedAtUs_;
            invalid.frameEndedAtUs = haveTimeline_
                ? lastEventEndUs_ : event.startTimeUs;
            output.push_back(std::move(invalid));
            resetFrame();
            continue;
        }
        haveTimeline_ = true;
        lastEventEndUs_ = end;
        const Tone tone = classify(event.frequencyHz);

        if (state_ == SstvNarrowVisDetectorState::SearchingLeader) {
            if (tone == Tone::One
                && event.confidence >= config_.minimumConfidence
                && durationMatches(event.durationUs, kLeaderDurationUs)) {
                frameStartedAtUs_ = event.startTimeUs;
                expectedNextUs_ = end;
                confidence_ = event.confidence;
                offsetTimeSum_ = static_cast<long double>(
                    event.frequencyHz
                    - SstvNarrowVisCodec::OneFrequencyHz)
                    * static_cast<long double>(event.durationUs);
                offsetDurationUs_ = event.durationUs;
                state_ = SstvNarrowVisDetectorState::AwaitingGuard;
                saturatingAdd(metrics_.framesStarted);
            }
            continue;
        }

        if (event.startTimeUs > expectedNextUs_
            && event.startTimeUs - expectedNextUs_
                > config_.maximumGapUs) {
            reject(output, event.startTimeUs);
            // The current event may itself be the next leader.
            if (tone == Tone::One
                && event.confidence >= config_.minimumConfidence
                && durationMatches(event.durationUs, kLeaderDurationUs)) {
                frameStartedAtUs_ = event.startTimeUs;
                expectedNextUs_ = end;
                confidence_ = event.confidence;
                offsetTimeSum_ = static_cast<long double>(
                    event.frequencyHz
                    - SstvNarrowVisCodec::OneFrequencyHz)
                    * static_cast<long double>(event.durationUs);
                offsetDurationUs_ = event.durationUs;
                state_ = SstvNarrowVisDetectorState::AwaitingGuard;
                saturatingAdd(metrics_.framesStarted);
            }
            continue;
        }

        confidence_ = std::min(confidence_, event.confidence);
        if (state_ == SstvNarrowVisDetectorState::AwaitingGuard) {
            if (tone != Tone::Zero
                || event.confidence < config_.minimumConfidence
                || !durationMatches(event.durationUs, kGuardDurationUs)) {
                reject(output, end);
                continue;
            }
            offsetTimeSum_ += static_cast<long double>(
                event.frequencyHz - SstvNarrowVisCodec::ZeroFrequencyHz)
                * static_cast<long double>(event.durationUs);
            offsetDurationUs_ = cappedEnd(
                offsetDurationUs_, event.durationUs);
            expectedNextUs_ = end;
            state_ = SstvNarrowVisDetectorState::ReadingSymbols;
            continue;
        }

        if (tone == Tone::Unknown
            || event.confidence < config_.minimumConfidence) {
            reject(output, end);
            continue;
        }
        const auto runLength = symbolRunLength(event.durationUs);
        if (!runLength.has_value()
            || *runLength > SymbolCount - symbols_.size()) {
            reject(output, end);
            continue;
        }
        const bool one = tone == Tone::One;
        symbols_.insert(symbols_.end(), *runLength, one);
        const double nominal = one
            ? SstvNarrowVisCodec::OneFrequencyHz
            : SstvNarrowVisCodec::ZeroFrequencyHz;
        offsetTimeSum_ += static_cast<long double>(
            event.frequencyHz - nominal)
            * static_cast<long double>(event.durationUs);
        offsetDurationUs_ = cappedEnd(offsetDurationUs_, event.durationUs);
        expectedNextUs_ = end;
        if (!symbols_.empty() && !symbols_.front()) {
            reject(output, end);
            continue;
        }
        if (symbols_.size() == SymbolCount) {
            complete(output, end);
        }
    }
    return output;
}

std::vector<SstvNarrowVisDetection> SstvNarrowVisDetector::consume(
    const std::vector<SstvNarrowVisToneEvent>& events)
{
    return consume(events.data(), events.size());
}

std::optional<SstvNarrowVisDetection> SstvNarrowVisDetector::finish(
    std::uint64_t nowUs)
{
    if (state_ == SstvNarrowVisDetectorState::SearchingLeader
        || state_ == SstvNarrowVisDetectorState::Cancelled) {
        return std::nullopt;
    }
    SstvNarrowVisDetection result;
    result.status = SstvNarrowVisDetectionStatus::Truncated;
    result.frameStartedAtUs = frameStartedAtUs_;
    result.frameEndedAtUs = std::max(nowUs, lastEventEndUs_);
    result.confidence = confidence_;
    resetFrame();
    return result;
}

std::optional<SstvNarrowVisDetection> SstvNarrowVisDetector::cancel(
    std::uint64_t nowUs)
{
    SstvNarrowVisDetection result;
    result.status = SstvNarrowVisDetectionStatus::Cancelled;
    result.frameStartedAtUs = frameStartedAtUs_;
    result.frameEndedAtUs = std::max(nowUs, lastEventEndUs_);
    result.confidence = confidence_;
    state_ = SstvNarrowVisDetectorState::Cancelled;
    symbols_.clear();
    return result;
}

void SstvNarrowVisDetector::reset(bool clearMetrics) noexcept
{
    if (clearMetrics) {
        metrics_ = {};
    }
    resetFrame();
    haveTimeline_ = false;
    lastEventEndUs_ = 0U;
}

SstvNarrowVisDetectorState SstvNarrowVisDetector::state() const noexcept
{
    return state_;
}

const SstvNarrowVisDetectorMetrics&
SstvNarrowVisDetector::metrics() const noexcept
{
    return metrics_;
}

} // namespace decodium::sstv
