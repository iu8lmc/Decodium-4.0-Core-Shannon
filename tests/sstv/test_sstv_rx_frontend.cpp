// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

#include "src/sstv/dsp/SstvFrequencyDemodulator.h"
#include "src/sstv/dsp/SstvPreprocessor.h"
#include "src/sstv/dsp/SstvSignalMetrics.h"

namespace
{
using decodium::sstv::SstvFrequencyDemodulator;
using decodium::sstv::SstvFrequencyDemodulatorConfig;
using decodium::sstv::SstvFrequencyObservation;
using decodium::sstv::SstvPreprocessor;
using decodium::sstv::SstvPreprocessorConfig;
using decodium::sstv::SstvSignalMetrics;

constexpr double Pi = 3.141592653589793238462643383279502884;
constexpr double TwoPi = 2.0 * Pi;
constexpr double SampleRateHz = 12'000.0;

std::vector<float> makeTone (double frequencyHz,
                             std::size_t count,
                             double amplitude = 0.5,
                             double dc = 0.0,
                             double noiseAmplitude = 0.0,
                             std::uint32_t seed = 0x31415926u)
{
  std::vector<float> samples;
  samples.reserve (count);
  std::uint32_t state = seed;
  for (std::size_t index = 0u; index < count; ++index)
    {
      state ^= state << 13u;
      state ^= state >> 17u;
      state ^= state << 5u;
      double const uniform = static_cast<double> (state)
                             / static_cast<double> (
                                 std::numeric_limits<std::uint32_t>::max ());
      double const noise = noiseAmplitude * (2.0 * uniform - 1.0);
      double const phase = 2.0 * Pi * frequencyHz
                           * static_cast<double> (index) / SampleRateHz
                           + 0.271;
      samples.push_back (static_cast<float> (
          dc + amplitude * std::sin (phase) + noise));
    }
  return samples;
}

double tailRms (std::vector<float> const& samples, std::size_t skip)
{
  double power = 0.0;
  std::size_t count = 0u;
  for (std::size_t index = std::min (skip, samples.size ());
       index < samples.size ();
       ++index)
    {
      double const value = static_cast<double> (samples[index]);
      power += value * value;
      ++count;
    }
  return count == 0u ? 0.0
                     : std::sqrt (power / static_cast<double> (count));
}

double tailMean (std::vector<float> const& samples, std::size_t skip)
{
  double sum = 0.0;
  std::size_t count = 0u;
  for (std::size_t index = std::min (skip, samples.size ());
       index < samples.size ();
       ++index)
    {
      sum += static_cast<double> (samples[index]);
      ++count;
    }
  return count == 0u ? 0.0 : sum / static_cast<double> (count);
}

std::vector<SstvFrequencyObservation> consumeInChunks (
    SstvFrequencyDemodulator& demodulator,
    std::vector<float> const& samples,
    std::vector<std::size_t> const& chunkPattern)
{
  std::vector<SstvFrequencyObservation> observations;
  std::size_t offset = 0u;
  std::size_t chunkIndex = 0u;
  while (offset < samples.size ())
    {
      std::size_t const requested = chunkPattern[chunkIndex
                                                 % chunkPattern.size ()];
      std::size_t const count = std::min (requested,
                                          samples.size () - offset);
      auto const produced = demodulator.consume (samples.data () + offset,
                                                 count);
      observations.insert (observations.end (),
                           produced.begin (),
                           produced.end ());
      offset += count;
      ++chunkIndex;
    }
  return observations;
}

double medianValidFrequency (
    std::vector<SstvFrequencyObservation> const& observations,
    bool corrected = false)
{
  std::vector<double> values;
  for (SstvFrequencyObservation const& observation : observations)
    {
      if (observation.valid ())
        {
          values.push_back (corrected ? observation.correctedFrequencyHz
                                      : observation.rawFrequencyHz);
        }
    }
  if (values.empty ())
    {
      return 0.0;
    }
  auto const middle = values.begin ()
                      + static_cast<std::ptrdiff_t> (values.size () / 2u);
  std::nth_element (values.begin (), middle, values.end ());
  return *middle;
}

void verifyFinite (SstvFrequencyObservation const& observation)
{
  QVERIFY (std::isfinite (observation.rawFrequencyHz));
  QVERIFY (std::isfinite (observation.correctedFrequencyHz));
  QVERIFY (std::isfinite (observation.afcCorrectionHz));
  QVERIFY (std::isfinite (observation.rms));
  QVERIFY (std::isfinite (observation.phaseJitterHz));
  QVERIFY (std::isfinite (observation.estimatedNoiseRms));
  QVERIFY (std::isfinite (observation.snrDb));
  QVERIFY (std::isfinite (observation.confidence));
  QVERIFY (std::isfinite (observation.validSampleFraction));
}
}

