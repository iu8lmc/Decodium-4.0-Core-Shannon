// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include "src/sstv/dsp/SstvSlantEstimator.h"
#include "src/sstv/dsp/SstvSyncTracker.h"

namespace
{
using decodium::sstv::SstvExplicitSyncPulse;
using decodium::sstv::SstvSlantEstimator;
using decodium::sstv::SstvSlantEstimatorConfig;
using decodium::sstv::SstvSlantObservation;
using decodium::sstv::SstvSlantStatus;
using decodium::sstv::SstvSyncEvent;
using decodium::sstv::SstvSyncEventType;
using decodium::sstv::SstvSyncFrequencyObservation;
using decodium::sstv::SstvSyncLockState;
using decodium::sstv::SstvSyncRejectReason;
using decodium::sstv::SstvSyncTracker;
using decodium::sstv::SstvSyncTrackerConfig;

constexpr std::uint64_t SyncDuration = 120u;
constexpr std::uint64_t LinePeriod = 1'200u;

SstvSlantEstimatorConfig slantConfig ()
{
  SstvSlantEstimatorConfig config;
  config.nominalLinePeriodSamples = LinePeriod;
  config.windowLines = 32u;
  config.minimumLines = 4u;
  config.minimumConfidence = 0.25;
  config.outlierToleranceSamples = 8.0;
  config.warningClockErrorPpm = 350.0;
  config.maximumClockErrorPpm = 5'000.0;
  return config;
}

SstvSyncTrackerConfig trackerConfig ()
{
  SstvSyncTrackerConfig config;
  config.syncFrequencyHz = 1'200.0;
  config.enterFrequencyToleranceHz = 35.0;
  config.exitFrequencyToleranceHz = 75.0;
  config.minimumEnterConfidence = 0.65;
  config.minimumHoldConfidence = 0.30;
  config.maximumAfcCorrectionHz = 150.0;
  config.nominalSyncDurationSamples = SyncDuration;
  config.syncDurationToleranceSamples = 18u;
  config.nominalLinePeriodSamples = LinePeriod;
  config.lineTimingToleranceSamples = 24u;
  config.enterDebounceSamples = 18u;
  config.exitDebounceSamples = 12u;
  config.maximumObservationGapSamples = 12u;
  config.maximumPredictedLines = 2u;
  config.reacquireConfirmations = 2u;
  config.slant = slantConfig ();
  return config;
}

std::vector<SstvExplicitSyncPulse> makePulses (double clockErrorPpm,
                                               std::size_t count,
                                               std::uint64_t first = 120u)
{
  std::vector<SstvExplicitSyncPulse> pulses;
  pulses.reserve (count);
  double const actualPeriod = static_cast<double> (LinePeriod)
                              * (1.0 + clockErrorPpm / 1'000'000.0);
  for (std::size_t line = 0u; line < count; ++line)
    {
      double const jitter = static_cast<double> (
          static_cast<int> (line % 5u) - 2) * 0.20;
      std::uint64_t const start = first + static_cast<std::uint64_t> (
          std::llround (static_cast<double> (line) * actualPeriod + jitter));
      pulses.push_back (SstvExplicitSyncPulse {
          start, start + SyncDuration, 0.94});
    }
  return pulses;
}

std::vector<SstvSyncFrequencyObservation> makeFrequencyTimeline (
    std::uint64_t endSample,
    std::vector<std::pair<std::uint64_t, std::uint64_t>> const& syncRanges,
    std::vector<std::pair<std::uint64_t, std::uint64_t>> const& falseRanges = {})
{
  constexpr std::uint32_t Span = 6u;
  std::vector<SstvSyncFrequencyObservation> observations;
  observations.reserve (static_cast<std::size_t> (endSample / Span + 1u));
  for (std::uint64_t start = 0u; start < endSample; start += Span)
    {
      auto contains = [start] (
                          std::vector<std::pair<std::uint64_t,
                                               std::uint64_t>> const& ranges) {
        return std::any_of (
            ranges.begin (), ranges.end (), [start] (auto const& range) {
              return start >= range.first && start < range.second;
            });
      };
      bool const sync = contains (syncRanges);
      bool const falseSync = contains (falseRanges);
      double const correctedFrequency = sync || falseSync ? 1'200.0 : 1'900.0;
      observations.push_back (SstvSyncFrequencyObservation {
          start + Span / 2u,
          Span,
          correctedFrequency + 50.0,
          50.0,
          sync || falseSync ? 0.93 : 0.85,
          true});
    }
  return observations;
}

std::vector<SstvSyncEvent> consumeFrequencyChunks (
    SstvSyncTracker& tracker,
    std::vector<SstvSyncFrequencyObservation> const& observations,
    std::vector<std::size_t> const& pattern)
{
  std::vector<SstvSyncEvent> events;
  std::size_t offset = 0u;
  std::size_t patternIndex = 0u;
  while (offset < observations.size ())
    {
      std::size_t const count = std::min (
          pattern[patternIndex % pattern.size ()],
          observations.size () - offset);
      auto const produced = tracker.consume (observations.data () + offset,
                                             count);
      events.insert (events.end (), produced.begin (), produced.end ());
      offset += count;
      ++patternIndex;
    }
  return events;
}

std::vector<SstvSyncEvent> eventsOfType (
    std::vector<SstvSyncEvent> const& events,
    SstvSyncEventType type)
{
  std::vector<SstvSyncEvent> selected;
  std::copy_if (events.begin (),
                events.end (),
                std::back_inserter (selected),
                [type] (SstvSyncEvent const& event) {
                  return event.type == type;
                });
  return selected;
}

void verifyFinite (SstvSyncEvent const& event)
{
  QVERIFY (std::isfinite (event.correctedFrequencyHz));
  QVERIFY (std::isfinite (event.confidence));
  QVERIFY (std::isfinite (event.timingErrorSamples));
  QVERIFY (std::isfinite (event.clockErrorPpm));
}
}

