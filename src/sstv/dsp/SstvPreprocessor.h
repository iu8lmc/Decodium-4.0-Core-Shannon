// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace decodium::sstv
{

struct SstvPreprocessorConfig
{
  // The native SSTV DSP domain is deliberately fixed at 12 kHz.  Sources at
  // other rates must pass through SstvResampler before this component.
  double sampleRateHz {12'000.0};

  bool dcBlockerEnabled {true};
  double dcBlockerCutoffHz {30.0};

  bool bandPassEnabled {true};
  double bandPassLowHz {900.0};
  double bandPassHighHz {2'500.0};

  // Optional narrow mains-hum rejection.  The default band-pass already
  // rejects 50/60 Hz strongly, but this independently testable stage is useful
  // when an operator deliberately widens or disables that pass-band for
  // diagnostics.  It is off by default so the ordinary SSTV phase path is
  // unchanged.
  bool humNotchEnabled {false};
  double humFrequencyHz {50.0};
  double humNotchQ {20.0};

  // Optional three-sample causal outlier suppressor.  Only a current sample
  // whose distance from the median of the last two inputs and itself exceeds
  // this normalized-amplitude threshold is replaced.  Keeping this opt-in
  // avoids silently rounding legitimate high-frequency SSTV transitions.
  bool impulseSuppressorEnabled {false};
  double impulseThreshold {0.45};

  double inputGain {1.0};
  bool levelControlEnabled {true};
  double targetRms {0.22};
  double minimumAutomaticGain {0.25};
  double maximumAutomaticGain {8.0};
  double levelAttackMs {20.0};
  double levelReleaseMs {250.0};

  bool limiterEnabled {true};
  double limiterThreshold {0.98};
  double clippingThreshold {1.0};
  double metricsTimeConstantMs {125.0};
  double peakReleaseTimeMs {500.0};

  static SstvPreprocessorConfig sstvDefaults ();
};

struct SstvPreprocessorMetrics
{
  std::uint64_t samplesConsumed {0};
  std::uint64_t samplesProduced {0};
  std::uint64_t validInputSamples {0};
  std::uint64_t invalidInputSamples {0};
  std::uint64_t invalidInputCalls {0};
  std::uint64_t rejectedOversizeCalls {0};
  std::uint64_t rejectedSamples {0};
  std::uint64_t inputClippedSamples {0};
  std::uint64_t preLimiterClippedSamples {0};
  std::uint64_t impulseSamplesSuppressed {0};
  std::uint64_t limiterEvents {0};
  std::uint64_t outputClippedSamples {0};
  std::uint64_t internalNumericFaults {0};

  double inputRms {0.0};
  double inputPeak {0.0};
  double outputRms {0.0};
  double outputPeak {0.0};
  double currentAutomaticGain {1.0};
  double effectiveGain {1.0};
};

// Fixed-state streaming preprocessor for normalized 12 kHz mono SSTV audio.
// The class is safe for one processing thread plus concurrent snapshot/reset
// callers.  It does not retain input blocks and supports in-place processing.
class SstvPreprocessor final
{
public:
  static constexpr std::size_t MaximumSamplesPerCall = 262'144u;

  explicit SstvPreprocessor (
      SstvPreprocessorConfig config = SstvPreprocessorConfig::sstvDefaults ());

  // Returns false for a null pointer with non-zero count, or when one or more
  // samples were non-finite.  A non-finite sample is replaced by silence so it
  // cannot poison subsequent filter state.
  bool process (float const* input, std::size_t count, float* output);
  std::vector<float> process (std::vector<float> const& input);

  void reset ();
  SstvPreprocessorMetrics metricsSnapshot () const;
  SstvPreprocessorConfig const& config () const noexcept;

  std::size_t bufferedSampleCount () const noexcept;
  std::size_t maximumBufferedSampleCount () const noexcept;

private:
  struct Biquad
  {
    double b0 {1.0};
    double b1 {0.0};
    double b2 {0.0};
    double a1 {0.0};
    double a2 {0.0};
    double z1 {0.0};
    double z2 {0.0};

    double process (double sample) noexcept;
    void reset () noexcept;
  };

  static Biquad makeHighPass (double sampleRateHz, double cutoffHz);
  static Biquad makeLowPass (double sampleRateHz, double cutoffHz);
  static Biquad makeNotch (double sampleRateHz,
                           double frequencyHz,
                           double qualityFactor);
  static void validateConfig (SstvPreprocessorConfig const& config);
  static void saturatingAdd (std::uint64_t& value,
                             std::uint64_t increment) noexcept;
  static double smoothingAlpha (double sampleRateHz,
                                double timeMs) noexcept;
  static double releaseCoefficient (double sampleRateHz,
                                    double timeMs) noexcept;
  void resetStateUnlocked () noexcept;
  void updateLevelMetrics (double input, double output) noexcept;

  SstvPreprocessorConfig config_;
  double dcCoefficient_ {0.0};
  double levelPowerAlpha_ {0.0};
  double levelAttackAlpha_ {0.0};
  double levelReleaseAlpha_ {0.0};
  double peakRelease_ {0.0};
  Biquad highPass_;
  Biquad lowPass_;
  Biquad humNotch_;

  mutable std::mutex mutex_;
  double previousDcInput_ {0.0};
  double previousDcOutput_ {0.0};
  double impulsePreviousOne_ {0.0};
  double impulsePreviousTwo_ {0.0};
  std::uint8_t impulseHistorySize_ {0u};
  double levelPower_ {0.0};
  bool levelPowerInitialized_ {false};
  double automaticGain_ {1.0};
  double inputMetricPower_ {0.0};
  double inputMetricPeak_ {0.0};
  double outputMetricPower_ {0.0};
  double outputMetricPeak_ {0.0};
  SstvPreprocessorMetrics counters_;
};

} // namespace decodium::sstv
