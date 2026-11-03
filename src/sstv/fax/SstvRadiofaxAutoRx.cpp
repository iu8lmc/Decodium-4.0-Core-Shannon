// SPDX-License-Identifier: GPL-3.0-or-later
#include "SstvRadiofaxAutoRx.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace decodium::sstv {

SstvRadiofaxAutoRx::SstvRadiofaxAutoRx(SstvFaxConfig config, double frequencyOffsetHz)
    : config_(config), offset_(frequencyOffsetHz), phase_(config.sampleRate, frequencyOffsetHz),
      aptStart_(config.sampleRate, config.ioc, frequencyOffsetHz), stop_(config.sampleRate, frequencyOffsetHz)
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

void SstvRadiofaxAutoRx::collectPhasingLevel(const SstvFrequencyObservation& obs) noexcept
{
    if (!obs.valid() || !std::isfinite(obs.correctedFrequencyHz)
        || !std::isfinite(obs.confidence) || obs.confidence < 0.7 || obs.confidence > 1.0)
        return;
    const double f = obs.correctedFrequencyHz - offset_;
    if (f >= 1450.0 && f <= 1550.0)
        ++blackLevels_[static_cast<std::size_t>(std::lround(f - 1450.0))];
    else if (f >= 2250.0 && f <= 2350.0)
        ++whiteLevels_[static_cast<std::size_t>(std::lround(f - 2250.0))];
}

SstvRadiofaxLevels SstvRadiofaxAutoRx::measuredLevels() const noexcept
{
    if (!clockTracker_ || !clockTracker_->estimate()) return {};
    const auto median = [](const auto& bins) -> std::optional<std::size_t> {
        std::uint64_t total = 0U;
        for (auto count : bins) total += count;
        if (total < 64U) return std::nullopt;
        std::uint64_t cumulative = 0U;
        for (std::size_t i = 0; i < bins.size(); ++i) {
            cumulative += bins[i];
            if (cumulative >= (total + 1U) / 2U) {
                std::uint64_t nearby = 0U;
                for (std::size_t j = i > 5U ? i - 5U : 0U;
                     j < bins.size() && j <= i + 5U; ++j) nearby += bins[j];
                if (nearby * 2U < total) return std::nullopt;
                return i;
            }
        }
        return std::nullopt;
    };
    const auto black = median(blackLevels_);
    const auto white = median(whiteLevels_);
    if (!black || !white) return {};
    const SstvRadiofaxLevels levels {1450.0 + double(*black), 2250.0 + double(*white)};
    const double span = levels.whiteHz - levels.blackHz;
    return span >= 760.0 && span <= 840.0 ? levels : SstvRadiofaxLevels {};
}

