// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvScottieRxSession.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace decodium::sstv {
namespace {

bool isFiniteUnit(double value) noexcept
{
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

std::uint64_t intervalEnd(std::uint64_t centreSample,
                          std::uint32_t spanSamples) noexcept
{
    const std::uint64_t left = spanSamples / 2U;
    const std::uint64_t start = centreSample >= left
        ? centreSample - left
        : 0U;
    const std::uint64_t span = spanSamples;
    return span > std::numeric_limits<std::uint64_t>::max() - start
        ? std::numeric_limits<std::uint64_t>::max()
        : start + span;
}

std::uint64_t absoluteDifference(std::uint64_t left,
                                 std::uint64_t right) noexcept
{
    return left >= right ? left - right : right - left;
}

} // namespace

SstvScottieRxSession::SstvScottieRxSession(
    SstvScottieRxSessionConfig config)
    : config_([&config] {
        validateConfig(config);
        return config;
    }())
    , spec_(SstvScottieProtocol::spec(config_.mode))
    , mapper_({config_.mode,
               config_.sampleRate,
               config_.clockErrorPpm})
    , syncTracker_(makeSyncConfig(config_, mapper_))
    , decoder_(SstvScottieDecoderConfig {
          config_.mode,
          config_.sampleRate,
          config_.clockErrorPpm,
          config_.frequencyOffsetHz,
          config_.minimumObservationConfidence,
          config_.maximumPendingDirtyEvents})
{
    if (mapper_.imageSampleCount()
        > std::numeric_limits<std::uint64_t>::max()
            - config_.imageStartSample) {
        throw std::overflow_error("Scottie RX image sample range overflow");
    }
    imageEndSample_ = config_.imageStartSample + mapper_.imageSampleCount();
    lastSyncInputEndSample_ = config_.imageStartSample;

    const std::uint64_t lineSamples = mapper_.lineEndSample(0U)
        - mapper_.lineStartSample(0U);
    if (lineSamples > MaximumPendingObservations
        || lineSamples + MaximumObservationsPerConsume
            > MaximumPendingObservations) {
        throw std::length_error("Scottie RX replay window exceeds hard bound");
    }
    pendingCapacity_ = static_cast<std::size_t>(lineSamples)
        + MaximumObservationsPerConsume;
    pending_.reserve(pendingCapacity_);
    metrics_.pendingObservationCapacity = pendingCapacity_;
}

SstvScottieRxSessionUpdate SstvScottieRxSession::consume(
    const SstvFrequencyObservation* observations,
    std::size_t count)
{
    if (count > MaximumObservationsPerConsume) {
        throw std::length_error(
            "Scottie RX session observation call exceeds its work bound");
    }
    if (count != 0U && observations == nullptr) {
        throw std::invalid_argument(
            "Scottie RX session observations must not be null");
    }

    SstvScottieRxSessionUpdate update;
    update.inputObservations = count;
    update.publishedLineRevision = decoder_.metrics().linesPublished;
    update.linesPublished = update.publishedLineRevision;
    saturatingAdd(metrics_.consumeCalls);
    saturatingAdd(metrics_.inputObservations,
                  static_cast<std::uint64_t>(count));
    if (count == 0U || state_ != SstvScottieRxSessionState::Receiving) {
        return update;
    }

    std::uint64_t previousSample = lastInputSample_;
    std::uint64_t previousSequence = lastInputSequence_;
    bool havePreviousSample = haveLastInputSample_;
    bool havePreviousSequence = haveLastInputSequence_;
    for (std::size_t index = 0U; index < count; ++index) {
        if ((havePreviousSample
             && observations[index].centreSample <= previousSample)
            || (havePreviousSequence
                && observations[index].sequence <= previousSequence)) {
            saturatingAdd(metrics_.rejectedRegressions);
            return update;
        }
        previousSample = observations[index].centreSample;
        previousSequence = observations[index].sequence;
        havePreviousSample = true;
        havePreviousSequence = true;
    }

    std::vector<SstvFrequencyObservation> filtered;
    std::vector<SstvSyncFrequencyObservation> syncObservations;
    filtered.reserve(count);
    syncObservations.reserve(count);
    bool reachedEnd = false;
    for (std::size_t index = 0U; index < count; ++index) {
        const SstvFrequencyObservation& observation = observations[index];
        lastInputSample_ = observation.centreSample;
        lastInputSequence_ = observation.sequence;
        haveLastInputSample_ = true;
        haveLastInputSequence_ = true;
        const std::uint64_t observationEnd = intervalEnd(
            observation.centreSample, config_.observationSpanSamples);
        if (observation.centreSample < config_.imageStartSample) {
            saturatingAdd(metrics_.observationsBeforeImage);
            continue;
        }
        if (observation.centreSample >= imageEndSample_) {
            reachedEnd = true;
            saturatingAdd(metrics_.observationsAfterImage);
            continue;
        }
        reachedEnd = reachedEnd || observationEnd >= imageEndSample_;
        filtered.push_back(observation);

        const bool numeric = std::isfinite(observation.rawFrequencyHz)
            && std::isfinite(observation.afcCorrectionHz)
            && isFiniteUnit(observation.confidence);
        syncObservations.push_back(SstvSyncFrequencyObservation {
            observation.centreSample,
            config_.observationSpanSamples,
            numeric ? observation.rawFrequencyHz : 0.0,
            numeric ? observation.afcCorrectionHz : 0.0,
            numeric ? observation.confidence : 0.0,
            numeric && observation.valid()});
        lastSyncInputEndSample_ = observationEnd;
    }

    metrics_.peakFilteredObservations = std::max(
        metrics_.peakFilteredObservations, filtered.size());
    retainPending(filtered);
    saturatingAdd(metrics_.syncObservations,
                  static_cast<std::uint64_t>(syncObservations.size()));
    if (!syncObservations.empty()) {
        applySyncEvents(syncTracker_.consume(syncObservations), &update);
    }

    const auto before = decoder_.metrics();
    update.decoderAcceptedObservations += drainPending();
    saturatingAdd(metrics_.decoderAcceptedObservations,
                  static_cast<std::uint64_t>(
                      update.decoderAcceptedObservations));
    const auto after = decoder_.metrics();
    update.linesPublished = after.linesPublished;
    update.publishedLineRevision = after.linesPublished;
    update.imageChanged = after.linesPublished > before.linesPublished;

    if (reachedEnd) {
        update.reachedImageEnd = true;
        finish();
        const auto terminal = decoder_.metrics();
        update.linesPublished = terminal.linesPublished;
        update.publishedLineRevision = terminal.linesPublished;
        update.imageChanged = update.imageChanged
            || terminal.linesPublished > after.linesPublished;
    }
    return update;
}

SstvScottieRxSessionUpdate SstvScottieRxSession::consume(
    const std::vector<SstvFrequencyObservation>& observations)
{
    return consume(observations.data(), observations.size());
}

SstvScottieRxSessionState SstvScottieRxSession::notifyDiscontinuity(
    std::uint64_t nextSample)
{
    if (state_ != SstvScottieRxSessionState::Receiving) {
        return state_;
    }
    saturatingAdd(metrics_.discontinuities);
    applySyncEvents(syncTracker_.notifyDiscontinuity(nextSample), nullptr);
    decodeThroughSample_ = std::numeric_limits<std::uint64_t>::max();
    haveDecodeThroughSample_ = true;
    saturatingAdd(metrics_.decoderAcceptedObservations,
                  static_cast<std::uint64_t>(drainPending()));
    updateTerminalState(decoder_.finish());
    pending_.clear();
    return state_;
}

SstvScottieRxSessionState SstvScottieRxSession::finish()
{
    saturatingAdd(metrics_.finishCalls);
    if (state_ != SstvScottieRxSessionState::Receiving) {
        return state_;
    }
    applySyncEvents(syncTracker_.flush(lastSyncInputEndSample_), nullptr);
    decodeThroughSample_ = std::numeric_limits<std::uint64_t>::max();
    haveDecodeThroughSample_ = true;
    saturatingAdd(metrics_.decoderAcceptedObservations,
                  static_cast<std::uint64_t>(drainPending()));
    updateTerminalState(decoder_.finish());
    pending_.clear();
    return state_;
}

void SstvScottieRxSession::cancel() noexcept
{
    saturatingAdd(metrics_.cancelCalls);
    if (state_ != SstvScottieRxSessionState::Receiving) {
        return;
    }
    decoder_.cancel();
    pending_.clear();
    state_ = SstvScottieRxSessionState::Cancelled;
}

SstvScottieRxSessionState SstvScottieRxSession::state() const noexcept
{
    return state_;
}

const SstvScottieRxSessionConfig& SstvScottieRxSession::config() const
    noexcept
{
    return config_;
}

SstvScottieMode SstvScottieRxSession::mode() const noexcept
{
    return config_.mode;
}

std::uint64_t SstvScottieRxSession::imageStartSample() const noexcept
{
    return config_.imageStartSample;
}

std::uint64_t SstvScottieRxSession::imageEndSample() const noexcept
{
    return imageEndSample_;
}

const SstvImageFrame& SstvScottieRxSession::imageFrame() const noexcept
{
    return decoder_.imageFrame();
}

SstvImageSnapshot SstvScottieRxSession::snapshot() const
{
    return decoder_.snapshot();
}

std::vector<SstvDirtyEvent> SstvScottieRxSession::takeDirtyEvents()
{
    return decoder_.takeDirtyEvents();
}

SstvScottieRxSessionMetrics SstvScottieRxSession::metrics() const noexcept
{
    return metrics_;
}

SstvSyncTrackerSnapshot SstvScottieRxSession::syncSnapshot() const
{
    return syncTracker_.snapshot();
}

SstvScottieDecoderMetrics SstvScottieRxSession::decoderMetrics() const
    noexcept
{
    return decoder_.metrics();
}

void SstvScottieRxSession::validateConfig(
    const SstvScottieRxSessionConfig& config)
{
    if (config.sampleRate != 12'000U
        || config.observationSpanSamples == 0U
        || config.observationSpanSamples > 4'096U
        || config.clockErrorPpm
            < -SstvScottieMapper::MaximumAbsoluteClockErrorPpm
        || config.clockErrorPpm
            > SstvScottieMapper::MaximumAbsoluteClockErrorPpm
        || !std::isfinite(config.frequencyOffsetHz)
        || std::abs(config.frequencyOffsetHz)
            > SstvScottieDecoder::MaximumAbsoluteFrequencyOffsetHz
        || !isFiniteUnit(config.minimumObservationConfidence)
        || config.maximumPendingDirtyEvents == 0U
        || config.maximumPendingDirtyEvents
            > SstvImageFrame::kMaximumDirtyEvents) {
        throw std::invalid_argument(
            "invalid Scottie RX session configuration");
    }
    static_cast<void>(SstvScottieProtocol::spec(config.mode));
}

void SstvScottieRxSession::saturatingAdd(std::uint64_t& value,
                                         std::uint64_t increment) noexcept
{
    value = increment > std::numeric_limits<std::uint64_t>::max() - value
        ? std::numeric_limits<std::uint64_t>::max()
        : value + increment;
}

SstvSyncTrackerConfig SstvScottieRxSession::makeSyncConfig(
    const SstvScottieRxSessionConfig& config,
    const SstvScottieMapper& mapper)
{
    const SstvScottieModeSpec spec = SstvScottieProtocol::spec(config.mode);
    const std::uint64_t syncSamples = static_cast<std::uint64_t>(std::llround(
        static_cast<long double>(spec.syncDuration.count)
        * config.sampleRate / 1.0e12L));
    const std::uint64_t lineSamples = mapper.lineEndSample(0U)
        - mapper.lineStartSample(0U);
    if (syncSamples == 0U || lineSamples == 0U) {
        throw std::invalid_argument("Scottie RX timing rounds to zero samples");
    }

    SstvSyncTrackerConfig tracker;
    tracker.sampleRateHz = static_cast<double>(config.sampleRate);
    tracker.syncFrequencyHz = SstvScottieProtocol::SyncFrequencyHz
        + config.frequencyOffsetHz;
    tracker.enterFrequencyToleranceHz = 55.0;
    tracker.exitFrequencyToleranceHz = 105.0;
    tracker.minimumEnterConfidence = 0.45;
    tracker.minimumHoldConfidence = 0.25;
    tracker.maximumAfcCorrectionHz = 150.0;
    tracker.nominalSyncDurationSamples = syncSamples;
    tracker.syncDurationToleranceSamples = std::max<std::uint64_t>(
        4U, syncSamples / 3U);
    tracker.nominalLinePeriodSamples = lineSamples;
    tracker.lineTimingToleranceSamples = std::max<std::uint64_t>(
        16U, lineSamples / 40U);
    tracker.enterDebounceSamples = std::max<std::uint64_t>(
        3U, syncSamples / 4U);
    tracker.exitDebounceSamples = std::max<std::uint64_t>(
        3U, syncSamples / 8U);
    tracker.maximumObservationGapSamples = std::max<std::uint64_t>(
        static_cast<std::uint64_t>(config.observationSpanSamples) * 4U,
        tracker.exitDebounceSamples);
    tracker.maximumPredictedLines = 8U;
    tracker.reacquireConfirmations = 2U;
    tracker.slant.nominalLinePeriodSamples = lineSamples;
    tracker.slant.windowLines = 32U;
    tracker.slant.minimumLines = 4U;
    tracker.slant.minimumConfidence = 0.25;
    tracker.slant.outlierToleranceSamples = std::max(
        8.0, static_cast<double>(lineSamples) * 0.002);
    tracker.slant.warningClockErrorPpm = 300.0;
    tracker.slant.maximumClockErrorPpm = 5'000.0;
    return tracker;
}

void SstvScottieRxSession::retainPending(
    const std::vector<SstvFrequencyObservation>& input)
{
    if (input.empty()) {
        return;
    }
    if (input.size() > pendingCapacity_) {
        throw std::length_error("Scottie RX pending input exceeds hard bound");
    }
    if (pending_.size() > pendingCapacity_ - input.size()) {
        const std::size_t evicted = pending_.size()
            - (pendingCapacity_ - input.size());
        pending_.erase(pending_.begin(),
                       pending_.begin()
                           + static_cast<std::ptrdiff_t>(evicted));
        saturatingAdd(metrics_.pendingObservationsEvicted,
                      static_cast<std::uint64_t>(evicted));
    }
    pending_.insert(pending_.end(), input.begin(), input.end());
    metrics_.peakPendingObservations = std::max(
        metrics_.peakPendingObservations, pending_.size());
}

std::size_t SstvScottieRxSession::drainPending()
{
    if (!haveDecodeThroughSample_ || pending_.empty()) {
        return 0U;
    }
    const auto end = std::lower_bound(
        pending_.cbegin(), pending_.cend(), decodeThroughSample_,
        [](const SstvFrequencyObservation& observation,
           std::uint64_t sample) {
            return observation.centreSample < sample;
        });
    const std::size_t drainCount = static_cast<std::size_t>(
        end - pending_.cbegin());
    std::size_t accepted = 0U;
    std::size_t offset = 0U;
    while (offset < drainCount) {
        const std::size_t count = std::min(
            drainCount - offset,
            SstvScottieDecoder::MaximumObservationsPerConsume);
        accepted += decoder_.consume(pending_.data() + offset, count);
        offset += count;
    }
    pending_.erase(pending_.begin(),
                   pending_.begin()
                       + static_cast<std::ptrdiff_t>(drainCount));
    return accepted;
}

void SstvScottieRxSession::applySyncEvents(
    const std::vector<SstvSyncEvent>& events,
    SstvScottieRxSessionUpdate* update)
{
    saturatingAdd(metrics_.syncEvents,
                  static_cast<std::uint64_t>(events.size()));
    for (const SstvSyncEvent& event : events) {
        const bool observed =
            event.type == SstvSyncEventType::LineSyncObserved;
        const bool predicted =
            event.type == SstvSyncEventType::LineSyncPredicted;
        if (!observed && !predicted) {
            continue;
        }
        std::uint32_t line = 0U;
        if (!physicalLineForEvent(event, line)) {
            saturatingAdd(metrics_.rejectedLineSyncs);
            continue;
        }
        const SstvScottieLineSync sync {
            line,
            event.syncStartSample,
            event.confidence,
            predicted};
        const bool accepted = decoder_.consumeLineSyncs(&sync, 1U) == 1U;
        if (!accepted) {
            saturatingAdd(metrics_.rejectedLineSyncs);
            continue;
        }
        const std::uint64_t mappedLineStart = mapper_.lineStartSample(line);
        const std::uint64_t syncOffset =
            mapper_.embeddedSyncStartSample(line) - mappedLineStart;
        const std::uint64_t lineSpan =
            mapper_.lineEndSample(line) - mappedLineStart;
        if (event.syncStartSample < syncOffset
            || lineSpan > std::numeric_limits<std::uint64_t>::max()
                    - (event.syncStartSample - syncOffset)) {
            saturatingAdd(metrics_.rejectedLineSyncs);
            continue;
        }
        const std::uint64_t lineEnd = event.syncStartSample - syncOffset
            + lineSpan;
        // A later anchor can quantise a few samples before the preceding
        // anchor's projected end.  Hold a bounded tail until that next anchor
        // exists; otherwise a chunk boundary decides which row owns the
        // overlap.  Terminal paths explicitly drain the remaining tail.
        const std::uint64_t guard = std::min(
            lineSpan - 1U,
            syncTracker_.config().lineTimingToleranceSamples);
        const std::uint64_t safeLineEnd = lineEnd - guard;
        decodeThroughSample_ = haveDecodeThroughSample_
            ? std::max(decodeThroughSample_, safeLineEnd)
            : safeLineEnd;
        haveDecodeThroughSample_ = true;
        if (observed) {
            saturatingAdd(metrics_.observedLineSyncs);
            if (update != nullptr) {
                ++update->observedLineSyncs;
            }
        } else {
            saturatingAdd(metrics_.predictedLineSyncs);
            if (update != nullptr) {
                ++update->predictedLineSyncs;
            }
        }
    }
}

bool SstvScottieRxSession::physicalLineForEvent(
    const SstvSyncEvent& event,
    std::uint32_t& line)
{
    if (!haveTrackerLineOffset_) {
        if (event.type != SstvSyncEventType::LineSyncObserved) {
            return false;
        }
        const std::uint64_t firstExpected = config_.imageStartSample
            + mapper_.embeddedSyncStartSample(0U);
        const std::uint64_t period = mapper_.lineEndSample(0U)
            - mapper_.lineStartSample(0U);
        std::uint64_t physical = 0U;
        if (event.syncStartSample > firstExpected) {
            const std::uint64_t delta = event.syncStartSample - firstExpected;
            physical = delta / period;
            if (delta % period >= (period + 1U) / 2U) {
                ++physical;
            }
        }
        if (physical >= spec_.height || physical < event.lineIndex) {
            return false;
        }
        const std::uint64_t expected = config_.imageStartSample
            + mapper_.embeddedSyncStartSample(
                static_cast<std::uint32_t>(physical));
        if (absoluteDifference(expected, event.syncStartSample)
            > syncTracker_.config().lineTimingToleranceSamples) {
            return false;
        }
        trackerLineOffset_ = physical - event.lineIndex;
        haveTrackerLineOffset_ = true;
    }

    if (event.lineIndex
        > std::numeric_limits<std::uint64_t>::max() - trackerLineOffset_) {
        return false;
    }
    const std::uint64_t physical = event.lineIndex + trackerLineOffset_;
    if (physical >= spec_.height) {
        return false;
    }
    line = static_cast<std::uint32_t>(physical);
    return true;
}

void SstvScottieRxSession::updateTerminalState(
    SstvScottieDecodeState decoderState) noexcept
{
    switch (decoderState) {
    case SstvScottieDecodeState::Receiving:
        state_ = SstvScottieRxSessionState::Receiving;
        break;
    case SstvScottieDecodeState::Complete:
        state_ = SstvScottieRxSessionState::Complete;
        break;
    case SstvScottieDecodeState::Partial:
        state_ = SstvScottieRxSessionState::Partial;
        break;
    case SstvScottieDecodeState::Cancelled:
        state_ = SstvScottieRxSessionState::Cancelled;
        break;
    }
}

} // namespace decodium::sstv
