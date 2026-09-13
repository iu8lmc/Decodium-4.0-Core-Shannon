// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvMartinM1RxSession.h"

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
        ? centreSample - leftSpan
        : 0U;
    const std::uint64_t span = spanSamples;
    return span > std::numeric_limits<std::uint64_t>::max() - intervalStart
        ? std::numeric_limits<std::uint64_t>::max()
        : intervalStart + span;
}

} // namespace

SstvMartinM1RxSession::SstvMartinM1RxSession(
    SstvMartinM1RxSessionConfig config)
    : config_([&config] {
        validateConfig(config);
        return config;
    }())
    , spec_(SstvMartinM1Protocol::spec(config_.mode))
    , mapper_(SstvMartinM1MapperConfig {
          config_.sampleRate, config_.clockErrorPpm, config_.mode})
    , syncTracker_(makeSyncConfig(config_))
    , decoder_(SstvMartinM1DecoderConfig {
          config_.sampleRate,
          config_.clockErrorPpm,
          config_.frequencyOffsetHz,
          config_.minimumObservationConfidence,
          config_.maximumPendingDirtyEvents,
          config_.mode})
{
    if (mapper_.imageSampleCount()
        > std::numeric_limits<std::uint64_t>::max()
            - config_.imageStartSample) {
        throw std::overflow_error("Martin M1 RX image sample range overflow");
    }
    imageEndSample_ = config_.imageStartSample + mapper_.imageSampleCount();

    const std::uint64_t syncSamples = samplesFor(
        config_.sampleRate, spec_.syncDuration);
    if (syncSamples == 0U
        || syncSamples > std::numeric_limits<std::uint64_t>::max()
            - config_.imageStartSample) {
        throw std::overflow_error("Martin M1 RX initial sync range overflow");
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
        throw std::logic_error("Martin M1 RX failed to seed line-zero sync");
    }
}

