// SPDX-License-Identifier: GPL-3.0-or-later
#include "SstvRadiofaxRxSession.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace decodium::sstv {

SstvRadiofaxRxSession::SstvRadiofaxRxSession(SstvFaxConfig config,
                                             std::uint64_t imageStartSample,
                                             double frequencyOffsetHz,
                                             std::optional<SstvRadiofaxClockEstimate> clock,
                                             SstvRadiofaxLevels levels)
    : mapper_(config), image_(config.width, config.height), imageStart_(imageStartSample),
      imageEnd_(0U), lineSamples_(std::uint64_t(config.sampleRate) * 60U),
      lineCount_(config.linesPerMinute), wmoDeadSector_(config.wmoDeadSector), frequencyOffset_(frequencyOffsetHz), levels_(levels), sums_(config.width, 0.0),
      weights_(config.width, 0.0)
{
    if (clock) {
        const double nominal = double(config.sampleRate) * 60.0 / config.linesPerMinute;
        if (!clock->lines || clock->lines > 14'400U || !clock->samples
            || clock->samples > std::uint64_t(config.sampleRate) * 3600U
            || clock->samples / clock->lines
                - (config.wmoDeadSector ? sstvRadiofaxDeadSectorSamples(clock->samples / clock->lines) : 0U) < config.width
            || std::abs(double(clock->samples) / clock->lines - nominal) > nominal * kRadiofaxMaximumClockDeviation)
            throw std::invalid_argument("invalid radiofax receive clock estimate");
        lineSamples_ = clock->samples;
        lineCount_ = clock->lines;
    }
    const auto imageSamples = std::uint64_t(config.height) * lineSamples_ / lineCount_;
    if (imageSamples > std::numeric_limits<std::uint64_t>::max() - imageStart_
        || !std::isfinite(frequencyOffset_) || std::abs(frequencyOffset_) > 400.0
        || !std::isfinite(levels_.blackHz) || !std::isfinite(levels_.whiteHz)
        || std::abs(levels_.blackHz - 1500.0) > 50.0
        || std::abs(levels_.whiteHz - 2300.0) > 50.0
        || levels_.whiteHz - levels_.blackHz < 760.0
        || levels_.whiteHz - levels_.blackHz > 840.0) {
        throw std::invalid_argument("invalid radiofax receiver anchor or offset");
    }
    imageEnd_ = imageStart_ + imageSamples;
}

void SstvRadiofaxRxSession::publishLine()
{
    if (!hasLine_) return;
    for (std::size_t x = 0U; x < sums_.size(); ++x) {
        if (weights_[x] > 0.0) {
            const double measured = sums_[x] / weights_[x];
            const auto value = SstvRadiofaxMapper::valueForFrequency(
                1500.0 + (measured - levels_.blackHz) * 800.0 / (levels_.whiteHz - levels_.blackHz));
            image_.writeChannel(static_cast<std::uint32_t>(x), line_,
                                SstvImageChannel::Grayscale, value);
        }
    }
    std::fill(sums_.begin(), sums_.end(), 0.0);
    std::fill(weights_.begin(), weights_.end(), 0.0);
    hasLine_ = false;
}

std::size_t SstvRadiofaxRxSession::consume(const SstvFrequencyObservation* observations,
                                           std::size_t count)
{
    if (count > MaximumObservationsPerConsume) throw std::length_error("radiofax observation bound");
    if (count && !observations) throw std::invalid_argument("null radiofax observations");
    if (state_ != SstvRadiofaxRxState::Receiving) return 0U;
    // Reject a whole non-chronological batch before it can publish any pixels.
    std::uint64_t previous = lastSample_;
    bool havePrevious = hasLastSample_;
    for (std::size_t i = 0; i < count; ++i) {
        if (havePrevious && observations[i].centreSample < previous)
            throw std::invalid_argument("radiofax observation timestamp regression");
        previous = observations[i].centreSample;
        havePrevious = true;
    }
    std::size_t accepted = 0U;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& obs = observations[i];
        lastSample_ = obs.centreSample;
        hasLastSample_ = true;
        if (obs.centreSample < imageStart_) continue;
        if (obs.centreSample >= imageEnd_) { finish(); break; }
        if (!obs.valid() || !std::isfinite(obs.correctedFrequencyHz)
            || !std::isfinite(obs.confidence) || obs.confidence <= 0.0
            || obs.confidence > 1.0) continue;
        const auto offset = obs.centreSample - imageStart_;
        const auto line = ((offset + 1U) * lineCount_ - 1U) / lineSamples_;
        const auto start = line * lineSamples_ / lineCount_;
        const auto end = (line + 1U) * lineSamples_ / lineCount_;
        const auto dead = wmoDeadSector_ ? sstvRadiofaxDeadSectorSamples(end - start) : 0U;
        if (offset - start < dead) continue;
        const auto pixel = (offset - start - dead) * sums_.size() / (end - start - dead);
        if (!hasLine_ || line_ != line) {
            publishLine();
            line_ = static_cast<std::uint32_t>(line);
            hasLine_ = true;
        }
        sums_[pixel] += (obs.correctedFrequencyHz - frequencyOffset_) * obs.confidence;
        weights_[pixel] += obs.confidence;
        ++accepted;
    }
    return accepted;
}

std::size_t SstvRadiofaxRxSession::consume(const std::vector<SstvFrequencyObservation>& observations)
{
    return consume(observations.data(), observations.size());
}

SstvRadiofaxRxState SstvRadiofaxRxSession::finish()
{
    if (state_ == SstvRadiofaxRxState::Receiving) {
        publishLine();
        state_ = image_.isComplete() ? SstvRadiofaxRxState::Complete : SstvRadiofaxRxState::Partial;
    }
    return state_;
}

SstvRadiofaxRxState SstvRadiofaxRxSession::notifyDiscontinuity()
{
    return finish();
}

void SstvRadiofaxRxSession::cancel() noexcept
{
    if (state_ == SstvRadiofaxRxState::Receiving) {
        image_.cancel();
        state_ = SstvRadiofaxRxState::Cancelled;
    }
}
SstvRadiofaxRxState SstvRadiofaxRxSession::state() const noexcept { return state_; }
SstvImageSnapshot SstvRadiofaxRxSession::snapshot() const { return image_.snapshot(); }
std::vector<SstvDirtyEvent> SstvRadiofaxRxSession::takeDirtyEvents() { return image_.takeDirtyEvents(); }

} // namespace decodium::sstv