class TestSstvRxFrontend final : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void preprocessorChunkBoundaryEquivalence ()
  {
    std::vector<float> const input = makeTone (1'500.0,
                                               24'000u,
                                               0.18,
                                               0.27,
                                               0.01);
    SstvPreprocessor whole;
    std::vector<float> const expected = whole.process (input);

    SstvPreprocessor chunked;
    std::vector<float> actual (input.size (), 0.0F);
    std::vector<std::size_t> const pattern {1u, 7u, 113u, 2u, 509u, 31u};
    std::size_t offset = 0u;
    std::size_t patternIndex = 0u;
    while (offset < input.size ())
      {
        std::size_t const count = std::min (
            pattern[patternIndex % pattern.size ()], input.size () - offset);
        QVERIFY (chunked.process (input.data () + offset,
                                  count,
                                  actual.data () + offset));
        offset += count;
        ++patternIndex;
      }

    QCOMPARE (actual.size (), expected.size ());
    for (std::size_t index = 0u; index < expected.size (); ++index)
      {
        QCOMPARE (actual[index], expected[index]);
      }
    auto const wholeMetrics = whole.metricsSnapshot ();
    auto const chunkedMetrics = chunked.metricsSnapshot ();
    QCOMPARE (chunkedMetrics.samplesConsumed, wholeMetrics.samplesConsumed);
    QCOMPARE (chunkedMetrics.samplesProduced, wholeMetrics.samplesProduced);
    QCOMPARE (chunkedMetrics.currentAutomaticGain,
              wholeMetrics.currentAutomaticGain);
    QCOMPARE (chunkedMetrics.outputRms, wholeMetrics.outputRms);
  }

  void preprocessorBandPassAndDcBlocker ()
  {
    SstvPreprocessorConfig config = SstvPreprocessorConfig::sstvDefaults ();
    config.levelControlEnabled = false;
    config.limiterEnabled = false;

    auto response = [&config] (double frequencyHz, double dc) {
      SstvPreprocessor processor {config};
      return processor.process (makeTone (frequencyHz,
                                          24'000u,
                                          0.4,
                                          dc));
    };

    std::vector<float> const low = response (300.0, 0.0);
    std::vector<float> const pass = response (1'500.0, 0.0);
    std::vector<float> const high = response (4'000.0, 0.0);
    std::vector<float> const withDc = response (1'500.0, 0.6);
    double const passRms = tailRms (pass, 6'000u);
    QVERIFY2 (passRms > 4.0 * tailRms (low, 6'000u),
              "900 Hz high-pass did not reject low audio strongly enough");
    QVERIFY2 (passRms > 3.0 * tailRms (high, 6'000u),
              "2500 Hz low-pass did not reject high audio strongly enough");
    QVERIFY (std::abs (tailMean (withDc, 6'000u)) < 1.0e-3);
  }

  void optionalHumNotchRejectsMainsWithoutDamagingImageBand ()
  {
    SstvPreprocessorConfig config = SstvPreprocessorConfig::sstvDefaults ();
    config.dcBlockerEnabled = false;
    config.bandPassEnabled = false;
    config.levelControlEnabled = false;
    config.limiterEnabled = false;
    config.humNotchEnabled = true;
    config.humFrequencyHz = 50.0;
    config.humNotchQ = 20.0;

    auto response = [&config] (double frequencyHz) {
      SstvPreprocessor processor {config};
      return processor.process (makeTone (frequencyHz, 36'000u, 0.4));
    };
    std::vector<float> const rejected = response (50.0);
    std::vector<float> const preserved = response (1'500.0);
    double const inputRms = 0.4 / std::sqrt (2.0);
    QVERIFY2 (tailRms (rejected, 18'000u) < inputRms / 20.0,
              "50 Hz notch did not settle to the required rejection");
    QVERIFY2 (tailRms (preserved, 18'000u) > inputRms * 0.98,
              "mains notch damaged the SSTV image-tone band");

    config.humFrequencyHz = 60.0;
    SstvPreprocessor sixtyHertz {config};
    auto const sixty = sixtyHertz.process (makeTone (60.0, 36'000u, 0.4));
    QVERIFY (tailRms (sixty, 18'000u) < inputRms / 20.0);
  }

  void optionalImpulseSuppressorIsBoundedAndChunkInvariant ()
  {
    SstvPreprocessorConfig config = SstvPreprocessorConfig::sstvDefaults ();
    config.dcBlockerEnabled = false;
    config.bandPassEnabled = false;
    config.levelControlEnabled = false;
    config.limiterEnabled = false;
    config.impulseSuppressorEnabled = true;
    config.impulseThreshold = 0.20;

    std::vector<float> input (4'096u, 0.05F);
    input[3] = 1.0F;
    input[257] = -1.0F;
    input[2'048] = 0.95F;

    SstvPreprocessor whole {config};
    std::vector<float> const expected = whole.process (input);
    QCOMPARE (whole.metricsSnapshot ().impulseSamplesSuppressed,
              std::uint64_t {3u});
    QVERIFY (std::abs (expected[3] - 0.05F) < 1.0e-6F);
    QVERIFY (std::abs (expected[257] - 0.05F) < 1.0e-6F);
    QVERIFY (std::abs (expected[2'048] - 0.05F) < 1.0e-6F);

    SstvPreprocessor fragmented {config};
    std::vector<float> actual (input.size (), 0.0F);
    std::vector<std::size_t> const chunks {1u, 2u, 17u, 251u, 7u};
    std::size_t offset = 0u;
    std::size_t chunk = 0u;
    while (offset < input.size ())
      {
        std::size_t const count = std::min (
            chunks[chunk++ % chunks.size ()], input.size () - offset);
        QVERIFY (fragmented.process (input.data () + offset,
                                     count,
                                     actual.data () + offset));
        offset += count;
      }
    QCOMPARE (actual, expected);
    QCOMPARE (fragmented.metricsSnapshot ().impulseSamplesSuppressed,
              whole.metricsSnapshot ().impulseSamplesSuppressed);

    fragmented.reset ();
    QCOMPARE (fragmented.metricsSnapshot ().impulseSamplesSuppressed,
              std::uint64_t {0u});
  }

  void levelControlLimiterAndClippingRemainVisible ()
  {
    std::vector<float> const input = makeTone (1'500.0, 12'000u, 0.4);
    SstvPreprocessorConfig limitedConfig =
        SstvPreprocessorConfig::sstvDefaults ();
    limitedConfig.dcBlockerEnabled = false;
    limitedConfig.bandPassEnabled = false;
    limitedConfig.levelControlEnabled = false;
    limitedConfig.inputGain = 4.0;
    limitedConfig.limiterEnabled = true;
    limitedConfig.limiterThreshold = 0.8;
    SstvPreprocessor limited {limitedConfig};
    std::vector<float> const limitedOutput = limited.process (input);
    auto const limitedMetrics = limited.metricsSnapshot ();
    QVERIFY (*std::max_element (limitedOutput.begin (), limitedOutput.end ())
             <= 0.800001F);
    QVERIFY (limitedMetrics.preLimiterClippedSamples > 0u);
    QCOMPARE (limitedMetrics.outputClippedSamples, std::uint64_t {0u});
    QCOMPARE (limitedMetrics.limiterEvents,
              limitedMetrics.preLimiterClippedSamples);

    limitedConfig.limiterEnabled = false;
    SstvPreprocessor unlimited {limitedConfig};
    std::vector<float> const unlimitedOutput = unlimited.process (input);
    auto const unlimitedMetrics = unlimited.metricsSnapshot ();
    QVERIFY (*std::max_element (unlimitedOutput.begin (),
                               unlimitedOutput.end ()) > 1.5F);
    QVERIFY (unlimitedMetrics.outputClippedSamples > 0u);
    QCOMPARE (unlimitedMetrics.limiterEvents, std::uint64_t {0u});

    SstvPreprocessorConfig controlledConfig = limitedConfig;
    controlledConfig.inputGain = 1.0;
    controlledConfig.levelControlEnabled = true;
    controlledConfig.maximumAutomaticGain = 8.0;
    SstvPreprocessor controlled {controlledConfig};
    controlled.process (makeTone (1'500.0, 36'000u, 0.025));
    QVERIFY (controlled.metricsSnapshot ().currentAutomaticGain > 2.0);
  }

  void finiteExtremeInputCannotOverflowFloatOutput ()
  {
    SstvPreprocessorConfig config = SstvPreprocessorConfig::sstvDefaults ();
    config.dcBlockerEnabled = false;
    config.bandPassEnabled = false;
    config.levelControlEnabled = false;
    config.limiterEnabled = false;
    config.inputGain = 64.0;
    SstvPreprocessor processor {config};

    float const input[] {std::numeric_limits<float>::max (),
                         -std::numeric_limits<float>::max ()};
    float output[] {0.0F, 0.0F};
    QVERIFY (!processor.process (input, std::size (input), output));
    QCOMPARE (output[0], std::numeric_limits<float>::max ());
    QCOMPARE (output[1], -std::numeric_limits<float>::max ());
    QVERIFY (std::isfinite (output[0]));
    QVERIFY (std::isfinite (output[1]));
    QCOMPARE (processor.metricsSnapshot ().internalNumericFaults,
              std::uint64_t {2u});
  }

  void signalMetricsAreFiniteHonestAndSnapshotSafe ()
  {
    SstvSignalMetrics metrics;
    std::vector<float> samples = makeTone (1'500.0, 12'000u, 0.5);
    samples.push_back (1.2F);
    samples.push_back (std::numeric_limits<float>::quiet_NaN ());
    QVERIFY (!metrics.observeSamples (samples.data (), samples.size ()));
    QVERIFY (metrics.observeSpectralEstimate (0.25, 0.0025, 0.8));
    QVERIFY (!metrics.observeSpectralEstimate (
        std::numeric_limits<double>::quiet_NaN (), 0.0, 0.0));
    metrics.recordDroppedSamples (17u);

    auto const first = metrics.snapshot ();
    QCOMPARE (first.samplesObserved,
              static_cast<std::uint64_t> (samples.size ()));
    QCOMPARE (first.invalidSamples, std::uint64_t {1u});
    QCOMPARE (first.clippedSamples, std::uint64_t {1u});
    QCOMPARE (first.droppedSamples, std::uint64_t {17u});
    QCOMPARE (first.invalidSpectralEstimates, std::uint64_t {1u});
    QVERIFY (first.noiseEstimateAvailable);
    QVERIFY (first.snrAvailable);
    QVERIFY (first.confidenceAvailable);
    QVERIFY (std::abs (first.snrDb - 20.0) < 1.0e-9);
    QVERIFY (std::abs (first.confidence - 0.8) < 1.0e-12);
    QVERIFY (std::isfinite (first.rmsDbfs));
    QVERIFY (std::isfinite (first.noiseFloorDbfs));

    SstvSignalMetrics extreme;
    QVERIFY (extreme.observeSpectralEstimate (
        std::numeric_limits<double>::max (),
        std::numeric_limits<double>::denorm_min (),
        1.0));
    auto const extremeSnapshot = extreme.snapshot ();
    QVERIFY (std::isfinite (extremeSnapshot.snrDb));
    QCOMPARE (extremeSnapshot.snrDb, 120.0);

    SstvSignalMetrics concurrent;
    std::atomic<bool> running {true};
    std::thread writer {[&concurrent, &running] {
      std::vector<float> const block = makeTone (1'900.0, 64u, 0.2);
      for (std::size_t iteration = 0u; iteration < 1'000u; ++iteration)
        {
          concurrent.observeSamples (block.data (), block.size ());
        }
      running.store (false, std::memory_order_release);
    }};
    while (running.load (std::memory_order_acquire))
      {
        auto const snapshot = concurrent.snapshot ();
        QVERIFY (std::isfinite (snapshot.rms));
        QVERIFY (std::isfinite (snapshot.peak));
      }
    writer.join ();
    QCOMPARE (concurrent.snapshot ().samplesObserved, std::uint64_t {64'000u});

    concurrent.reset ();
    auto const reset = concurrent.snapshot ();
    QCOMPARE (reset.samplesObserved, std::uint64_t {0u});
    QVERIFY (!reset.noiseEstimateAvailable);
    QCOMPARE (reset.rms, 0.0);
  }

  void demodulatesCanonicalTones_data ()
  {
    QTest::addColumn<double> ("frequencyHz");
    QTest::newRow ("black-1100") << 1'100.0;
    QTest::newRow ("vis-sync-1200") << 1'200.0;
    QTest::newRow ("mid-grey-1500") << 1'500.0;
    QTest::newRow ("white-1900") << 1'900.0;
    QTest::newRow ("leader-2300") << 2'300.0;
  }

  void demodulatesCanonicalTones ()
  {
    QFETCH (double, frequencyHz);
    SstvFrequencyDemodulator demodulator;
    auto const observations = consumeInChunks (
        demodulator,
        makeTone (frequencyHz, 12'000u, 0.35),
        {1u, 17u, 64u, 5u, 251u});
    QVERIFY (observations.size () > 1'000u);
    double const measured = medianValidFrequency (observations);
    QVERIFY2 (std::abs (measured - frequencyHz) < 5.0,
              qPrintable (QStringLiteral ("measured %1 Hz for %2 Hz")
                              .arg (measured)
                              .arg (frequencyHz)));
    for (SstvFrequencyObservation const& observation : observations)
      {
        verifyFinite (observation);
      }
    auto const metrics = demodulator.metricsSnapshot ();
    QCOMPARE (metrics.samplesConsumed, std::uint64_t {12'000u});
    QVERIFY (metrics.validObservations > 1'000u);
    QVERIFY (metrics.signal.noiseEstimateAvailable);
  }

  void demodulatorChunkBoundaryEquivalence ()
  {
    std::vector<float> const input = makeTone (1'900.0,
                                               18'000u,
                                               0.2,
                                               0.0,
                                               0.005);
    SstvFrequencyDemodulator whole;
    auto const expected = whole.consume (input);
    SstvFrequencyDemodulator chunked;
    auto const actual = consumeInChunks (chunked,
                                         input,
                                         {1u, 2u, 37u, 509u, 11u, 128u});
    QCOMPARE (actual.size (), expected.size ());
    for (std::size_t index = 0u; index < expected.size (); ++index)
      {
        QCOMPARE (actual[index].sequence, expected[index].sequence);
        QCOMPARE (actual[index].centreSample, expected[index].centreSample);
        QCOMPARE (static_cast<int> (actual[index].status),
                  static_cast<int> (expected[index].status));
        QCOMPARE (actual[index].rawFrequencyHz,
                  expected[index].rawFrequencyHz);
        QCOMPARE (actual[index].confidence, expected[index].confidence);
      }
  }

  void handlesAmplitudeNoiseAndDcThroughFrontend ()
  {
    std::vector<float> const input = makeTone (1'900.0,
                                               24'000u,
                                               0.10,
                                               0.35,
                                               0.012);
    SstvPreprocessorConfig preConfig = SstvPreprocessorConfig::sstvDefaults ();
    preConfig.levelControlEnabled = false;
    SstvPreprocessor preprocessor {preConfig};
    std::vector<float> const filtered = preprocessor.process (input);
    SstvFrequencyDemodulator demodulator;
    auto const observations = consumeInChunks (demodulator,
                                                filtered,
                                                {73u, 1u, 511u, 19u});
    double const measured = medianValidFrequency (observations);
    QVERIFY (std::abs (measured - 1'900.0) < 12.0);
    QVERIFY (demodulator.metricsSnapshot ().validObservations > 2'000u);
    QVERIFY (std::abs (tailMean (filtered, 6'000u)) < 1.0e-3);

    SstvFrequencyDemodulator lowLevel;
    auto const lowObservations = lowLevel.consume (
        makeTone (1'900.0, 2'000u, 0.0001));
    QVERIFY (std::any_of (lowObservations.begin (),
                         lowObservations.end (),
                         [] (SstvFrequencyObservation const& observation) {
                           return observation.status
                                  == decodium::sstv::SstvFrequencyStatus::LowSignal;
                         }));

    SstvFrequencyDemodulator silence;
    auto const silenceObservations = silence.consume (
        std::vector<float> (2'000u, 0.0F));
    QVERIFY (!silenceObservations.empty ());
    QVERIFY (std::all_of (
        silenceObservations.begin (),
        silenceObservations.end (),
        [] (SstvFrequencyObservation const& observation) {
          return observation.status
                     == decodium::sstv::SstvFrequencyStatus::LowSignal
                 && observation.validSampleFraction == 1.0;
        }));
    auto const silenceMetrics = silence.metricsSnapshot ();
    QCOMPARE (silenceMetrics.invalidObservations, std::uint64_t {0u});
    QCOMPARE (silenceMetrics.lowSignalObservations,
              static_cast<std::uint64_t> (silenceObservations.size ()));
    QVERIFY (!silenceMetrics.signal.noiseEstimateAvailable);
  }

  void resetIsDeterministic ()
  {
    std::vector<float> const input = makeTone (1'500.0,
                                               6'000u,
                                               0.23,
                                               0.2,
                                               0.003);
    SstvPreprocessor preprocessor;
    std::vector<float> const firstFiltered = preprocessor.process (input);
    preprocessor.reset ();
    std::vector<float> const secondFiltered = preprocessor.process (input);
    QVERIFY (secondFiltered == firstFiltered);

    SstvFrequencyDemodulator demodulator;
    auto const first = demodulator.consume (firstFiltered);
    demodulator.setAfcCorrectionHz (41.0);
    demodulator.reset (false);
    QCOMPARE (demodulator.afcCorrectionHz (), 0.0);
    auto const second = demodulator.consume (secondFiltered);
    QCOMPARE (second.size (), first.size ());
    for (std::size_t index = 0u; index < first.size (); ++index)
      {
        QCOMPARE (second[index].rawFrequencyHz, first[index].rawFrequencyHz);
        QCOMPARE (second[index].confidence, first[index].confidence);
      }
    QCOMPARE (demodulator.metricsSnapshot ().samplesConsumed,
              std::uint64_t {6'000u});
  }

  void afcIsExplicitAndBounded ()
  {
    SstvFrequencyDemodulator demodulator;
    QCOMPARE (demodulator.setAfcCorrectionHz (500.0), 150.0);
    QCOMPARE (demodulator.setAfcCorrectionHz (-500.0), -150.0);
    QCOMPARE (demodulator.setAfcCorrectionHz (50.0), 50.0);
    auto const observations = demodulator.consume (
        makeTone (1'550.0, 12'000u, 0.4));
    QVERIFY (std::abs (medianValidFrequency (observations, true) - 1'500.0)
             < 5.0);

    demodulator.clearAfcCorrection ();
    QCOMPARE (demodulator.updateAfcFromReference (1'600.0, 1'500.0), 20.0);
    demodulator.reset (true);
    QCOMPARE (demodulator.afcCorrectionHz (), 20.0);
    double const beforeInvalid = demodulator.afcCorrectionHz ();
    QCOMPARE (demodulator.setAfcCorrectionHz (
                  std::numeric_limits<double>::quiet_NaN ()),
              beforeInvalid);
    QCOMPARE (demodulator.metricsSnapshot ().invalidAfcRequests,
              std::uint64_t {1u});
  }

  void invalidInputCannotPoisonState ()
  {
    std::vector<float> input = makeTone (1'500.0, 1'000u, 0.3);
    input[100] = std::numeric_limits<float>::quiet_NaN ();
    input[101] = std::numeric_limits<float>::infinity ();
    std::vector<float> output (input.size (), 0.0F);
    SstvPreprocessor preprocessor;
    QVERIFY (!preprocessor.process (input.data (), input.size (), output.data ()));
    for (float sample : output)
      {
        QVERIFY (std::isfinite (sample));
      }
    auto const preMetrics = preprocessor.metricsSnapshot ();
    QCOMPARE (preMetrics.invalidInputSamples, std::uint64_t {2u});
    QVERIFY (!preprocessor.process (nullptr, 4u, output.data ()));
    QCOMPARE (preprocessor.metricsSnapshot ().invalidInputCalls,
              std::uint64_t {1u});

    SstvFrequencyDemodulator demodulator;
    auto const observations = demodulator.consume (input);
    QVERIFY (demodulator.consume (nullptr, 16u).empty ());
    for (SstvFrequencyObservation const& observation : observations)
      {
        verifyFinite (observation);
      }
    auto const demodMetrics = demodulator.metricsSnapshot ();
    QCOMPARE (demodMetrics.invalidSamples, std::uint64_t {2u});
    QCOMPARE (demodMetrics.invalidInputCalls, std::uint64_t {1u});
    QCOMPARE (demodMetrics.rejectedSamples, std::uint64_t {16u});
    QVERIFY (demodMetrics.invalidObservations > 0u);

    SstvPreprocessorConfig invalidPre = SstvPreprocessorConfig::sstvDefaults ();
    invalidPre.sampleRateHz = 44'100.0;
    QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                              SstvPreprocessor {invalidPre});
    invalidPre = SstvPreprocessorConfig::sstvDefaults ();
    invalidPre.humFrequencyHz = 70.0;
    QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                              SstvPreprocessor {invalidPre});
    invalidPre = SstvPreprocessorConfig::sstvDefaults ();
    invalidPre.humNotchQ = 1.0;
    QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                              SstvPreprocessor {invalidPre});
    invalidPre = SstvPreprocessorConfig::sstvDefaults ();
    invalidPre.impulseThreshold = 0.0;
    QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                              SstvPreprocessor {invalidPre});
    SstvFrequencyDemodulatorConfig invalidDemod =
        SstvFrequencyDemodulatorConfig::sstvDefaults ();
    invalidDemod.hilbertTaps = 32u;
    QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                              SstvFrequencyDemodulator {invalidDemod});
    invalidDemod = SstvFrequencyDemodulatorConfig::sstvDefaults ();
    invalidDemod.maximumAfcCorrectionHz = 501.0;
    QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                              SstvFrequencyDemodulator {invalidDemod});

    invalidDemod = SstvFrequencyDemodulatorConfig::sstvDefaults ();
    invalidDemod.averagingSamples = 4'096u;
    invalidDemod.hopSamples = 1u;
    QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                              SstvFrequencyDemodulator {invalidDemod});

    invalidDemod.hopSamples = invalidDemod.averagingSamples
                              / SstvFrequencyDemodulator::
                                  MaximumAveragingWorkRatio;
    SstvFrequencyDemodulator boundedMaximum {invalidDemod};
    QVERIFY (boundedMaximum.consume (
                 makeTone (1'500.0, 8'192u, 0.2)).size ()
             <= boundedMaximum.maximumObservationsPerCall ());
  }

  void nullAndOversizeCallsAreTransactional ()
  {
    float const inputSentinel = 0.25F;
    float outputSentinel = 123.0F;

    SstvSignalMetrics signalMetrics;
    auto const signalBefore = signalMetrics.snapshot ();
    QVERIFY (!signalMetrics.observeSamples (nullptr, 7u));
    QVERIFY (!signalMetrics.observeSamples (
        &inputSentinel, SstvSignalMetrics::MaximumSamplesPerCall + 1u));
    auto const signalAfter = signalMetrics.snapshot ();
    QCOMPARE (signalAfter.samplesObserved, signalBefore.samplesObserved);
    QCOMPARE (signalAfter.rejectedInputCalls, std::uint64_t {2u});
    QCOMPARE (signalAfter.rejectedOversizeCalls, std::uint64_t {1u});

    SstvPreprocessor preprocessor;
    auto const preBefore = preprocessor.metricsSnapshot ();
    QVERIFY (!preprocessor.process (nullptr, 7u, &outputSentinel));
    QCOMPARE (outputSentinel, 123.0F);
    QVERIFY (!preprocessor.process (
        &inputSentinel,
        SstvPreprocessor::MaximumSamplesPerCall + 1u,
        &outputSentinel));
    QCOMPARE (outputSentinel, 123.0F);
    auto const preAfter = preprocessor.metricsSnapshot ();
    QCOMPARE (preAfter.samplesConsumed, preBefore.samplesConsumed);
    QCOMPARE (preAfter.samplesProduced, preBefore.samplesProduced);
    QCOMPARE (preAfter.invalidInputCalls, std::uint64_t {2u});
    QCOMPARE (preAfter.rejectedOversizeCalls, std::uint64_t {1u});

    std::vector<float> oversizedPreprocessorInput (
        SstvPreprocessor::MaximumSamplesPerCall + 1u, 0.25F);
    QVERIFY (preprocessor.process (oversizedPreprocessorInput).empty ());
    auto const preAfterVector = preprocessor.metricsSnapshot ();
    QCOMPARE (preAfterVector.samplesConsumed, preBefore.samplesConsumed);
    QCOMPARE (preAfterVector.samplesProduced, preBefore.samplesProduced);
    QCOMPARE (preAfterVector.invalidInputCalls, std::uint64_t {3u});
    QCOMPARE (preAfterVector.rejectedOversizeCalls, std::uint64_t {2u});

    SstvFrequencyDemodulator demodulator;
    demodulator.consume (makeTone (1'500.0, 100u, 0.2));
    auto const demodBefore = demodulator.metricsSnapshot ();
    std::size_t const bufferedBefore = demodulator.bufferedSampleCount ();
    QVERIFY (demodulator.consume (nullptr, 7u).empty ());
    QVERIFY (demodulator.consume (
        &inputSentinel,
        SstvFrequencyDemodulator::MaximumSamplesPerCall + 1u).empty ());
    auto const demodAfter = demodulator.metricsSnapshot ();
    QCOMPARE (demodAfter.samplesConsumed, demodBefore.samplesConsumed);
    QCOMPARE (demodulator.bufferedSampleCount (), bufferedBefore);
    QCOMPARE (demodAfter.invalidInputCalls, std::uint64_t {2u});
    QCOMPARE (demodAfter.rejectedOversizeCalls, std::uint64_t {1u});

    decodium::sstv::SstvSignalMetricsConfig hostile =
        decodium::sstv::SstvSignalMetricsConfig::sstvDefaults ();
    hostile.reportFloorDb = -1'000.0;
    QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                              SstvSignalMetrics {hostile});
    hostile = decodium::sstv::SstvSignalMetricsConfig::sstvDefaults ();
    hostile.levelTimeConstantMs = 1.0e300;
    QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                              SstvSignalMetrics {hostile});
  }

  void longStreamRemainsBoundedAndFinite ()
  {
    constexpr std::size_t TotalSamples = 1'200'000u;
    constexpr std::size_t ChunkSamples = 257u;
    SstvFrequencyDemodulator demodulator;
    std::vector<float> chunk (ChunkSamples, 0.0F);
    std::size_t generated = 0u;
    std::uint64_t observationCount = 0u;
    while (generated < TotalSamples)
      {
        std::size_t const count = std::min (ChunkSamples,
                                            TotalSamples - generated);
        for (std::size_t index = 0u; index < count; ++index)
          {
            double const phase = TwoPi * 1'900.0
                                 * static_cast<double> (generated + index)
                                 / SampleRateHz;
            chunk[index] = static_cast<float> (0.2 * std::sin (phase));
          }
        auto const observations = demodulator.consume (chunk.data (), count);
        QVERIFY (observations.size ()
                 <= demodulator.maximumObservationsPerCall ());
        observationCount += static_cast<std::uint64_t> (observations.size ());
        for (SstvFrequencyObservation const& observation : observations)
          {
            verifyFinite (observation);
          }
        QVERIFY (demodulator.bufferedSampleCount ()
                 <= demodulator.maximumBufferedSampleCount ());
        generated += count;
      }
    auto const metrics = demodulator.metricsSnapshot ();
    QCOMPARE (metrics.samplesConsumed,
              static_cast<std::uint64_t> (TotalSamples));
    QCOMPARE (metrics.numericFaults, std::uint64_t {0u});
    QVERIFY (metrics.peakBufferedSamples
             <= demodulator.maximumBufferedSampleCount ());
    QVERIFY (observationCount > 190'000u);
  }
};

QTEST_GUILESS_MAIN (TestSstvRxFrontend)

#include "test_sstv_rx_frontend.moc"
