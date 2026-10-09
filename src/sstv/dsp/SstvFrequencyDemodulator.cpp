// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvFrequencyDemodulator.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace decodium::sstv
{
namespace
{
constexpr double Pi = 3.141592653589793238462643383279502884;
constexpr double TwoPi = 2.0 * Pi;
constexpr double NativeSampleRateHz = 12'000.0;
constexpr double MinimumPower = 1.0e-18;
constexpr double MinimumSnrDb = -120.0;
constexpr double MaximumSnrDb = 120.0;
}

SstvFrequencyDemodulatorConfig
SstvFrequencyDemodulatorConfig::sstvDefaults ()
{
  return {};
}

SstvFrequencyDemodulator::SstvFrequencyDemodulator (
    SstvFrequencyDemodulatorConfig config)
    : config_ {[&config] {
        validateConfig (config);
        return config;
      } ()}
    , sampleHistory_ (config_.hilbertTaps, 0.0)
    , sampleValidity_ (config_.hilbertTaps, 0u)
    , phaseHistory_ (config_.averagingSamples)
    , signalMetrics_ ([this] {
        SstvSignalMetricsConfig metrics =
            SstvSignalMetricsConfig::sstvDefaults ();
        metrics.sampleRateHz = config_.sampleRateHz;
        return metrics;
      } ())
{
  hilbertCoefficients_.resize (config_.hilbertTaps, 0.0);
  std::size_t const centre = config_.hilbertTaps / 2u;
  for (std::size_t tap = 0u; tap < config_.hilbertTaps; ++tap)
    {
      std::int64_t const offset = static_cast<std::int64_t> (tap)
                                  - static_cast<std::int64_t> (centre);
      if (offset == 0 || (std::abs (offset) % 2) == 0)
        {
          continue;
        }
      double const window = 0.54
                            - 0.46 * std::cos (
                                  TwoPi * static_cast<double> (tap)
                                  / static_cast<double> (
                                      config_.hilbertTaps - 1u));
      hilbertCoefficients_[tap] =
          window * 2.0 / (Pi * static_cast<double> (offset));
    }
}

std::vector<SstvFrequencyObservation> SstvFrequencyDemodulator::consume (
    float const* samples,
    std::size_t count)
{
  std::lock_guard<std::mutex> const lock {mutex_};
  return consumeUnlocked (samples, count);
}

std::vector<SstvFrequencyObservation> SstvFrequencyDemodulator::consume (
    std::vector<float> const& samples)
{
  return consume (samples.data (), samples.size ());
}

double SstvFrequencyDemodulator::setAfcCorrectionHz (double requestedHz)
{
  std::lock_guard<std::mutex> const lock {mutex_};
  if (!std::isfinite (requestedHz))
    {
      saturatingAdd (counters_.invalidAfcRequests, 1u);
      return afcCorrectionHz_;
    }
  afcCorrectionHz_ = clamp (requestedHz,
                            -config_.maximumAfcCorrectionHz,
                            config_.maximumAfcCorrectionHz);
  return afcCorrectionHz_;
}

double SstvFrequencyDemodulator::updateAfcFromReference (double measuredHz,
                                                         double nominalHz)
{
  std::lock_guard<std::mutex> const lock {mutex_};
  if (!std::isfinite (measuredHz) || !std::isfinite (nominalHz))
    {
      saturatingAdd (counters_.invalidAfcRequests, 1u);
      return afcCorrectionHz_;
    }
  double const target = clamp (measuredHz - nominalHz,
                               -config_.maximumAfcCorrectionHz,
                               config_.maximumAfcCorrectionHz);
  afcCorrectionHz_ += config_.afcUpdateSmoothing
                      * (target - afcCorrectionHz_);
  afcCorrectionHz_ = clamp (afcCorrectionHz_,
                            -config_.maximumAfcCorrectionHz,
                            config_.maximumAfcCorrectionHz);
  return afcCorrectionHz_;
}

void SstvFrequencyDemodulator::clearAfcCorrection ()
{
  std::lock_guard<std::mutex> const lock {mutex_};
  afcCorrectionHz_ = 0.0;
}

double SstvFrequencyDemodulator::afcCorrectionHz () const
{
  std::lock_guard<std::mutex> const lock {mutex_};
  return afcCorrectionHz_;
}

void SstvFrequencyDemodulator::recordDroppedSamples (std::uint64_t count)
{
  std::lock_guard<std::mutex> const lock {mutex_};
  signalMetrics_.recordDroppedSamples (count);
}

void SstvFrequencyDemodulator::reset (bool preserveAfcCorrection)
{
  std::lock_guard<std::mutex> const lock {mutex_};
  resetUnlocked (preserveAfcCorrection);
}

SstvFrequencyDemodulatorMetrics
SstvFrequencyDemodulator::metricsSnapshot () const
{
  std::lock_guard<std::mutex> const lock {mutex_};
  SstvFrequencyDemodulatorMetrics result = counters_;
  result.bufferedSamples = sampleHistorySize_ + phaseHistorySize_;
  result.signal = signalMetrics_.snapshot ();
  return result;
}

SstvFrequencyDemodulatorConfig const&
SstvFrequencyDemodulator::config () const noexcept
{
  return config_;
}

std::size_t SstvFrequencyDemodulator::bufferedSampleCount () const
{
  std::lock_guard<std::mutex> const lock {mutex_};
  return sampleHistorySize_ + phaseHistorySize_;
}

std::size_t
SstvFrequencyDemodulator::maximumBufferedSampleCount () const noexcept
{
  return config_.hilbertTaps + config_.averagingSamples;
}

std::size_t
SstvFrequencyDemodulator::maximumObservationsPerCall () const noexcept
{
  return MaximumSamplesPerCall / config_.hopSamples + 1u;
}

std::vector<SstvFrequencyObservation>
SstvFrequencyDemodulator::consumeUnlocked (float const* samples,
                                           std::size_t count)
{
  std::vector<SstvFrequencyObservation> output;
  if (count == 0u)
    {
      return output;
    }
  if (samples == nullptr || count > MaximumSamplesPerCall)
    {
      saturatingAdd (counters_.invalidInputCalls, 1u);
      saturatingAdd (counters_.rejectedSamples,
                     static_cast<std::uint64_t> (count));
      if (count > MaximumSamplesPerCall)
        {
          saturatingAdd (counters_.rejectedOversizeCalls, 1u);
        }
      return output;
    }
  output.reserve (count / config_.hopSamples + 1u);
  saturatingAdd (counters_.samplesConsumed,
                 static_cast<std::uint64_t> (count));

  signalMetrics_.observeSamples (samples, count);
  for (std::size_t index = 0u; index < count; ++index)
    {
      double sample = static_cast<double> (samples[index]);
      bool const valid = std::isfinite (sample);
      if (!valid)
        {
          sample = 0.0;
          saturatingAdd (counters_.invalidSamples, 1u);
        }
      acceptSample (sample, valid, output);
      saturatingAdd (streamSampleIndex_, 1u);
    }
  return output;
}

void SstvFrequencyDemodulator::acceptSample (
    double sample,
    bool valid,
    std::vector<SstvFrequencyObservation>& output)
{
  sampleHistory_[sampleWriteIndex_] = sample;
  sampleValidity_[sampleWriteIndex_] = valid ? 1u : 0u;
  sampleWriteIndex_ = (sampleWriteIndex_ + 1u) % sampleHistory_.size ();
  sampleHistorySize_ = std::min (sampleHistorySize_ + 1u,
                                 sampleHistory_.size ());
  counters_.peakBufferedSamples = std::max (
      counters_.peakBufferedSamples,
      sampleHistorySize_ + phaseHistorySize_);
  if (sampleHistorySize_ < sampleHistory_.size ())
    {
      return;
    }

  std::size_t const newest = (sampleWriteIndex_ + sampleHistory_.size () - 1u)
                             % sampleHistory_.size ();
  std::size_t const delay = config_.hilbertTaps / 2u;
  std::size_t const delayedIndex =
      (newest + sampleHistory_.size () - delay) % sampleHistory_.size ();
  double inPhase = sampleHistory_[delayedIndex];
  double quadrature = 0.0;
  bool analyticValid = sampleValidity_[delayedIndex] != 0u;
  for (std::size_t tap = 0u; tap < config_.hilbertTaps; ++tap)
    {
      std::size_t const historyIndex =
          (newest + sampleHistory_.size () - tap) % sampleHistory_.size ();
      quadrature += hilbertCoefficients_[tap] * sampleHistory_[historyIndex];
      analyticValid = analyticValid && sampleValidity_[historyIndex] != 0u;
    }
  bool const numericValid = std::isfinite (inPhase)
                            && std::isfinite (quadrature);
  analyticValid = analyticValid && numericValid;
  saturatingAdd (counters_.analyticSamples, 1u);

  double const analyticPower = inPhase * inPhase
                               + quadrature * quadrature;
  double const phase = analyticValid ? std::atan2 (quadrature, inPhase) : 0.0;
  PhaseEstimate estimate;
  estimate.analyticPower = std::isfinite (analyticPower)
                               ? std::max (0.0, analyticPower)
                               : 0.0;
  estimate.inputValid = analyticValid;
  if (analyticValid && previousPhaseValid_)
    {
      estimate.deltaRadians = wrapPhase (phase - previousPhase_);
      estimate.valid = std::isfinite (estimate.deltaRadians)
                       && estimate.deltaRadians > 0.0
                       && estimate.deltaRadians < Pi;
    }
  previousPhase_ = phase;
  previousPhaseValid_ = analyticValid;
  if (!numericValid || !std::isfinite (analyticPower))
    {
      saturatingAdd (counters_.numericFaults, 1u);
    }
  pushPhaseEstimate (estimate, output);
}

void SstvFrequencyDemodulator::pushPhaseEstimate (
    PhaseEstimate estimate,
    std::vector<SstvFrequencyObservation>& output)
{
  phaseHistory_[phaseWriteIndex_] = estimate;
  phaseWriteIndex_ = (phaseWriteIndex_ + 1u) % phaseHistory_.size ();
  phaseHistorySize_ = std::min (phaseHistorySize_ + 1u,
                                phaseHistory_.size ());
  counters_.peakBufferedSamples = std::max (
      counters_.peakBufferedSamples,
      sampleHistorySize_ + phaseHistorySize_);
  ++phaseSamplesSinceObservation_;
  if (phaseHistorySize_ < phaseHistory_.size ()
      || phaseSamplesSinceObservation_ < config_.hopSamples)
    {
      return;
    }

  phaseSamplesSinceObservation_ = 0u;
  SstvFrequencyObservation observation = analyseEstimateWindow ();
  output.push_back (observation);
  saturatingAdd (counters_.observationsProduced, 1u);
  switch (observation.status)
    {
    case SstvFrequencyStatus::Valid:
      saturatingAdd (counters_.validObservations, 1u);
      break;
    case SstvFrequencyStatus::LowSignal:
      saturatingAdd (counters_.lowSignalObservations, 1u);
      break;
    case SstvFrequencyStatus::OutOfBand:
      saturatingAdd (counters_.outOfBandObservations, 1u);
      break;
    case SstvFrequencyStatus::LowConfidence:
      saturatingAdd (counters_.lowConfidenceObservations, 1u);
      break;
    case SstvFrequencyStatus::InvalidInput:
      saturatingAdd (counters_.invalidObservations, 1u);
      break;
    }
}

SstvFrequencyObservation
SstvFrequencyDemodulator::analyseEstimateWindow ()
{
  SstvFrequencyObservation result;
  result.sequence = observationSequence_;
  saturatingAdd (observationSequence_, 1u);
  std::size_t const totalDelay = config_.hilbertTaps / 2u
                                 + config_.averagingSamples / 2u;
  result.centreSample = streamSampleIndex_ > totalDelay
                            ? streamSampleIndex_ - totalDelay
                            : 0u;
  result.afcCorrectionHz = afcCorrectionHz_;

  std::size_t validCount = 0u;
  std::size_t validInputCount = 0u;
  double weightSum = 0.0;
  double weightedDelta = 0.0;
  double analyticPowerSum = 0.0;
  for (PhaseEstimate const& estimate : phaseHistory_)
    {
      if (estimate.inputValid)
        {
          ++validInputCount;
        }
      if (!estimate.valid || !(estimate.analyticPower > 0.0))
        {
          continue;
        }
      ++validCount;
      weightSum += estimate.analyticPower;
      weightedDelta += estimate.analyticPower * estimate.deltaRadians;
      analyticPowerSum += estimate.analyticPower;
    }

  result.validSampleFraction = static_cast<double> (validInputCount)
                               / static_cast<double> (phaseHistory_.size ());
  if (validCount == 0u || !(weightSum > 0.0))
    {
      result.status = result.validSampleFraction < 0.75
                          ? SstvFrequencyStatus::InvalidInput
                          : SstvFrequencyStatus::LowSignal;
      return result;
    }

  double const meanDelta = weightedDelta / weightSum;
  result.rawFrequencyHz = meanDelta * config_.sampleRateHz / TwoPi;
  result.correctedFrequencyHz = result.rawFrequencyHz - afcCorrectionHz_;
  result.rms = std::sqrt (std::max (0.0,
                                   analyticPowerSum
                                       / (2.0
                                          * static_cast<double> (
                                              validCount))));

  double weightedVariance = 0.0;
  for (PhaseEstimate const& estimate : phaseHistory_)
    {
      if (!estimate.valid || !(estimate.analyticPower > 0.0))
        {
          continue;
        }
      double const difference = wrapPhase (estimate.deltaRadians - meanDelta);
      weightedVariance += estimate.analyticPower * difference * difference;
    }
  weightedVariance /= weightSum;
  double const phaseJitter = std::sqrt (std::max (0.0, weightedVariance));
  result.phaseJitterHz = phaseJitter * config_.sampleRateHz / TwoPi;

  double const totalPower = result.rms * result.rms;
  // For a phase discriminator, small-angle phase variance approximates the
  // relative quadrature-noise power.  It is deliberately labelled heuristic.
  double const noiseRatio = clamp (weightedVariance, 0.0, 1.0);
  double const noisePower = totalPower * noiseRatio;
  double const signalPower = std::max (0.0, totalPower - noisePower);
  result.estimatedNoiseRms = std::sqrt (std::max (0.0, noisePower));
  result.snrDb = clamp (10.0 * std::log10 (
                            std::max (signalPower, MinimumPower)
                            / std::max (noisePower, MinimumPower)),
                        MinimumSnrDb,
                        MaximumSnrDb);

  double const amplitudeScore = clamp (
      result.rms / (2.0 * config_.minimumRms), 0.0, 1.0);
  double const normalizedJitter = result.phaseJitterHz
                                  / config_.maximumPhaseJitterHz;
  double const jitterScore = 1.0 / (1.0
                                    + normalizedJitter * normalizedJitter);
  result.confidence = clamp (amplitudeScore * jitterScore
                                 * result.validSampleFraction,
                             0.0,
                             1.0);
  signalMetrics_.observeSpectralEstimate (signalPower,
                                          noisePower,
                                          result.confidence);

  if (result.validSampleFraction < 0.75)
    {
      result.status = SstvFrequencyStatus::InvalidInput;
    }
  else if (result.rms < config_.minimumRms)
    {
      result.status = SstvFrequencyStatus::LowSignal;
    }
  else if (result.correctedFrequencyHz < config_.minimumFrequencyHz
           || result.correctedFrequencyHz > config_.maximumFrequencyHz)
    {
      result.status = SstvFrequencyStatus::OutOfBand;
    }
  else if (result.confidence < config_.minimumConfidence)
    {
      result.status = SstvFrequencyStatus::LowConfidence;
    }
  else
    {
      result.status = SstvFrequencyStatus::Valid;
    }
  return result;
}

void SstvFrequencyDemodulator::resetUnlocked (bool preserveAfcCorrection)
{
  double const retainedAfc = preserveAfcCorrection ? afcCorrectionHz_ : 0.0;
  std::fill (sampleHistory_.begin (), sampleHistory_.end (), 0.0);
  std::fill (sampleValidity_.begin (), sampleValidity_.end (), 0u);
  std::fill (phaseHistory_.begin (), phaseHistory_.end (), PhaseEstimate {});
  sampleWriteIndex_ = 0u;
  sampleHistorySize_ = 0u;
  phaseWriteIndex_ = 0u;
  phaseHistorySize_ = 0u;
  phaseSamplesSinceObservation_ = 0u;
  previousPhaseValid_ = false;
  previousPhase_ = 0.0;
  afcCorrectionHz_ = retainedAfc;
  streamSampleIndex_ = 0u;
  observationSequence_ = 0u;
  counters_ = {};
  signalMetrics_.reset ();
}

void SstvFrequencyDemodulator::validateConfig (
    SstvFrequencyDemodulatorConfig const& config)
{
  if (!std::isfinite (config.sampleRateHz)
      || std::abs (config.sampleRateHz - NativeSampleRateHz) > 1.0e-6
      || config.hilbertTaps < 15u || config.hilbertTaps > 255u
      || (config.hilbertTaps % 2u) == 0u
      || config.averagingSamples < 3u || config.averagingSamples > 4'096u
      || config.hopSamples == 0u
      || config.hopSamples > config.averagingSamples
      || config.averagingSamples
             > config.hopSamples * MaximumAveragingWorkRatio
      || !std::isfinite (config.minimumFrequencyHz)
      || !std::isfinite (config.maximumFrequencyHz)
      || config.minimumFrequencyHz <= 0.0
      || config.maximumFrequencyHz <= config.minimumFrequencyHz
      || config.maximumFrequencyHz >= config.sampleRateHz / 2.0
      || !std::isfinite (config.minimumRms) || config.minimumRms <= 0.0
      || !std::isfinite (config.maximumPhaseJitterHz)
      || config.maximumPhaseJitterHz <= 0.0
      || !std::isfinite (config.minimumConfidence)
      || config.minimumConfidence < 0.0 || config.minimumConfidence > 1.0
      || !std::isfinite (config.maximumAfcCorrectionHz)
      || config.maximumAfcCorrectionHz < 0.0
      || config.maximumAfcCorrectionHz > 500.0
      || !std::isfinite (config.afcUpdateSmoothing)
      || config.afcUpdateSmoothing <= 0.0
      || config.afcUpdateSmoothing > 1.0)
    {
      throw std::invalid_argument {
          "invalid 12 kHz SSTV frequency-demodulator configuration"};
    }
}

void SstvFrequencyDemodulator::saturatingAdd (std::uint64_t& value,
                                              std::uint64_t increment) noexcept
{
  std::uint64_t const maximum = std::numeric_limits<std::uint64_t>::max ();
  value = increment > maximum - value ? maximum : value + increment;
}

double SstvFrequencyDemodulator::clamp (double value,
                                        double minimum,
                                        double maximum) noexcept
{
  if (!std::isfinite (value))
    {
      return minimum;
    }
  return std::max (minimum, std::min (maximum, value));
}

double SstvFrequencyDemodulator::wrapPhase (double radians) noexcept
{
  while (radians > Pi)
    {
      radians -= TwoPi;
    }
  while (radians < -Pi)
    {
      radians += TwoPi;
    }
  return radians;
}

} // namespace decodium::sstv
