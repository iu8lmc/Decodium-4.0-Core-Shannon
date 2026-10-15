// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvRobotRxSession.h"

#include "../core/SstvTimingAccumulator.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace decodium::sstv {
namespace {

std::uint64_t samplesFor(std::uint32_t sampleRate, Picoseconds duration)
{
    SstvTimingAccumulator timing(sampleRate);
    return timing.samplesFor(duration);
}

bool isFiniteUnit(double value) noexcept
{
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

std::uint64_t observationIntervalEnd(std::uint64_t centreSample,
                                     std::uint32_t spanSamples) noexcept
{
    const std::uint64_t leftSpan = spanSamples / 2U;
    const std::uint64_t intervalStart = centreSample >= leftSpan
        ? centreSample - leftSpan : 0U;
    return spanSamples
            > std::numeric_limits<std::uint64_t>::max() - intervalStart
        ? std::numeric_limits<std::uint64_t>::max()
        : intervalStart + spanSamples;
}

} // namespace

SstvRobotRxSession::SstvRobotRxSession(SstvRobotRxSessionConfig config)
    : config_([&config] {
        validateConfig(config);
        return config;
    }())
    , spec_(SstvRobotProtocol::spec(config_.mode))
    , mapper_({config_.mode,
               config_.sampleRate,
               config_.clockErrorPpm})
    , syncTracker_(makeSyncConfig(config_))
    , decoder_({config_.mode,
                config_.sampleRate,
                config_.clockErrorPpm,
                config_.frequencyOffsetHz,
                config_.minimumObservationConfidence,
                config_.maximumPendingDirtyEvents,
                config_.allowTerminalRowRecovery})
{
    const std::uint64_t canonicalImageSamples = mapper_.imageSampleCount();
    std::uint64_t receiverImageSamples = canonicalImageSamples;
    if (spec_.mode == SstvRobotMode::Bw8) {
        // Robot B/W 8 exists in two interoperable timing profiles under the
        // same VIS identity.  The canonical profile is 10 ms sync + 56 ms
        // scan; PySSTV and established peers emit 7 ms + 60 ms.  Retain the
        // longer bounded extent so the compatibility profile's final line is
        // decoded instead of being clipped at the canonical 7.92 s boundary.
        const std::uint64_t compatibilityLineSamples = samplesFor(
            config_.sampleRate, Picoseconds {67'000'000'000LL});
        if (compatibilityLineSamples
            > std::numeric_limits<std::uint64_t>::max() / spec_.height) {
            throw std::overflow_error("Robot RX compatibility range overflow");
        }
        receiverImageSamples = std::max(
            receiverImageSamples,
            compatibilityLineSamples * spec_.height);
        // AudioQueue/BlackHole clock and callback scheduling can leave the
        // final Robot B/W lines a little beyond the nominal 67 ms profile.
        // Keep a small bounded terminal guard so a valid tail is not clipped;
        // refineBw8ImageEndFromObservedSync() still contracts native 66 ms
        // captures back to the canonical extent once slant is known.
        const std::uint64_t terminalGuardSamples =
            (static_cast<std::uint64_t>(config_.sampleRate) * 250U) / 1'000U;
        if (receiverImageSamples
            <= std::numeric_limits<std::uint64_t>::max()
                - terminalGuardSamples) {
            receiverImageSamples += terminalGuardSamples;
        }
    }
    if (receiverImageSamples
        > std::numeric_limits<std::uint64_t>::max()
            - config_.imageStartSample) {
        throw std::overflow_error("Robot RX image sample range overflow");
    }
    canonicalImageEndSample_ = config_.imageStartSample
        + canonicalImageSamples;
    imageEndSample_ = config_.imageStartSample + receiverImageSamples;

    const std::uint64_t syncSamples = samplesFor(
        config_.sampleRate, spec_.syncDuration);
    if (syncSamples == 0U
        || syncSamples > std::numeric_limits<std::uint64_t>::max()
            - config_.imageStartSample) {
        throw std::overflow_error("Robot RX initial sync range overflow");
    }
    const SstvExplicitSyncPulse initial {
        config_.imageStartSample,
        config_.imageStartSample + syncSamples,
        1.0};
    initialSyncEndSample_ = initial.endSample;
    lastSyncInputEndSample_ = initial.endSample;
    const auto events = syncTracker_.consumeExplicit(&initial, 1U);
    applySyncEvents(events, nullptr);
    if (decoder_.metrics().storedSyncAnchors != 1U) {
        throw std::logic_error("Robot RX failed to seed line-zero sync");
    }
}

SstvRobotRxSessionUpdate SstvRobotRxSession::consume(
    const SstvFrequencyObservation* observations,
    std::size_t count)
{
    if (count > MaximumObservationsPerConsume) {
        throw std::length_error(
            "Robot RX session observation call exceeds its work bound");
    }
    if (count != 0U && observations == nullptr) {
        throw std::invalid_argument(
            "Robot RX session observations must not be null");
    }

    SstvRobotRxSessionUpdate update;
    update.inputObservations = count;
    update.publishedLineRevision = decoder_.metrics().linesPublished;
    update.linesPublished = update.publishedLineRevision;
    saturatingAdd(metrics_.consumeCalls);
    saturatingAdd(metrics_.inputObservations,
                  static_cast<std::uint64_t>(count));
    if (count == 0U || state_ != SstvRobotRxSessionState::Receiving) {
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
        lastInputEndSample_ = observationIntervalEnd(
            observation.centreSample, config_.observationSpanSamples);
        lastInputSequence_ = observation.sequence;
        haveLastInputSample_ = true;
        haveLastInputSequence_ = true;
        if (observation.centreSample < config_.imageStartSample) {
            saturatingAdd(metrics_.observationsBeforeImage);
            continue;
        }
        if (observation.centreSample >= imageEndSample_) {
            reachedEnd = true;
            saturatingAdd(metrics_.observationsAfterImage);
            continue;
        }
        reachedEnd = reachedEnd || lastInputEndSample_ >= imageEndSample_;
        filtered.push_back(observation);
        const bool numeric = std::isfinite(observation.rawFrequencyHz)
            && std::isfinite(observation.afcCorrectionHz)
            && isFiniteUnit(observation.confidence);
        if (lastInputEndSample_ > initialSyncEndSample_) {
            lastSyncInputEndSample_ = lastInputEndSample_;
            syncObservations.push_back({
                observation.centreSample,
                config_.observationSpanSamples,
                numeric ? observation.rawFrequencyHz : 0.0,
                numeric ? observation.afcCorrectionHz : 0.0,
                numeric ? observation.confidence : 0.0,
                numeric && observation.valid()});
        }
    }

    metrics_.peakFilteredObservations = std::max(
        metrics_.peakFilteredObservations, filtered.size());
    saturatingAdd(metrics_.syncObservations,
                  static_cast<std::uint64_t>(syncObservations.size()));
    if (!syncObservations.empty()) {
        applySyncEvents(syncTracker_.consume(syncObservations), &update);
        refineBw8ImageEndFromObservedSync();
        reachedEnd = reachedEnd
            || lastInputEndSample_ >= imageEndSample_;
    }

    const SstvRobotDecoderMetrics before = decoder_.metrics();
    if (!filtered.empty()) {
        update.decoderAcceptedObservations = decoder_.consume(filtered);
        saturatingAdd(metrics_.decoderAcceptedObservations,
                      static_cast<std::uint64_t>(
                          update.decoderAcceptedObservations));
    }
    const SstvRobotDecoderMetrics after = decoder_.metrics();
    update.linesPublished = after.linesPublished;
    update.publishedLineRevision = after.linesPublished;
    update.imageChanged = after.linesPublished > before.linesPublished;

    if (reachedEnd) {
        update.reachedImageEnd = true;
        finish();
        const SstvRobotDecoderMetrics terminal = decoder_.metrics();
        update.linesPublished = terminal.linesPublished;
        update.publishedLineRevision = terminal.linesPublished;
        update.imageChanged = update.imageChanged
            || terminal.linesPublished > after.linesPublished;
    }
    return update;
}

SstvRobotRxSessionUpdate SstvRobotRxSession::consume(
    const std::vector<SstvFrequencyObservation>& observations)
{
    return consume(observations.data(), observations.size());
}

SstvRobotRxSessionState SstvRobotRxSession::notifyDiscontinuity(
    std::uint64_t nextSample)
{
    if (state_ != SstvRobotRxSessionState::Receiving) {
        return state_;
    }
    saturatingAdd(metrics_.discontinuities);
    applySyncEvents(syncTracker_.notifyDiscontinuity(nextSample), nullptr);
    updateTerminalState(decoder_.finish());
    return state_;
}

SstvRobotRxSessionState SstvRobotRxSession::finish()
{
    saturatingAdd(metrics_.finishCalls);
    if (state_ != SstvRobotRxSessionState::Receiving) {
        return state_;
    }
    // Flush through the bounded image extent, not merely the last observed
    // FFT window.  This lets the sync tracker emit its already-bounded
    // terminal predictions when the final sync pulses are hidden by an audio
    // callback boundary; pixel observations remain the authority for whether
    // the resulting frame is complete.
    applySyncEvents(syncTracker_.flush(
                        std::max(lastSyncInputEndSample_, imageEndSample_)),
                   nullptr);
    updateTerminalState(decoder_.finish());
    return state_;
}

void SstvRobotRxSession::cancel() noexcept
{
    saturatingAdd(metrics_.cancelCalls);
    if (state_ == SstvRobotRxSessionState::Receiving) {
        decoder_.cancel();
        state_ = SstvRobotRxSessionState::Cancelled;
    }
}

SstvRobotRxSessionState SstvRobotRxSession::state() const noexcept
{
    return state_;
}

SstvRobotMode SstvRobotRxSession::mode() const noexcept
{
    return spec_.mode;
}

const SstvRobotRxSessionConfig& SstvRobotRxSession::config() const noexcept
{
    return config_;
}

std::uint64_t SstvRobotRxSession::imageStartSample() const noexcept
{
    return config_.imageStartSample;
}

std::uint64_t SstvRobotRxSession::imageEndSample() const noexcept
{
    return imageEndSample_;
}

const SstvImageFrame& SstvRobotRxSession::imageFrame() const noexcept
{
    return decoder_.imageFrame();
}

SstvImageSnapshot SstvRobotRxSession::snapshot() const
{
    return decoder_.snapshot();
}

std::vector<SstvDirtyEvent> SstvRobotRxSession::takeDirtyEvents()
{
    return decoder_.takeDirtyEvents();
}

SstvRobotRxSessionMetrics SstvRobotRxSession::metrics() const noexcept
{
    return metrics_;
}

SstvSyncTrackerSnapshot SstvRobotRxSession::syncSnapshot() const
{
    return syncTracker_.snapshot();
}

SstvRobotDecoderMetrics SstvRobotRxSession::decoderMetrics() const noexcept
{
    return decoder_.metrics();
}

void SstvRobotRxSession::validateConfig(
    const SstvRobotRxSessionConfig& config)
{
    if (config.sampleRate != 12'000U
        || config.observationSpanSamples == 0U
        || config.observationSpanSamples > 4'096U
        || config.clockErrorPpm
            < -SstvRobotMapper::MaximumAbsoluteClockErrorPpm
        || config.clockErrorPpm
            > SstvRobotMapper::MaximumAbsoluteClockErrorPpm
        || !std::isfinite(config.frequencyOffsetHz)
        || std::abs(config.frequencyOffsetHz)
            > SstvRobotDecoder::MaximumAbsoluteFrequencyOffsetHz
        || !isFiniteUnit(config.minimumObservationConfidence)
        || config.maximumPendingDirtyEvents == 0U
        || config.maximumPendingDirtyEvents
            > SstvImageFrame::kMaximumDirtyEvents) {
        throw std::invalid_argument("invalid Robot RX session configuration");
    }
    static_cast<void>(SstvRobotProtocol::spec(config.mode));
}

void SstvRobotRxSession::saturatingAdd(std::uint64_t& value,
                                       std::uint64_t increment) noexcept
{
    value = increment > std::numeric_limits<std::uint64_t>::max() - value
        ? std::numeric_limits<std::uint64_t>::max()
        : value + increment;
}

SstvSyncTrackerConfig SstvRobotRxSession::makeSyncConfig(
    const SstvRobotRxSessionConfig& config)
{
    const SstvRobotModeSpec spec = SstvRobotProtocol::spec(config.mode);
    const SstvRobotMapper mapper({
        config.mode, config.sampleRate, config.clockErrorPpm});
    const std::uint64_t syncSamples = samplesFor(
        config.sampleRate, spec.syncDuration);
    const std::uint64_t lineSamples = mapper.lineEndSample(0U);
    const std::uint64_t trackerLineSamples =
        spec.mode == SstvRobotMode::Bw8
        ? samplesFor(config.sampleRate, Picoseconds {67'000'000'000LL})
        : lineSamples;
    if (syncSamples == 0U || lineSamples == 0U) {
        throw std::invalid_argument("Robot RX timing rounds to zero samples");
    }

    SstvSyncTrackerConfig tracker;
    tracker.sampleRateHz = static_cast<double>(config.sampleRate);
    tracker.syncFrequencyHz = SstvRobotProtocol::SyncFrequencyHz
        + config.frequencyOffsetHz;
    tracker.enterFrequencyToleranceHz = 55.0;
    tracker.exitFrequencyToleranceHz = 105.0;
    tracker.minimumEnterConfidence = 0.45;
    tracker.minimumHoldConfidence = 0.25;
    tracker.maximumAfcCorrectionHz = 150.0;
    tracker.nominalSyncDurationSamples = syncSamples;
    // Robot B/W 8 interoperability transmitters (including pinned PySSTV)
    // use a 7 ms sync / 60 ms scan partition while retaining the same VIS
    // identity.  Accept that discrete pulse alongside the native 10 ms
    // profile; all other Robot modes keep their canonical timing envelope.
    tracker.syncDurationToleranceSamples = std::max<std::uint64_t>(
        3U,
        spec.mode == SstvRobotMode::Bw8
            ? syncSamples / 2U : syncSamples / 3U);
    tracker.nominalLinePeriodSamples = trackerLineSamples;
    tracker.lineTimingToleranceSamples = std::max<std::uint64_t>(
        12U, trackerLineSamples / 40U);
    tracker.enterDebounceSamples = std::max<std::uint64_t>(
        2U, syncSamples / 4U);
    tracker.exitDebounceSamples = std::max<std::uint64_t>(
        2U, syncSamples / 6U);
    tracker.maximumObservationGapSamples = std::max<std::uint64_t>(
        static_cast<std::uint64_t>(config.observationSpanSamples) * 4U,
        tracker.exitDebounceSamples);
    tracker.maximumPredictedLines = 8U;
    tracker.reacquireConfirmations = 2U;
    tracker.slant.nominalLinePeriodSamples = trackerLineSamples;
    tracker.slant.windowLines = 32U;
    tracker.slant.minimumLines = 4U;
    tracker.slant.minimumConfidence = 0.25;
    tracker.slant.outlierToleranceSamples = std::max(
        8.0, static_cast<double>(trackerLineSamples) * 0.002);
    tracker.slant.warningClockErrorPpm = 300.0;
    tracker.slant.maximumClockErrorPpm = 5'000.0;
    return tracker;
}

void SstvRobotRxSession::applySyncEvents(
    const std::vector<SstvSyncEvent>& events,
    SstvRobotRxSessionUpdate* update)
{
    saturatingAdd(metrics_.syncEvents,
                  static_cast<std::uint64_t>(events.size()));
    std::vector<SstvRobotLineSync> syncs;
    syncs.reserve(std::min<std::size_t>(
        events.size(), SstvRobotDecoder::MaximumSyncsPerConsume));
    const auto flush = [&] {
        if (!syncs.empty()) {
            static_cast<void>(decoder_.consumeLineSyncs(syncs));
            syncs.clear();
        }
    };
    for (const SstvSyncEvent& event : events) {
        const bool observed =
            event.type == SstvSyncEventType::LineSyncObserved;
        const bool predicted =
            event.type == SstvSyncEventType::LineSyncPredicted;
        if ((!observed && !predicted) || event.lineIndex >= spec_.height) {
            continue;
        }
        syncs.push_back({static_cast<std::uint32_t>(event.lineIndex),
                         event.syncStartSample,
                         event.confidence,
                         predicted});
        if (syncs.size() == SstvRobotDecoder::MaximumSyncsPerConsume) {
            flush();
        }
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
    flush();
}

void SstvRobotRxSession::refineBw8ImageEndFromObservedSync()
{
    if (spec_.mode != SstvRobotMode::Bw8
        || config_.preserveTerminalGuard
        || imageEndSample_ == canonicalImageEndSample_) {
        return;
    }

    const SstvSyncTrackerSnapshot sync = syncTracker_.snapshot();
    if (!sync.slant.valid
        || sync.slant.confidence
            < syncTracker_.config().slant.minimumConfidence) {
        return;
    }

    const double lineCount = static_cast<double>(spec_.height);
    const double canonicalLineSamples = static_cast<double>(
        canonicalImageEndSample_ - config_.imageStartSample) / lineCount;
    const double compatibilityLineSamples = static_cast<double>(
        imageEndSample_ - config_.imageStartSample) / lineCount;
    const double observedLineSamples =
        sync.slant.estimatedLinePeriodSamples;
    const double canonicalDistance = std::abs(
        observedLineSamples - canonicalLineSamples);
    const double compatibilityDistance = std::abs(
        observedLineSamples - compatibilityLineSamples);

    // Four or more observed (never predicted) sync anchors feed the robust
    // slant estimate.  Shorten the acquisition window only when that estimate
    // unambiguously identifies native 66 ms timing.  An ambiguous estimate
    // deliberately retains the longer 67 ms compatibility extent so a peer
    // using the alternate Robot B/W 8 profile cannot be clipped.
    constexpr double profileDecisionMarginSamples = 1.0;
    if (canonicalDistance + profileDecisionMarginSamples
        < compatibilityDistance) {
        imageEndSample_ = canonicalImageEndSample_;
    }
}

void SstvRobotRxSession::updateTerminalState(
    SstvRobotDecodeState decoderState) noexcept
{
    switch (decoderState) {
    case SstvRobotDecodeState::Receiving:
        state_ = SstvRobotRxSessionState::Receiving;
        break;
    case SstvRobotDecodeState::Complete:
        state_ = SstvRobotRxSessionState::Complete;
        break;
    case SstvRobotDecodeState::Partial:
        state_ = SstvRobotRxSessionState::Partial;
        break;
    case SstvRobotDecodeState::Cancelled:
        state_ = SstvRobotRxSessionState::Cancelled;
        break;
    }
}

} // namespace decodium::sstv
