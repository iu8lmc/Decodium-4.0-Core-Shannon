// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include "src/sstv/rx/SstvVisDetector.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace decodium::sstv;

namespace
{

struct GoldenOptions
{
  double scale {1.0};
  double offsetHz {0.0};
  double confidence {0.93};
  bool corruptParity {false};
};

unsigned populationCount7 (std::uint8_t value)
{
  unsigned count = 0;
  value &= 0x7fu;
  while (value != 0)
    {
      count += value & 1u;
      value >>= 1u;
    }
  return count;
}

std::uint64_t scaled (std::uint64_t duration, double scale)
{
  return static_cast<std::uint64_t> (
      std::llround (static_cast<double> (duration) * scale));
}

void appendTone (std::vector<SstvVisToneEvent>& events,
                 std::uint64_t& time,
                 std::uint64_t duration,
                 double frequency,
                 GoldenOptions const& options)
{
  events.push_back ({time,
                     scaled (duration, options.scale),
                     frequency + options.offsetHz,
                     options.confidence});
  time += events.back ().durationUs;
}

void appendCodeword (std::vector<SstvVisToneEvent>& events,
                     std::uint64_t& time,
                     std::uint8_t payload,
                     bool oddParity,
                     GoldenOptions const& options)
{
  for (unsigned bit = 0; bit < 7; ++bit)
    appendTone (events, time, 30'000,
                ((payload >> bit) & 1u) != 0u ? 1'100.0 : 1'300.0,
                options);

  bool parityOne = oddParity ? (populationCount7 (payload) & 1u) == 0u
                             : (populationCount7 (payload) & 1u) != 0u;
  if (options.corruptParity)
    parityOne = !parityOne;
  appendTone (events, time, 30'000,
              parityOne ? 1'100.0 : 1'300.0, options);
}

std::vector<SstvVisToneEvent> header (GoldenOptions const& options,
                                      std::uint64_t start = 0)
{
  std::vector<SstvVisToneEvent> events;
  std::uint64_t time = start;
  appendTone (events, time, 300'000, 1'900.0, options);
  appendTone (events, time, 10'000, 1'200.0, options);
  appendTone (events, time, 300'000, 1'900.0, options);
  appendTone (events, time, 30'000, 1'200.0, options);
  return events;
}

std::vector<SstvVisToneEvent> standardFrame (
    std::uint8_t payload,
    GoldenOptions const& options = {},
    std::uint64_t start = 0)
{
  auto events = header (options, start);
  std::uint64_t time = events.back ().startTimeUs
                       + events.back ().durationUs;
  appendCodeword (events, time, payload, false, options);
  appendTone (events, time, 30'000, 1'200.0, options);
  return events;
}

std::vector<SstvVisToneEvent> extendedFrame (
    std::uint8_t payload,
    GoldenOptions const& options = {},
    std::uint64_t start = 0)
{
  auto events = header (options, start);
  std::uint64_t time = events.back ().startTimeUs
                       + events.back ().durationUs;
  GoldenOptions markerOptions = options;
  markerOptions.corruptParity = false;
  appendCodeword (events, time, 0x23u, true, markerOptions);
  appendCodeword (events, time, payload, true, options);
  appendTone (events, time, 30'000, 1'200.0, options);
  return events;
}

std::vector<SstvVisToneEvent> fragmentEvents (
    std::vector<SstvVisToneEvent> const& input)
{
  std::vector<SstvVisToneEvent> output;
  output.reserve (input.size () * 2);
  for (auto const& event : input)
    {
      std::uint64_t const first = event.durationUs / 2;
      output.push_back ({event.startTimeUs, first, event.frequencyHz,
                         event.confidence});
      output.push_back ({event.startTimeUs + first,
                         event.durationUs - first,
                         event.frequencyHz,
                         event.confidence});
    }
  return output;
}

} // namespace

