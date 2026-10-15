// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include "src/sstv/dsp/SstvToneDetector.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

using namespace decodium::sstv;

namespace
{

constexpr double kPi = 3.141592653589793238462643383279502884;

SstvToneDetectorConfig testConfig ()
{
  auto config = SstvToneDetectorConfig::sstvDefaults (12'000.0);
  config.windowSamples = 480;
  config.hopSamples = 120;
  config.searchStepHz = 2.0;
  config.minimumRms = 0.002;
  config.minimumSnrDb = 6.0;
  config.minimumDominanceDb = 2.0;
  config.minimumConfidence = 0.35;
  config.offsetSmoothing = 0.4;
  return config;
}

std::vector<float> tone (double sampleRate,
                         double frequency,
                         std::size_t sampleCount,
                         double amplitude = 0.7,
                         double phase = 0.23)
{
  std::vector<float> output;
  output.reserve (sampleCount);
  for (std::size_t index = 0; index < sampleCount; ++index)
    output.push_back (static_cast<float> (
        amplitude * std::sin (phase + 2.0 * kPi * frequency
                                          * static_cast<double> (index)
                                          / sampleRate)));
  return output;
}

std::vector<float> whiteNoise (std::size_t sampleCount,
                               double standardDeviation,
                               std::uint32_t seed = 0x51a7u)
{
  std::mt19937 generator {seed};
  std::normal_distribution<double> distribution {0.0, standardDeviation};
  std::vector<float> output;
  output.reserve (sampleCount);
  for (std::size_t index = 0; index < sampleCount; ++index)
    output.push_back (static_cast<float> (distribution (generator)));
  return output;
}

std::vector<float> addSignals (std::vector<float> lhs,
                               std::vector<float> const& rhs)
{
  Q_ASSERT (lhs.size () == rhs.size ());
  for (std::size_t index = 0; index < lhs.size (); ++index)
    lhs[index] += rhs[index];
  return lhs;
}

SstvToneObservation const* lastDetected (
    std::vector<SstvToneObservation> const& observations)
{
  auto const found = std::find_if (
      observations.rbegin (), observations.rend (),
      [] (SstvToneObservation const& observation) {
        return observation.valid ();
      });
  return found == observations.rend () ? nullptr : &*found;
}

} // namespace

class TestSstvToneDetector final : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void validatesConfigurationAndExposesRequiredBank ();
  void rejectsHostileConfigurationAndOversizedCallsTransactionally ();
  void detectsEveryRequiredTone_data ();
  void detectsEveryRequiredTone ();
  void rejectsSilenceDcAndLowLevelTone ();
  void rejectsNoiseDualToneAndOffFrequency ();
  void reportsNonFiniteInputWithoutUnboundedState ();
  void chunkingProducesIdenticalObservations ();
  void tracksPositiveAndNegativeCommonOffset ();
  void trackedOffsetDisambiguatesCloselySpacedTones ();
  void overlapIndexesAndMemoryStayBounded ();
  void resetAndExternalOffsetSeedAreExplicit ();
  void exactBoundaryResetDropsOverlapAndPreservesStreamCoordinates ();
};