SstvMartinM1RxSessionUpdate SstvMartinM1RxSession::consume(
    const SstvFrequencyObservation* observations,
    std::size_t count)
{
    if (count > MaximumObservationsPerConsume) {
        throw std::length_error(
            "Martin M1 RX session observation call exceeds its work bound");
    }
    if (count != 0U && observations == nullptr) {
        throw std::invalid_argument(
            "Martin M1 RX session observations must not be null");
    }

    SstvMartinM1RxSessionUpdate update;
    update.inputObservations = count;
    update.publishedLineRevision = decoder_.metrics().linesPublished;
    update.linesPublished = update.publishedLineRevision;
    saturatingAdd(metrics_.consumeCalls);
    saturatingAdd(metrics_.inputObservations,
                  static_cast<std::uint64_t>(count));
    if (count == 0U || state_ != SstvMartinM1RxSessionState::Receiving) {
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
        reachedEnd = reachedEnd
            || lastInputEndSample_ >= imageEndSample_;

        filtered.push_back(observation);
        const bool numeric = std::isfinite(observation.rawFrequencyHz)
            && std::isfinite(observation.afcCorrectionHz)
            && isFiniteUnit(observation.confidence);
        // Line zero is already an explicit pulse.  Do not replay frequency
        // windows wholly behind that seeded tracker timeline; doing so would
        // manufacture clock regressions solely from coordinator startup.
        if (lastInputEndSample_ > initialSyncEndSample_) {
            lastSyncInputEndSample_ = lastInputEndSample_;
            syncObservations.push_back(SstvSyncFrequencyObservation {
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
        const auto events = syncTracker_.consume(syncObservations);
        applySyncEvents(events, &update);
    }

    const auto before = decoder_.metrics();
    if (!filtered.empty()) {
        update.decoderAcceptedObservations = decoder_.consume(filtered);
        saturatingAdd(metrics_.decoderAcceptedObservations,
                      static_cast<std::uint64_t>(
                          update.decoderAcceptedObservations));
    }
    const auto after = decoder_.metrics();
    update.linesPublished = after.linesPublished;
    update.publishedLineRevision = after.linesPublished;
    update.imageChanged = after.linesPublished > before.linesPublished;

    if (reachedEnd) {
        update.reachedImageEnd = true;
        finish();
        const auto terminalMetrics = decoder_.metrics();
        update.linesPublished = terminalMetrics.linesPublished;
        update.publishedLineRevision = terminalMetrics.linesPublished;
        update.imageChanged = update.imageChanged
            || terminalMetrics.linesPublished > after.linesPublished;
    }
    return update;
}

SstvMartinM1RxSessionUpdate SstvMartinM1RxSession::consume(
    const std::vector<SstvFrequencyObservation>& observations)
{
    return consume(observations.data(), observations.size());
}

SstvMartinM1RxSessionState SstvMartinM1RxSession::notifyDiscontinuity(
    std::uint64_t nextSample)
{
    if (state_ != SstvMartinM1RxSessionState::Receiving) {
        return state_;
    }
    saturatingAdd(metrics_.discontinuities);
    const auto events = syncTracker_.notifyDiscontinuity(nextSample);
    applySyncEvents(events, nullptr);
    updateTerminalState(decoder_.finish());
    return state_;
}

SstvMartinM1RxSessionState SstvMartinM1RxSession::finish()
{
    saturatingAdd(metrics_.finishCalls);
    if (state_ != SstvMartinM1RxSessionState::Receiving) {
        return state_;
    }
    const auto events = syncTracker_.flush(lastSyncInputEndSample_);
    applySyncEvents(events, nullptr);
    updateTerminalState(decoder_.finish());
    return state_;
}

void SstvMartinM1RxSession::cancel() noexcept
{
    saturatingAdd(metrics_.cancelCalls);
    if (state_ != SstvMartinM1RxSessionState::Receiving) {
        return;
    }
    decoder_.cancel();
    state_ = SstvMartinM1RxSessionState::Cancelled;
}

SstvMartinM1RxSessionState SstvMartinM1RxSession::state() const noexcept
{
    return state_;
}

SstvMartinMode SstvMartinM1RxSession::mode() const noexcept
{
    return spec_.mode;
}

const SstvMartinM1RxSessionConfig& SstvMartinM1RxSession::config() const
    noexcept
{
    return config_;
}

std::uint64_t SstvMartinM1RxSession::imageStartSample() const noexcept
{
    return config_.imageStartSample;
}

std::uint64_t SstvMartinM1RxSession::imageEndSample() const noexcept
{
    return imageEndSample_;
}

const SstvImageFrame& SstvMartinM1RxSession::imageFrame() const noexcept
{
    return decoder_.imageFrame();
}

SstvImageSnapshot SstvMartinM1RxSession::snapshot() const
{
    return decoder_.snapshot();
}

std::vector<SstvDirtyEvent> SstvMartinM1RxSession::takeDirtyEvents()
{
    return decoder_.takeDirtyEvents();
}

SstvMartinM1RxSessionMetrics SstvMartinM1RxSession::metrics() const noexcept
{
    return metrics_;
}

SstvSyncTrackerSnapshot SstvMartinM1RxSession::syncSnapshot() const
{
    return syncTracker_.snapshot();
}

SstvMartinM1DecoderMetrics SstvMartinM1RxSession::decoderMetrics() const
    noexcept
{
    return decoder_.metrics();
}

void SstvMartinM1RxSession::validateConfig(
    const SstvMartinM1RxSessionConfig& config)
{
    // The shared DSP pipeline normalises every live source to 12 kHz before
    // this coordinator.  SstvSyncTracker intentionally enforces that native
    // clock, while the lower-level mapper/codec remain reusable at other rates.
    if (config.sampleRate != 12'000U
        || config.observationSpanSamples == 0U
        || config.observationSpanSamples > 4'096U
        || config.clockErrorPpm < -5'000
        || config.clockErrorPpm > 5'000
        || !std::isfinite(config.frequencyOffsetHz)
        || std::abs(config.frequencyOffsetHz)
            > SstvMartinM1Decoder::MaximumAbsoluteFrequencyOffsetHz
        || !isFiniteUnit(config.minimumObservationConfidence)
        || config.maximumPendingDirtyEvents == 0U
        || config.maximumPendingDirtyEvents
            > SstvImageFrame::kMaximumDirtyEvents) {
        throw std::invalid_argument(
            "invalid Martin M1 RX session configuration");
    }
}

void SstvMartinM1RxSession::saturatingAdd(std::uint64_t& value,
                                          std::uint64_t increment) noexcept
{
    value = increment > std::numeric_limits<std::uint64_t>::max() - value
        ? std::numeric_limits<std::uint64_t>::max()
        : value + increment;
}

SstvSyncTrackerConfig SstvMartinM1RxSession::makeSyncConfig(
    const SstvMartinM1RxSessionConfig& config)
{
    const SstvMartinModeSpec spec = SstvMartinM1Protocol::spec(config.mode);
    const std::uint64_t syncSamples = samplesFor(
        config.sampleRate, spec.syncDuration);
    const std::uint64_t lineSamples = samplesFor(
        config.sampleRate, spec.lineDuration);
    if (syncSamples == 0U || lineSamples == 0U) {
        throw std::invalid_argument("Martin M1 RX timing rounds to zero samples");
    }

    SstvSyncTrackerConfig tracker;
    tracker.sampleRateHz = static_cast<double>(config.sampleRate);
    // The decoder subtracts the explicit receiver calibration from its
    // corrected pixel frequencies.  Track sync against the same calibrated
    // frequency domain (raw - AFC) so a valid +/- offset cannot silently
    // disable line acquisition.
    tracker.syncFrequencyHz = SstvMartinM1Protocol::SyncFrequencyHz
        + config.frequencyOffsetHz;
    tracker.enterFrequencyToleranceHz = 55.0;
    tracker.exitFrequencyToleranceHz = 105.0;
    tracker.minimumEnterConfidence = 0.45;
    tracker.minimumHoldConfidence = 0.25;
    tracker.maximumAfcCorrectionHz = 150.0;
    tracker.nominalSyncDurationSamples = syncSamples;
    tracker.syncDurationToleranceSamples = std::max<std::uint64_t>(
        3U, syncSamples / 3U);
    tracker.nominalLinePeriodSamples = lineSamples;
    tracker.lineTimingToleranceSamples = std::max<std::uint64_t>(
        12U, lineSamples / 40U);
    tracker.enterDebounceSamples = std::max<std::uint64_t>(
        2U, syncSamples / 4U);
    tracker.exitDebounceSamples = std::max<std::uint64_t>(
        2U, syncSamples / 6U);
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

void SstvMartinM1RxSession::applySyncEvents(
    const std::vector<SstvSyncEvent>& events,
    SstvMartinM1RxSessionUpdate* update)
{
    saturatingAdd(metrics_.syncEvents,
                  static_cast<std::uint64_t>(events.size()));
    std::vector<SstvMartinM1LineSync> syncs;
    syncs.reserve(std::min<std::size_t>(
        events.size(), SstvMartinM1Decoder::MaximumSyncsPerConsume));
    auto flushSyncs = [&] {
        if (!syncs.empty()) {
            decoder_.consumeLineSyncs(syncs);
            syncs.clear();
        }
    };
    for (const SstvSyncEvent& event : events) {
        const bool observed =
            event.type == SstvSyncEventType::LineSyncObserved;
        const bool predicted =
            event.type == SstvSyncEventType::LineSyncPredicted;
        if ((!observed && !predicted)
            || event.lineIndex >= spec_.height) {
            continue;
        }
        syncs.push_back(SstvMartinM1LineSync {
            static_cast<std::uint32_t>(event.lineIndex),
            event.syncStartSample,
            event.confidence,
            predicted});
        if (syncs.size()
            == SstvMartinM1Decoder::MaximumSyncsPerConsume) {
            flushSyncs();
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
    flushSyncs();
}

void SstvMartinM1RxSession::updateTerminalState(
    SstvMartinM1DecodeState decoderState) noexcept
{
    switch (decoderState) {
    case SstvMartinM1DecodeState::Receiving:
        state_ = SstvMartinM1RxSessionState::Receiving;
        break;
    case SstvMartinM1DecodeState::Complete:
        state_ = SstvMartinM1RxSessionState::Complete;
        break;
    case SstvMartinM1DecodeState::Partial:
        state_ = SstvMartinM1RxSessionState::Partial;
        break;
    case SstvMartinM1DecodeState::Cancelled:
        state_ = SstvMartinM1RxSessionState::Cancelled;
        break;
    }
}

} // namespace decodium::sstv