class TestSstvVisDetector final : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void validatesConfigurationAndHostileBounds ();
  void decodesIndependentStandardGolden_data ();
  void decodesIndependentStandardGolden ();
  void decodesWideExtendedGolden ();
  void measuresClockDriftOffsetAndConfidence ();
  void handlesFragmentedEventsAndArbitraryConsumeChunks ();
  void trimsBoundedOverlapWithoutChangingFrame ();
  void rejectsGapsBeyondConfiguredBound ();
  void reacquiresLeaderThatStartsAfterRejectedGap ();
  void ignoresNoiseAndLowConfidenceWithoutLocking ();
  void rejectsHeaderTimingAndBadParity ();
  void reportsAmbiguousAndUnderCoveredBits ();
  void handlesEarlyStopFinishAndTimeout ();
  void decodesBackToBackFrames ();
  void doesNotInterpretExtraBitsAsNarrowNvis ();
  void classifiedInputUsesTheSamePhysicalContract ();
  void cancelAndResetAreExplicit ();
  void rejectsTimestampOverflowRegressionAndRawFlood ();
};

void TestSstvVisDetector::validatesConfigurationAndHostileBounds ()
{
  SstvVisDetectorConfig invalid;
  invalid.symbolDurationUs = 0;
  QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                            SstvVisDetector {invalid});
  invalid = {};
  invalid.frequencyToleranceHz = 50.0;
  QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                            SstvVisDetector {invalid});
  invalid = {};
  invalid.minimumSlotCoverage = 0.49;
  QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                            SstvVisDetector {invalid});
  invalid = {};
  invalid.maximumFrameDurationUs = 500'000;
  QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                            SstvVisDetector {invalid});

  SstvVisDetector detector;
  QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                            detector.consume (nullptr, 1));
  SstvVisToneEvent sentinel;
  auto const before = detector.metrics ();
  QVERIFY_THROWS_EXCEPTION (
      std::length_error,
      detector.consume (&sentinel,
                        SstvVisDetector::MaximumEventsPerConsume + 1));
  QCOMPARE (detector.metrics ().observationsConsumed,
            before.observationsConsumed);
  QCOMPARE (detector.bufferedRawEventCount (), std::size_t {0});
}

void TestSstvVisDetector::decodesIndependentStandardGolden_data ()
{
  QTest::addColumn<int> ("payload");
  QTest::newRow ("all zero") << 0x00;
  QTest::newRow ("mixed 2c") << 0x2c;
  QTest::newRow ("mixed 55") << 0x55;
  QTest::newRow ("all one") << 0x7f;
}

void TestSstvVisDetector::decodesIndependentStandardGolden ()
{
  QFETCH (int, payload);
  SstvVisDetector detector;
  auto const results = detector.consume (
      standardFrame (static_cast<std::uint8_t> (payload)));
  QCOMPARE (results.size (), std::size_t {1});
  QVERIFY (results.front ().valid ());
  QCOMPARE (results.front ().codecResult.format, SstvVisFormat::Standard);
  QCOMPARE (results.front ().codecResult.primary.payload,
            static_cast<std::uint8_t> (payload));
  QVERIFY (results.front ().codecResult.primary.parityValid);
  QCOMPARE (results.front ().codecResult.observedRawBitCount,
            SstvVisCodec::StandardRawBitCount);
  QCOMPARE (results.front ().symbolObservations.size (), std::size_t {10});
  QCOMPARE (results.front ().rawEvents.size (), std::size_t {13});
  QCOMPARE (detector.state (), SstvVisDetectorState::SearchingLeader);
  QCOMPARE (detector.metrics ().framesDecoded, std::uint64_t {1});
}

