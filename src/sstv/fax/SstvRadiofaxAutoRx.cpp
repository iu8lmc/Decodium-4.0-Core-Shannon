// SPDX-License-Identifier: GPL-3.0-or-later
#include "SstvRadiofaxAutoRx.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace decodium::sstv {

SstvRadiofaxAutoRx::SstvRadiofaxAutoRx(SstvFaxConfig config, double frequencyOffsetHz)
    : config_(config), offset_(frequencyOffsetHz), phase_(config.sampleRate, frequencyOffsetHz), stop_(config.sampleRate, frequencyOffsetHz)
{
    // Validate output and nominal rate up front. Detected speed is validated
    // again when the image session is created.
    static_cast<void>(SstvRadiofaxMapper(config));
    if (static_cast<std::uint64_t>(config.width) * config.height > SstvImageFrame::kMaximumPixels)
        throw std::invalid_argument("radiofax automatic RX image exceeds frame bound");
}

std::size_t SstvRadiofaxAutoRx::maximumBufferedObservations() const noexcept
{
    return static_cast<std::size_t>(config_.sampleRate) * 3U + 8192U;
}

void SstvRadiofaxAutoRx::buffer(const SstvFrequencyObservation& obs)
{
    const std::uint64_t span = static_cast<std::uint64_t>(config_.sampleRate) * 2U;
    while (!replay_.empty() && obs.centreSample - replay_.front().centreSample > span)
        replay_.pop_front();
    if (replay_.size() >= maximumBufferedObservations()) {
        state_ = SstvRadiofaxAutoRxState::Failed;
        replay_.clear();
        return;
    }
    replay_.push_back(obs);
}

void SstvRadiofaxAutoRx::beginImage(std::uint64_t sample)
{
    config_.linesPerMinute = lock_->linesPerMinute;
    config_.phasing = lock_->pattern;
    try {
        receiver_ = std::make_unique<SstvRadiofaxRxSession>(config_, sample, offset_);
    } catch (const std::invalid_argument&) {
        state_ = SstvRadiofaxAutoRxState::Failed;
        replay_.clear();
        return;
    }
    imageStart_ = sample;
    state_ = SstvRadiofaxAutoRxState::Receiving;
    while (!replay_.empty()) {
        std::vector<SstvFrequencyObservation> part;
        const auto count = std::min(replay_.size(), SstvRadiofaxRxSession::MaximumObservationsPerConsume);
        part.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            part.push_back(replay_.front());
            replay_.pop_front();
        }
        for (const auto& observation : part) {
            if (state_ != SstvRadiofaxAutoRxState::Receiving) break;
            consumeImageObservation(observation);
        }
    }
    if (state_ == SstvRadiofaxAutoRxState::Receiving) updateRxState();
}

void SstvRadiofaxAutoRx::flushPending(std::optional<std::uint64_t> before)
{
    while (!pendingImage_.empty()) {
        const auto observation = pendingImage_.front();
        pendingImage_.pop_front();
        if (!before || observation.centreSample < *before)
            receiver_->consume(&observation, 1U);
    }
}

