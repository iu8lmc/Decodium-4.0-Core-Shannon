// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvSignalMetrics.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace decodium::sstv
{
namespace
{
constexpr double MillisecondsPerSecond = 1'000.0;
constexpr double MinimumSnrDb = -120.0;
constexpr double MaximumSnrDb = 120.0;
}

SstvSignalMetricsConfig SstvSignalMetricsConfig::sstvDefaults ()
{
  return {};
}

SstvSignalMetrics::SstvSignalMetrics (SstvSignalMetricsConfig config)
    : config_ {config}
{
  validateConfig (config_);
  double const levelSamples = config_.sampleRateHz
                              * config_.levelTimeConstantMs
                              / MillisecondsPerSecond;
  double const peakSamples = config_.sampleRateHz
                             * config_.peakReleaseTimeMs
                             / MillisecondsPerSecond;
  levelAlpha_ = 1.0 - std::exp (-1.0 / levelSamples);
  peakRelease_ = std::exp (-1.0 / peakSamples);
}

bool SstvSignalMetrics::observeSamples (float const* samples,
                                        std::size_t count)
{
  std::lock_guard<std::mutex> const lock {mutex_};
  if (count == 0u)
    {
      return true;
    }
  if (samples == nullptr || count > MaximumSamplesPerCall)
    {
      saturatingAdd (rejectedInputCalls_, 1u);
      saturatingAdd (rejectedSamples_, static_cast<std::uint64_t> (count));
      if (count > MaximumSamplesPerCall)
        {
          saturatingAdd (rejectedOversizeCalls_, 1u);
        }
      return false;
    }

  bool allFinite = true;
  for (std::size_t index = 0u; index < count; ++index)
    {
      saturatingAdd (samplesObserved_, 1u);
      double const sample = static_cast<double> (samples[index]);
      if (!std::isfinite (sample))
        {
          saturatingAdd (invalidSamples_, 1u);
          allFinite = false;
          continue;
        }
      observeFiniteSampleUnlocked (sample);
    }
  return allFinite;
}

bool SstvSignalMetrics::observeSample (float sample)
{
  return observeSamples (&sample, 1u);
}

bool SstvSignalMetrics::observeSpectralEstimate (double signalPower,
                                                 double noisePower,
                                                 double confidence)
{
  std::lock_guard<std::mutex> const lock {mutex_};
  if (!std::isfinite (signalPower) || signalPower < 0.0
      || !std::isfinite (noisePower) || noisePower < 0.0
      || !std::isfinite (confidence) || confidence < 0.0
      || confidence > 1.0)
    {
      saturatingAdd (invalidSpectralEstimates_, 1u);
      return false;
    }

  double const alpha = spectralEstimateAvailable_
                           ? config_.spectralEstimateSmoothing
                           : 1.0;
  signalPowerEstimate_ += alpha * (signalPower - signalPowerEstimate_);
  noisePowerEstimate_ += alpha * (noisePower - noisePowerEstimate_);
  confidenceEstimate_ += alpha * (confidence - confidenceEstimate_);
  spectralEstimateAvailable_ = true;
  saturatingAdd (spectralEstimates_, 1u);
  return true;
}

void SstvSignalMetrics::recordInvalidSamples (std::uint64_t count)
{
  std::lock_guard<std::mutex> const lock {mutex_};
  saturatingAdd (samplesObserved_, count);
  saturatingAdd (invalidSamples_, count);
}

void SstvSignalMetrics::recordDroppedSamples (std::uint64_t count)
{
  std::lock_guard<std::mutex> const lock {mutex_};
  saturatingAdd (droppedSamples_, count);
}

void SstvSignalMetrics::reset ()
{
  std::lock_guard<std::mutex> const lock {mutex_};
  samplesObserved_ = 0u;
  validSamples_ = 0u;
  invalidSamples_ = 0u;
  rejectedInputCalls_ = 0u;
  rejectedOversizeCalls_ = 0u;
  rejectedSamples_ = 0u;
  clippedSamples_ = 0u;
  droppedSamples_ = 0u;
  spectralEstimates_ = 0u;
  invalidSpectralEstimates_ = 0u;
  levelPower_ = 0.0;
  peakEnvelope_ = 0.0;
  spectralEstimateAvailable_ = false;
  signalPowerEstimate_ = 0.0;
  noisePowerEstimate_ = 0.0;
  confidenceEstimate_ = 0.0;
}

SstvSignalMetricsSnapshot SstvSignalMetrics::snapshot () const
{
  std::lock_guard<std::mutex> const lock {mutex_};
  SstvSignalMetricsSnapshot result;
  result.samplesObserved = samplesObserved_;
  result.validSamples = validSamples_;
  result.invalidSamples = invalidSamples_;
  result.rejectedInputCalls = rejectedInputCalls_;
  result.rejectedOversizeCalls = rejectedOversizeCalls_;
  result.rejectedSamples = rejectedSamples_;
  result.clippedSamples = clippedSamples_;
  result.droppedSamples = droppedSamples_;
  result.spectralEstimates = spectralEstimates_;
  result.invalidSpectralEstimates = invalidSpectralEstimates_;

  result.rms = std::sqrt (std::max (0.0, levelPower_));
  result.peak = std::max (0.0, peakEnvelope_);
  result.rmsDbfs = amplitudeToDb (result.rms, config_.reportFloorDb);
  result.peakDbfs = amplitudeToDb (result.peak, config_.reportFloorDb);

  result.noiseEstimateAvailable = spectralEstimateAvailable_;
  result.snrAvailable = spectralEstimateAvailable_;
  result.confidenceAvailable = spectralEstimateAvailable_;
  if (spectralEstimateAvailable_)
    {
      double const reportFloorAmplitude =
          std::pow (10.0, config_.reportFloorDb / 20.0);
      double const reportFloorPower =
          reportFloorAmplitude * reportFloorAmplitude;
      double const reportedNoisePower =
          std::max (noisePowerEstimate_, reportFloorPower);
      result.snrLimitedByReportFloor = noisePowerEstimate_ < reportFloorPower;
      result.noiseFloorRms = std::sqrt (std::max (0.0,
                                                 noisePowerEstimate_));
      result.noiseFloorDbfs = amplitudeToDb (result.noiseFloorRms,
                                             config_.reportFloorDb);
      double const signalPowerDb = 10.0 * std::log10 (
          std::max (signalPowerEstimate_, reportFloorPower));
      double const noisePowerDb = 10.0 * std::log10 (reportedNoisePower);
      result.snrDb = clampFinite (signalPowerDb - noisePowerDb,
                                  MinimumSnrDb,
                                  MaximumSnrDb);
      result.confidence = clampFinite (confidenceEstimate_, 0.0, 1.0);
    }
  return result;
}

SstvSignalMetricsConfig const& SstvSignalMetrics::config () const noexcept
{
  return config_;
}

void SstvSignalMetrics::observeFiniteSampleUnlocked (double sample) noexcept
{
  saturatingAdd (validSamples_, 1u);
  if (std::abs (sample) >= config_.clippingThreshold)
    {
      saturatingAdd (clippedSamples_, 1u);
    }
  double const power = sample * sample;
  levelPower_ += levelAlpha_ * (power - levelPower_);
  double const magnitude = std::abs (sample);
  peakEnvelope_ = std::max (magnitude, peakEnvelope_ * peakRelease_);
}

void SstvSignalMetrics::saturatingAdd (std::uint64_t& value,
                                       std::uint64_t increment) noexcept
{
  std::uint64_t const maximum = std::numeric_limits<std::uint64_t>::max ();
  if (increment > maximum - value)
    {
      value = maximum;
      return;
    }
  value += increment;
}

double SstvSignalMetrics::amplitudeToDb (double amplitude,
                                         double reportFloorDb) noexcept
{
  if (!(amplitude > 0.0) || !std::isfinite (amplitude))
    {
      return reportFloorDb;
    }
  return std::max (reportFloorDb, 20.0 * std::log10 (amplitude));
}

double SstvSignalMetrics::clampFinite (double value,
                                       double minimum,
                                       double maximum) noexcept
{
  if (!std::isfinite (value))
    {
      return minimum;
    }
  return std::max (minimum, std::min (maximum, value));
}

void SstvSignalMetrics::validateConfig (
    SstvSignalMetricsConfig const& config)
{
  if (!std::isfinite (config.sampleRateHz) || config.sampleRateHz < 1'000.0
      || config.sampleRateHz > 384'000.0
      || !std::isfinite (config.levelTimeConstantMs)
      || config.levelTimeConstantMs < 0.1
      || config.levelTimeConstantMs > 60'000.0
      || !std::isfinite (config.peakReleaseTimeMs)
      || config.peakReleaseTimeMs < 0.1
      || config.peakReleaseTimeMs > 60'000.0
      || !std::isfinite (config.spectralEstimateSmoothing)
      || config.spectralEstimateSmoothing < 0.0001
      || config.spectralEstimateSmoothing > 1.0
      || !std::isfinite (config.clippingThreshold)
      || config.clippingThreshold <= 0.0
      || config.clippingThreshold > 100.0
      || !std::isfinite (config.reportFloorDb)
      || config.reportFloorDb < -300.0
      || config.reportFloorDb > -20.0)
    {
      throw std::invalid_argument {"invalid SSTV signal-metrics configuration"};
    }
}

} // namespace decodium::sstv
