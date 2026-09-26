// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvSignalMetrics.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace decodium::sstv
{

enum class SstvFrequencyStatus : std::uint8_t
{
  Valid,
  LowSignal,
  OutOfBand,
  LowConfidence,
  InvalidInput
};

struct SstvFrequencyDemodulatorConfig
{
  double sampleRateHz {12'000.0};
  std::size_t hilbertTaps {31u};
  std::size_t averagingSamples {12u};
  std::size_t hopSamples {6u};
  double minimumFrequencyHz {700.0};
  double maximumFrequencyHz {2'800.0};
  double minimumRms {0.003};
  double maximumPhaseJitterHz {180.0};
  double minimumConfidence {0.30};
  double maximumAfcCorrectionHz {150.0};
  double afcUpdateSmoothing {0.20};

  static SstvFrequencyDemodulatorConfig sstvDefaults ();
};

struct SstvFrequencyObservation
{
  SstvFrequencyStatus status {SstvFrequencyStatus::InvalidInput};
  std::uint64_t sequence {0};
  std::uint64_t centreSample {0};
  double rawFrequencyHz {0.0};
  double correctedFrequencyHz {0.0};
  double afcCorrectionHz {0.0};
  double rms {0.0};
  double phaseJitterHz {0.0};
  double estimatedNoiseRms {0.0};
  double snrDb {0.0};
  double confidence {0.0};
  double validSampleFraction {0.0};
  bool noiseEstimateIsHeuristic {true};

  bool valid () const noexcept
  {
    return status == SstvFrequencyStatus::Valid;
  }
};

struct SstvFrequencyDemodulatorMetrics
{
  std::uint64_t samplesConsumed {0};
  std::uint64_t invalidSamples {0};
  std::uint64_t invalidInputCalls {0};
  std::uint64_t rejectedOversizeCalls {0};
  std::uint64_t rejectedSamples {0};
  std::uint64_t analyticSamples {0};
  std::uint64_t observationsProduced {0};
  std::uint64_t validObservations {0};
  std::uint64_t lowSignalObservations {0};
  std::uint64_t outOfBandObservations {0};
  std::uint64_t lowConfidenceObservations {0};
  std::uint64_t invalidObservations {0};
  std::uint64_t invalidAfcRequests {0};
  std::uint64_t numericFaults {0};
  std::size_t bufferedSamples {0};
  std::size_t peakBufferedSamples {0};
  SstvSignalMetricsSnapshot signal;
};

// Streaming analytic-signal discriminator for analog SSTV tones.  The
// Hilbert history, previous phase and averaging window survive chunk
// boundaries, so splitting the same stream does not change observations.
// Internal storage is allocated once from the validated configuration.
class SstvFrequencyDemodulator final
{
public:
  static constexpr std::size_t MaximumSamplesPerCall = 262'144u;
  // analyseEstimateWindow() scans the averaging window twice per emitted
  // observation.  Bound averagingSamples/hopSamples so even hostile but valid
  // configurations have a fixed upper work ratio.
  static constexpr std::size_t MaximumAveragingWorkRatio = 32u;

  explicit SstvFrequencyDemodulator (
      SstvFrequencyDemodulatorConfig config =
          SstvFrequencyDemodulatorConfig::sstvDefaults ());

  std::vector<SstvFrequencyObservation> consume (float const* samples,
                                                 std::size_t count);
  std::vector<SstvFrequencyObservation> consume (
      std::vector<float> const& samples);

  // AFC is never inferred silently.  These explicit control calls clamp the
  // receiver offset to +/- maximumAfcCorrectionHz.  A positive correction is
  // subtracted from the measured audio frequency.
  double setAfcCorrectionHz (double requestedHz);
  double updateAfcFromReference (double measuredHz, double nominalHz);
  void clearAfcCorrection ();
  double afcCorrectionHz () const;

  void recordDroppedSamples (std::uint64_t count = 1u);
  void reset (bool preserveAfcCorrection = false);

  SstvFrequencyDemodulatorMetrics metricsSnapshot () const;
  SstvFrequencyDemodulatorConfig const& config () const noexcept;
  std::size_t bufferedSampleCount () const;
  std::size_t maximumBufferedSampleCount () const noexcept;
  std::size_t maximumObservationsPerCall () const noexcept;

private:
  struct PhaseEstimate
  {
    double deltaRadians {0.0};
    double analyticPower {0.0};
    bool inputValid {false};
    bool valid {false};
  };

  std::vector<SstvFrequencyObservation> consumeUnlocked (
      float const* samples,
      std::size_t count);
  void acceptSample (double sample,
                     bool valid,
                     std::vector<SstvFrequencyObservation>& output);
  void pushPhaseEstimate (PhaseEstimate estimate,
                          std::vector<SstvFrequencyObservation>& output);
  SstvFrequencyObservation analyseEstimateWindow ();
  void resetUnlocked (bool preserveAfcCorrection);
  static void validateConfig (SstvFrequencyDemodulatorConfig const& config);
  static void saturatingAdd (std::uint64_t& value,
                             std::uint64_t increment) noexcept;
  static double clamp (double value, double minimum, double maximum) noexcept;
  static double wrapPhase (double radians) noexcept;

  SstvFrequencyDemodulatorConfig config_;
  std::vector<double> hilbertCoefficients_;
  std::vector<double> sampleHistory_;
  std::vector<std::uint8_t> sampleValidity_;
  std::vector<PhaseEstimate> phaseHistory_;
  SstvSignalMetrics signalMetrics_;

  mutable std::mutex mutex_;
  std::size_t sampleWriteIndex_ {0u};
  std::size_t sampleHistorySize_ {0u};
  std::size_t phaseWriteIndex_ {0u};
  std::size_t phaseHistorySize_ {0u};
  std::size_t phaseSamplesSinceObservation_ {0u};
  bool previousPhaseValid_ {false};
  double previousPhase_ {0.0};
  double afcCorrectionHz_ {0.0};
  std::uint64_t streamSampleIndex_ {0u};
  std::uint64_t observationSequence_ {0u};
  SstvFrequencyDemodulatorMetrics counters_;
};

} // namespace decodium::sstv