void TestSstvToneDetector::validatesConfigurationAndExposesRequiredBank ()
{
  auto const defaults = SstvToneDetector::defaultSstvFrequencies ();
  for (double required : {1'100.0, 1'200.0, 1'300.0, 1'500.0,
                          1'900.0, 2'100.0})
    QVERIFY (std::find (defaults.begin (), defaults.end (), required)
             != defaults.end ());

  QVERIFY_THROWS_EXCEPTION (
      std::invalid_argument,
      SstvToneDetectorConfig::sstvDefaults (
          std::numeric_limits<double>::quiet_NaN ()));
  QVERIFY_THROWS_EXCEPTION (
      std::invalid_argument,
      SstvToneDetectorConfig::sstvDefaults (
          std::numeric_limits<double>::infinity ()));
  QVERIFY_THROWS_EXCEPTION (
      std::invalid_argument,
      SstvToneDetectorConfig::sstvDefaults (
          SstvToneDetector::MaximumSampleRateHz + 1.0));
  QVERIFY_THROWS_EXCEPTION (
      std::invalid_argument,
      SstvToneDetectorConfig::sstvDefaults (
          SstvToneDetector::MinimumSampleRateHz - 1.0));

  auto invalid = testConfig ();
  invalid.sampleRateHz = 0.0;
  QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                            SstvToneDetector {invalid});
  invalid = testConfig ();
  invalid.hopSamples = invalid.windowSamples + 1;
  QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                            SstvToneDetector {invalid});
  invalid = testConfig ();
  invalid.nominalFrequenciesHz.push_back (1'900.0);
  QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                            SstvToneDetector {invalid});
  invalid = testConfig ();
  invalid.nominalFrequenciesHz = {5'950.0};
  QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                            SstvToneDetector {invalid});

  SstvToneDetector detector {testConfig ()};
  QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                            detector.consume (nullptr, 1));
  QVERIFY (detector.consume (nullptr, 0).empty ());
}

void TestSstvToneDetector::rejectsHostileConfigurationAndOversizedCallsTransactionally ()
{
  auto invalid = testConfig ();
  invalid.sampleRateHz = SstvToneDetector::MaximumSampleRateHz + 1.0;
  QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                            SstvToneDetector {invalid});

  invalid = testConfig ();
  invalid.windowSamples = SstvToneDetector::MaximumWindowSamples + 1;
  invalid.hopSamples = invalid.windowSamples;
  QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                            SstvToneDetector {invalid});

  invalid = testConfig ();
  invalid.nominalFrequenciesHz.assign (
      SstvToneDetector::MaximumNominalFrequencies + 1, 1'500.0);
  QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                            SstvToneDetector {invalid});

  invalid = testConfig ();
  invalid.searchStepHz = std::numeric_limits<double>::denorm_min ();
  QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                            SstvToneDetector {invalid});

  invalid = testConfig ();
  invalid.maximumOffsetHz =
      SstvToneDetector::MaximumConfiguredOffsetHz + 1.0;
  QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                            SstvToneDetector {invalid});

  invalid = testConfig ();
  invalid.windowSamples = SstvToneDetector::MaximumWindowSamples;
  invalid.hopSamples = invalid.windowSamples;
  invalid.searchStepHz = 2.0;
  QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                            SstvToneDetector {invalid});

  SstvToneDetector detector {testConfig ()};
  float const sentinel = 0.0f;
  auto const metricsBefore = detector.metrics ();
  QVERIFY_THROWS_EXCEPTION (
      std::length_error,
      detector.consume (&sentinel,
                        SstvToneDetector::MaximumSamplesPerConsume + 1));
  QCOMPARE (detector.metrics ().samplesConsumed,
            metricsBefore.samplesConsumed);
  QCOMPARE (detector.metrics ().windowsAnalysed,
            metricsBefore.windowsAnalysed);
  QCOMPARE (detector.bufferedSampleCount (), std::size_t {0});

  // Equal to the public sample bound still exceeds the separately bounded
  // output/work budget for this overlap.  The pointer is intentionally one
  // sample: transactional preflight must reject before dereferencing it.
  QVERIFY_THROWS_EXCEPTION (
      std::length_error,
      detector.consume (&sentinel,
                        SstvToneDetector::MaximumSamplesPerConsume));
  QCOMPARE (detector.metrics ().samplesConsumed, std::uint64_t {0});
  QCOMPARE (detector.bufferedSampleCount (), std::size_t {0});
}

void TestSstvToneDetector::detectsEveryRequiredTone_data ()
{
  QTest::addColumn<double> ("frequency");
  QTest::newRow ("VIS zero 1100") << 1'100.0;
  QTest::newRow ("sync 1200") << 1'200.0;
  QTest::newRow ("VIS one 1300") << 1'300.0;
  QTest::newRow ("black 1500") << 1'500.0;
  QTest::newRow ("leader FSK one 1900") << 1'900.0;
  QTest::newRow ("FSK zero 2100") << 2'100.0;
  QTest::newRow ("white 2300") << 2'300.0;
}

