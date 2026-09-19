// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvPreprocessor.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace decodium::sstv
{
namespace
{
constexpr double Pi = 3.141592653589793238462643383279502884;
constexpr double ButterworthQ = 0.707106781186547524400844362104849039;
constexpr double MillisecondsPerSecond = 1'000.0;
constexpr double NativeSampleRateHz = 12'000.0;
}

SstvPreprocessorConfig SstvPreprocessorConfig::sstvDefaults ()
{
  return {};
}

SstvPreprocessor::SstvPreprocessor (SstvPreprocessorConfig config)
    : config_ {config}
{
  validateConfig (config_);
  dcCoefficient_ = std::exp (-2.0 * Pi * config_.dcBlockerCutoffHz
                             / config_.sampleRateHz);
  levelPowerAlpha_ = smoothingAlpha (config_.sampleRateHz,
                                     config_.metricsTimeConstantMs);
  levelAttackAlpha_ = smoothingAlpha (config_.sampleRateHz,
                                      config_.levelAttackMs);
  levelReleaseAlpha_ = smoothingAlpha (config_.sampleRateHz,
                                       config_.levelReleaseMs);
  peakRelease_ = releaseCoefficient (config_.sampleRateHz,
                                     config_.peakReleaseTimeMs);
  highPass_ = makeHighPass (config_.sampleRateHz, config_.bandPassLowHz);
  lowPass_ = makeLowPass (config_.sampleRateHz, config_.bandPassHighHz);
  humNotch_ = makeNotch (config_.sampleRateHz,
                         config_.humFrequencyHz,
                         config_.humNotchQ);
  resetStateUnlocked ();
}

bool SstvPreprocessor::process (float const* input,
                                std::size_t count,
                                float* output)
{
  std::lock_guard<std::mutex> const lock {mutex_};
  if (count == 0u)
    {
      return true;
    }

  std::uint64_t const count64 = static_cast<std::uint64_t> (count);
  if (input == nullptr || output == nullptr || count > MaximumSamplesPerCall)
    {
      saturatingAdd (counters_.invalidInputCalls, 1u);
      saturatingAdd (counters_.rejectedSamples, count64);
      if (count > MaximumSamplesPerCall)
        {
          saturatingAdd (counters_.rejectedOversizeCalls, 1u);
        }
      return false;
    }
  saturatingAdd (counters_.samplesConsumed, count64);

  bool allFinite = true;
  for (std::size_t index = 0u; index < count; ++index)
    {
      double raw = static_cast<double> (input[index]);
      bool const finiteInput = std::isfinite (raw);
      if (!finiteInput)
        {
          raw = 0.0;
          allFinite = false;
          saturatingAdd (counters_.invalidInputSamples, 1u);
        }
      else
        {
          saturatingAdd (counters_.validInputSamples, 1u);
          if (std::abs (raw) >= config_.clippingThreshold)
            {
              saturatingAdd (counters_.inputClippedSamples, 1u);
            }
        }

      double filtered = raw;
      if (config_.dcBlockerEnabled)
        {
          double const blocked = raw - previousDcInput_
                                 + dcCoefficient_ * previousDcOutput_;
          previousDcInput_ = raw;
          previousDcOutput_ = blocked;
          filtered = blocked;
        }
      if (config_.impulseSuppressorEnabled)
        {
          double const original = filtered;
          if (impulseHistorySize_ >= 2u)
            {
              double const minimum = std::min (
                  impulsePreviousTwo_, impulsePreviousOne_);
              double const maximum = std::max (
                  impulsePreviousTwo_, impulsePreviousOne_);
              double const median = std::max (minimum,
                                              std::min (maximum, original));
              if (std::abs (original - median) > config_.impulseThreshold)
                {
                  filtered = median;
                  saturatingAdd (counters_.impulseSamplesSuppressed, 1u);
                }
            }
          impulsePreviousTwo_ = impulsePreviousOne_;
          impulsePreviousOne_ = original;
          if (impulseHistorySize_ < 2u)
            {
              ++impulseHistorySize_;
            }
        }
      if (config_.humNotchEnabled)
        {
          filtered = humNotch_.process (filtered);
        }
      if (config_.bandPassEnabled)
        {
          filtered = lowPass_.process (highPass_.process (filtered));
        }

      double const filteredPower = filtered * filtered;
      if (!levelPowerInitialized_)
        {
          levelPower_ = filteredPower;
          levelPowerInitialized_ = true;
        }
      else
        {
          levelPower_ += levelPowerAlpha_ * (filteredPower - levelPower_);
        }

      if (config_.levelControlEnabled)
        {
          double const measuredRms = std::sqrt (std::max (
              levelPower_, std::numeric_limits<double>::min ()));
          double const requestedGain = std::max (
              config_.minimumAutomaticGain,
              std::min (config_.maximumAutomaticGain,
                        config_.targetRms / measuredRms));
          double const alpha = requestedGain < automaticGain_
                                   ? levelAttackAlpha_
                                   : levelReleaseAlpha_;
          automaticGain_ += alpha * (requestedGain - automaticGain_);
        }
      else
        {
          automaticGain_ = 1.0;
        }

      double processed = filtered * config_.inputGain * automaticGain_;
      if (std::abs (processed) >= config_.clippingThreshold)
        {
          saturatingAdd (counters_.preLimiterClippedSamples, 1u);
        }
      if (config_.limiterEnabled
          && std::abs (processed) > config_.limiterThreshold)
        {
          processed = std::copysign (config_.limiterThreshold, processed);
          saturatingAdd (counters_.limiterEvents, 1u);
        }
      if (std::abs (processed) >= config_.clippingThreshold)
        {
          saturatingAdd (counters_.outputClippedSamples, 1u);
        }

      double const maximumFloat = static_cast<double> (
          std::numeric_limits<float>::max ());
      if (!std::isfinite (processed))
        {
          processed = 0.0;
          allFinite = false;
          saturatingAdd (counters_.internalNumericFaults, 1u);
          highPass_.reset ();
          lowPass_.reset ();
          humNotch_.reset ();
          previousDcInput_ = 0.0;
          previousDcOutput_ = 0.0;
          impulsePreviousOne_ = 0.0;
          impulsePreviousTwo_ = 0.0;
          impulseHistorySize_ = 0u;
          levelPower_ = 0.0;
          levelPowerInitialized_ = false;
          automaticGain_ = 1.0;
        }
      else if (std::abs (processed) > maximumFloat)
        {
          // A finite double outside the float range makes the conversion below
          // undefined.  Preserve its sign, report the numeric fault, and keep
          // both the returned PCM and the metric state finite.
          processed = std::copysign (maximumFloat, processed);
          allFinite = false;
          saturatingAdd (counters_.internalNumericFaults, 1u);
          highPass_.reset ();
          lowPass_.reset ();
          humNotch_.reset ();
          previousDcInput_ = 0.0;
          previousDcOutput_ = 0.0;
          impulsePreviousOne_ = 0.0;
          impulsePreviousTwo_ = 0.0;
          impulseHistorySize_ = 0u;
          levelPower_ = 0.0;
          levelPowerInitialized_ = false;
          automaticGain_ = 1.0;
        }

      output[index] = static_cast<float> (processed);
      updateLevelMetrics (finiteInput ? raw : 0.0, processed);
      saturatingAdd (counters_.samplesProduced, 1u);
    }
  return allFinite;
}

std::vector<float> SstvPreprocessor::process (
    std::vector<float> const& input)
{
  if (input.size () > MaximumSamplesPerCall)
    {
      // Reuse the pointer overload's accounting without allocating an output
      // proportional to hostile input.
      process (input.data (), input.size (), nullptr);
      return {};
    }
  std::vector<float> output (input.size (), 0.0F);
  process (input.data (), input.size (), output.data ());
  return output;
}

void SstvPreprocessor::reset ()
{
  std::lock_guard<std::mutex> const lock {mutex_};
  resetStateUnlocked ();
}

SstvPreprocessorMetrics SstvPreprocessor::metricsSnapshot () const
{
  std::lock_guard<std::mutex> const lock {mutex_};
  SstvPreprocessorMetrics result = counters_;
  result.inputRms = std::sqrt (std::max (0.0, inputMetricPower_));
  result.inputPeak = std::max (0.0, inputMetricPeak_);
  result.outputRms = std::sqrt (std::max (0.0, outputMetricPower_));
  result.outputPeak = std::max (0.0, outputMetricPeak_);
  result.currentAutomaticGain = automaticGain_;
  result.effectiveGain = config_.inputGain * automaticGain_;
  return result;
}

SstvPreprocessorConfig const& SstvPreprocessor::config () const noexcept
{
  return config_;
}

std::size_t SstvPreprocessor::bufferedSampleCount () const noexcept
{
  return 0u;
}

std::size_t SstvPreprocessor::maximumBufferedSampleCount () const noexcept
{
  return 0u;
}

double SstvPreprocessor::Biquad::process (double sample) noexcept
{
  double const output = b0 * sample + z1;
  z1 = b1 * sample - a1 * output + z2;
  z2 = b2 * sample - a2 * output;
  return output;
}

void SstvPreprocessor::Biquad::reset () noexcept
{
  z1 = 0.0;
  z2 = 0.0;
}

SstvPreprocessor::Biquad SstvPreprocessor::makeHighPass (
    double sampleRateHz,
    double cutoffHz)
{
  double const omega = 2.0 * Pi * cutoffHz / sampleRateHz;
  double const cosine = std::cos (omega);
  double const sine = std::sin (omega);
  double const alpha = sine / (2.0 * ButterworthQ);
  double const a0 = 1.0 + alpha;

  Biquad result;
  result.b0 = ((1.0 + cosine) / 2.0) / a0;
  result.b1 = (-(1.0 + cosine)) / a0;
  result.b2 = result.b0;
  result.a1 = (-2.0 * cosine) / a0;
  result.a2 = (1.0 - alpha) / a0;
  return result;
}

SstvPreprocessor::Biquad SstvPreprocessor::makeLowPass (
    double sampleRateHz,
    double cutoffHz)
{
  double const omega = 2.0 * Pi * cutoffHz / sampleRateHz;
  double const cosine = std::cos (omega);
  double const sine = std::sin (omega);
  double const alpha = sine / (2.0 * ButterworthQ);
  double const a0 = 1.0 + alpha;

  Biquad result;
  result.b0 = ((1.0 - cosine) / 2.0) / a0;
  result.b1 = (1.0 - cosine) / a0;
  result.b2 = result.b0;
  result.a1 = (-2.0 * cosine) / a0;
  result.a2 = (1.0 - alpha) / a0;
  return result;
}

SstvPreprocessor::Biquad SstvPreprocessor::makeNotch (
    double sampleRateHz,
    double frequencyHz,
    double qualityFactor)
{
  double const omega = 2.0 * Pi * frequencyHz / sampleRateHz;
  double const cosine = std::cos (omega);
  double const sine = std::sin (omega);
  double const alpha = sine / (2.0 * qualityFactor);
  double const a0 = 1.0 + alpha;

  Biquad result;
  result.b0 = 1.0 / a0;
  result.b1 = (-2.0 * cosine) / a0;
  result.b2 = result.b0;
  result.a1 = result.b1;
  result.a2 = (1.0 - alpha) / a0;
  return result;
}

void SstvPreprocessor::validateConfig (SstvPreprocessorConfig const& config)
{
  double const nyquist = config.sampleRateHz / 2.0;
  if (!std::isfinite (config.sampleRateHz)
      || std::abs (config.sampleRateHz - NativeSampleRateHz) > 1.0e-6
      || !std::isfinite (config.dcBlockerCutoffHz)
      || config.dcBlockerCutoffHz <= 0.0
      || config.dcBlockerCutoffHz >= nyquist
      || !std::isfinite (config.bandPassLowHz)
      || !std::isfinite (config.bandPassHighHz)
      || config.bandPassLowHz <= 0.0
      || config.bandPassHighHz <= config.bandPassLowHz
      || config.bandPassHighHz >= nyquist
      || !std::isfinite (config.humFrequencyHz)
      || config.humFrequencyHz < 45.0
      || config.humFrequencyHz > 65.0
      || config.humFrequencyHz >= nyquist
      || !std::isfinite (config.humNotchQ)
      || config.humNotchQ < 2.0
      || config.humNotchQ > 100.0
      || !std::isfinite (config.impulseThreshold)
      || config.impulseThreshold <= 0.0
      || config.impulseThreshold > 4.0
      || !std::isfinite (config.inputGain) || config.inputGain <= 0.0
      || config.inputGain > 64.0
      || !std::isfinite (config.targetRms) || config.targetRms <= 0.0
      || config.targetRms > 1.0
      || !std::isfinite (config.minimumAutomaticGain)
      || config.minimumAutomaticGain <= 0.0
      || config.minimumAutomaticGain > 64.0
      || !std::isfinite (config.maximumAutomaticGain)
      || config.maximumAutomaticGain < config.minimumAutomaticGain
      || config.maximumAutomaticGain > 64.0
      || !std::isfinite (config.levelAttackMs) || config.levelAttackMs < 0.1
      || config.levelAttackMs > 60'000.0
      || !std::isfinite (config.levelReleaseMs)
      || config.levelReleaseMs < 0.1
      || config.levelReleaseMs > 60'000.0
      || !std::isfinite (config.limiterThreshold)
      || config.limiterThreshold <= 0.0
      || config.limiterThreshold > 1.0
      || !std::isfinite (config.clippingThreshold)
      || config.clippingThreshold <= 0.0
      || config.clippingThreshold > 16.0
      || (config.limiterEnabled
          && config.limiterThreshold > config.clippingThreshold)
      || !std::isfinite (config.metricsTimeConstantMs)
      || config.metricsTimeConstantMs < 0.1
      || config.metricsTimeConstantMs > 60'000.0
      || !std::isfinite (config.peakReleaseTimeMs)
      || config.peakReleaseTimeMs < 0.1
      || config.peakReleaseTimeMs > 60'000.0)
    {
      throw std::invalid_argument {"invalid 12 kHz SSTV preprocessor configuration"};
    }
}

void SstvPreprocessor::saturatingAdd (std::uint64_t& value,
                                      std::uint64_t increment) noexcept
{
  std::uint64_t const maximum = std::numeric_limits<std::uint64_t>::max ();
  value = increment > maximum - value ? maximum : value + increment;
}

double SstvPreprocessor::smoothingAlpha (double sampleRateHz,
                                         double timeMs) noexcept
{
  double const samples = sampleRateHz * timeMs / MillisecondsPerSecond;
  return 1.0 - std::exp (-1.0 / samples);
}

double SstvPreprocessor::releaseCoefficient (double sampleRateHz,
                                             double timeMs) noexcept
{
  double const samples = sampleRateHz * timeMs / MillisecondsPerSecond;
  return std::exp (-1.0 / samples);
}

void SstvPreprocessor::resetStateUnlocked () noexcept
{
  previousDcInput_ = 0.0;
  previousDcOutput_ = 0.0;
  impulsePreviousOne_ = 0.0;
  impulsePreviousTwo_ = 0.0;
  impulseHistorySize_ = 0u;
  levelPower_ = 0.0;
  levelPowerInitialized_ = false;
  automaticGain_ = 1.0;
  inputMetricPower_ = 0.0;
  inputMetricPeak_ = 0.0;
  outputMetricPower_ = 0.0;
  outputMetricPeak_ = 0.0;
  highPass_.reset ();
  lowPass_.reset ();
  humNotch_.reset ();
  counters_ = {};
}

void SstvPreprocessor::updateLevelMetrics (double input,
                                           double output) noexcept
{
  inputMetricPower_ += levelPowerAlpha_
                       * (input * input - inputMetricPower_);
  outputMetricPower_ += levelPowerAlpha_
                        * (output * output - outputMetricPower_);
  inputMetricPeak_ = std::max (std::abs (input),
                               inputMetricPeak_ * peakRelease_);
  outputMetricPeak_ = std::max (std::abs (output),
                                outputMetricPeak_ * peakRelease_);
}

} // namespace decodium::sstv
