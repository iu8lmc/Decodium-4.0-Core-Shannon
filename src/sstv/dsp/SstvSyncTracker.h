// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvSlantEstimator.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace decodium::sstv
{

struct SstvSyncTrackerConfig
{
  double sampleRateHz {12'000.0};
  double syncFrequencyHz {0.0};
  double enterFrequencyToleranceHz {0.0};
  double exitFrequencyToleranceHz {0.0};
  double minimumEnterConfidence {0.0};
  double minimumHoldConfidence {0.0};
  double maximumAfcCorrectionHz {150.0};

  std::uint64_t nominalSyncDurationSamples {0u};
  std::uint64_t syncDurationToleranceSamples {0u};
  std::uint64_t nominalLinePeriodSamples {0u};
  std::uint64_t lineTimingToleranceSamples {0u};
  std::uint64_t enterDebounceSamples {0u};
  std::uint64_t exitDebounceSamples {0u};
  std::uint64_t maximumObservationGapSamples {0u};
  std::uint32_t maximumPredictedLines {0u};
  std::uint32_t reacquireConfirmations {2u};

  SstvSlantEstimatorConfig slant;
};

struct SstvSyncFrequencyObservation
{
  std::uint64_t centreSample {0u};
  std::uint32_t spanSamples {0u};
  double measuredFrequencyHz {0.0};
  double afcCorrectionHz {0.0};
  double confidence {0.0};
  bool valid {true};
};

struct SstvExplicitSyncPulse
{
  std::uint64_t startSample {0u};
  std::uint64_t endSample {0u}; // exclusive
  double confidence {0.0};
};

enum class SstvSyncLockState : std::uint8_t
{
  Unlocked,
  Locked,
  Predicting,
  Reacquiring
};

enum class SstvSyncEventType : std::uint8_t
{
  SyncStarted,
  SyncEnded,
  LineSyncObserved,
  LineSyncPredicted,
  LockAcquired,
  SyncRecovered,
  LockLost,
  ReacquireCandidate,
  Reacquired,
  PulseRejected,
  ObservationRejected,
  Discontinuity
};

enum class SstvSyncRejectReason : std::uint8_t
{
  None,
  Debounce,
  Duration,
  TimingOutlier,
  InvalidInput,
  ClockRegression,
  ExternalDiscontinuity,
  PredictionLimit,
  NumericOverflow,
  SlantRejected,
  WorkBudgetExceeded
};

struct SstvSyncEvent
{
  SstvSyncEventType type {SstvSyncEventType::PulseRejected};
  SstvSyncRejectReason rejectReason {SstvSyncRejectReason::None};
  std::uint64_t sequence {0u};
  std::uint64_t sampleIndex {0u};
  std::uint64_t syncStartSample {0u};
  std::uint64_t syncEndSample {0u};
  std::uint64_t syncDurationSamples {0u};
  std::uint64_t lineIndex {0u};
  std::uint64_t expectedSyncSample {0u};
  double correctedFrequencyHz {0.0};
  double confidence {0.0};
  double timingErrorSamples {0.0};
  double clockErrorPpm {0.0};
  bool predicted {false};
  bool tentative {false};
};

struct SstvSyncTrackerMetrics
{
  std::uint64_t frequencyObservations {0u};
  std::uint64_t invalidFrequencyObservations {0u};
  std::uint64_t explicitPulses {0u};
  std::uint64_t syncStarts {0u};
  std::uint64_t syncEnds {0u};
  std::uint64_t completedPulses {0u};
  std::uint64_t durationValidPulses {0u};
  std::uint64_t rejectedPulses {0u};
  std::uint64_t debounceRejects {0u};
  std::uint64_t durationRejects {0u};
  std::uint64_t timingOutliers {0u};
  std::uint64_t observedLines {0u};
  std::uint64_t predictedLines {0u};
  std::uint64_t lockAcquisitions {0u};
  std::uint64_t lockLosses {0u};
  std::uint64_t recoveries {0u};
  std::uint64_t reacquireCandidates {0u};
  std::uint64_t discontinuities {0u};
  std::uint64_t clockRegressions {0u};
  std::uint64_t numericOverflows {0u};
  std::uint64_t staleInputsRejected {0u};
  std::uint64_t slantRejects {0u};
  std::uint64_t slantUpdatesPerformed {0u};
  // Budget-deferred updates are deliberately not queued: retaining arbitrary
  // audio observations would make memory unbounded.  Their associated pulses
  // are rejected and counted as dropped as well.
  std::uint64_t slantUpdatesDeferred {0u};
  std::uint64_t slantUpdatesDropped {0u};
  std::uint64_t slantBudgetExhaustions {0u};
  std::size_t peakSlantUpdatesPerCall {0u};
  std::uint64_t rejectedInputCalls {0u};
  std::uint64_t rejectedOversizeCalls {0u};
  std::uint64_t rejectedInputItems {0u};
  std::uint64_t emittedEvents {0u};
  std::uint64_t droppedEvents {0u};
  std::size_t bufferedTimingObservations {0u};
  std::size_t peakBufferedTimingObservations {0u};
};

struct SstvSyncTrackerSnapshot
{
  SstvSyncLockState state {SstvSyncLockState::Unlocked};
  bool toneCandidateActive {false};
  bool syncPulseActive {false};
  bool hasExpectedSync {false};
  std::uint64_t nextLineIndex {0u};
  std::uint64_t nextExpectedSyncSample {0u};
  std::uint32_t consecutivePredictedLines {0u};
  double estimatedLinePeriodSamples {0.0};
  double clockErrorPpm {0.0};
  SstvSlantEstimate slant;
  SstvSyncTrackerMetrics metrics;
};

// Mode-parametric sync detector and line-clock tracker.  Predicted line syncs
// are marked and never fed back into the robust slant model.
class SstvSyncTracker final
{
public:
  static constexpr std::size_t MaximumFrequencyObservationsPerConsume =
      8'192u;
  static constexpr std::size_t MaximumExplicitPulsesPerConsume = 512u;
  static constexpr std::size_t MaximumEventsPerCall = 262'144u;
  static constexpr std::size_t MaximumSlantUpdatesPerCall = 128u;
  static constexpr std::uint32_t MaximumPredictedLines = 128u;
  static constexpr std::uint32_t MaximumReacquireConfirmations = 16u;
  static constexpr std::uint32_t MaximumObservationSpanSamples = 1'200'000u;
  static constexpr std::uint64_t MaximumTimingSamples = 12'000'000u;
  static constexpr double MaximumConfiguredFrequencyToleranceHz = 1'000.0;

  explicit SstvSyncTracker (SstvSyncTrackerConfig config);

  std::vector<SstvSyncEvent> consume (
      SstvSyncFrequencyObservation const* observations,
      std::size_t count);
  std::vector<SstvSyncEvent> consume (
      std::vector<SstvSyncFrequencyObservation> const& observations);
  std::vector<SstvSyncEvent> consumeExplicit (
      SstvExplicitSyncPulse const* pulses,
      std::size_t count);
  std::vector<SstvSyncEvent> consumeExplicit (
      std::vector<SstvExplicitSyncPulse> const& pulses);

  // Advances prediction deadlines without fabricating an audio observation.
  std::vector<SstvSyncEvent> advanceTo (std::uint64_t sampleIndex);
  // Finalizes an active tone at a known stream boundary, then advances time.
  std::vector<SstvSyncEvent> flush (std::uint64_t endSample);
  std::vector<SstvSyncEvent> notifyDiscontinuity (
      std::uint64_t newSampleIndex);
  void reset ();

  SstvSyncTrackerSnapshot snapshot () const;
  SstvSyncTrackerConfig const& config () const noexcept;
  std::size_t bufferedTimingObservationCount () const;
  std::size_t maximumBufferedTimingObservationCount () const noexcept;

private:
  std::vector<SstvSyncEvent> consumeUnlocked (
      SstvSyncFrequencyObservation const* observations,
      std::size_t count);
  void processFrequencyObservation (
      SstvSyncFrequencyObservation const& observation,
      std::vector<SstvSyncEvent>& events);
  void processExplicitPulse (SstvExplicitSyncPulse const& pulse,
                             std::vector<SstvSyncEvent>& events);
  void processToneInterval (std::uint64_t intervalStart,
                            std::uint64_t intervalEnd,
                            bool enterTone,
                            bool holdTone,
                            double correctedFrequencyHz,
                            double confidence,
                            std::vector<SstvSyncEvent>& events);
  void finishPulse (std::uint64_t endSample,
                    double correctedFrequencyHz,
                    std::vector<SstvSyncEvent>& events);
  void acceptPulse (SstvExplicitSyncPulse const& pulse,
                    double correctedFrequencyHz,
                    std::vector<SstvSyncEvent>& events);
  void acceptLockedPulse (SstvExplicitSyncPulse const& pulse,
                          double correctedFrequencyHz,
                          std::vector<SstvSyncEvent>& events);
  void processReacquirePulse (SstvExplicitSyncPulse const& pulse,
                              double correctedFrequencyHz,
                              std::vector<SstvSyncEvent>& events);
  void advancePredictions (std::uint64_t sampleIndex,
                           std::vector<SstvSyncEvent>& events);
  void loseLock (std::uint64_t sampleIndex,
                 SstvSyncRejectReason reason,
                 std::vector<SstvSyncEvent>& events);
  void discontinuityUnlocked (std::uint64_t sampleIndex,
                              SstvSyncRejectReason reason,
                              std::vector<SstvSyncEvent>& events);
  void clearAcquisitionStateUnlocked (bool clearTimeline);
  void emitEvent (SstvSyncEvent event,
                  std::vector<SstvSyncEvent>& events);
  bool observeSlantUnlocked (SstvSlantObservation observation,
                             SstvSlantUpdate& update);
  void recordSlantBudgetDropUnlocked (std::size_t count) noexcept;
  void emitPulseRejectedUnlocked (SstvExplicitSyncPulse const& pulse,
                                  double correctedFrequencyHz,
                                  SstvSyncRejectReason reason,
                                  std::vector<SstvSyncEvent>& events);
  std::uint64_t nextPeriodSamples () const noexcept;
  double currentClockErrorPpm () const noexcept;
  static void validateConfig (SstvSyncTrackerConfig const& config);
  static void saturatingAdd (std::uint64_t& value,
                             std::uint64_t increment) noexcept;
  static bool checkedAdd (std::uint64_t left,
                          std::uint64_t right,
                          std::uint64_t& result) noexcept;
  static double signedDifference (std::uint64_t value,
                                  std::uint64_t reference) noexcept;

  SstvSyncTrackerConfig config_;
  SstvSlantEstimator slantEstimator_;
  mutable std::mutex mutex_;

  SstvSyncLockState state_ {SstvSyncLockState::Unlocked};
  bool timelineValid_ {false};
  std::uint64_t lastTimelineSample_ {0u};
  bool haveLastObservation_ {false};
  std::uint64_t lastObservationCentre_ {0u};
  std::uint64_t lastObservationEnd_ {0u};

  bool toneCandidateActive_ {false};
  bool syncPulseActive_ {false};
  std::uint64_t toneCandidateStart_ {0u};
  std::uint64_t toneCandidateCoverage_ {0u};
  std::uint64_t pulseStart_ {0u};
  std::uint64_t exitCandidateStart_ {0u};
  std::uint64_t exitCandidateCoverage_ {0u};
  double pulseConfidenceSum_ {0.0};
  std::uint64_t pulseConfidenceCount_ {0u};
  double pulseFrequencySum_ {0.0};
  std::uint64_t pulseFrequencyCount_ {0u};

  bool haveExpectedSync_ {false};
  std::uint64_t nextExpectedSync_ {0u};
  std::uint64_t nextLineIndex_ {0u};
  std::uint32_t consecutivePredictedLines_ {0u};
  bool haveLastObservedLine_ {false};
  std::uint64_t lastObservedLineIndex_ {0u};
  std::uint64_t lastObservedSyncStart_ {0u};
  double retainedPeriodSamples_ {0.0};

  std::vector<SstvSlantObservation> reacquirePulses_;
  double reacquireConfidenceSum_ {0.0};

  std::uint64_t eventSequence_ {0u};
  std::size_t slantUpdatesThisCall_ {0u};
  SstvSyncTrackerMetrics metrics_;
};

} // namespace decodium::sstv