void TestSstvToneDetector::detectsEveryRequiredTone ()
{
  QFETCH (double, frequency);
  auto const config = testConfig ();
  SstvToneDetector detector {config};
  auto const observations = detector.consume (
      tone (config.sampleRateHz, frequency, config.windowSamples * 3));
  QVERIFY (!observations.empty ());
  auto const* detected = lastDetected (observations);
  QVERIFY (detected != nullptr);
  QCOMPARE (detected->nominalFrequencyHz, frequency);
  QVERIFY (std::abs (detected->detectedFrequencyHz - frequency) < 2.0);
  QVERIFY (detected->snrDb > 20.0);
  QVERIFY (detected->confidence > 0.7);
  QCOMPARE (detector.metrics ().detections,
            static_cast<std::uint64_t> (observations.size ()));
}

void TestSstvToneDetector::rejectsSilenceDcAndLowLevelTone ()
{
  auto const config = testConfig ();
  for (float value : {0.0f, 0.6f})
    {
      SstvToneDetector detector {config};
      std::vector<float> samples (config.windowSamples * 2, value);
      auto const observations = detector.consume (samples);
      QVERIFY (!observations.empty ());
      QVERIFY (std::all_of (
          observations.begin (), observations.end (),
          [] (SstvToneObservation const& observation) {
            return observation.status == SstvToneStatus::LowSignal
                   && !observation.valid ();
          }));
    }

  SstvToneDetector quiet {config};
  auto const low = quiet.consume (
      tone (config.sampleRateHz, 1'900.0, config.windowSamples * 2,
            config.minimumRms * 0.5));
  QVERIFY (std::all_of (
      low.begin (), low.end (), [] (SstvToneObservation const& observation) {
        return observation.status == SstvToneStatus::LowSignal;
      }));
}

