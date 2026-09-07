// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>

namespace decodium::sstv
{

struct SstvSignalMetricsConfig
{
  double sampleRateHz {12'000.0};
  double levelTimeConstantMs {125.0};
  double peakReleaseTimeMs {500.0};
  double spectralEstimateSmoothing {0.2};
  double clippingThreshold {1.0};
  double reportFloorDb {-120.0};

  static SstvSignalMetricsConfig sstvDefaults ();
};

// A value snapshot is self-contained and can safely be passed between the
// audio/DSP thread and UI or logging threads.  Noise, SNR and confidence are
// explicitly marked unavailable until a detector supplies a spectral
// estimate; the level tracker never guesses them from amplitude alone.
struct SstvSignalMetricsSnapshot
{
  std::uint64_t samplesObserved {0};
  std::uint64_t validSamples {0};
  std::uint64_t invalidSamples {0};
  std::uint64_t rejectedInputCalls {0};
  std::uint64_t rejectedOversizeCalls {0};
  std::uint64_t rejectedSamples {0};
  std::uint64_t clippedSamples {0};
  std::uint64_t droppedSamples {0};
  std::uint64_t spectralEstimates {0};
  std::uint64_t invalidSpectralEstimates {0};

  double rms {0.0};
  double peak {0.0};
  double rmsDbfs {-120.0};
  double peakDbfs {-120.0};

  bool noiseEstimateAvailable {false};
  bool snrAvailable {false};
  bool confidenceAvailable {false};
  bool snrLimitedByReportFloor {false};
  double noiseFloorRms {0.0};
  double noiseFloorDbfs {-120.0};
  double snrDb {0.0};
  double confidence {0.0};
};

// Thread-safe bounded telemetry accumulator.  It stores only exponential
// state and counters, never audio or an unbounded history.  The supplied
// signal/noise powers are detector estimates, not calibrated RF measurements.
class SstvSignalMetrics final
{
public:
  static constexpr std::size_t MaximumSamplesPerCall = 262'144u;

  explicit SstvSignalMetrics (
      SstvSignalMetricsConfig config = SstvSignalMetricsConfig::sstvDefaults ());

  bool observeSamples (float const* samples, std::size_t count);
  bool observeSample (float sample);

  // signalPower and noisePower are mean-square values in the normalized
  // digital-audio domain.  Confidence must be in [0, 1].
  bool observeSpectralEstimate (double signalPower,
                                double noisePower,
                                double confidence);

  void recordInvalidSamples (std::uint64_t count = 1u);
  void recordDroppedSamples (std::uint64_t count = 1u);
  void reset ();

  SstvSignalMetricsSnapshot snapshot () const;
  SstvSignalMetricsConfig const& config () const noexcept;

private:
  void observeFiniteSampleUnlocked (double sample) noexcept;
  static void saturatingAdd (std::uint64_t& value,
                             std::uint64_t increment) noexcept;
  static double amplitudeToDb (double amplitude,
                               double reportFloorDb) noexcept;
  static double clampFinite (double value,
                             double minimum,
                             double maximum) noexcept;
  static void validateConfig (SstvSignalMetricsConfig const& config);

  SstvSignalMetricsConfig config_;
  double levelAlpha_ {0.0};
  double peakRelease_ {0.0};

  mutable std::mutex mutex_;
  std::uint64_t samplesObserved_ {0};
  std::uint64_t validSamples_ {0};
  std::uint64_t invalidSamples_ {0};
  std::uint64_t rejectedInputCalls_ {0};
  std::uint64_t rejectedOversizeCalls_ {0};
  std::uint64_t rejectedSamples_ {0};
  std::uint64_t clippedSamples_ {0};
  std::uint64_t droppedSamples_ {0};
  std::uint64_t spectralEstimates_ {0};
  std::uint64_t invalidSpectralEstimates_ {0};

  double levelPower_ {0.0};
  double peakEnvelope_ {0.0};
  bool spectralEstimateAvailable_ {false};
  double signalPowerEstimate_ {0.0};
  double noisePowerEstimate_ {0.0};
  double confidenceEstimate_ {0.0};
};

} // namespace decodium::sstv