void SstvRadiofaxAutoRx::beginImage(std::uint64_t sample)
{
    config_.linesPerMinute = lock_->linesPerMinute;
    config_.phasing = lock_->pattern;
    clock_ = clockTracker_ ? clockTracker_->estimate() : std::nullopt;
    bool usingFreshProvisional = false;
    if (!clock_ && phasingClockRestarts_ && clockTracker_) {
        const auto provisional = clockTracker_->provisionalEstimate();
        const auto edge = clockTracker_->lastConfirmedWhiteEdge();
        if (provisional && sample >= edge && (sample - edge) * provisional->lines
                <= provisional->samples * 2U) {
            clock_ = provisional;
            imageClockProvisional_ = true;
            usingFreshProvisional = true;
        }
    }
    // If an image follows a disturbance immediately, a new estimate cannot
    // yet be confirmed. A recently confirmed clock is a better provisional
    // raster hint than silently reverting to nominal speed. Bound its age:
    // an old clock must not survive prolonged loss of phasing.
    if (!clock_ && lastConfirmedClock_ && lastConfirmedClock_->lines
        && sample >= lastConfirmedClockEdge_
        && (sample - lastConfirmedClockEdge_) * lastConfirmedClock_->lines
            <= lastConfirmedClock_->samples * 4U) {
        clock_ = lastConfirmedClock_;
        imageClockProvisional_ = true;
    }
    if (clock_) {
        const double nominal = double(config_.sampleRate) * 60.0 / config_.linesPerMinute;
        if (!clock_->lines || clock_->samples / clock_->lines
                - (config_.wmoDeadSector ? sstvRadiofaxDeadSectorSamples(clock_->samples / clock_->lines) : 0U) < config_.width
            || std::abs(double(clock_->samples) / clock_->lines - nominal) > nominal * kRadiofaxMaximumClockDeviation)
            { clock_.reset(); imageClockProvisional_ = false; }
    }
    if (clock_) {
        const auto trackedEdge = imageClockProvisional_ && !usingFreshProvisional ? lastConfirmedClockEdge_
            : clockTracker_->lastConfirmedWhiteEdge();
        const auto anchor = trackedEdge <= sample ? trackedEdge : lock_->latestWhiteEdgeSample;
        const auto elapsed = sample - anchor;
        const auto line = (elapsed * clock_->lines + clock_->samples / 2U)
            / clock_->samples;
        sample = anchor + line * clock_->samples / clock_->lines;
    }
    if (config_.centeredPhasingWithWhiteEndLine
        && lock_->pattern == SstvFaxPhasePattern::WhiteFivePercent) {
        const std::uint64_t lineSamples = clock_ ? clock_->samples / clock_->lines
            : std::uint64_t(config_.sampleRate) * 60U / lock_->linesPerMinute;
        sample += lineSamples + lineSamples / 40U;
    }
    try {
        levels_ = measuredLevels();
        receiver_ = std::make_unique<SstvRadiofaxRxSession>(config_, sample, offset_, clock_, *levels_);
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
    const auto isStopWindow = [&](std::uint64_t begin) {
        const auto end = begin + window;
        std::array<double, 3> re {}, im {};
        double amplitude = 0.0, mean = 0.0;
        std::size_t total = 0U, valid = 0U;
        for (const auto& obs : pendingImage_) {
            if (obs.centreSample < begin) continue;
            if (obs.centreSample >= end) break;
            ++total;
            if (!obs.valid() || !std::isfinite(obs.correctedFrequencyHz)
                || !std::isfinite(obs.confidence) || obs.confidence < 0.2 || obs.confidence > 1.0) continue;
            const double value = obs.correctedFrequencyHz - offset_ - 1900.0;
            // The main stop detector accepts 440-460 Hz. Use a small bank
            // across that same range so a valid off-centre stop can also be
            // backtracked to the image boundary.
            for (std::size_t bin = 0; bin < re.size(); ++bin) {
                const auto frequency = 440U + 10U * bin;
                const auto phaseIndex = (obs.centreSample % config_.sampleRate) * frequency % config_.sampleRate;
                const double phase = 2.0 * std::acos(-1.0) * double(phaseIndex) / config_.sampleRate;
                re[bin] += value * std::cos(phase);
                im[bin] += value * std::sin(phase);
            }
            amplitude += std::abs(value); mean += value; ++valid;
        }
        if (total < 10U || valid * 5U < total * 3U
            || amplitude < 100.0 * double(valid) || std::abs(mean) > 100.0 * double(valid))
            return false;
        for (std::size_t bin = 0; bin < re.size(); ++bin)
            if (2.0 * std::hypot(re[bin], im[bin]) >= 0.85 * amplitude) return true;
        return false;
    };
    // A matching one-second window can begin before the stop itself. Find
    // the first short matching window within it before backtracking.
    std::uint64_t boundary = candidate;
    for (unsigned i = 0U; i < 20U && !isStopWindow(boundary); ++i)
        boundary += window;
    const std::uint64_t lower = candidate > config_.sampleRate
        ? std::max(*imageStart_, candidate - config_.sampleRate) : *imageStart_;
    for (unsigned i = 0; i < 20U && boundary >= lower + window; ++i) {
        const auto begin = boundary - window;
        if (!isStopWindow(begin)) break;
        boundary = begin;
    }
    const std::uint64_t lineCount = clock_ ? clock_->lines : config_.linesPerMinute;
    const std::uint64_t denominator = clock_ ? clock_->samples : std::uint64_t(config_.sampleRate) * 60U;
    const std::uint64_t numerator = (boundary - *imageStart_) * lineCount;
    const std::uint64_t nearestLine = (numerator + denominator / 2U) / denominator;
    return *imageStart_ + nearestLine * denominator / lineCount;
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

std::size_t SstvRadiofaxAutoRx::consume(const SstvFrequencyObservation* data, std::size_t count)
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
    std::size_t consumed = 0U;
    for (std::size_t i = 0; i < count; ++i) {
        if (state_ == SstvRadiofaxAutoRxState::Complete || state_ == SstvRadiofaxAutoRxState::Partial
            || state_ == SstvRadiofaxAutoRxState::Cancelled || state_ == SstvRadiofaxAutoRxState::Failed) break;
        ++consumed;
        const auto& obs = data[i];
        lastSample_ = obs.centreSample;
        hasLast_ = true;
        if (state_ == SstvRadiofaxAutoRxState::SearchingPhasing) {
            if (config_.wmoTimedPhasing) aptStart_.consume(obs);
            lock_ = phase_.consume(&obs, 1U);
            if (!lock_) continue;
            if (config_.wmoTimedPhasing) {
                const auto aptEdge = aptStart_.lastAptWhiteEdge();
                const auto nominal = std::uint64_t(config_.sampleRate) * 60U / lock_->linesPerMinute;
                const auto period = lock_->detectedPeriodSamples ? lock_->detectedPeriodSamples : nominal;
                if (aptEdge && lock_->latestWhiteEdgeSample > *aptEdge) {
                    const auto elapsed = lock_->latestWhiteEdgeSample - *aptEdge;
                    const auto lines = elapsed / period;
                    const auto phasingLines = std::uint64_t(lock_->linesPerMinute) / 2U;
                    if (lines >= 1U && lines <= 10U && lines < phasingLines)
                        wmoLockedPhasingLines_ = lines;
                }
            }
            start_ = std::make_unique<SstvRadiofaxImageStartDetector>(config_.sampleRate, *lock_, offset_);
            clockTracker_ = std::make_unique<SstvRadiofaxClockTracker>(config_.sampleRate, *lock_, offset_);
            state_ = SstvRadiofaxAutoRxState::SearchingImage;
        }
        if (state_ == SstvRadiofaxAutoRxState::SearchingImage) {
            collectPhasingLevel(obs);
            const auto previousEstimate = clockTracker_->estimate();
            clockTracker_->consume(obs);
            if (const auto estimate = clockTracker_->estimate()) {
                hasConfirmedClock_ = true;
                lastConfirmedClock_ = estimate;
                lastConfirmedClockEdge_ = clockTracker_->lastConfirmedWhiteEdge();
                start_->setClockEstimate(estimate);
            } else if (!clockTracker_->valid() && hasConfirmedClock_ && phasingClockRestarts_ < 1800U) {
                // Retain the last confirmed clock as an acquisition anchor
                // while a fresh tracker rechecks phasing. beginImage() may
                // also use a recent estimate as a labelled provisional hint.
                auto restartLock = *lock_;
                restartLock.latestWhiteEdgeSample = previousEstimate
                    ? clockTracker_->lastConfirmedWhiteEdge() : obs.centreSample;
                priorIgnoredPhasingPulses_ += clockTracker_->ignoredIrregularPulses();
                ++phasingClockRestarts_;
                clockTracker_ = std::make_unique<SstvRadiofaxClockTracker>(config_.sampleRate, restartLock, offset_);
                if (previousEstimate)
                    clockTracker_->setPhaseReference(restartLock.latestWhiteEdgeSample, *previousEstimate);
                clockTracker_->consume(obs);
            } else if (!clockTracker_->valid()) {
                if (hasConfirmedClock_ && phasingClockRestarts_ >= 1800U)
                    retainExhaustedClockAnchor_ = true;
                if (!retainExhaustedClockAnchor_)
                    start_->setClockEstimate(std::nullopt);
            }
            buffer(obs);
            if (state_ == SstvRadiofaxAutoRxState::Failed) break;
            if (wmoLockedPhasingLines_) {
                const auto remaining = std::uint64_t(lock_->linesPerMinute) / 2U - *wmoLockedPhasingLines_;
                const auto estimate = clockTracker_->estimate();
                const auto predicted = lock_->latestWhiteEdgeSample + (estimate
                    ? remaining * estimate->samples / estimate->lines
                    : remaining * std::uint64_t(config_.sampleRate) * 60U / lock_->linesPerMinute);
                if (obs.centreSample >= predicted) {
                    const auto recentClock = estimate ? estimate : clockTracker_->provisionalEstimate();
                    const auto lastEdge = clockTracker_->lastConfirmedWhiteEdge();
                    if (recentClock && lastEdge <= predicted
                        && (predicted - lastEdge) * recentClock->lines <= recentClock->samples * 3U) {
                        usedWmoTimedPhasing_ = true;
                        beginImage(predicted);
                    } else {
                        // Do not fabricate an image when the signal vanished
                        // before the timer expired. Resume clean acquisition.
                        phase_.reset();
                        aptStart_ = SstvRadiofaxAptStartDetector(config_.sampleRate, config_.ioc, offset_);
                        lock_.reset();
                        wmoLockedPhasingLines_.reset();
                        start_.reset();
                        clockTracker_.reset();
                        lastConfirmedClock_.reset();
                        lastConfirmedClockEdge_ = 0U;
                        hasConfirmedClock_ = false;
                        retainExhaustedClockAnchor_ = false;
                        phasingClockRestarts_ = priorIgnoredPhasingPulses_ = 0U;
                        blackLevels_.fill(0U); whiteLevels_.fill(0U);
                        replay_.clear();
                        state_ = SstvRadiofaxAutoRxState::SearchingPhasing;
                    }
                }
                // With an explicit 30-second profile and a verified IOC
                // start, an isolated non-phasing row is not an image start.
                continue;
            }
            const auto candidate = start_->consume(&obs, 1U);
            if (!start_->valid()) {
                replay_.clear();
                state_ = SstvRadiofaxAutoRxState::Failed;
            } else if (candidate && whiteEndCandidate_) {
                start_->rejectCandidate();
            } else if (candidate) {
                bool accept = true;
                if (config_.centeredPhasingWithWhiteEndLine) {
                    const auto duration = std::uint64_t(config_.sampleRate) * 60U / lock_->linesPerMinute;
                    std::size_t white = 0U, observed = 0U;
                    for (const auto& item : replay_) {
                        if (item.centreSample < *candidate || item.centreSample >= *candidate + duration
                            || !item.valid() || !std::isfinite(item.correctedFrequencyHz)
                            || !std::isfinite(item.confidence) || item.confidence < 0.2 || item.confidence > 1.0) continue;
                        ++observed;
                        const auto frequency = item.correctedFrequencyHz - offset_;
                        if (frequency >= 2100.0 && frequency <= 2450.0) ++white;
                    }
                    accept = observed >= 8U && white * 5U >= observed * 3U;
                }
                if (accept && config_.centeredPhasingWithWhiteEndLine) {
                    whiteEndCandidate_ = *candidate;
                    start_->rejectCandidate();
                } else if (accept) beginImage(*candidate);
                else start_->rejectCandidate();
            }
            if (whiteEndCandidate_ && state_ == SstvRadiofaxAutoRxState::SearchingImage) {
                const auto duration = std::uint64_t(config_.sampleRate) * 60U / lock_->linesPerMinute;
                if (obs.centreSample >= *whiteEndCandidate_ + duration * 5U / 4U) {
                    // Search around the nominal candidate: clock reacquisition
                    // can shift it significantly relative to the white line.
                    const auto begin = *whiteEndCandidate_ > duration / 4U
                        ? *whiteEndCandidate_ - duration / 4U : 0U;
                    std::array<std::size_t, 60> total {}, white {};
                    for (const auto& item : replay_) {
                        if (item.centreSample < begin || item.centreSample >= begin + duration * 3U / 2U
                            || !item.valid() || !std::isfinite(item.correctedFrequencyHz)
                            || !std::isfinite(item.confidence) || item.confidence < 0.2 || item.confidence > 1.0) continue;
                        const auto bin = (item.centreSample - begin) * 40U / duration;
                        if (bin >= total.size()) continue;
                        ++total[bin];
                        const auto f = item.correctedFrequencyHz - offset_;
                        if (f >= 2100.0 && f <= 2450.0) ++white[bin];
                    }
                    bool confirmed = false;
                    for (std::size_t first = 0; first <= 20U; ++first) {
                        std::size_t n = 0U, w = 0U;
                        for (std::size_t bin = first; bin < first + 40U; ++bin) {
                            n += total[bin]; w += white[bin];
                        }
                        if (n >= 8U && w * 4U >= n * 3U) { confirmed = true; break; }
                    }
                    const auto saved = *whiteEndCandidate_;
                    whiteEndCandidate_.reset();
                    if (confirmed) beginImage(saved);
                    else start_->rejectCandidate();
                }
            }
        } else if (state_ == SstvRadiofaxAutoRxState::Receiving) {
            consumeImageObservation(obs);
        }
    }
    return consumed;
}

std::size_t SstvRadiofaxAutoRx::consume(const std::vector<SstvFrequencyObservation>& data)
{
    return consume(data.data(), data.size());
}

bool SstvRadiofaxAutoRx::stopAtAudioSample(std::uint64_t sample)
{
    if (state_ != SstvRadiofaxAutoRxState::Receiving || !receiver_
        || !imageStart_ || sample < *imageStart_ || !hasLast_ || sample > lastSample_)
        return false;
    const std::uint64_t denominator = clock_ ? clock_->samples : std::uint64_t(config_.sampleRate) * 60U;
    const std::uint64_t lineCount = clock_ ? clock_->lines : config_.linesPerMinute;
    const std::uint64_t line = ((sample - *imageStart_) * lineCount
                                + denominator / 2U) / denominator;
    stopSample_ = *imageStart_ + line * denominator / lineCount;
    stopWasAudioTone_ = true;
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
std::optional<SstvRadiofaxClockEstimate> SstvRadiofaxAutoRx::imageClock() const noexcept { return clock_; }
bool SstvRadiofaxAutoRx::imageClockProvisional() const noexcept { return imageClockProvisional_; }
bool SstvRadiofaxAutoRx::usedWmoTimedPhasing() const noexcept { return usedWmoTimedPhasing_; }
bool SstvRadiofaxAutoRx::wmoTimingArmed() const noexcept { return wmoLockedPhasingLines_.has_value(); }
unsigned SstvRadiofaxAutoRx::ignoredIrregularPhasingPulses() const noexcept
{ return priorIgnoredPhasingPulses_ + (clockTracker_ ? clockTracker_->ignoredIrregularPulses() : 0U); }
bool SstvRadiofaxAutoRx::phasingClockTrackerValid() const noexcept
{ return clockTracker_ && clockTracker_->valid(); }
unsigned SstvRadiofaxAutoRx::phasingClockRestarts() const noexcept { return phasingClockRestarts_; }
std::optional<SstvRadiofaxLevels> SstvRadiofaxAutoRx::imageLevels() const noexcept { return levels_; }
bool SstvRadiofaxAutoRx::stopWasAudioTone() const noexcept { return stopWasAudioTone_; }
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
