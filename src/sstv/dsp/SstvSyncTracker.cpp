// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvSyncTracker.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace decodium::sstv
{
namespace
{
constexpr double NativeSampleRateHz = 12'000.0;
constexpr std::uint64_t MaximumReacquireLineAdvance = 1'000'000u;
}

SstvSyncTracker::SstvSyncTracker (SstvSyncTrackerConfig config)
    : config_ {[&config] {
        validateConfig (config);
        return config;
      } ()}
    , slantEstimator_ {config_.slant}
    , retainedPeriodSamples_ {
          static_cast<double> (config_.nominalLinePeriodSamples)}
{
  reacquirePulses_.reserve (config_.reacquireConfirmations);
}

std::vector<SstvSyncEvent> SstvSyncTracker::consume (
    SstvSyncFrequencyObservation const* observations,
    std::size_t count)
{
  std::lock_guard<std::mutex> const lock {mutex_};
  slantUpdatesThisCall_ = 0u;
  return consumeUnlocked (observations, count);
}

std::vector<SstvSyncEvent> SstvSyncTracker::consume (
    std::vector<SstvSyncFrequencyObservation> const& observations)
{
  return consume (observations.data (), observations.size ());
}

std::vector<SstvSyncEvent> SstvSyncTracker::consumeExplicit (
    SstvExplicitSyncPulse const* pulses,
    std::size_t count)
{
  std::lock_guard<std::mutex> const lock {mutex_};
  slantUpdatesThisCall_ = 0u;
  std::vector<SstvSyncEvent> events;
  if (count == 0u)
    {
      return events;
    }
  if (pulses == nullptr || count > MaximumExplicitPulsesPerConsume)
    {
      saturatingAdd (metrics_.rejectedInputCalls, 1u);
      saturatingAdd (metrics_.rejectedInputItems,
                     static_cast<std::uint64_t> (count));
      if (count > MaximumExplicitPulsesPerConsume)
        {
          saturatingAdd (metrics_.rejectedOversizeCalls, 1u);
        }
      return events;
    }

  std::size_t const reserveCount = std::min (
      MaximumEventsPerCall,
      count * 4u + static_cast<std::size_t> (config_.maximumPredictedLines)
          + 4u);
  events.reserve (reserveCount);
  for (std::size_t index = 0u; index < count; ++index)
    {
      processExplicitPulse (pulses[index], events);
    }
  return events;
}

std::vector<SstvSyncEvent> SstvSyncTracker::consumeExplicit (
    std::vector<SstvExplicitSyncPulse> const& pulses)
{
  return consumeExplicit (pulses.data (), pulses.size ());
}

std::vector<SstvSyncEvent> SstvSyncTracker::advanceTo (
    std::uint64_t sampleIndex)
{
  std::lock_guard<std::mutex> const lock {mutex_};
  slantUpdatesThisCall_ = 0u;
  std::vector<SstvSyncEvent> events;
  events.reserve (static_cast<std::size_t> (config_.maximumPredictedLines)
                  + 2u);
  if (timelineValid_ && sampleIndex < lastTimelineSample_)
    {
      discontinuityUnlocked (sampleIndex,
                              SstvSyncRejectReason::ClockRegression,
                              events);
      return events;
    }
  advancePredictions (sampleIndex, events);
  timelineValid_ = true;
  lastTimelineSample_ = sampleIndex;
  return events;
}

std::vector<SstvSyncEvent> SstvSyncTracker::flush (std::uint64_t endSample)
{
  std::lock_guard<std::mutex> const lock {mutex_};
  slantUpdatesThisCall_ = 0u;
  std::vector<SstvSyncEvent> events;
  events.reserve (static_cast<std::size_t> (config_.maximumPredictedLines)
                  + 4u);
  if (timelineValid_ && endSample < lastTimelineSample_)
    {
      discontinuityUnlocked (endSample,
                              SstvSyncRejectReason::ClockRegression,
                              events);
      return events;
    }
  if (syncPulseActive_)
    {
      finishPulse (endSample, 0.0, events);
    }
  else if (toneCandidateActive_)
    {
      SstvSyncEvent rejected;
      rejected.type = SstvSyncEventType::PulseRejected;
      rejected.rejectReason = SstvSyncRejectReason::Debounce;
      rejected.sampleIndex = endSample;
      rejected.syncStartSample = toneCandidateStart_;
      rejected.syncEndSample = endSample;
      rejected.syncDurationSamples = endSample >= toneCandidateStart_
                                         ? endSample - toneCandidateStart_
                                         : 0u;
      emitEvent (rejected, events);
      saturatingAdd (metrics_.rejectedPulses, 1u);
      saturatingAdd (metrics_.debounceRejects, 1u);
      toneCandidateActive_ = false;
      toneCandidateCoverage_ = 0u;
    }
  advancePredictions (endSample, events);
  timelineValid_ = true;
  lastTimelineSample_ = endSample;
  return events;
}

std::vector<SstvSyncEvent> SstvSyncTracker::notifyDiscontinuity (
    std::uint64_t newSampleIndex)
{
  std::lock_guard<std::mutex> const lock {mutex_};
  slantUpdatesThisCall_ = 0u;
  std::vector<SstvSyncEvent> events;
  events.reserve (1u);
  discontinuityUnlocked (newSampleIndex,
                          SstvSyncRejectReason::ExternalDiscontinuity,
                          events);
  return events;
}

void SstvSyncTracker::reset ()
{
  std::lock_guard<std::mutex> const lock {mutex_};
  metrics_ = {};
  eventSequence_ = 0u;
  slantUpdatesThisCall_ = 0u;
  retainedPeriodSamples_ = static_cast<double> (
      config_.nominalLinePeriodSamples);
  slantEstimator_.reset ();
  clearAcquisitionStateUnlocked (true);
}

SstvSyncTrackerSnapshot SstvSyncTracker::snapshot () const
{
  std::lock_guard<std::mutex> const lock {mutex_};
  SstvSyncTrackerSnapshot result;
  result.state = state_;
  result.toneCandidateActive = toneCandidateActive_;
  result.syncPulseActive = syncPulseActive_;
  result.hasExpectedSync = haveExpectedSync_;
  result.nextLineIndex = nextLineIndex_;
  result.nextExpectedSyncSample = nextExpectedSync_;
  result.consecutivePredictedLines = consecutivePredictedLines_;
  result.estimatedLinePeriodSamples = retainedPeriodSamples_;
  result.clockErrorPpm = currentClockErrorPpm ();
  result.slant = slantEstimator_.snapshot ();
  result.metrics = metrics_;
  result.metrics.bufferedTimingObservations =
      result.slant.observationCount + reacquirePulses_.size ();
  return result;
}

SstvSyncTrackerConfig const& SstvSyncTracker::config () const noexcept
{
  return config_;
}

std::size_t SstvSyncTracker::bufferedTimingObservationCount () const
{
  std::lock_guard<std::mutex> const lock {mutex_};
  return slantEstimator_.bufferedObservationCount ()
         + reacquirePulses_.size ();
}

std::size_t
SstvSyncTracker::maximumBufferedTimingObservationCount () const noexcept
{
  return config_.slant.windowLines + config_.reacquireConfirmations;
}

std::vector<SstvSyncEvent> SstvSyncTracker::consumeUnlocked (
    SstvSyncFrequencyObservation const* observations,
    std::size_t count)
{
  std::vector<SstvSyncEvent> events;
  if (count == 0u)
    {
      return events;
    }
  if (observations == nullptr
      || count > MaximumFrequencyObservationsPerConsume)
    {
      saturatingAdd (metrics_.rejectedInputCalls, 1u);
      saturatingAdd (metrics_.rejectedInputItems,
                     static_cast<std::uint64_t> (count));
      if (count > MaximumFrequencyObservationsPerConsume)
        {
          saturatingAdd (metrics_.rejectedOversizeCalls, 1u);
        }
      return events;
    }

  std::size_t const reserveCount = std::min (
      MaximumEventsPerCall,
      count * 2u + static_cast<std::size_t> (config_.maximumPredictedLines)
          + 8u);
  events.reserve (reserveCount);
  for (std::size_t index = 0u; index < count; ++index)
    {
      processFrequencyObservation (observations[index], events);
    }
  return events;
}

void SstvSyncTracker::processFrequencyObservation (
    SstvSyncFrequencyObservation const& observation,
    std::vector<SstvSyncEvent>& events)
{
  saturatingAdd (metrics_.frequencyObservations, 1u);
  bool const numericValid = std::isfinite (observation.measuredFrequencyHz)
                            && observation.measuredFrequencyHz >= 0.0
                            && observation.measuredFrequencyHz
                                   < config_.sampleRateHz / 2.0
                            && std::isfinite (observation.afcCorrectionHz)
                            && std::abs (observation.afcCorrectionHz)
                                   <= config_.maximumAfcCorrectionHz
                            && std::isfinite (observation.confidence)
                            && observation.confidence >= 0.0
                            && observation.confidence <= 1.0
                            && observation.spanSamples > 0u
                            && observation.spanSamples
                                   <= MaximumObservationSpanSamples
                            && observation.spanSamples
                                   <= config_.maximumObservationGapSamples;
  if (!numericValid)
    {
      saturatingAdd (metrics_.invalidFrequencyObservations, 1u);
      return;
    }

  std::uint64_t const leftSpan = observation.spanSamples / 2u;
  std::uint64_t const intervalStart = observation.centreSample >= leftSpan
                                          ? observation.centreSample - leftSpan
                                          : 0u;
  std::uint64_t intervalEnd = 0u;
  if (!checkedAdd (intervalStart,
                   static_cast<std::uint64_t> (observation.spanSamples),
                   intervalEnd))
    {
      saturatingAdd (metrics_.invalidFrequencyObservations, 1u);
      discontinuityUnlocked (observation.centreSample,
                              SstvSyncRejectReason::NumericOverflow,
                              events);
      return;
    }

  bool const centreRegression = haveLastObservation_
                                && observation.centreSample
                                       <= lastObservationCentre_;
  bool const fullyBehindTimeline = timelineValid_
                                   && intervalEnd <= lastTimelineSample_;
  if (centreRegression || fullyBehindTimeline)
    {
      SstvSyncEvent rejected;
      rejected.type = SstvSyncEventType::ObservationRejected;
      rejected.rejectReason = SstvSyncRejectReason::ClockRegression;
      rejected.sampleIndex = observation.centreSample;
      rejected.syncStartSample = intervalStart;
      rejected.syncEndSample = intervalEnd;
      rejected.syncDurationSamples = intervalEnd - intervalStart;
      rejected.correctedFrequencyHz = observation.measuredFrequencyHz
                                      - observation.afcCorrectionHz;
      rejected.confidence = observation.confidence;
      emitEvent (rejected, events);
      saturatingAdd (metrics_.invalidFrequencyObservations, 1u);
      saturatingAdd (metrics_.staleInputsRejected, 1u);
      saturatingAdd (metrics_.clockRegressions, 1u);
      return;
    }

  bool const gap = haveLastObservation_
                   && observation.centreSample - lastObservationCentre_
                          > config_.maximumObservationGapSamples;
  std::uint64_t processingStart = intervalStart;
  if (haveLastObservation_ && !gap)
    {
      processingStart = std::max (processingStart, lastObservationEnd_);
    }
  if (timelineValid_)
    {
      processingStart = std::max (processingStart, lastTimelineSample_);
    }
  if (gap)
    {
      if (syncPulseActive_)
        {
          finishPulse (lastObservationEnd_, 0.0, events);
        }
      else if (toneCandidateActive_)
        {
          SstvSyncEvent rejected;
          rejected.type = SstvSyncEventType::PulseRejected;
          rejected.rejectReason = SstvSyncRejectReason::Debounce;
          rejected.sampleIndex = lastObservationEnd_;
          rejected.syncStartSample = toneCandidateStart_;
          rejected.syncEndSample = lastObservationEnd_;
          rejected.syncDurationSamples =
              lastObservationEnd_ >= toneCandidateStart_
                  ? lastObservationEnd_ - toneCandidateStart_
                  : 0u;
          emitEvent (rejected, events);
          saturatingAdd (metrics_.rejectedPulses, 1u);
          saturatingAdd (metrics_.debounceRejects, 1u);
          toneCandidateActive_ = false;
          toneCandidateCoverage_ = 0u;
        }
    }

  if (!toneCandidateActive_ && !syncPulseActive_)
    {
      advancePredictions (processingStart, events);
    }

  double const correctedFrequency = observation.measuredFrequencyHz
                                    - observation.afcCorrectionHz;
  bool const enterTone = observation.valid
                         && observation.confidence
                                >= config_.minimumEnterConfidence
                         && std::abs (correctedFrequency
                                      - config_.syncFrequencyHz)
                                <= config_.enterFrequencyToleranceHz;
  bool const holdTone = observation.valid
                        && observation.confidence
                               >= config_.minimumHoldConfidence
                        && std::abs (correctedFrequency
                                     - config_.syncFrequencyHz)
                               <= config_.exitFrequencyToleranceHz;
  if (intervalEnd > processingStart)
    {
      processToneInterval (processingStart,
                           intervalEnd,
                           enterTone,
                           holdTone,
                           correctedFrequency,
                           observation.confidence,
                           events);
    }
  if (!toneCandidateActive_ && !syncPulseActive_)
    {
      advancePredictions (intervalEnd, events);
    }

  haveLastObservation_ = true;
  lastObservationCentre_ = observation.centreSample;
  lastObservationEnd_ = std::max (lastObservationEnd_, intervalEnd);
  timelineValid_ = true;
  lastTimelineSample_ = std::max (lastTimelineSample_, intervalEnd);
}

void SstvSyncTracker::processExplicitPulse (
    SstvExplicitSyncPulse const& pulse,
    std::vector<SstvSyncEvent>& events)
{
  saturatingAdd (metrics_.explicitPulses, 1u);
  if (pulse.endSample <= pulse.startSample
      || !std::isfinite (pulse.confidence) || pulse.confidence < 0.0
      || pulse.confidence > 1.0
      || pulse.confidence < config_.minimumEnterConfidence)
    {
      saturatingAdd (metrics_.rejectedPulses, 1u);
      SstvSyncEvent rejected;
      rejected.type = SstvSyncEventType::PulseRejected;
      rejected.rejectReason = SstvSyncRejectReason::InvalidInput;
      rejected.sampleIndex = pulse.startSample;
      rejected.syncStartSample = pulse.startSample;
      rejected.syncEndSample = pulse.endSample;
      rejected.confidence = std::isfinite (pulse.confidence)
                                ? pulse.confidence
                                : 0.0;
      emitEvent (rejected, events);
      return;
    }
  if (timelineValid_ && pulse.startSample < lastTimelineSample_)
    {
      emitPulseRejectedUnlocked (pulse,
                                 config_.syncFrequencyHz,
                                 SstvSyncRejectReason::ClockRegression,
                                 events);
      saturatingAdd (metrics_.staleInputsRejected, 1u);
      saturatingAdd (metrics_.clockRegressions, 1u);
      return;
    }
  advancePredictions (pulse.startSample, events);

  SstvSyncEvent started;
  started.type = SstvSyncEventType::SyncStarted;
  started.sampleIndex = pulse.startSample;
  started.syncStartSample = pulse.startSample;
  started.confidence = pulse.confidence;
  emitEvent (started, events);
  saturatingAdd (metrics_.syncStarts, 1u);

  SstvSyncEvent ended = started;
  ended.type = SstvSyncEventType::SyncEnded;
  ended.sampleIndex = pulse.endSample;
  ended.syncEndSample = pulse.endSample;
  ended.syncDurationSamples = pulse.endSample - pulse.startSample;
  emitEvent (ended, events);
  saturatingAdd (metrics_.syncEnds, 1u);
  acceptPulse (pulse, config_.syncFrequencyHz, events);
  timelineValid_ = true;
  lastTimelineSample_ = pulse.endSample;
}

void SstvSyncTracker::processToneInterval (
    std::uint64_t intervalStart,
    std::uint64_t intervalEnd,
    bool enterTone,
    bool holdTone,
    double correctedFrequencyHz,
    double confidence,
    std::vector<SstvSyncEvent>& events)
{
  std::uint64_t const span = intervalEnd - intervalStart;
  if (syncPulseActive_)
    {
      if (holdTone)
        {
          exitCandidateCoverage_ = 0u;
          saturatingAdd (pulseConfidenceCount_, 1u);
          pulseConfidenceSum_ += confidence;
          saturatingAdd (pulseFrequencyCount_, 1u);
          pulseFrequencySum_ += correctedFrequencyHz;
          std::uint64_t const maximumDuration =
              config_.nominalSyncDurationSamples
              + config_.syncDurationToleranceSamples;
          if (intervalEnd >= pulseStart_
              && intervalEnd - pulseStart_ > maximumDuration)
            {
              // A continuously held sync tone must not suspend line-clock
              // prediction forever.  Close it as soon as the configured
              // physical duration bound is crossed; acceptPulse() reports the
              // deterministic Duration rejection and normal tracking resumes.
              finishPulse (intervalEnd, correctedFrequencyHz, events);
            }
          return;
        }
      if (exitCandidateCoverage_ == 0u)
        {
          exitCandidateStart_ = intervalStart;
        }
      saturatingAdd (exitCandidateCoverage_, span);
      if (exitCandidateCoverage_ >= config_.exitDebounceSamples)
        {
          finishPulse (exitCandidateStart_, correctedFrequencyHz, events);
        }
      return;
    }

  if (toneCandidateActive_)
    {
      if (!enterTone)
        {
          SstvSyncEvent rejected;
          rejected.type = SstvSyncEventType::PulseRejected;
          rejected.rejectReason = SstvSyncRejectReason::Debounce;
          rejected.sampleIndex = intervalStart;
          rejected.syncStartSample = toneCandidateStart_;
          rejected.syncEndSample = intervalStart;
          rejected.syncDurationSamples = intervalStart >= toneCandidateStart_
                                             ? intervalStart
                                                   - toneCandidateStart_
                                             : 0u;
          emitEvent (rejected, events);
          saturatingAdd (metrics_.rejectedPulses, 1u);
          saturatingAdd (metrics_.debounceRejects, 1u);
          toneCandidateActive_ = false;
          toneCandidateCoverage_ = 0u;
          pulseConfidenceSum_ = 0.0;
          pulseConfidenceCount_ = 0u;
          pulseFrequencySum_ = 0.0;
          pulseFrequencyCount_ = 0u;
          return;
        }
      saturatingAdd (toneCandidateCoverage_, span);
      saturatingAdd (pulseConfidenceCount_, 1u);
      pulseConfidenceSum_ += confidence;
      saturatingAdd (pulseFrequencyCount_, 1u);
      pulseFrequencySum_ += correctedFrequencyHz;
    }
  else if (enterTone)
    {
      toneCandidateActive_ = true;
      toneCandidateStart_ = intervalStart;
      toneCandidateCoverage_ = span;
      pulseConfidenceSum_ = confidence;
      pulseConfidenceCount_ = 1u;
      pulseFrequencySum_ = correctedFrequencyHz;
      pulseFrequencyCount_ = 1u;
    }

  if (toneCandidateActive_
      && toneCandidateCoverage_ >= config_.enterDebounceSamples)
    {
      syncPulseActive_ = true;
      toneCandidateActive_ = false;
      pulseStart_ = toneCandidateStart_;
      exitCandidateCoverage_ = 0u;
      SstvSyncEvent started;
      started.type = SstvSyncEventType::SyncStarted;
      started.sampleIndex = pulseStart_;
      started.syncStartSample = pulseStart_;
      started.correctedFrequencyHz = pulseFrequencyCount_ == 0u
                                         ? 0.0
                                         : pulseFrequencySum_
                                               / static_cast<double> (
                                                   pulseFrequencyCount_);
      started.confidence = pulseConfidenceCount_ == 0u
                               ? 0.0
                               : pulseConfidenceSum_
                                     / static_cast<double> (
                                         pulseConfidenceCount_);
      emitEvent (started, events);
      saturatingAdd (metrics_.syncStarts, 1u);

      std::uint64_t const maximumDuration =
          config_.nominalSyncDurationSamples
          + config_.syncDurationToleranceSamples;
      if (intervalEnd >= pulseStart_
          && intervalEnd - pulseStart_ > maximumDuration)
        {
          // A single coarse observation can cross both the entry debounce and
          // the duration ceiling.  Apply the same bound as the already-active
          // path in this call rather than waiting for another observation.
          finishPulse (intervalEnd, correctedFrequencyHz, events);
        }
    }
}

void SstvSyncTracker::finishPulse (
    std::uint64_t endSample,
    double correctedFrequencyHz,
    std::vector<SstvSyncEvent>& events)
{
  if (!syncPulseActive_)
    {
      return;
    }
  double const averageConfidence = pulseConfidenceCount_ == 0u
                                       ? 0.0
                                       : pulseConfidenceSum_
                                             / static_cast<double> (
                                                 pulseConfidenceCount_);
  double const averageFrequency = pulseFrequencyCount_ == 0u
                                      ? correctedFrequencyHz
                                      : pulseFrequencySum_
                                            / static_cast<double> (
                                                pulseFrequencyCount_);
  SstvSyncEvent ended;
  ended.type = SstvSyncEventType::SyncEnded;
  ended.sampleIndex = endSample;
  ended.syncStartSample = pulseStart_;
  ended.syncEndSample = endSample;
  ended.syncDurationSamples = endSample >= pulseStart_
                                  ? endSample - pulseStart_
                                  : 0u;
  ended.correctedFrequencyHz = averageFrequency;
  ended.confidence = averageConfidence;
  emitEvent (ended, events);
  saturatingAdd (metrics_.syncEnds, 1u);

  SstvExplicitSyncPulse pulse;
  pulse.startSample = pulseStart_;
  pulse.endSample = endSample;
  pulse.confidence = averageConfidence;
  syncPulseActive_ = false;
  toneCandidateActive_ = false;
  toneCandidateCoverage_ = 0u;
  exitCandidateCoverage_ = 0u;
  pulseConfidenceSum_ = 0.0;
  pulseConfidenceCount_ = 0u;
  pulseFrequencySum_ = 0.0;
  pulseFrequencyCount_ = 0u;
  acceptPulse (pulse, averageFrequency, events);
}

void SstvSyncTracker::acceptPulse (
    SstvExplicitSyncPulse const& pulse,
    double correctedFrequencyHz,
    std::vector<SstvSyncEvent>& events)
{
  saturatingAdd (metrics_.completedPulses, 1u);
  if (pulse.endSample <= pulse.startSample)
    {
      saturatingAdd (metrics_.rejectedPulses, 1u);
      SstvSyncEvent rejected;
      rejected.type = SstvSyncEventType::PulseRejected;
      rejected.rejectReason = SstvSyncRejectReason::InvalidInput;
      rejected.sampleIndex = pulse.startSample;
      emitEvent (rejected, events);
      return;
    }
  std::uint64_t const duration = pulse.endSample - pulse.startSample;
  std::uint64_t const minimumDuration =
      config_.nominalSyncDurationSamples
              > config_.syncDurationToleranceSamples
          ? config_.nominalSyncDurationSamples
                - config_.syncDurationToleranceSamples
          : 0u;
  std::uint64_t maximumDuration = 0u;
  if (!checkedAdd (config_.nominalSyncDurationSamples,
                   config_.syncDurationToleranceSamples,
                   maximumDuration))
    {
      saturatingAdd (metrics_.numericOverflows, 1u);
      maximumDuration = std::numeric_limits<std::uint64_t>::max ();
    }
  if (duration < minimumDuration || duration > maximumDuration)
    {
      saturatingAdd (metrics_.rejectedPulses, 1u);
      saturatingAdd (metrics_.durationRejects, 1u);
      SstvSyncEvent rejected;
      rejected.type = SstvSyncEventType::PulseRejected;
      rejected.rejectReason = SstvSyncRejectReason::Duration;
      rejected.sampleIndex = pulse.startSample;
      rejected.syncStartSample = pulse.startSample;
      rejected.syncEndSample = pulse.endSample;
      rejected.syncDurationSamples = duration;
      rejected.correctedFrequencyHz = correctedFrequencyHz;
      rejected.confidence = pulse.confidence;
      emitEvent (rejected, events);
      return;
    }

  saturatingAdd (metrics_.durationValidPulses, 1u);
  if (state_ == SstvSyncLockState::Reacquiring)
    {
      processReacquirePulse (pulse, correctedFrequencyHz, events);
      return;
    }
  if (state_ == SstvSyncLockState::Locked
      || state_ == SstvSyncLockState::Predicting)
    {
      acceptLockedPulse (pulse, correctedFrequencyHz, events);
      return;
    }

  SstvSlantUpdate slantUpdate;
  if (!observeSlantUnlocked (SstvSlantObservation {0u,
                                                   pulse.startSample,
                                                   pulse.confidence,
                                                   false},
                             slantUpdate))
    {
      emitPulseRejectedUnlocked (pulse,
                                 correctedFrequencyHz,
                                 SstvSyncRejectReason::WorkBudgetExceeded,
                                 events);
      return;
    }
  if (!slantUpdate.accepted)
    {
      saturatingAdd (metrics_.slantRejects, 1u);
      if (slantUpdate.status == SstvSlantStatus::OutlierRejected)
        {
          saturatingAdd (metrics_.timingOutliers, 1u);
        }
      emitPulseRejectedUnlocked (pulse,
                                 correctedFrequencyHz,
                                 SstvSyncRejectReason::SlantRejected,
                                 events);
      return;
    }

  state_ = SstvSyncLockState::Locked;
  nextLineIndex_ = 1u;
  consecutivePredictedLines_ = 0u;
  haveLastObservedLine_ = true;
  lastObservedLineIndex_ = 0u;
  lastObservedSyncStart_ = pulse.startSample;
  metrics_.peakBufferedTimingObservations = std::max (
      metrics_.peakBufferedTimingObservations,
      slantEstimator_.bufferedObservationCount ()
          + reacquirePulses_.size ());

  SstvSyncEvent acquired;
  acquired.type = SstvSyncEventType::LockAcquired;
  acquired.sampleIndex = pulse.startSample;
  acquired.syncStartSample = pulse.startSample;
  acquired.syncEndSample = pulse.endSample;
  acquired.syncDurationSamples = pulse.endSample - pulse.startSample;
  acquired.confidence = pulse.confidence;
  acquired.correctedFrequencyHz = correctedFrequencyHz;
  emitEvent (acquired, events);
  saturatingAdd (metrics_.lockAcquisitions, 1u);

  SstvSyncEvent line = acquired;
  line.type = SstvSyncEventType::LineSyncObserved;
  line.lineIndex = 0u;
  emitEvent (line, events);
  saturatingAdd (metrics_.observedLines, 1u);

  if (!checkedAdd (pulse.startSample,
                   config_.nominalLinePeriodSamples,
                   nextExpectedSync_))
    {
      loseLock (pulse.startSample,
                SstvSyncRejectReason::NumericOverflow,
                events);
      return;
    }
  haveExpectedSync_ = true;
}

void SstvSyncTracker::acceptLockedPulse (
    SstvExplicitSyncPulse const& pulse,
    double correctedFrequencyHz,
    std::vector<SstvSyncEvent>& events)
{
  if (!haveExpectedSync_)
    {
      loseLock (pulse.startSample,
                SstvSyncRejectReason::NumericOverflow,
                events);
      return;
    }
  double const timingError = signedDifference (pulse.startSample,
                                                nextExpectedSync_);
  if (std::abs (timingError)
      > static_cast<double> (config_.lineTimingToleranceSamples))
    {
      saturatingAdd (metrics_.timingOutliers, 1u);
      saturatingAdd (metrics_.rejectedPulses, 1u);
      SstvSyncEvent rejected;
      rejected.type = SstvSyncEventType::PulseRejected;
      rejected.rejectReason = SstvSyncRejectReason::TimingOutlier;
      rejected.sampleIndex = pulse.startSample;
      rejected.syncStartSample = pulse.startSample;
      rejected.syncEndSample = pulse.endSample;
      rejected.syncDurationSamples = pulse.endSample - pulse.startSample;
      rejected.lineIndex = nextLineIndex_;
      rejected.expectedSyncSample = nextExpectedSync_;
      rejected.timingErrorSamples = timingError;
      rejected.correctedFrequencyHz = correctedFrequencyHz;
      rejected.confidence = pulse.confidence;
      emitEvent (rejected, events);
      return;
    }

  SstvSlantUpdate slantUpdate;
  if (!observeSlantUnlocked (SstvSlantObservation {nextLineIndex_,
                                                   pulse.startSample,
                                                   pulse.confidence,
                                                   false},
                             slantUpdate))
    {
      emitPulseRejectedUnlocked (pulse,
                                 correctedFrequencyHz,
                                 SstvSyncRejectReason::WorkBudgetExceeded,
                                 events);
      return;
    }
  if (!slantUpdate.accepted)
    {
      saturatingAdd (metrics_.slantRejects, 1u);
      if (slantUpdate.status == SstvSlantStatus::OutlierRejected)
        {
          saturatingAdd (metrics_.timingOutliers, 1u);
        }
      emitPulseRejectedUnlocked (pulse,
                                 correctedFrequencyHz,
                                 SstvSyncRejectReason::SlantRejected,
                                 events);
      return;
    }
  metrics_.peakBufferedTimingObservations = std::max (
      metrics_.peakBufferedTimingObservations,
      slantEstimator_.bufferedObservationCount ()
          + reacquirePulses_.size ());
  if (slantUpdate.estimate.valid)
    {
      retainedPeriodSamples_ = slantUpdate.estimate.estimatedLinePeriodSamples;
    }

  bool const recovered = state_ == SstvSyncLockState::Predicting;
  std::uint64_t const acceptedLineIndex = nextLineIndex_;
  state_ = SstvSyncLockState::Locked;
  consecutivePredictedLines_ = 0u;
  haveLastObservedLine_ = true;
  lastObservedLineIndex_ = acceptedLineIndex;
  lastObservedSyncStart_ = pulse.startSample;
  if (recovered)
    {
      SstvSyncEvent recovery;
      recovery.type = SstvSyncEventType::SyncRecovered;
      recovery.sampleIndex = pulse.startSample;
      recovery.lineIndex = acceptedLineIndex;
      recovery.clockErrorPpm = currentClockErrorPpm ();
      emitEvent (recovery, events);
      saturatingAdd (metrics_.recoveries, 1u);
    }

  SstvSyncEvent line;
  line.type = SstvSyncEventType::LineSyncObserved;
  line.sampleIndex = pulse.startSample;
  line.syncStartSample = pulse.startSample;
  line.syncEndSample = pulse.endSample;
  line.syncDurationSamples = pulse.endSample - pulse.startSample;
  line.lineIndex = acceptedLineIndex;
  line.expectedSyncSample = nextExpectedSync_;
  line.timingErrorSamples = timingError;
  line.correctedFrequencyHz = correctedFrequencyHz;
  line.confidence = pulse.confidence;
  line.clockErrorPpm = currentClockErrorPpm ();
  emitEvent (line, events);
  saturatingAdd (metrics_.observedLines, 1u);

  if (nextLineIndex_ == std::numeric_limits<std::uint64_t>::max ())
    {
      loseLock (pulse.startSample,
                SstvSyncRejectReason::NumericOverflow,
                events);
      return;
    }
  ++nextLineIndex_;
  if (!checkedAdd (pulse.startSample,
                   nextPeriodSamples (),
                   nextExpectedSync_))
    {
      loseLock (pulse.startSample,
                SstvSyncRejectReason::NumericOverflow,
                events);
    }
}

void SstvSyncTracker::processReacquirePulse (
    SstvExplicitSyncPulse const& pulse,
    double correctedFrequencyHz,
    std::vector<SstvSyncEvent>& events)
{
  auto startCandidate = [&] (std::uint64_t lineIndex) {
    reacquirePulses_.clear ();
    reacquireConfidenceSum_ = pulse.confidence;
    reacquirePulses_.push_back (SstvSlantObservation {
        lineIndex, pulse.startSample, pulse.confidence, false});
    metrics_.peakBufferedTimingObservations = std::max (
        metrics_.peakBufferedTimingObservations,
        slantEstimator_.bufferedObservationCount ()
            + reacquirePulses_.size ());
    SstvSyncEvent candidate;
    candidate.type = SstvSyncEventType::ReacquireCandidate;
    candidate.sampleIndex = pulse.startSample;
    candidate.syncStartSample = pulse.startSample;
    candidate.syncEndSample = pulse.endSample;
    candidate.lineIndex = lineIndex;
    candidate.correctedFrequencyHz = correctedFrequencyHz;
    candidate.confidence = pulse.confidence;
    candidate.tentative = true;
    emitEvent (candidate, events);
    saturatingAdd (metrics_.reacquireCandidates, 1u);
  };

  if (reacquirePulses_.empty ())
    {
      std::uint64_t candidateLine = nextLineIndex_;
      if (haveLastObservedLine_ && pulse.startSample > lastObservedSyncStart_)
        {
          double const period = std::max (1.0, retainedPeriodSamples_);
          double const ratio = static_cast<double> (
              pulse.startSample - lastObservedSyncStart_) / period;
          std::uint64_t gap = static_cast<std::uint64_t> (
              std::max (1.0,
                        std::min (static_cast<double> (
                                      MaximumReacquireLineAdvance),
                                  std::floor (ratio + 0.5))));
          if (!checkedAdd (lastObservedLineIndex_, gap, candidateLine))
            {
              discontinuityUnlocked (pulse.startSample,
                                      SstvSyncRejectReason::NumericOverflow,
                                      events);
              return;
            }
        }
      startCandidate (candidateLine);
    }
  else
    {
      SstvSlantObservation const& previous = reacquirePulses_.back ();
      std::uint64_t expected = 0u;
      if (!checkedAdd (previous.syncStartSample,
                       nextPeriodSamples (),
                       expected))
        {
          discontinuityUnlocked (pulse.startSample,
                                  SstvSyncRejectReason::NumericOverflow,
                                  events);
          return;
        }
      double const timingError = signedDifference (pulse.startSample, expected);
      if (std::abs (timingError)
          > static_cast<double> (config_.lineTimingToleranceSamples))
        {
          saturatingAdd (metrics_.timingOutliers, 1u);
          saturatingAdd (metrics_.rejectedPulses, 1u);
          SstvSyncEvent rejected;
          rejected.type = SstvSyncEventType::PulseRejected;
          rejected.rejectReason = SstvSyncRejectReason::TimingOutlier;
          rejected.sampleIndex = pulse.startSample;
          rejected.expectedSyncSample = expected;
          rejected.timingErrorSamples = timingError;
          emitEvent (rejected, events);
          double const period = std::max (1.0, retainedPeriodSamples_);
          double const ratio = static_cast<double> (
              pulse.startSample - previous.syncStartSample) / period;
          std::uint64_t const lineAdvance = static_cast<std::uint64_t> (
              std::max (1.0,
                        std::min (static_cast<double> (
                                      MaximumReacquireLineAdvance),
                                  std::floor (ratio + 0.5))));
          std::uint64_t replacementLine = 0u;
          if (!checkedAdd (previous.lineIndex,
                           lineAdvance,
                           replacementLine))
            {
              discontinuityUnlocked (pulse.startSample,
                                      SstvSyncRejectReason::NumericOverflow,
                                      events);
              return;
            }
          startCandidate (replacementLine);
          return;
        }
      if (previous.lineIndex == std::numeric_limits<std::uint64_t>::max ())
        {
          discontinuityUnlocked (pulse.startSample,
                                  SstvSyncRejectReason::NumericOverflow,
                                  events);
          return;
        }
      reacquireConfidenceSum_ += pulse.confidence;
      reacquirePulses_.push_back (SstvSlantObservation {
          previous.lineIndex + 1u,
          pulse.startSample,
          pulse.confidence,
          false});
    }

  if (reacquirePulses_.size () < config_.reacquireConfirmations)
    {
      return;
    }

  std::size_t const remainingSlantBudget =
      slantUpdatesThisCall_ >= MaximumSlantUpdatesPerCall
          ? 0u
          : MaximumSlantUpdatesPerCall - slantUpdatesThisCall_;
  if (reacquirePulses_.size () > remainingSlantBudget)
    {
      recordSlantBudgetDropUnlocked (reacquirePulses_.size ());
      emitPulseRejectedUnlocked (pulse,
                                 correctedFrequencyHz,
                                 SstvSyncRejectReason::WorkBudgetExceeded,
                                 events);
      reacquirePulses_.clear ();
      reacquireConfidenceSum_ = 0.0;
      slantEstimator_.notifyDiscontinuity ();
      return;
    }

  for (SstvSlantObservation const& observation : reacquirePulses_)
    {
      SstvSlantUpdate update;
      if (!observeSlantUnlocked (observation, update))
        {
          // The preflight above makes this unreachable unless the budget
          // invariant is changed without updating this path.
          emitPulseRejectedUnlocked (pulse,
                                     correctedFrequencyHz,
                                     SstvSyncRejectReason::WorkBudgetExceeded,
                                     events);
          reacquirePulses_.clear ();
          reacquireConfidenceSum_ = 0.0;
          slantEstimator_.notifyDiscontinuity ();
          return;
        }
      if (!update.accepted)
        {
          saturatingAdd (metrics_.slantRejects, 1u);
          if (update.status == SstvSlantStatus::OutlierRejected)
            {
              saturatingAdd (metrics_.timingOutliers, 1u);
            }
          emitPulseRejectedUnlocked (pulse,
                                     correctedFrequencyHz,
                                     SstvSyncRejectReason::SlantRejected,
                                     events);
          reacquirePulses_.clear ();
          reacquireConfidenceSum_ = 0.0;
          slantEstimator_.notifyDiscontinuity ();
          return;
        }
      if (update.estimate.valid)
        {
          retainedPeriodSamples_ = update.estimate.estimatedLinePeriodSamples;
        }
    }
  metrics_.peakBufferedTimingObservations = std::max (
      metrics_.peakBufferedTimingObservations,
      slantEstimator_.bufferedObservationCount ()
          + reacquirePulses_.size ());
  SstvSlantObservation const accepted = reacquirePulses_.back ();
  state_ = SstvSyncLockState::Locked;
  haveExpectedSync_ = true;
  consecutivePredictedLines_ = 0u;
  haveLastObservedLine_ = true;
  lastObservedLineIndex_ = accepted.lineIndex;
  lastObservedSyncStart_ = accepted.syncStartSample;
  nextLineIndex_ = accepted.lineIndex;
  if (nextLineIndex_ == std::numeric_limits<std::uint64_t>::max ())
    {
      loseLock (pulse.startSample,
                SstvSyncRejectReason::NumericOverflow,
                events);
      return;
    }
  ++nextLineIndex_;
  if (!checkedAdd (accepted.syncStartSample,
                   nextPeriodSamples (),
                   nextExpectedSync_))
    {
      loseLock (pulse.startSample,
                SstvSyncRejectReason::NumericOverflow,
                events);
      return;
    }

  SstvSyncEvent reacquired;
  reacquired.type = SstvSyncEventType::Reacquired;
  reacquired.sampleIndex = accepted.syncStartSample;
  reacquired.lineIndex = accepted.lineIndex;
  reacquired.confidence = reacquireConfidenceSum_
                          / static_cast<double> (reacquirePulses_.size ());
  reacquired.clockErrorPpm = currentClockErrorPpm ();
  emitEvent (reacquired, events);
  saturatingAdd (metrics_.recoveries, 1u);
  saturatingAdd (metrics_.lockAcquisitions, 1u);

  SstvSyncEvent line = reacquired;
  line.type = SstvSyncEventType::LineSyncObserved;
  line.syncStartSample = pulse.startSample;
  line.syncEndSample = pulse.endSample;
  line.syncDurationSamples = pulse.endSample - pulse.startSample;
  line.correctedFrequencyHz = correctedFrequencyHz;
  line.confidence = pulse.confidence;
  emitEvent (line, events);
  saturatingAdd (metrics_.observedLines, 1u);
  reacquirePulses_.clear ();
  reacquireConfidenceSum_ = 0.0;
}

void SstvSyncTracker::advancePredictions (
    std::uint64_t sampleIndex,
    std::vector<SstvSyncEvent>& events)
{
  if (toneCandidateActive_ || syncPulseActive_)
    {
      return;
    }
  while ((state_ == SstvSyncLockState::Locked
          || state_ == SstvSyncLockState::Predicting)
         && haveExpectedSync_)
    {
      std::uint64_t deadline = 0u;
      std::uint64_t toleranceWithDebounce = 0u;
      if (!checkedAdd (config_.lineTimingToleranceSamples,
                       config_.enterDebounceSamples,
                       toleranceWithDebounce)
          || !checkedAdd (nextExpectedSync_,
                          toleranceWithDebounce,
                          deadline))
        {
          loseLock (sampleIndex,
                    SstvSyncRejectReason::NumericOverflow,
                    events);
          return;
        }
      if (sampleIndex <= deadline)
        {
          return;
        }
      if (consecutivePredictedLines_ >= config_.maximumPredictedLines)
        {
          loseLock (sampleIndex,
                    SstvSyncRejectReason::PredictionLimit,
                    events);
          return;
        }

      SstvSyncEvent predicted;
      predicted.type = SstvSyncEventType::LineSyncPredicted;
      predicted.sampleIndex = nextExpectedSync_;
      predicted.syncStartSample = nextExpectedSync_;
      predicted.lineIndex = nextLineIndex_;
      predicted.expectedSyncSample = nextExpectedSync_;
      predicted.clockErrorPpm = currentClockErrorPpm ();
      predicted.predicted = true;
      emitEvent (predicted, events);
      saturatingAdd (metrics_.predictedLines, 1u);
      if (consecutivePredictedLines_
          != std::numeric_limits<std::uint32_t>::max ())
        {
          ++consecutivePredictedLines_;
        }
      state_ = SstvSyncLockState::Predicting;
      if (nextLineIndex_ == std::numeric_limits<std::uint64_t>::max ())
        {
          loseLock (sampleIndex,
                    SstvSyncRejectReason::NumericOverflow,
                    events);
          return;
        }
      ++nextLineIndex_;
      if (!checkedAdd (nextExpectedSync_,
                       nextPeriodSamples (),
                       nextExpectedSync_))
        {
          loseLock (sampleIndex,
                    SstvSyncRejectReason::NumericOverflow,
                    events);
          return;
        }
    }
}

void SstvSyncTracker::loseLock (std::uint64_t sampleIndex,
                                SstvSyncRejectReason reason,
                                std::vector<SstvSyncEvent>& events)
{
  if (reason == SstvSyncRejectReason::NumericOverflow)
    {
      saturatingAdd (metrics_.numericOverflows, 1u);
    }
  SstvSyncEvent lost;
  lost.type = SstvSyncEventType::LockLost;
  lost.rejectReason = reason;
  lost.sampleIndex = sampleIndex;
  lost.lineIndex = nextLineIndex_;
  lost.expectedSyncSample = nextExpectedSync_;
  lost.clockErrorPpm = currentClockErrorPpm ();
  emitEvent (lost, events);
  saturatingAdd (metrics_.lockLosses, 1u);
  state_ = SstvSyncLockState::Reacquiring;
  haveExpectedSync_ = false;
  reacquirePulses_.clear ();
  reacquireConfidenceSum_ = 0.0;
  slantEstimator_.notifyDiscontinuity ();
}

void SstvSyncTracker::discontinuityUnlocked (
    std::uint64_t sampleIndex,
    SstvSyncRejectReason reason,
    std::vector<SstvSyncEvent>& events)
{
  SstvSyncEvent discontinuity;
  discontinuity.type = SstvSyncEventType::Discontinuity;
  discontinuity.rejectReason = reason;
  discontinuity.sampleIndex = sampleIndex;
  emitEvent (discontinuity, events);
  saturatingAdd (metrics_.discontinuities, 1u);
  if (reason == SstvSyncRejectReason::ClockRegression)
    {
      saturatingAdd (metrics_.clockRegressions, 1u);
    }
  if (reason == SstvSyncRejectReason::NumericOverflow)
    {
      saturatingAdd (metrics_.numericOverflows, 1u);
    }
  slantEstimator_.notifyDiscontinuity ();
  retainedPeriodSamples_ = static_cast<double> (
      config_.nominalLinePeriodSamples);
  clearAcquisitionStateUnlocked (false);
  timelineValid_ = true;
  lastTimelineSample_ = sampleIndex;
}

void SstvSyncTracker::clearAcquisitionStateUnlocked (
    bool clearTimeline)
{
  state_ = SstvSyncLockState::Unlocked;
  if (clearTimeline)
    {
      timelineValid_ = false;
      lastTimelineSample_ = 0u;
    }
  haveLastObservation_ = false;
  lastObservationCentre_ = 0u;
  lastObservationEnd_ = 0u;
  toneCandidateActive_ = false;
  syncPulseActive_ = false;
  toneCandidateStart_ = 0u;
  toneCandidateCoverage_ = 0u;
  pulseStart_ = 0u;
  exitCandidateStart_ = 0u;
  exitCandidateCoverage_ = 0u;
  pulseConfidenceSum_ = 0.0;
  pulseConfidenceCount_ = 0u;
  pulseFrequencySum_ = 0.0;
  pulseFrequencyCount_ = 0u;
  haveExpectedSync_ = false;
  nextExpectedSync_ = 0u;
  nextLineIndex_ = 0u;
  consecutivePredictedLines_ = 0u;
  haveLastObservedLine_ = false;
  lastObservedLineIndex_ = 0u;
  lastObservedSyncStart_ = 0u;
  reacquirePulses_.clear ();
  reacquireConfidenceSum_ = 0.0;
}

bool SstvSyncTracker::observeSlantUnlocked (
    SstvSlantObservation observation,
    SstvSlantUpdate& update)
{
  if (slantUpdatesThisCall_ >= MaximumSlantUpdatesPerCall)
    {
      recordSlantBudgetDropUnlocked (1u);
      return false;
    }
  ++slantUpdatesThisCall_;
  saturatingAdd (metrics_.slantUpdatesPerformed, 1u);
  metrics_.peakSlantUpdatesPerCall = std::max (
      metrics_.peakSlantUpdatesPerCall,
      slantUpdatesThisCall_);
  update = slantEstimator_.observe (observation);
  return true;
}

void SstvSyncTracker::recordSlantBudgetDropUnlocked (
    std::size_t count) noexcept
{
  saturatingAdd (metrics_.slantBudgetExhaustions, 1u);
  saturatingAdd (metrics_.slantUpdatesDeferred,
                 static_cast<std::uint64_t> (count));
  saturatingAdd (metrics_.slantUpdatesDropped,
                 static_cast<std::uint64_t> (count));
}

void SstvSyncTracker::emitPulseRejectedUnlocked (
    SstvExplicitSyncPulse const& pulse,
    double correctedFrequencyHz,
    SstvSyncRejectReason reason,
    std::vector<SstvSyncEvent>& events)
{
  SstvSyncEvent rejected;
  rejected.type = SstvSyncEventType::PulseRejected;
  rejected.rejectReason = reason;
  rejected.sampleIndex = pulse.startSample;
  rejected.syncStartSample = pulse.startSample;
  rejected.syncEndSample = pulse.endSample;
  rejected.syncDurationSamples = pulse.endSample > pulse.startSample
                                     ? pulse.endSample - pulse.startSample
                                     : 0u;
  rejected.correctedFrequencyHz = correctedFrequencyHz;
  rejected.confidence = pulse.confidence;
  emitEvent (rejected, events);
  saturatingAdd (metrics_.rejectedPulses, 1u);
}

void SstvSyncTracker::emitEvent (SstvSyncEvent event,
                                 std::vector<SstvSyncEvent>& events)
{
  if (events.size () >= MaximumEventsPerCall)
    {
      saturatingAdd (metrics_.droppedEvents, 1u);
      return;
    }
  event.sequence = eventSequence_;
  saturatingAdd (eventSequence_, 1u);
  events.push_back (event);
  saturatingAdd (metrics_.emittedEvents, 1u);
}

std::uint64_t SstvSyncTracker::nextPeriodSamples () const noexcept
{
  double const bounded = std::max (
      1.0,
      std::min (static_cast<double> (MaximumTimingSamples),
                retainedPeriodSamples_));
  return static_cast<std::uint64_t> (std::floor (bounded + 0.5));
}

double SstvSyncTracker::currentClockErrorPpm () const noexcept
{
  double const nominal = static_cast<double> (
      config_.nominalLinePeriodSamples);
  double const ppm = (retainedPeriodSamples_ - nominal) / nominal
                     * 1'000'000.0;
  return std::isfinite (ppm) ? ppm : 0.0;
}

void SstvSyncTracker::validateConfig (SstvSyncTrackerConfig const& config)
{
  if (!std::isfinite (config.sampleRateHz)
      || std::abs (config.sampleRateHz - NativeSampleRateHz) > 1.0e-6
      || !std::isfinite (config.syncFrequencyHz)
      || config.syncFrequencyHz <= 0.0
      || config.syncFrequencyHz >= config.sampleRateHz / 2.0
      || !std::isfinite (config.enterFrequencyToleranceHz)
      || config.enterFrequencyToleranceHz <= 0.0
      || config.enterFrequencyToleranceHz
             > MaximumConfiguredFrequencyToleranceHz
      || !std::isfinite (config.exitFrequencyToleranceHz)
      || config.exitFrequencyToleranceHz
             < config.enterFrequencyToleranceHz
      || config.exitFrequencyToleranceHz
             > MaximumConfiguredFrequencyToleranceHz
      || !std::isfinite (config.minimumEnterConfidence)
      || config.minimumEnterConfidence < 0.0
      || config.minimumEnterConfidence > 1.0
      || !std::isfinite (config.minimumHoldConfidence)
      || config.minimumHoldConfidence < 0.0
      || config.minimumHoldConfidence > config.minimumEnterConfidence
      || !std::isfinite (config.maximumAfcCorrectionHz)
      || config.maximumAfcCorrectionHz < 0.0
      || config.maximumAfcCorrectionHz > 500.0
      || config.nominalSyncDurationSamples == 0u
      || config.nominalSyncDurationSamples > MaximumTimingSamples
      || config.syncDurationToleranceSamples
             >= config.nominalSyncDurationSamples
      || config.nominalLinePeriodSamples == 0u
      || config.nominalLinePeriodSamples > MaximumTimingSamples
      || config.nominalSyncDurationSamples
             >= config.nominalLinePeriodSamples
      || config.lineTimingToleranceSamples == 0u
      || config.lineTimingToleranceSamples
             >= config.nominalLinePeriodSamples / 2u
      || config.enterDebounceSamples == 0u
      || config.enterDebounceSamples
             > config.nominalSyncDurationSamples
                    + config.syncDurationToleranceSamples
      || config.exitDebounceSamples == 0u
      || config.exitDebounceSamples > config.nominalLinePeriodSamples
      || config.maximumObservationGapSamples == 0u
      || config.maximumObservationGapSamples
             > config.nominalLinePeriodSamples
      || config.maximumPredictedLines > MaximumPredictedLines
      || config.reacquireConfirmations == 0u
      || config.reacquireConfirmations > MaximumReacquireConfirmations
      || config.slant.nominalLinePeriodSamples
             != config.nominalLinePeriodSamples
      || config.slant.minimumConfidence > config.minimumHoldConfidence)
    {
      throw std::invalid_argument {"invalid mode-specific SSTV sync configuration"};
    }
  // The nested constructor performs the rest of the robust-estimator bounds
  // validation before any tracker buffers are allocated.
  SstvSlantEstimator const validatedSlant {config.slant};
  (void) validatedSlant;
}

void SstvSyncTracker::saturatingAdd (std::uint64_t& value,
                                     std::uint64_t increment) noexcept
{
  std::uint64_t const maximum = std::numeric_limits<std::uint64_t>::max ();
  value = increment > maximum - value ? maximum : value + increment;
}

bool SstvSyncTracker::checkedAdd (std::uint64_t left,
                                  std::uint64_t right,
                                  std::uint64_t& result) noexcept
{
  std::uint64_t const maximum = std::numeric_limits<std::uint64_t>::max ();
  if (right > maximum - left)
    {
      return false;
    }
  result = left + right;
  return true;
}

double SstvSyncTracker::signedDifference (std::uint64_t value,
                                          std::uint64_t reference) noexcept
{
  if (value >= reference)
    {
      return static_cast<double> (value - reference);
    }
  return -static_cast<double> (reference - value);
}

} // namespace decodium::sstv