class TestSstvSyncTracker final : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void frequencyHysteresisDebounceAndAfc ()
  {
    auto observations = makeFrequencyTimeline (
        1'700u,
        {{120u, 240u}, {1'320u, 1'440u}},
        {{600u, 612u}});
    // Once the second pulse has passed entry debounce, keep it alive with a
    // frequency outside the enter band and confidence below the enter gate,
    // but still inside the wider/lower hold hysteresis thresholds.
    for (auto& observation : observations)
      {
        std::uint64_t const intervalStart = observation.centreSample - 3u;
        if (intervalStart >= 1'338u && intervalStart < 1'440u)
          {
            observation.measuredFrequencyHz = 1'310.0; // 1260 after AFC
            observation.confidence = 0.40;
          }
      }
    SstvSyncTracker tracker {trackerConfig ()};
    auto const events = consumeFrequencyChunks (tracker,
                                                 observations,
                                                 {1u, 7u, 31u, 2u, 113u});
    auto const starts = eventsOfType (events, SstvSyncEventType::SyncStarted);
    auto const lines = eventsOfType (events,
                                     SstvSyncEventType::LineSyncObserved);
    auto const rejects = eventsOfType (events,
                                       SstvSyncEventType::PulseRejected);
    QCOMPARE (starts.size (), std::size_t {2u});
    QCOMPARE (starts[0].syncStartSample, std::uint64_t {120u});
    QVERIFY (std::abs (starts[0].correctedFrequencyHz - 1'200.0) < 1.0e-9);
    QCOMPARE (lines.size (), std::size_t {2u});
    QCOMPARE (lines[0].lineIndex, std::uint64_t {0u});
    QCOMPARE (lines[1].lineIndex, std::uint64_t {1u});
    QCOMPARE (lines[1].timingErrorSamples, 0.0);
    QVERIFY (std::any_of (rejects.begin (), rejects.end (), [] (auto const& event) {
      return event.rejectReason == SstvSyncRejectReason::Debounce;
    }));
    auto const snapshot = tracker.snapshot ();
    QCOMPARE (static_cast<int> (snapshot.state),
              static_cast<int> (SstvSyncLockState::Locked));
    QCOMPARE (snapshot.metrics.debounceRejects, std::uint64_t {1u});
    QCOMPARE (snapshot.metrics.observedLines, std::uint64_t {2u});
    for (SstvSyncEvent const& event : events)
      {
        verifyFinite (event);
      }
  }

  void heldSyncToneIsRejectedAtDurationBound ()
  {
    std::uint64_t const firstSync = 120u;
    std::uint64_t const maximumDuration =
        SyncDuration + trackerConfig ().syncDurationToleranceSamples;
    std::uint64_t const endSample = firstSync + maximumDuration + 6u;
    auto const observations = makeFrequencyTimeline (
        endSample, {{firstSync, endSample}});

    SstvSyncTracker tracker {trackerConfig ()};
    auto const events = tracker.consume (observations);
    auto const rejected = eventsOfType (events,
                                        SstvSyncEventType::PulseRejected);
    QCOMPARE (rejected.size (), std::size_t {1u});
    QCOMPARE (static_cast<int> (rejected[0].rejectReason),
              static_cast<int> (SstvSyncRejectReason::Duration));
    QVERIFY (rejected[0].syncDurationSamples > maximumDuration);
    QCOMPARE (eventsOfType (events,
                            SstvSyncEventType::LineSyncObserved).size (),
              std::size_t {0u});
    auto const snapshot = tracker.snapshot ();
    QVERIFY (!snapshot.syncPulseActive);
    QCOMPARE (snapshot.metrics.durationRejects, std::uint64_t {1u});

    auto coarseConfig = trackerConfig ();
    coarseConfig.maximumObservationGapSamples = LinePeriod;
    SstvSyncTracker coarse {coarseConfig};
    std::uint32_t const coarseSpan = static_cast<std::uint32_t> (
        maximumDuration + 6u);
    SstvSyncFrequencyObservation const coarseObservation {
        firstSync + coarseSpan / 2u,
        coarseSpan,
        1'200.0,
        0.0,
        0.95,
        true};
    auto const coarseEvents = coarse.consume (&coarseObservation, 1u);
    auto const coarseRejects = eventsOfType (
        coarseEvents, SstvSyncEventType::PulseRejected);
    QCOMPARE (coarseRejects.size (), std::size_t {1u});
    QCOMPARE (static_cast<int> (coarseRejects[0].rejectReason),
              static_cast<int> (SstvSyncRejectReason::Duration));
    QVERIFY (!coarse.snapshot ().syncPulseActive);
  }

  void trackerClockDrift_data ()
  {
    QTest::addColumn<double> ("clockErrorPpm");
    QTest::addColumn<bool> ("beyondTolerance");
    QTest::newRow ("plus-300") << 300.0 << false;
    QTest::newRow ("minus-300") << -300.0 << false;
    QTest::newRow ("plus-1200") << 1'200.0 << true;
    QTest::newRow ("minus-1200") << -1'200.0 << true;
  }

  void trackerClockDrift ()
  {
    QFETCH (double, clockErrorPpm);
    QFETCH (bool, beyondTolerance);
    SstvSyncTracker tracker {trackerConfig ()};
    auto const pulses = makePulses (clockErrorPpm, 80u);
    auto const events = tracker.consumeExplicit (pulses);
    auto const snapshot = tracker.snapshot ();
    QCOMPARE (snapshot.metrics.observedLines, std::uint64_t {80u});
    QVERIFY (snapshot.slant.valid);
    QVERIFY (std::abs (snapshot.clockErrorPpm - clockErrorPpm) < 35.0);
    QCOMPARE (snapshot.slant.status
                  == SstvSlantStatus::BeyondConfiguredTolerance,
              beyondTolerance);
    QVERIFY (events.size () < SstvSyncTracker::MaximumEventsPerCall);
  }

  void missingSyncPredictionAndRecovery ()
  {
    SstvSyncTracker tracker {trackerConfig ()};
    tracker.consumeExplicit (makePulses (0.0, 2u, 100u));

    auto predicted = tracker.advanceTo (2'700u);
    auto predictedLines = eventsOfType (
        predicted, SstvSyncEventType::LineSyncPredicted);
    QCOMPARE (predictedLines.size (), std::size_t {1u});
    QCOMPARE (predictedLines[0].lineIndex, std::uint64_t {2u});
    QVERIFY (predictedLines[0].predicted);
    QCOMPARE (tracker.snapshot ().slant.metrics.predictedSyncsIgnored,
              std::uint64_t {0u});

    auto recovery = tracker.consumeExplicit (
        std::vector<SstvExplicitSyncPulse> {{3'700u, 3'820u, 0.95}});
    QCOMPARE (eventsOfType (recovery,
                            SstvSyncEventType::SyncRecovered).size (),
              std::size_t {1u});
    QCOMPARE (tracker.snapshot ().metrics.predictedLines,
              std::uint64_t {1u});

    auto lost = tracker.advanceTo (8'500u);
    QCOMPARE (eventsOfType (lost, SstvSyncEventType::LineSyncPredicted).size (),
              std::size_t {2u});
    QCOMPARE (eventsOfType (lost, SstvSyncEventType::LockLost).size (),
              std::size_t {1u});
    QCOMPARE (static_cast<int> (tracker.snapshot ().state),
              static_cast<int> (SstvSyncLockState::Reacquiring));

    auto firstCandidate = tracker.consumeExplicit (
        std::vector<SstvExplicitSyncPulse> {{8'500u, 8'620u, 0.93}});
    QCOMPARE (eventsOfType (firstCandidate,
                            SstvSyncEventType::ReacquireCandidate).size (),
              std::size_t {1u});
    auto reacquired = tracker.consumeExplicit (
        std::vector<SstvExplicitSyncPulse> {{9'700u, 9'820u, 0.94}});
    QCOMPARE (eventsOfType (reacquired,
                            SstvSyncEventType::Reacquired).size (),
              std::size_t {1u});
    QCOMPARE (static_cast<int> (tracker.snapshot ().state),
              static_cast<int> (SstvSyncLockState::Locked));
    QVERIFY (tracker.snapshot ().metrics.predictedLines <= 3u);
  }

  void skippedPulseDuringReacquirePreservesLineIndex ()
  {
    SstvSyncTracker tracker {trackerConfig ()};
    tracker.consumeExplicit (makePulses (0.0, 2u, 100u));
    tracker.advanceTo (8'500u);
    QCOMPARE (static_cast<int> (tracker.snapshot ().state),
              static_cast<int> (SstvSyncLockState::Reacquiring));

    auto const first = tracker.consumeExplicit (
        std::vector<SstvExplicitSyncPulse> {{8'500u, 8'620u, 0.94}});
    auto const firstCandidates = eventsOfType (
        first, SstvSyncEventType::ReacquireCandidate);
    QCOMPARE (firstCandidates.size (), std::size_t {1u});
    QCOMPARE (firstCandidates[0].lineIndex, std::uint64_t {7u});

    // Line 8 is absent.  The next pulse is two periods after the candidate,
    // so it becomes a fresh candidate for line 9 rather than silently
    // shifting the recovered line numbering by one.
    auto const skipped = tracker.consumeExplicit (
        std::vector<SstvExplicitSyncPulse> {{10'900u, 11'020u, 0.94}});
    auto const skippedRejects = eventsOfType (
        skipped, SstvSyncEventType::PulseRejected);
    QCOMPARE (skippedRejects.size (), std::size_t {1u});
    QCOMPARE (static_cast<int> (skippedRejects[0].rejectReason),
              static_cast<int> (SstvSyncRejectReason::TimingOutlier));
    auto const replacementCandidates = eventsOfType (
        skipped, SstvSyncEventType::ReacquireCandidate);
    QCOMPARE (replacementCandidates.size (), std::size_t {1u});
    QCOMPARE (replacementCandidates[0].lineIndex, std::uint64_t {9u});

    auto const recovered = tracker.consumeExplicit (
        std::vector<SstvExplicitSyncPulse> {{12'100u, 12'220u, 0.94}});
    auto const reacquired = eventsOfType (recovered,
                                          SstvSyncEventType::Reacquired);
    QCOMPARE (reacquired.size (), std::size_t {1u});
    QCOMPARE (reacquired[0].lineIndex, std::uint64_t {10u});
    auto const lines = eventsOfType (recovered,
                                     SstvSyncEventType::LineSyncObserved);
    QCOMPARE (lines.size (), std::size_t {1u});
    QCOMPARE (lines[0].lineIndex, std::uint64_t {10u});
    QCOMPARE (tracker.snapshot ().nextLineIndex, std::uint64_t {11u});
  }

  void slantRejectionPreventsReacquisition ()
  {
    SstvSyncTracker tracker {trackerConfig ()};
    tracker.consumeExplicit (makePulses (0.0, 2u, 100u));
    auto const lost = tracker.advanceTo (8'500u);
    QCOMPARE (eventsOfType (lost, SstvSyncEventType::LockLost).size (),
              std::size_t {1u});

    tracker.consumeExplicit (
        std::vector<SstvExplicitSyncPulse> {{8'500u, 8'620u, 0.93}});
    auto const rejected = tracker.consumeExplicit (
        std::vector<SstvExplicitSyncPulse> {{9'720u, 9'840u, 0.94}});
    QCOMPARE (eventsOfType (rejected,
                            SstvSyncEventType::Reacquired).size (),
              std::size_t {0u});
    auto const pulseRejects = eventsOfType (
        rejected, SstvSyncEventType::PulseRejected);
    QCOMPARE (pulseRejects.size (), std::size_t {1u});
    QCOMPARE (static_cast<int> (pulseRejects[0].rejectReason),
              static_cast<int> (SstvSyncRejectReason::SlantRejected));

    auto const snapshot = tracker.snapshot ();
    QCOMPARE (static_cast<int> (snapshot.state),
              static_cast<int> (SstvSyncLockState::Reacquiring));
    QCOMPARE (snapshot.metrics.observedLines, std::uint64_t {2u});
    QCOMPARE (snapshot.metrics.slantRejects, std::uint64_t {1u});
    QCOMPARE (snapshot.metrics.timingOutliers, std::uint64_t {1u});
    QCOMPARE (snapshot.slant.observationCount, std::size_t {0u});
  }

  void backToBackPulseRejectedAndResetIsClean ()
  {
    SstvSyncTracker tracker {trackerConfig ()};
    auto first = tracker.consumeExplicit (
        std::vector<SstvExplicitSyncPulse> {{100u, 220u, 0.9}});
    QCOMPARE (eventsOfType (first,
                            SstvSyncEventType::LineSyncObserved).size (),
              std::size_t {1u});
    auto backToBack = tracker.consumeExplicit (
        std::vector<SstvExplicitSyncPulse> {{220u, 340u, 0.9}});
    auto const rejected = eventsOfType (backToBack,
                                        SstvSyncEventType::PulseRejected);
    QCOMPARE (rejected.size (), std::size_t {1u});
    QCOMPARE (static_cast<int> (rejected[0].rejectReason),
              static_cast<int> (SstvSyncRejectReason::TimingOutlier));

    tracker.reset ();
    auto afterReset = tracker.consumeExplicit (
        std::vector<SstvExplicitSyncPulse> {{100u, 220u, 0.9}});
    auto const line = eventsOfType (afterReset,
                                    SstvSyncEventType::LineSyncObserved);
    QCOMPARE (line.size (), std::size_t {1u});
    QCOMPARE (line[0].lineIndex, std::uint64_t {0u});
    QCOMPARE (afterReset.front ().sequence, std::uint64_t {0u});
    QCOMPARE (tracker.snapshot ().metrics.timingOutliers,
              std::uint64_t {0u});
  }

  void frequencyChunkBoundaryEquivalence ()
  {
    auto const observations = makeFrequencyTimeline (
        4'000u,
        {{120u, 240u}, {1'320u, 1'440u}, {2'520u, 2'640u},
         {3'720u, 3'840u}},
        {{800u, 812u}});
    SstvSyncTracker whole {trackerConfig ()};
    auto const expected = whole.consume (observations);
    SstvSyncTracker chunked {trackerConfig ()};
    auto const actual = consumeFrequencyChunks (chunked,
                                                 observations,
                                                 {1u, 3u, 97u, 2u, 211u});
    QCOMPARE (actual.size (), expected.size ());
    for (std::size_t index = 0u; index < expected.size (); ++index)
      {
        QCOMPARE (static_cast<int> (actual[index].type),
                  static_cast<int> (expected[index].type));
        QCOMPARE (static_cast<int> (actual[index].rejectReason),
                  static_cast<int> (expected[index].rejectReason));
        QCOMPARE (actual[index].sequence, expected[index].sequence);
        QCOMPARE (actual[index].sampleIndex, expected[index].sampleIndex);
        QCOMPARE (actual[index].lineIndex, expected[index].lineIndex);
        QCOMPARE (actual[index].timingErrorSamples,
                  expected[index].timingErrorSamples);
      }
    QCOMPARE (chunked.snapshot ().metrics.emittedEvents,
              whole.snapshot ().metrics.emittedEvents);
    QCOMPARE (chunked.snapshot ().clockErrorPpm,
              whole.snapshot ().clockErrorPpm);
  }

  void slantEstimatorDrift_data ()
  {
    QTest::addColumn<double> ("clockErrorPpm");
    QTest::addColumn<bool> ("beyondTolerance");
    QTest::newRow ("plus-300") << 300.0 << false;
    QTest::newRow ("minus-300") << -300.0 << false;
    QTest::newRow ("plus-1800") << 1'800.0 << true;
    QTest::newRow ("minus-1800") << -1'800.0 << true;
  }

  void slantEstimatorDrift ()
  {
    QFETCH (double, clockErrorPpm);
    QFETCH (bool, beyondTolerance);
    SstvSlantEstimator estimator {slantConfig ()};
    auto const pulses = makePulses (clockErrorPpm, 96u, 1'000u);
    std::vector<SstvSlantObservation> observations;
    observations.reserve (pulses.size ());
    for (std::size_t line = 0u; line < pulses.size (); ++line)
      {
        observations.push_back (SstvSlantObservation {
            static_cast<std::uint64_t> (line),
            pulses[line].startSample,
            0.95,
            false});
      }
    auto const updates = estimator.consume (observations);
    QCOMPARE (updates.size (), observations.size ());
    auto const estimate = estimator.snapshot ();
    QVERIFY (estimate.valid);
    QVERIFY (std::abs (estimate.clockErrorPpm - clockErrorPpm) < 35.0);
    QCOMPARE (estimate.status
                  == SstvSlantStatus::BeyondConfiguredTolerance,
              beyondTolerance);
    QVERIFY (std::abs (estimate.correctionSamplesPerLine
                       + estimate.errorSamplesPerLine) < 1.0e-12);
    QVERIFY (estimate.observationCount <= slantConfig ().windowLines);
  }

  void slantRejectsOutlierPredictionAndHandlesDiscontinuity ()
  {
    SstvSlantEstimator estimator {slantConfig ()};
    for (std::uint64_t line = 0u; line < 10u; ++line)
      {
        QVERIFY (estimator.observe (SstvSlantObservation {
                    line, 100u + line * LinePeriod, 0.9, false})
                     .accepted);
      }
    double const before = estimator.snapshot ().clockErrorPpm;
    auto const outlier = estimator.observe (SstvSlantObservation {
        10u, 100u + 10u * LinePeriod + 200u, 0.9, false});
    QVERIFY (!outlier.accepted);
    QCOMPARE (static_cast<int> (outlier.status),
              static_cast<int> (SstvSlantStatus::OutlierRejected));
    QCOMPARE (estimator.snapshot ().clockErrorPpm, before);

    auto const predicted = estimator.observe (SstvSlantObservation {
        10u, 100u + 10u * LinePeriod, 1.0, true});
    QVERIFY (!predicted.accepted);
    QCOMPARE (static_cast<int> (predicted.status),
              static_cast<int> (SstvSlantStatus::PredictedSyncIgnored));
    QCOMPARE (estimator.snapshot ().metrics.predictedSyncsIgnored,
              std::uint64_t {1u});

    auto const resumed = estimator.observe (SstvSlantObservation {
        10u, 100u + 10u * LinePeriod, 0.9, false});
    QVERIFY (resumed.accepted);
    auto const discontinuity = estimator.observe (SstvSlantObservation {
        2u, 50u, 0.9, false});
    QVERIFY (discontinuity.accepted);
    QCOMPARE (static_cast<int> (discontinuity.status),
              static_cast<int> (SstvSlantStatus::Discontinuity));
    QCOMPARE (estimator.bufferedObservationCount (), std::size_t {1u});
    QCOMPARE (estimator.snapshot ().metrics.discontinuities,
              std::uint64_t {1u});
    estimator.reset ();
    QCOMPARE (estimator.snapshot ().metrics.observationsReceived,
              std::uint64_t {0u});
  }

  void invalidOverflowAndOversizeAreBounded ()
  {
    SstvSyncTrackerConfig invalid = trackerConfig ();
    invalid.nominalLinePeriodSamples = 0u;
    QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                              SstvSyncTracker {invalid});
    invalid = trackerConfig ();
    invalid.slant.maximumClockErrorPpm = 200'000.0;
    QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                              SstvSyncTracker {invalid});
    invalid = trackerConfig ();
    invalid.slant.minimumConfidence =
        invalid.minimumHoldConfidence + 0.01;
    QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                              SstvSyncTracker {invalid});

    SstvSyncTracker tracker {trackerConfig ()};
    SstvSyncFrequencyObservation sentinel {
        3u, 6u, 1'200.0, 0.0, 0.9, true};
    auto const before = tracker.snapshot ();
    QVERIFY (tracker.consume (nullptr, 7u).empty ());
    QVERIFY (tracker.consume (
        &sentinel,
        SstvSyncTracker::MaximumFrequencyObservationsPerConsume + 1u).empty ());
    auto const rejected = tracker.snapshot ();
    QCOMPARE (rejected.metrics.frequencyObservations,
              before.metrics.frequencyObservations);
    QCOMPARE (rejected.metrics.rejectedInputCalls, std::uint64_t {2u});
    QCOMPARE (rejected.metrics.rejectedOversizeCalls, std::uint64_t {1u});

    sentinel.measuredFrequencyHz =
        std::numeric_limits<double>::quiet_NaN ();
    QVERIFY (tracker.consume (&sentinel, 1u).empty ());
    QCOMPARE (tracker.snapshot ().metrics.invalidFrequencyObservations,
              std::uint64_t {1u});

    sentinel = SstvSyncFrequencyObservation {
        std::numeric_limits<std::uint64_t>::max () - 1u,
        6u,
        1'200.0,
        0.0,
        0.9,
        true};
    auto overflowEvents = tracker.consume (&sentinel, 1u);
    QCOMPARE (eventsOfType (overflowEvents,
                            SstvSyncEventType::Discontinuity).size (),
              std::size_t {1u});
    QCOMPARE (tracker.snapshot ().metrics.numericOverflows,
              std::uint64_t {1u});

    SstvSlantEstimator estimator {slantConfig ()};
    SstvSlantObservation slantSentinel {0u, 0u, 0.9, false};
    QVERIFY (estimator.consume (nullptr, 2u).empty ());
    QVERIFY (estimator.consume (
        &slantSentinel,
        SstvSlantEstimator::MaximumObservationsPerConsume + 1u).empty ());
    auto const slantSnapshot = estimator.snapshot ();
    QCOMPARE (slantSnapshot.metrics.observationsReceived, std::uint64_t {0u});
    QCOMPARE (slantSnapshot.metrics.rejectedInputCalls, std::uint64_t {2u});
  }

  void clockRegressionProducesExplicitDiscontinuity ()
  {
    SstvSyncTracker tracker {trackerConfig ()};
    tracker.consumeExplicit (
        std::vector<SstvExplicitSyncPulse> {{1'000u, 1'120u, 0.9}});
    auto const events = tracker.advanceTo (500u);
    auto const discontinuities = eventsOfType (
        events, SstvSyncEventType::Discontinuity);
    QCOMPARE (discontinuities.size (), std::size_t {1u});
    QCOMPARE (static_cast<int> (discontinuities[0].rejectReason),
              static_cast<int> (SstvSyncRejectReason::ClockRegression));
    QCOMPARE (static_cast<int> (tracker.snapshot ().state),
              static_cast<int> (SstvSyncLockState::Unlocked));
    QCOMPARE (tracker.snapshot ().metrics.clockRegressions,
              std::uint64_t {1u});

    SstvSyncTracker external {trackerConfig ()};
    auto const explicitEvents = external.notifyDiscontinuity (42u);
    QCOMPARE (explicitEvents.size (), std::size_t {1u});
    QCOMPARE (static_cast<int> (explicitEvents[0].rejectReason),
              static_cast<int> (SstvSyncRejectReason::ExternalDiscontinuity));
    QCOMPARE (external.snapshot ().metrics.clockRegressions,
              std::uint64_t {0u});
  }

  void staleItemsAreRejectedWithoutRebasing ()
  {
    SstvSyncTracker explicitTracker {trackerConfig ()};
    explicitTracker.consumeExplicit (
        std::vector<SstvExplicitSyncPulse> {{1'000u, 1'120u, 0.9}});
    auto const before = explicitTracker.snapshot ();
    auto const stalePulse = explicitTracker.consumeExplicit (
        std::vector<SstvExplicitSyncPulse> {{100u, 220u, 0.9}});
    auto const pulseRejects = eventsOfType (
        stalePulse, SstvSyncEventType::PulseRejected);
    QCOMPARE (pulseRejects.size (), std::size_t {1u});
    QCOMPARE (static_cast<int> (pulseRejects[0].rejectReason),
              static_cast<int> (SstvSyncRejectReason::ClockRegression));
    QCOMPARE (eventsOfType (stalePulse,
                            SstvSyncEventType::Discontinuity).size (),
              std::size_t {0u});
    QCOMPARE (eventsOfType (stalePulse,
                            SstvSyncEventType::LockAcquired).size (),
              std::size_t {0u});
    auto const afterStalePulse = explicitTracker.snapshot ();
    QCOMPARE (afterStalePulse.nextExpectedSyncSample,
              before.nextExpectedSyncSample);
    QCOMPARE (afterStalePulse.metrics.observedLines,
              before.metrics.observedLines);
    QCOMPARE (afterStalePulse.metrics.discontinuities,
              before.metrics.discontinuities);
    QCOMPARE (afterStalePulse.metrics.staleInputsRejected,
              std::uint64_t {1u});

    auto const resumed = explicitTracker.consumeExplicit (
        std::vector<SstvExplicitSyncPulse> {{2'200u, 2'320u, 0.9}});
    QCOMPARE (eventsOfType (resumed,
                            SstvSyncEventType::LineSyncObserved).size (),
              std::size_t {1u});

    SstvSyncTracker frequencyTracker {trackerConfig ()};
    frequencyTracker.advanceTo (10'000u);
    auto const staleFrequency = makeFrequencyTimeline (
        252u, {{120u, 240u}});
    auto const frequencyEvents = frequencyTracker.consume (staleFrequency);
    QCOMPARE (eventsOfType (frequencyEvents,
                            SstvSyncEventType::ObservationRejected).size (),
              staleFrequency.size ());
    QCOMPARE (eventsOfType (frequencyEvents,
                            SstvSyncEventType::LockAcquired).size (),
              std::size_t {0u});
    auto const frequencySnapshot = frequencyTracker.snapshot ();
    QCOMPARE (static_cast<int> (frequencySnapshot.state),
              static_cast<int> (SstvSyncLockState::Unlocked));
    QCOMPARE (frequencySnapshot.metrics.staleInputsRejected,
              static_cast<std::uint64_t> (staleFrequency.size ()));
    QCOMPARE (frequencySnapshot.metrics.discontinuities, std::uint64_t {0u});
  }

  void containedOverlapCannotRegressCoverage ()
  {
    auto config = trackerConfig ();
    config.maximumObservationGapSamples = 120u;
    SstvSyncTracker tracker {config};
    std::vector<SstvSyncFrequencyObservation> observations;
    observations.push_back (SstvSyncFrequencyObservation {
        100u, 100u, 1'900.0, 0.0, 0.9, true});
    for (std::uint64_t centre = 101u; centre < 125u; ++centre)
      {
        observations.push_back (SstvSyncFrequencyObservation {
            centre, 1u, 1'200.0, 0.0, 0.95, true});
      }
    for (std::uint64_t centre = 150u; centre < 168u; ++centre)
      {
        observations.push_back (SstvSyncFrequencyObservation {
            centre, 1u, 1'200.0, 0.0, 0.95, true});
      }

    auto const events = tracker.consume (observations);
    QCOMPARE (eventsOfType (events,
                            SstvSyncEventType::ObservationRejected).size (),
              std::size_t {24u});
    auto const starts = eventsOfType (events,
                                      SstvSyncEventType::SyncStarted);
    QCOMPARE (starts.size (), std::size_t {1u});
    QCOMPARE (starts[0].syncStartSample, std::uint64_t {150u});
  }

  void realtimeWorkBudgetsAreEnforced ()
  {
    auto maximumWindowConfig = slantConfig ();
    maximumWindowConfig.windowLines = SstvSlantEstimator::MaximumWindowLines;
    SstvSlantEstimator estimator {maximumWindowConfig};
    std::vector<SstvSlantObservation> warmup;
    warmup.reserve (SstvSlantEstimator::MaximumWindowLines);
    for (std::uint64_t line = 0u;
         line < SstvSlantEstimator::MaximumWindowLines;
         ++line)
      {
        warmup.push_back (SstvSlantObservation {
            line, 100u + line * LinePeriod, 0.95, false});
      }
    estimator.consume (warmup);
    std::uint64_t const workBefore =
        estimator.snapshot ().metrics.pairwiseWorkUnits;

    std::vector<SstvSlantObservation> hostileBulk;
    hostileBulk.reserve (SstvSlantEstimator::MaximumObservationsPerConsume);
    for (std::size_t index = 0u;
         index < SstvSlantEstimator::MaximumObservationsPerConsume;
         ++index)
      {
        std::uint64_t const line = static_cast<std::uint64_t> (
            SstvSlantEstimator::MaximumWindowLines + index);
        hostileBulk.push_back (SstvSlantObservation {
            line, 100u + line * LinePeriod, 0.95, false});
      }
    QCOMPARE (estimator.consume (hostileBulk).size (), hostileBulk.size ());
    std::uint64_t const bulkWork =
        estimator.snapshot ().metrics.pairwiseWorkUnits - workBefore;
    QCOMPARE (bulkWork,
              SstvSlantEstimator::MaximumPairwiseWorkUnitsPerConsume);

    auto trackerWorkConfig = trackerConfig ();
    trackerWorkConfig.slant.windowLines =
        SstvSlantEstimator::MaximumWindowLines;
    SstvSyncTracker tracker {trackerWorkConfig};
    auto const pulses = makePulses (
        0.0, SstvSyncTracker::MaximumSlantUpdatesPerCall + 1u, 100u);
    auto const events = tracker.consumeExplicit (pulses);
    auto const budgetRejects = eventsOfType (
        events, SstvSyncEventType::PulseRejected);
    QCOMPARE (budgetRejects.size (), std::size_t {1u});
    QCOMPARE (static_cast<int> (budgetRejects[0].rejectReason),
              static_cast<int> (SstvSyncRejectReason::WorkBudgetExceeded));
    auto const snapshot = tracker.snapshot ();
    QCOMPARE (snapshot.metrics.observedLines,
              static_cast<std::uint64_t> (
                  SstvSyncTracker::MaximumSlantUpdatesPerCall));
    QCOMPARE (snapshot.metrics.slantUpdatesPerformed,
              static_cast<std::uint64_t> (
                  SstvSyncTracker::MaximumSlantUpdatesPerCall));
    QCOMPARE (snapshot.metrics.slantUpdatesDeferred, std::uint64_t {1u});
    QCOMPARE (snapshot.metrics.slantUpdatesDropped, std::uint64_t {1u});
    QCOMPARE (snapshot.metrics.slantBudgetExhaustions, std::uint64_t {1u});
    QCOMPARE (snapshot.metrics.peakSlantUpdatesPerCall,
              SstvSyncTracker::MaximumSlantUpdatesPerCall);
    QVERIFY (snapshot.slant.metrics.pairwiseWorkUnits
             <= static_cast<std::uint64_t> (
                    SstvSyncTracker::MaximumSlantUpdatesPerCall)
                    * SstvSlantEstimator::MaximumPairwiseWorkUnits);

    std::uint64_t const nextLine =
        static_cast<std::uint64_t> (
            SstvSyncTracker::MaximumSlantUpdatesPerCall + 1u);
    std::uint64_t const nextStart = 100u + nextLine * LinePeriod;
    auto const recovery = tracker.consumeExplicit (
        std::vector<SstvExplicitSyncPulse> {
            {nextStart, nextStart + SyncDuration, 0.95}});
    QCOMPARE (eventsOfType (recovery,
                            SstvSyncEventType::SyncRecovered).size (),
              std::size_t {1u});
    QCOMPARE (eventsOfType (recovery,
                            SstvSyncEventType::LineSyncObserved).size (),
              std::size_t {1u});
  }

  void longStreamKeepsMemoryAndWorkBounded ()
  {
    SstvSyncTracker tracker {trackerConfig ()};
    constexpr std::size_t TotalLines = 20'000u;
    constexpr std::size_t ChunkLines =
        SstvSyncTracker::MaximumSlantUpdatesPerCall;
    std::size_t generated = 0u;
    while (generated < TotalLines)
      {
        std::size_t const count = std::min (ChunkLines,
                                            TotalLines - generated);
        std::vector<SstvExplicitSyncPulse> pulses;
        pulses.reserve (count);
        for (std::size_t index = 0u; index < count; ++index)
          {
            std::uint64_t const line = static_cast<std::uint64_t> (
                generated + index);
            std::uint64_t const start = 100u + line * LinePeriod;
            pulses.push_back (SstvExplicitSyncPulse {
                start, start + SyncDuration, 0.95});
          }
        auto const events = tracker.consumeExplicit (pulses);
        QVERIFY (events.size () <= SstvSyncTracker::MaximumEventsPerCall);
        QVERIFY (tracker.bufferedTimingObservationCount ()
                 <= tracker.maximumBufferedTimingObservationCount ());
        for (SstvSyncEvent const& event : events)
          {
            verifyFinite (event);
          }
        generated += count;
      }
    auto const snapshot = tracker.snapshot ();
    QCOMPARE (snapshot.metrics.observedLines,
              static_cast<std::uint64_t> (TotalLines));
    QVERIFY (snapshot.slant.metrics.peakWindowOccupancy
             <= slantConfig ().windowLines);
    QVERIFY (snapshot.slant.metrics.pairwiseWorkUnits
             <= static_cast<std::uint64_t> (TotalLines)
                    * SstvSlantEstimator::MaximumPairwiseWorkUnits);
    QVERIFY (std::isfinite (snapshot.clockErrorPpm));
  }
};

QTEST_GUILESS_MAIN (TestSstvSyncTracker)

#include "test_sstv_sync_tracker.moc"
