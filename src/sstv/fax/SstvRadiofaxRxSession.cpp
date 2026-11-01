// SPDX-License-Identifier: GPL-3.0-or-later
#include "SstvRadiofaxRxSession.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace decodium::sstv {

SstvRadiofaxRxSession::SstvRadiofaxRxSession(SstvFaxConfig config,
                                             std::uint64_t imageStartSample,
                                             double frequencyOffsetHz)
    : mapper_(config), image_(config.width, config.height), imageStart_(imageStartSample),
      imageEnd_(0U), frequencyOffset_(frequencyOffsetHz), sums_(config.width, 0.0),
      weights_(config.width, 0.0)
{
    const auto imageSamples = mapper_.imageEndSample() - mapper_.imageStartSample();
    if (imageSamples > std::numeric_limits<std::uint64_t>::max() - imageStart_
        || !std::isfinite(frequencyOffset_) || std::abs(frequencyOffset_) > 400.0) {
        throw std::invalid_argument("invalid radiofax receiver anchor or offset");
    }
    imageEnd_ = imageStart_ + imageSamples;
}

void SstvRadiofaxRxSession::publishLine()
{
    if (!hasLine_) return;
    for (std::size_t x = 0U; x < sums_.size(); ++x) {
        if (weights_[x] > 0.0) {
            const auto value = SstvRadiofaxMapper::valueForFrequency(sums_[x] / weights_[x]);
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
        const auto pos = mapper_.positionAtSample(
            mapper_.imageStartSample() + (obs.centreSample - imageStart_));
        if (pos.region != SstvFaxRegion::Image) continue;
        if (!hasLine_ || line_ != pos.line) {
            publishLine();
            line_ = pos.line;
            hasLine_ = true;
        }
        sums_[pos.pixel] += (obs.correctedFrequencyHz - frequencyOffset_) * obs.confidence;
        weights_[pos.pixel] += obs.confidence;
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