void TestSstvVisDetector::decodesWideExtendedGolden ()
{
  SstvVisDetector detector;
  auto const results = detector.consume (extendedFrame (0x29u));
  QCOMPARE (results.size (), std::size_t {1});
  QVERIFY (results.front ().valid ());
  QCOMPARE (results.front ().codecResult.format, SstvVisFormat::Extended);
  QCOMPARE (results.front ().codecResult.primary.rawOctet,
            SstvVisCodec::ExtendedMarkerRawOctet);
  QVERIFY (results.front ().codecResult.primary.parityValid);
  QVERIFY (results.front ().codecResult.extension.has_value ());
  QCOMPARE (results.front ().codecResult.extension->payload,
            std::uint8_t {0x29});
  QVERIFY (results.front ().codecResult.extension->parityValid);
  QCOMPARE (results.front ().codecResult.observedRawBitCount,
            SstvVisCodec::ExtendedRawBitCount);
  QCOMPARE (results.front ().symbolObservations.size (), std::size_t {18});
}

void TestSstvVisDetector::measuresClockDriftOffsetAndConfidence ()
{
  GoldenOptions options;
  options.scale = 1.03;
  options.offsetHz = 92.0;
  options.confidence = 0.81;
  SstvVisDetector detector;
  auto const results = detector.consume (standardFrame (0x36u, options));
  QCOMPARE (results.size (), std::size_t {1});
  QVERIFY (results.front ().valid ());
  QVERIFY (std::abs (results.front ().estimatedFrequencyOffsetHz - 92.0)
           < 0.01);
  QVERIFY (std::abs (results.front ().estimatedClockDriftPpm - 30'000.0)
           < 1.0);
  QVERIFY (std::abs (results.front ().confidence - 0.81) < 1.0e-9);
}

void TestSstvVisDetector::handlesFragmentedEventsAndArbitraryConsumeChunks ()
{
  auto const complete = standardFrame (0x4du);
  SstvVisDetector contiguous;
  auto const expected = contiguous.consume (complete);
  QCOMPARE (expected.size (), std::size_t {1});
  QVERIFY (expected.front ().valid ());

  auto const fragmented = fragmentEvents (complete);
  SstvVisDetector chunked;
  std::vector<SstvVisDetection> actual;
  std::size_t position = 0;
  std::size_t const chunkPattern[] {1, 3, 2, 5, 1, 4};
  std::size_t pattern = 0;
  while (position < fragmented.size ())
    {
      std::size_t const count = std::min (
          chunkPattern[pattern++ % 6], fragmented.size () - position);
      auto current = chunked.consume (fragmented.data () + position, count);
      actual.insert (actual.end (), current.begin (), current.end ());
      position += count;
    }
  QCOMPARE (actual.size (), std::size_t {1});
  QVERIFY (actual.front ().valid ());
  QCOMPARE (actual.front ().codecResult.primary.payload,
            expected.front ().codecResult.primary.payload);
  QCOMPARE (actual.front ().codecResult.rawBitsLsbFirst,
            expected.front ().codecResult.rawBitsLsbFirst);
  QCOMPARE (actual.front ().confidence, expected.front ().confidence);
  QCOMPARE (actual.front ().rawEvents.size (), fragmented.size ());
}

void TestSstvVisDetector::trimsBoundedOverlapWithoutChangingFrame ()
{
  auto overlapped = standardFrame (0x42u);
  for (std::size_t index = 1; index < overlapped.size (); ++index)
    {
      overlapped[index].startTimeUs -= 2'000;
      overlapped[index].durationUs += 2'000;
    }
  SstvVisDetector detector;
  auto const results = detector.consume (overlapped);
  QCOMPARE (results.size (), std::size_t {1});
  QVERIFY (results.front ().valid ());
  QCOMPARE (results.front ().codecResult.primary.payload,
            std::uint8_t {0x42});
}

void TestSstvVisDetector::rejectsGapsBeyondConfiguredBound ()
{
  auto headerGap = standardFrame (0x42u);
  headerGap[1].startTimeUs += 5'001u;
  SstvVisDetector headerDetector;
  auto const headerResult = headerDetector.consume (headerGap.data (), 2u);
  QCOMPARE (headerResult.size (), std::size_t {1u});
  QCOMPARE (headerResult.front ().status,
            SstvVisDetectionStatus::InvalidInput);
  QCOMPARE (headerResult.front ().cause,
            SstvVisDetectionCause::GapExceeded);
  QCOMPARE (headerResult.front ().frameEndedAtUs, std::uint64_t {305'001u});
  QCOMPARE (QString::fromLatin1 (SstvVisDetector::causeName (
                headerResult.front ().cause)),
            QStringLiteral ("gap-exceeded"));

  auto dataGap = standardFrame (0x42u);
  for (std::size_t index = 5u; index < dataGap.size (); ++index)
    dataGap[index].startTimeUs += 5'001u;
  SstvVisDetector dataDetector;
  auto const dataResult = dataDetector.consume (dataGap.data (), 6u);
  QCOMPARE (dataResult.size (), std::size_t {1u});
  QCOMPARE (dataResult.front ().cause,
            SstvVisDetectionCause::GapExceeded);

  std::vector<SstvVisClassifiedEvent> classified;
  classified.reserve (2u);
  classified.push_back ({0u, 300'000u, SstvVisToneKind::Leader,
                         0.0, 0.9});
  classified.push_back ({305'001u, 10'000u,
                         SstvVisToneKind::Separator, 0.0, 0.9});
  SstvVisDetector classifiedDetector;
  auto const classifiedResult =
      classifiedDetector.consumeClassified (classified);
  QCOMPARE (classifiedResult.size (), std::size_t {1u});
  QCOMPARE (classifiedResult.front ().cause,
            SstvVisDetectionCause::GapExceeded);
}

void TestSstvVisDetector::reacquiresLeaderThatStartsAfterRejectedGap ()
{
  constexpr std::uint64_t NewFrameStart = 400'000u;
  auto const newFrame = standardFrame (0x55u, {}, NewFrameStart);
  std::vector<SstvVisToneEvent> stream {
      {0u, 300'000u, 1'900.0, 0.9}
  };
  stream.insert (stream.end (), newFrame.begin (), newFrame.end ());

  SstvVisDetector detector;
  auto const results = detector.consume (stream);
  QCOMPARE (results.size (), std::size_t {2u});
  QCOMPARE (results[0].cause, SstvVisDetectionCause::GapExceeded);
  QCOMPARE (results[0].rawEvents.size (), std::size_t {1u});
  QVERIFY (results[1].valid ());
  QCOMPARE (results[1].codecResult.primary.payload, std::uint8_t {0x55u});
  QCOMPARE (results[1].frameStartedAtUs, NewFrameStart);
  QCOMPARE (results[1].rawEvents.front ().startTimeUs, NewFrameStart);
  QCOMPARE (detector.metrics ().observationsConsumed,
            static_cast<std::uint64_t> (stream.size ()));
  QCOMPARE (detector.metrics ().framesStarted, std::uint64_t {2u});
  QCOMPARE (detector.metrics ().framesRejected, std::uint64_t {1u});
  QCOMPARE (detector.metrics ().framesDecoded, std::uint64_t {1u});

  std::vector<SstvVisClassifiedEvent> classified;
  classified.reserve (stream.size ());
  for (auto const& event : stream)
    {
      SstvVisToneKind tone = SstvVisToneKind::Unknown;
      if (event.frequencyHz == 1'900.0)
        tone = SstvVisToneKind::Leader;
      else if (event.frequencyHz == 1'200.0)
        tone = SstvVisToneKind::Separator;
      else if (event.frequencyHz == 1'100.0)
        tone = SstvVisToneKind::BitOne;
      else if (event.frequencyHz == 1'300.0)
        tone = SstvVisToneKind::BitZero;
      classified.push_back ({event.startTimeUs, event.durationUs, tone,
                             0.0, event.confidence});
    }

  SstvVisDetector classifiedDetector;
  auto const classifiedResults =
      classifiedDetector.consumeClassified (classified);
  QCOMPARE (classifiedResults.size (), std::size_t {2u});
  QCOMPARE (classifiedResults[0].cause,
            SstvVisDetectionCause::GapExceeded);
  QVERIFY (classifiedResults[1].valid ());
  QCOMPARE (classifiedResults[1].codecResult.primary.payload,
            std::uint8_t {0x55u});
  QCOMPARE (classifiedResults[1].frameStartedAtUs, NewFrameStart);
  QCOMPARE (classifiedDetector.metrics ().observationsConsumed,
            static_cast<std::uint64_t> (classified.size ()));
}

void TestSstvVisDetector::ignoresNoiseAndLowConfidenceWithoutLocking ()
{
  std::vector<SstvVisToneEvent> noise {
      {0, 30'000, 1'500.0, 0.9},
      {30'000, 30'000, 2'100.0, 0.9},
      {60'000, 300'000, 2'050.0, 0.9},
      {360'000, 300'000, 1'900.0, 0.1}
  };
  SstvVisDetector detector;
  auto const results = detector.consume (noise);
  QVERIFY (results.empty ());
  QCOMPARE (detector.state (), SstvVisDetectorState::SearchingLeader);
  QCOMPARE (detector.metrics ().framesStarted, std::uint64_t {0});
  QCOMPARE (detector.metrics ().noiseEventsDiscarded,
            static_cast<std::uint64_t> (noise.size ()));

  GoldenOptions marginal;
  marginal.confidence = 0.40;
  SstvVisDetector lowFrameConfidence;
  auto const lowResult = lowFrameConfidence.consume (
      standardFrame (0x2cu, marginal));
  QCOMPARE (lowResult.size (), std::size_t {1});
  QCOMPARE (lowResult.front ().status, SstvVisDetectionStatus::Rejected);
  QCOMPARE (lowResult.front ().cause,
            SstvVisDetectionCause::LowConfidence);

  auto mixedConfidence = standardFrame (0x2cu);
  for (std::size_t index = 0u; index < 4u; ++index)
    mixedConfidence[index].confidence = 0.40;
  for (std::size_t index = 4u; index < mixedConfidence.size (); ++index)
    mixedConfidence[index].confidence = 0.98;
  SstvVisDetector headerAware;
  auto const headerAwareResult = headerAware.consume (mixedConfidence);
  QCOMPARE (headerAwareResult.size (), std::size_t {1u});
  QCOMPARE (headerAwareResult.front ().cause,
            SstvVisDetectionCause::LowConfidence);
  QVERIFY (std::abs (headerAwareResult.front ().confidence - 0.40)
           < 1.0e-9);
}

void TestSstvVisDetector::rejectsHeaderTimingAndBadParity ()
{
  GoldenOptions options;
  auto shortHeader = header (options);
  shortHeader.front ().durationUs = 100'000;
  shortHeader[1].startTimeUs = 100'000;
  shortHeader[2].startTimeUs = 110'000;
  shortHeader[3].startTimeUs = 410'000;
  shortHeader.push_back ({440'000, 30'000, 1'500.0, 0.9});
  SstvVisDetector timing;
  auto badTiming = timing.consume (shortHeader);
  QVERIFY (!badTiming.empty ());
  QCOMPARE (badTiming.front ().status, SstvVisDetectionStatus::Rejected);
  QCOMPARE (badTiming.front ().cause,
            SstvVisDetectionCause::LeaderTiming);
  QCOMPARE (timing.state (), SstvVisDetectorState::SearchingLeader);

  options.corruptParity = true;
  SstvVisDetector parity;
  auto badParity = parity.consume (standardFrame (0x2cu, options));
  QCOMPARE (badParity.size (), std::size_t {1});
  QCOMPARE (badParity.front ().status, SstvVisDetectionStatus::Rejected);
  QCOMPARE (badParity.front ().cause,
            SstvVisDetectionCause::ParityMismatch);
  QVERIFY (!badParity.front ().codecResult.primary.parityValid);
}

void TestSstvVisDetector::reportsAmbiguousAndUnderCoveredBits ()
{
  GoldenOptions options;
  auto ambiguous = header (options);
  std::uint64_t time = ambiguous.back ().startTimeUs
                       + ambiguous.back ().durationUs;
  ambiguous.push_back ({time, 15'000, 1'100.0, 0.9});
  ambiguous.push_back ({time + 15'000, 15'000, 1'300.0, 0.9});
  SstvVisDetector ambiguousDetector;
  auto ambiguousResult = ambiguousDetector.consume (ambiguous);
  QCOMPARE (ambiguousResult.size (), std::size_t {1});
  QCOMPARE (ambiguousResult.front ().cause,
            SstvVisDetectionCause::SymbolAmbiguous);

  auto underCovered = header (options);
  time = underCovered.back ().startTimeUs
         + underCovered.back ().durationUs;
  underCovered.push_back ({time, 15'000, 1'500.0, 0.9});
  underCovered.push_back ({time + 15'000, 15'000, 1'100.0, 0.9});
  SstvVisDetector coverageDetector;
  auto coverageResult = coverageDetector.consume (underCovered);
  QCOMPARE (coverageResult.size (), std::size_t {1});
  QCOMPARE (coverageResult.front ().cause,
            SstvVisDetectionCause::SlotCoverage);

  auto marginalDominance = header (options);
  time = marginalDominance.back ().startTimeUs
         + marginalDominance.back ().durationUs;
  marginalDominance.push_back ({time, 22'000, 1'100.0, 1.0});
  marginalDominance.push_back ({time + 22'000, 8'000, 1'300.0, 1.0});
  SstvVisDetector qualityDetector;
  QVERIFY (qualityDetector.consume (marginalDominance).empty ());
  auto const quality = qualityDetector.finish (time + 30'000);
  QVERIFY (quality.has_value ());
  QCOMPARE (quality->symbolObservations.size (), std::size_t {2u});
  QVERIFY (quality->symbolObservations[1].confidence > 0.73);
  QVERIFY (quality->symbolObservations[1].confidence < 0.74);
}

void TestSstvVisDetector::handlesEarlyStopFinishAndTimeout ()
{
  GoldenOptions options;
  auto early = header (options);
  std::uint64_t time = early.back ().startTimeUs + early.back ().durationUs;
  appendCodeword (early, time, 0x23u, true, options);
  appendTone (early, time, 30'000, 1'200.0, options);
  SstvVisDetector earlyDetector;
  auto earlyResult = earlyDetector.consume (early);
  QCOMPARE (earlyResult.size (), std::size_t {1});
  QCOMPARE (earlyResult.front ().status,
            SstvVisDetectionStatus::Truncated);
  QCOMPARE (earlyResult.front ().cause, SstvVisDetectionCause::EarlyStop);

  auto partial = header (options);
  time = partial.back ().startTimeUs + partial.back ().durationUs;
  appendTone (partial, time, 30'000, 1'100.0, options);
  SstvVisDetector finished;
  QVERIFY (finished.consume (partial).empty ());
  auto finishResult = finished.finish (time);
  QVERIFY (finishResult.has_value ());
  QCOMPARE (finishResult->status, SstvVisDetectionStatus::Truncated);
  QCOMPARE (finishResult->cause, SstvVisDetectionCause::EndOfInput);
  QVERIFY (!finishResult->codecResult.complete);

  SstvVisDetector timeout;
  SstvVisToneEvent leader {0, 300'000, 1'900.0, 0.9};
  QVERIFY (timeout.consume (&leader, 1).empty ());
  auto timedOut = timeout.tick (400'000);
  QCOMPARE (timedOut.size (), std::size_t {1});
  QCOMPARE (timedOut.front ().status, SstvVisDetectionStatus::TimedOut);
  QCOMPARE (timeout.state (), SstvVisDetectorState::SearchingLeader);

  SstvVisDetector finishTimeout;
  QVERIFY (finishTimeout.consume (&leader, 1).empty ());
  auto const finishedTooLate = finishTimeout.finish (2'000'000);
  QVERIFY (finishedTooLate.has_value ());
  QCOMPARE (finishedTooLate->status, SstvVisDetectionStatus::TimedOut);
  QCOMPARE (finishedTooLate->cause,
            SstvVisDetectionCause::FrameDurationExceeded);
  QCOMPARE (finishTimeout.metrics ().timeouts, std::uint64_t {1u});
}

void TestSstvVisDetector::decodesBackToBackFrames ()
{
  auto first = standardFrame (0x2cu);
  std::uint64_t const secondStart =
      first.back ().startTimeUs + first.back ().durationUs;
  auto second = standardFrame (0x55u, {}, secondStart);
  first.insert (first.end (), second.begin (), second.end ());

  SstvVisDetector detector;
  auto const results = detector.consume (first);
  QCOMPARE (results.size (), std::size_t {2});
  QVERIFY (results[0].valid ());
  QVERIFY (results[1].valid ());
  QCOMPARE (results[0].codecResult.primary.payload, std::uint8_t {0x2c});
  QCOMPARE (results[1].codecResult.primary.payload, std::uint8_t {0x55});
  QVERIFY (results[1].rawEvents.front ().sequence
           > results[0].rawEvents.back ().sequence);
  QCOMPARE (detector.metrics ().framesDecoded, std::uint64_t {2});
}

void TestSstvVisDetector::doesNotInterpretExtraBitsAsNarrowNvis ()
{
  GoldenOptions options;
  auto events = header (options);
  std::uint64_t time = events.back ().startTimeUs
                       + events.back ().durationUs;
  appendCodeword (events, time, 0x23u, true, options);
  appendCodeword (events, time, 0x29u, true, options);
  // A third codeword is not part of the supported wide-extended VIS frame.
  appendCodeword (events, time, 0x12u, true, options);
  appendTone (events, time, 30'000, 1'200.0, options);

  SstvVisDetector detector;
  auto const results = detector.consume (events);
  QVERIFY (!results.empty ());
  QVERIFY (std::none_of (results.begin (), results.end (),
                         [] (SstvVisDetection const& result) {
                           return result.valid ();
                         }));
  QCOMPARE (results.front ().cause, SstvVisDetectionCause::MissingStop);
}

void TestSstvVisDetector::classifiedInputUsesTheSamePhysicalContract ()
{
  auto const physical = standardFrame (0x31u, {1.0, -88.0, 0.9, false});
  std::vector<SstvVisClassifiedEvent> classified;
  classified.reserve (physical.size ());
  for (auto const& event : physical)
    {
      double const nominal = event.frequencyHz + 88.0;
      SstvVisToneKind tone = SstvVisToneKind::Unknown;
      if (nominal == 1'900.0) tone = SstvVisToneKind::Leader;
      else if (nominal == 1'200.0) tone = SstvVisToneKind::Separator;
      else if (nominal == 1'100.0) tone = SstvVisToneKind::BitOne;
      else if (nominal == 1'300.0) tone = SstvVisToneKind::BitZero;
      classified.push_back ({event.startTimeUs, event.durationUs, tone,
                             -88.0, event.confidence});
    }
  SstvVisDetector detector;
  auto const results = detector.consumeClassified (classified);
  QCOMPARE (results.size (), std::size_t {1});
  QVERIFY (results.front ().valid ());
  QCOMPARE (results.front ().codecResult.primary.payload,
            std::uint8_t {0x31});
  QVERIFY (std::abs (results.front ().estimatedFrequencyOffsetHz + 88.0)
           < 0.01);
}

void TestSstvVisDetector::cancelAndResetAreExplicit ()
{
  SstvVisDetector detector;
  auto events = header ({});
  events.resize (2);
  QVERIFY (detector.consume (events).empty ());
  auto cancelled = detector.cancel (events.back ().startTimeUs);
  QVERIFY (cancelled.has_value ());
  QCOMPARE (cancelled->status, SstvVisDetectionStatus::Cancelled);
  QCOMPARE (detector.state (), SstvVisDetectorState::Cancelled);
  QVERIFY (detector.consume (standardFrame (0x2cu)).empty ());

  detector.reset ();
  QCOMPARE (detector.state (), SstvVisDetectorState::SearchingLeader);
  auto decoded = detector.consume (standardFrame (0x2cu));
  QCOMPARE (decoded.size (), std::size_t {1});
  QVERIFY (decoded.front ().valid ());
}

void TestSstvVisDetector::rejectsTimestampOverflowRegressionAndRawFlood ()
{
  SstvVisDetector overflow;
  SstvVisToneEvent impossible {
      std::numeric_limits<std::uint64_t>::max () - 5, 10, 1'900.0, 0.9};
  auto overflowResult = overflow.consume (&impossible, 1);
  QCOMPARE (overflowResult.size (), std::size_t {1});
  QCOMPARE (overflowResult.front ().status,
            SstvVisDetectionStatus::InvalidInput);

  SstvVisDetectorConfig nearLimitConfig;
  std::uint64_t const nearLimitStart =
      std::numeric_limits<std::uint64_t>::max ()
      - nearLimitConfig.maximumFrameDurationUs;
  SstvVisDetector nearLimit {nearLimitConfig};
  auto const nearLimitResult = nearLimit.consume (
      standardFrame (0x2cu, {}, nearLimitStart));
  QCOMPARE (nearLimitResult.size (), std::size_t {1u});
  QVERIFY (nearLimitResult.front ().valid ());

  SstvVisDetector regression;
  SstvVisToneEvent first {100, 300'000, 1'900.0, 0.9};
  QVERIFY (regression.consume (&first, 1).empty ());
  SstvVisToneEvent backwards {0, 10'000, 1'200.0, 0.9};
  auto regressionResult = regression.consume (&backwards, 1);
  QCOMPARE (regressionResult.size (), std::size_t {1});
  QCOMPARE (regressionResult.front ().cause,
            SstvVisDetectionCause::TimestampRegression);

  std::vector<SstvVisToneEvent> flood;
  flood.reserve (SstvVisDetector::MaximumRawEventsPerFrame + 1);
  std::uint64_t time = 0;
  std::uint64_t const piece =
      300'000 / (SstvVisDetector::MaximumRawEventsPerFrame + 1);
  for (std::size_t index = 0;
       index < SstvVisDetector::MaximumRawEventsPerFrame + 1; ++index)
    {
      std::uint64_t const duration =
          index + 1 == SstvVisDetector::MaximumRawEventsPerFrame + 1
              ? 300'000 - time
              : piece;
      flood.push_back ({time, duration, 1'900.0, 0.9});
      time += duration;
    }
  SstvVisDetector bounded;
  auto floodResult = bounded.consume (flood);
  QCOMPARE (floodResult.size (), std::size_t {1});
  QCOMPARE (floodResult.front ().status,
            SstvVisDetectionStatus::BoundsExceeded);
  QCOMPARE (floodResult.front ().cause,
            SstvVisDetectionCause::RawEventLimit);
  QVERIFY (floodResult.front ().rawEvents.size ()
           <= SstvVisDetector::MaximumRawEventsPerFrame);
  QCOMPARE (bounded.bufferedRawEventCount (), std::size_t {0});
}

QTEST_APPLESS_MAIN (TestSstvVisDetector)

#include "test_sstv_vis_detector.moc"