std::uint64_t SstvRadiofaxAutoRx::refineStopBoundary(std::uint64_t candidate) const
{
    // Backtrack short windows while they still carry the 450 Hz pattern, then
    // align to the nearest raster boundary. Radiofax ends on a full line;
    // this prevents an otherwise up-to-one-second stop stripe in the image.
    const std::uint64_t window = std::max<std::uint64_t>(1U, config_.sampleRate / 20U);
    std::uint64_t boundary = candidate;
    const std::uint64_t lower = candidate > config_.sampleRate
        ? std::max(*imageStart_, candidate - config_.sampleRate) : *imageStart_;
    for (unsigned i = 0; i < 20U && boundary >= lower + window; ++i) {
        const auto begin = boundary - window;
        double re = 0.0, im = 0.0, amplitude = 0.0, mean = 0.0;
        std::size_t total = 0U, valid = 0U;
        for (const auto& obs : pendingImage_) {
            if (obs.centreSample < begin) continue;
            if (obs.centreSample >= boundary) break;
            ++total;
            if (!obs.valid() || !std::isfinite(obs.correctedFrequencyHz)
                || !std::isfinite(obs.confidence) || obs.confidence < 0.2) continue;
            const double value = obs.correctedFrequencyHz - offset_ - 1900.0;
            const auto phaseIndex = (obs.centreSample % config_.sampleRate) * 450U % config_.sampleRate;
            const double phase = 2.0 * std::acos(-1.0) * double(phaseIndex) / config_.sampleRate;
            re += value * std::cos(phase); im += value * std::sin(phase);
            amplitude += std::abs(value); mean += value; ++valid;
        }
        if (total < 10U || valid * 5U < total * 3U
            || amplitude < 100.0 * double(valid) || std::abs(mean) > 100.0 * double(valid)
            || 2.0 * std::hypot(re, im) < 0.85 * amplitude) break;
        boundary = begin;
    }
    const std::uint64_t numerator = (boundary - *imageStart_) * config_.linesPerMinute;
    const std::uint64_t denominator = std::uint64_t(config_.sampleRate) * 60U;
    const std::uint64_t nearestLine = (numerator + denominator / 2U) / denominator;
    return *imageStart_ + nearestLine * denominator / config_.linesPerMinute;
}

void SstvRadiofaxAutoRx::consumeImageObservation(const SstvFrequencyObservation& obs)
{
    if (obs.centreSample < *imageStart_) return;
    if (pendingImage_.size() >= maximumBufferedObservations()) {
        state_ = SstvRadiofaxAutoRxState::Failed;
        pendingImage_.clear();
        return;
    }
    pendingImage_.push_back(obs);
    const auto stop = stop_.consume(&obs, 1U);
    if (stop) {
        stopSample_ = refineStopBoundary(*stop);
        flushPending(stopSample_);
        receiver_->finish();
        updateRxState();
        return;
    }
    const auto holdSamples = static_cast<std::uint64_t>(config_.sampleRate) * 3U;
    while (!pendingImage_.empty()
           && obs.centreSample - pendingImage_.front().centreSample > holdSamples) {
        const auto oldest = pendingImage_.front();
        pendingImage_.pop_front();
        receiver_->consume(&oldest, 1U);
        if (receiver_->state() != SstvRadiofaxRxState::Receiving) {
            pendingImage_.clear();
            updateRxState();
            return;
        }
    }
}

void SstvRadiofaxAutoRx::consume(const SstvFrequencyObservation* data, std::size_t count)
{
    if (count > SstvRadiofaxRxSession::MaximumObservationsPerConsume)
        throw std::length_error("radiofax automatic RX observation bound");
    if (count && !data) throw std::invalid_argument("null radiofax automatic RX observations");
    auto last = lastSample_;
    bool have = hasLast_;
    for (std::size_t i = 0; i < count; ++i) {
        if (have && data[i].centreSample < last)
            throw std::invalid_argument("radiofax automatic RX timestamp regression");
        last = data[i].centreSample;
        have = true;
    }
    for (std::size_t i = 0; i < count; ++i) {
        if (state_ == SstvRadiofaxAutoRxState::Complete || state_ == SstvRadiofaxAutoRxState::Partial
            || state_ == SstvRadiofaxAutoRxState::Cancelled || state_ == SstvRadiofaxAutoRxState::Failed) break;
        const auto& obs = data[i];
        lastSample_ = obs.centreSample;
        hasLast_ = true;
        if (state_ == SstvRadiofaxAutoRxState::SearchingPhasing) {
            lock_ = phase_.consume(&obs, 1U);
            if (!lock_) continue;
            start_ = std::make_unique<SstvRadiofaxImageStartDetector>(config_.sampleRate, *lock_, offset_);
            state_ = SstvRadiofaxAutoRxState::SearchingImage;
        }
        if (state_ == SstvRadiofaxAutoRxState::SearchingImage) {
            buffer(obs);
            if (state_ == SstvRadiofaxAutoRxState::Failed) break;
            const auto candidate = start_->consume(&obs, 1U);
            if (!start_->valid()) {
                replay_.clear();
                state_ = SstvRadiofaxAutoRxState::Failed;
            } else if (candidate) {
                std::uint64_t imageStart = *candidate;
                if (config_.centeredPhasingWithWhiteEndLine
                    && lock_->pattern == SstvFaxPhasePattern::WhiteFivePercent) {
                    const std::uint64_t lineSamples =
                        std::uint64_t(config_.sampleRate) * 60U / lock_->linesPerMinute;
                    imageStart += lineSamples + lineSamples / 40U;
                }
                beginImage(imageStart);
            }
        } else if (state_ == SstvRadiofaxAutoRxState::Receiving) {
            consumeImageObservation(obs);
        }
    }
}