void TestSstvToneDetector::rejectsNoiseDualToneAndOffFrequency ()
{
  auto const config = testConfig ();

  SstvToneDetector noiseDetector {config};
  auto const noise = noiseDetector.consume (
      whiteNoise (config.windowSamples * 4, 0.18));
  QVERIFY (!noise.empty ());
  QVERIFY (std::none_of (noise.begin (), noise.end (),
                         [] (SstvToneObservation const& observation) {
                           return observation.valid ();
                         }));
  QVERIFY (noiseDetector.metrics ().ambiguousWindows > 0);

  SstvToneDetector dualDetector {config};
  auto dual = addSignals (
      tone (config.sampleRateHz, 1'100.0, config.windowSamples * 3, 0.35),
      tone (config.sampleRateHz, 1'300.0, config.windowSamples * 3, 0.35,
            1.1));
  auto const dualResults = dualDetector.consume (dual);
  QVERIFY (std::none_of (dualResults.begin (), dualResults.end (),
                         [] (SstvToneObservation const& observation) {
                           return observation.valid ();
                         }));

  SstvToneDetector offFrequency {config};
  auto const off = offFrequency.consume (
      tone (config.sampleRateHz, 1'700.0, config.windowSamples * 3));
  QVERIFY (std::none_of (off.begin (), off.end (),
                         [] (SstvToneObservation const& observation) {
                           return observation.valid ();
                         }));
}

void TestSstvToneDetector::reportsNonFiniteInputWithoutUnboundedState ()
{
  auto const config = testConfig ();
  SstvToneDetector detector {config};
  auto samples = tone (config.sampleRateHz, 1'900.0,
                       config.windowSamples * 3);
  samples[config.windowSamples / 2] =
      std::numeric_limits<float>::quiet_NaN ();
  auto const observations = detector.consume (samples);
  QVERIFY (!observations.empty ());
  QVERIFY (std::any_of (
      observations.begin (), observations.end (),
      [] (SstvToneObservation const& observation) {
        return observation.status == SstvToneStatus::InvalidInput;
      }));
  QVERIFY (detector.metrics ().invalidWindows > 0);
  QVERIFY (detector.bufferedSampleCount () < config.windowSamples);
  QVERIFY (detector.metrics ().peakBufferedSamples <= config.windowSamples);
}

void TestSstvToneDetector::chunkingProducesIdenticalObservations ()
{
  auto const config = testConfig ();
  auto samples = addSignals (
      tone (config.sampleRateHz, 1'900.0, config.windowSamples * 8, 0.65),
      whiteNoise (config.windowSamples * 8, 0.01));

  SstvToneDetector contiguous {config};
  auto const expected = contiguous.consume (samples);

  SstvToneDetector chunked {config};
  std::vector<SstvToneObservation> actual;
  std::size_t position = 0;
  std::size_t chunkIndex = 0;
  std::size_t const chunks[] {1, 17, 3, 511, 2, 89, 1'003, 7};
  while (position < samples.size ())
    {
      std::size_t const count = std::min (
          chunks[chunkIndex++ % (sizeof (chunks) / sizeof (chunks[0]))],
          samples.size () - position);
      auto observations = chunked.consume (samples.data () + position, count);
      actual.insert (actual.end (), observations.begin (), observations.end ());
      position += count;
    }

  QCOMPARE (actual.size (), expected.size ());
  for (std::size_t index = 0; index < expected.size (); ++index)
    {
      QCOMPARE (actual[index].status, expected[index].status);
      QCOMPARE (actual[index].sequence, expected[index].sequence);
      QCOMPARE (actual[index].startSample, expected[index].startSample);
      QCOMPARE (actual[index].centreSample, expected[index].centreSample);
      QCOMPARE (actual[index].nominalFrequencyHz,
                expected[index].nominalFrequencyHz);
      QCOMPARE (actual[index].detectedFrequencyHz,
                expected[index].detectedFrequencyHz);
      QCOMPARE (actual[index].frequencyOffsetHz,
                expected[index].frequencyOffsetHz);
      QCOMPARE (actual[index].confidence, expected[index].confidence);
    }
  QCOMPARE (chunked.metrics ().samplesConsumed,
            static_cast<std::uint64_t> (samples.size ()));
  QCOMPARE (chunked.bufferedSampleCount (), contiguous.bufferedSampleCount ());
}

void TestSstvToneDetector::tracksPositiveAndNegativeCommonOffset ()
{
  auto const config = testConfig ();
  for (double offset : {98.0, -98.0})
    {
      SstvToneDetector detector {config};
      auto const observations = detector.consume (
          tone (config.sampleRateHz, 1'900.0 + offset,
                config.windowSamples * 4));
      auto const* detected = lastDetected (observations);
      QVERIFY (detected != nullptr);
      QCOMPARE (detected->nominalFrequencyHz, 1'900.0);
      QVERIFY2 (std::abs (detected->frequencyOffsetHz - offset) < 3.0,
                qPrintable (QStringLiteral ("measured %1, expected %2")
                                .arg (detected->frequencyOffsetHz)
                                .arg (offset)));
      QVERIFY (detector.hasCommonOffset ());
      QVERIFY (std::abs (*detector.commonOffsetHz () - offset) < 3.0);
    }
}

void TestSstvToneDetector::trackedOffsetDisambiguatesCloselySpacedTones ()
{
  auto const config = testConfig ();
  SstvToneDetector detector {config};
  auto leader = detector.consume (
      tone (config.sampleRateHz, 1'980.0, config.windowSamples * 3));
  QVERIFY (lastDetected (leader) != nullptr);
  QVERIFY (std::abs (*detector.commonOffsetHz () - 80.0) < 3.0);

  // Clear only stream overlap: the shared radio offset remains available to
  // distinguish 1200+80 from the equally plausible 1300-20 hypothesis.
  detector.reset (true);
  auto separator = detector.consume (
      tone (config.sampleRateHz, 1'280.0, config.windowSamples * 3));
  auto const* detected = lastDetected (separator);
  QVERIFY (detected != nullptr);
  QCOMPARE (detected->nominalFrequencyHz, 1'200.0);
  QVERIFY (std::abs (detected->frequencyOffsetHz - 80.0) < 3.0);
}

void TestSstvToneDetector::overlapIndexesAndMemoryStayBounded ()
{
  auto config = testConfig ();
  config.hopSamples = 73;
  SstvToneDetector detector {config};
  auto const samples = tone (config.sampleRateHz, 1'500.0, 25'000);
  auto const observations = detector.consume (samples);
  QVERIFY (!observations.empty ());
  for (std::size_t index = 0; index < observations.size (); ++index)
    {
      QCOMPARE (observations[index].sequence,
                static_cast<std::uint64_t> (index));
      QCOMPARE (observations[index].startSample,
                static_cast<std::uint64_t> (index * config.hopSamples));
      QCOMPARE (observations[index].centreSample,
                static_cast<std::uint64_t> (index * config.hopSamples
                                            + config.windowSamples / 2));
    }

  std::size_t const expectedWindows =
      1 + (samples.size () - config.windowSamples) / config.hopSamples;
  QCOMPARE (observations.size (), expectedWindows);
  QVERIFY (detector.bufferedSampleCount () < config.windowSamples);
  QCOMPARE (detector.maximumBufferedSampleCount (), config.windowSamples);
  QCOMPARE (detector.metrics ().peakBufferedSamples, config.windowSamples);
  QCOMPARE (detector.metrics ().windowsAnalysed,
            static_cast<std::uint64_t> (expectedWindows));
}

void TestSstvToneDetector::resetAndExternalOffsetSeedAreExplicit ()
{
  auto const config = testConfig ();
  SstvToneDetector detector {config};
  detector.seedCommonOffset (75.0);
  QCOMPARE (detector.commonOffsetHz (), std::optional<double> {75.0});
  QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                            detector.seedCommonOffset (101.0));

  detector.consume (
      tone (config.sampleRateHz, 1'975.0, config.windowSamples * 2));
  QVERIFY (detector.metrics ().samplesConsumed > 0);
  detector.reset (true);
  QCOMPARE (detector.metrics ().samplesConsumed, std::uint64_t {0});
  QCOMPARE (detector.bufferedSampleCount (), std::size_t {0});
  QVERIFY (detector.hasCommonOffset ());

  detector.reset (false);
  QVERIFY (!detector.hasCommonOffset ());
  detector.seedCommonOffset (-100.0);
  detector.clearCommonOffset ();
  QVERIFY (!detector.hasCommonOffset ());
}

void TestSstvToneDetector::exactBoundaryResetDropsOverlapAndPreservesStreamCoordinates ()
{
  auto config = testConfig ();
  config.windowSamples = 120;
  config.hopSamples = 60;
  SstvToneDetector detector {config};

  const auto first = tone (config.sampleRateHz, 1'500.0, 175);
  QVERIFY (!detector.consume (first).empty ());
  QVERIFY (detector.bufferedSampleCount () > 0U);
  detector.seedCommonOffset (20.0);

  constexpr std::uint64_t boundary = 10'000U;
  detector.resetAtStreamSample (boundary, false);
  QCOMPARE (detector.metrics ().samplesConsumed, boundary);
  QCOMPARE (detector.bufferedSampleCount (), std::size_t {0});
  QVERIFY (!detector.hasCommonOffset ());

  const auto second = tone (config.sampleRateHz, 1'900.0, 240);
  const auto observations = detector.consume (second);
  QVERIFY (!observations.empty ());
  QCOMPARE (observations.front ().sequence, std::uint64_t {0});
  QCOMPARE (observations.front ().startSample, boundary);
  QCOMPARE (observations.front ().centreSample,
            boundary + config.windowSamples / 2U);
  QCOMPARE (detector.metrics ().samplesConsumed,
            boundary + static_cast<std::uint64_t> (second.size ()));
}

QTEST_APPLESS_MAIN (TestSstvToneDetector)

#include "test_sstv_tone_detector.moc"