void SstvRadiofaxAutoRx::consume(const std::vector<SstvFrequencyObservation>& data)
{
    consume(data.data(), data.size());
}

bool SstvRadiofaxAutoRx::stopAtAudioSample(std::uint64_t sample)
{
    if (state_ != SstvRadiofaxAutoRxState::Receiving || !receiver_
        || !imageStart_ || sample < *imageStart_ || !hasLast_ || sample > lastSample_)
        return false;
    const std::uint64_t denominator = std::uint64_t(config_.sampleRate) * 60U;
    const std::uint64_t line = ((sample - *imageStart_) * config_.linesPerMinute
                                + denominator / 2U) / denominator;
    stopSample_ = *imageStart_ + line * denominator / config_.linesPerMinute;
    flushPending(stopSample_);
    receiver_->finish();
    updateRxState();
    return true;
}

void SstvRadiofaxAutoRx::updateRxState() noexcept
{
    if (!receiver_) return;
    switch (receiver_->state()) {
    case SstvRadiofaxRxState::Receiving: state_ = SstvRadiofaxAutoRxState::Receiving; break;
    case SstvRadiofaxRxState::Complete: state_ = SstvRadiofaxAutoRxState::Complete; break;
    case SstvRadiofaxRxState::Partial: state_ = SstvRadiofaxAutoRxState::Partial; break;
    case SstvRadiofaxRxState::Cancelled: state_ = SstvRadiofaxAutoRxState::Cancelled; break;
    }
}

SstvRadiofaxAutoRxState SstvRadiofaxAutoRx::finish()
{
    if (receiver_ && state_ == SstvRadiofaxAutoRxState::Receiving) {
        flushPending(); receiver_->finish(); updateRxState();
    }
    else if (state_ == SstvRadiofaxAutoRxState::SearchingPhasing
             || state_ == SstvRadiofaxAutoRxState::SearchingImage) {
        replay_.clear();
        state_ = SstvRadiofaxAutoRxState::Partial;
    }
    return state_;
}
SstvRadiofaxAutoRxState SstvRadiofaxAutoRx::notifyDiscontinuity() { return finish(); }
void SstvRadiofaxAutoRx::cancel() noexcept
{
    if (receiver_) { pendingImage_.clear(); receiver_->cancel(); updateRxState(); }
    else if (state_ == SstvRadiofaxAutoRxState::SearchingPhasing
             || state_ == SstvRadiofaxAutoRxState::SearchingImage) {
        replay_.clear(); state_ = SstvRadiofaxAutoRxState::Cancelled;
    }
}
SstvRadiofaxAutoRxState SstvRadiofaxAutoRx::state() const noexcept { return state_; }
std::optional<std::uint64_t> SstvRadiofaxAutoRx::imageStartSample() const noexcept { return imageStart_; }
std::optional<SstvRadiofaxPhaseLock> SstvRadiofaxAutoRx::phasing() const noexcept { return lock_; }
std::optional<std::uint64_t> SstvRadiofaxAutoRx::stopSample() const noexcept { return stopSample_; }
std::size_t SstvRadiofaxAutoRx::bufferedObservations() const noexcept { return replay_.size() + pendingImage_.size(); }
std::optional<SstvImageSnapshot> SstvRadiofaxAutoRx::snapshot() const
{
    return receiver_ ? std::optional<SstvImageSnapshot>(receiver_->snapshot()) : std::nullopt;
}
std::vector<SstvDirtyEvent> SstvRadiofaxAutoRx::takeDirtyEvents()
{
    return receiver_ ? receiver_->takeDirtyEvents() : std::vector<SstvDirtyEvent> {};
}

} // namespace decodium::sstv
