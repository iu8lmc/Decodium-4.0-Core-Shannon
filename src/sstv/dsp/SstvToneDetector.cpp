// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvToneDetector.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace decodium::sstv
{
namespace
{

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kTiny = 1.0e-30;

double clamp01 (double value) noexcept
{
  return std::max (0.0, std::min (1.0, value));
}

double powerRatioDb (double numerator, double denominator) noexcept
{
  double const ratio = std::max (numerator, kTiny)
                       / std::max (denominator, kTiny);
  return std::max (-120.0,
                   std::min (120.0, 10.0 * std::log10 (ratio)));
}

struct Candidate
{
  double nominal {0.0};
  double frequency {0.0};
  double offset {0.0};
  double energy {-1.0};
};

struct SpectralPoint
{
  double frequency {0.0};
  double energy {0.0};
};

bool nearlyEqual (double lhs, double rhs, double tolerance) noexcept
{
  return std::abs (lhs - rhs) <= tolerance;
}

std::size_t searchPointUpperBound (
    SstvToneDetectorConfig const& config)
{
  long double const span =
      (2.0L * static_cast<long double> (config.maximumOffsetHz))
      / static_cast<long double> (config.searchStepHz);
  if (!std::isfinite (span)
      || span
             > static_cast<long double> (
                 SstvToneDetector::MaximumSearchPointsPerWindow))
    throw std::invalid_argument ("SSTV tone search grid is too dense");

  // Grid floor + initial point + a possible explicit upper endpoint, zero,
  // and the optional tracked-offset point.
  std::size_t const gridPoints = static_cast<std::size_t> (std::floor (span))
                                 + 4;
  if (gridPoints
      > SstvToneDetector::MaximumSearchPointsPerWindow
            / config.nominalFrequenciesHz.size ())
    throw std::invalid_argument ("SSTV tone search grid is too large");
  return gridPoints * config.nominalFrequenciesHz.size ();
}

bool wouldOverflow (std::uint64_t value, std::uint64_t increment) noexcept
{
  return increment > std::numeric_limits<std::uint64_t>::max () - value;
}

} // namespace

SstvToneDetectorConfig
SstvToneDetectorConfig::sstvDefaults (double sampleRate)
{
  if (!std::isfinite (sampleRate)
      || sampleRate < SstvToneDetector::MinimumSampleRateHz
      || sampleRate > SstvToneDetector::MaximumSampleRateHz)
    throw std::invalid_argument ("invalid SSTV default tone sample rate");
  SstvToneDetectorConfig result;
  result.sampleRateHz = sampleRate;
  double const symbolSamples = sampleRate * 0.022;
  result.windowSamples = static_cast<std::size_t> (
      std::max (64.0, std::round (symbolSamples)));
  result.hopSamples = std::max<std::size_t> (1, result.windowSamples / 2);
  result.nominalFrequenciesHz = SstvToneDetector::defaultSstvFrequencies ();
  return result;
}

SstvToneDetector::SstvToneDetector (SstvToneDetectorConfig config)
    : config_ (std::move (config))
{
  validateConfig (config_);
  searchPointUpperBound_ = searchPointUpperBound (config_);
  workUnitsPerWindow_ = static_cast<std::uint64_t> (config_.windowSamples)
                        * static_cast<std::uint64_t> (
                            searchPointUpperBound_);
  buffer_.reserve (config_.windowSamples);
}

std::vector<SstvToneObservation>
SstvToneDetector::consume (float const* samples, std::size_t count)
{
  if (!samples && count != 0)
    throw std::invalid_argument ("null SSTV tone sample buffer");
  if (count > MaximumSamplesPerConsume)
    throw std::length_error ("SSTV tone input chunk exceeds public limit");

  std::size_t const available = buffer_.size () + count;
  std::size_t const newWindows =
      available < config_.windowSamples
          ? 0
          : 1 + (available - config_.windowSamples) / config_.hopSamples;
  if (newWindows > MaximumObservationsPerConsume)
    throw std::length_error ("SSTV tone output count exceeds public limit");
  if (newWindows != 0
      && workUnitsPerWindow_
             > MaximumWorkUnitsPerConsume
                   / static_cast<std::uint64_t> (newWindows))
    throw std::length_error ("SSTV tone analysis work exceeds public limit");

  std::uint64_t const count64 = static_cast<std::uint64_t> (count);
  std::uint64_t const windows64 = static_cast<std::uint64_t> (newWindows);
  std::uint64_t const startAdvance =
      windows64 * static_cast<std::uint64_t> (config_.hopSamples);
  std::uint64_t const lastCentreAdvance =
      newWindows == 0
          ? 0
          : (windows64 - 1)
                    * static_cast<std::uint64_t> (config_.hopSamples)
                + static_cast<std::uint64_t> (config_.windowSamples / 2);
  std::uint64_t const largestSampleIndexAdvance =
      std::max (startAdvance, lastCentreAdvance);
  if (wouldOverflow (metrics_.samplesConsumed, count64)
      || wouldOverflow (metrics_.windowsAnalysed, windows64)
      || wouldOverflow (metrics_.detections, windows64)
      || wouldOverflow (metrics_.lowSignalWindows, windows64)
      || wouldOverflow (metrics_.ambiguousWindows, windows64)
      || wouldOverflow (metrics_.invalidWindows, windows64)
      || wouldOverflow (observationSequence_, windows64)
      || wouldOverflow (bufferStartSample_, largestSampleIndexAdvance))
    throw std::overflow_error ("SSTV tone stream counter exhausted");

  std::vector<SstvToneObservation> observations;
  observations.reserve (newWindows);

  for (std::size_t index = 0; index < count; ++index)
    {
      buffer_.push_back (samples[index]);
      ++metrics_.samplesConsumed;
      metrics_.peakBufferedSamples =
          std::max (metrics_.peakBufferedSamples, buffer_.size ());

      if (buffer_.size () != config_.windowSamples)
        continue;

      observations.push_back (analyseWindow ());
      ++metrics_.windowsAnalysed;

      buffer_.erase (buffer_.begin (),
                     buffer_.begin ()
                         + static_cast<std::ptrdiff_t> (config_.hopSamples));
      bufferStartSample_ += config_.hopSamples;
    }

  metrics_.bufferedSamples = buffer_.size ();
  return observations;
}

std::vector<SstvToneObservation>
SstvToneDetector::consume (std::vector<float> const& samples)
{
  return consume (samples.data (), samples.size ());
}

void SstvToneDetector::reset (bool preserveCommonOffset) noexcept
{
  std::optional<double> const retained =
      preserveCommonOffset ? commonOffsetHz_ : std::nullopt;
  metrics_ = {};
  buffer_.clear ();
  bufferStartSample_ = 0;
  observationSequence_ = 0;
  commonOffsetHz_ = retained;
}

void SstvToneDetector::resetAtStreamSample (
    std::uint64_t nextSample, bool preserveCommonOffset) noexcept
{
  std::optional<double> const retained =
      preserveCommonOffset ? commonOffsetHz_ : std::nullopt;
  metrics_ = {};
  metrics_.samplesConsumed = nextSample;
  buffer_.clear ();
  bufferStartSample_ = nextSample;
  observationSequence_ = 0;
  commonOffsetHz_ = retained;
}

void SstvToneDetector::seedCommonOffset (double offsetHz)
{
  if (!std::isfinite (offsetHz)
      || std::abs (offsetHz) > config_.maximumOffsetHz)
    throw std::invalid_argument ("invalid SSTV common frequency offset");
  commonOffsetHz_ = offsetHz;
}

void SstvToneDetector::clearCommonOffset () noexcept
{
  commonOffsetHz_.reset ();
}

bool SstvToneDetector::hasCommonOffset () const noexcept
{
  return commonOffsetHz_.has_value ();
}

std::optional<double> SstvToneDetector::commonOffsetHz () const noexcept
{
  return commonOffsetHz_;
}

SstvToneDetectorConfig const& SstvToneDetector::config () const noexcept
{
  return config_;
}

SstvToneDetectorMetrics const& SstvToneDetector::metrics () const noexcept
{
  return metrics_;
}

std::size_t SstvToneDetector::bufferedSampleCount () const noexcept
{
  return buffer_.size ();
}

std::size_t SstvToneDetector::maximumBufferedSampleCount () const noexcept
{
  return config_.windowSamples;
}

std::vector<double> SstvToneDetector::defaultSstvFrequencies ()
{
  return {1'100.0, 1'200.0, 1'300.0, 1'500.0,
          1'900.0, 2'100.0, 2'300.0};
}

SstvToneObservation SstvToneDetector::analyseWindow ()
{
  SstvToneObservation observation;
  observation.sequence = observationSequence_++;
  observation.startSample = bufferStartSample_;
  observation.centreSample =
      bufferStartSample_ + config_.windowSamples / 2;
  observation.commonOffsetHz = commonOffsetHz_.value_or (0.0);

  bool allFinite = true;
  long double sum = 0.0L;
  for (float sample : buffer_)
    {
      if (!std::isfinite (sample))
        {
          allFinite = false;
          break;
        }
      sum += sample;
    }

  if (!allFinite)
    {
      observation.status = SstvToneStatus::InvalidInput;
      ++metrics_.invalidWindows;
      return observation;
    }

  double const mean = static_cast<double> (
      sum / static_cast<long double> (config_.windowSamples));
  std::vector<double> centred;
  std::vector<double> windowed;
  centred.reserve (config_.windowSamples);
  windowed.reserve (config_.windowSamples);

  long double squareSum = 0.0L;
  for (std::size_t index = 0; index < buffer_.size (); ++index)
    {
      double const value = static_cast<double> (buffer_[index]) - mean;
      double const window = config_.windowSamples == 1
                                ? 1.0
                                : 0.5
                                      - 0.5
                                            * std::cos (
                                                2.0 * kPi
                                                * static_cast<double> (index)
                                                / static_cast<double> (
                                                    config_.windowSamples
                                                    - 1));
      centred.push_back (value);
      windowed.push_back (value * window);
      squareSum += static_cast<long double> (value)
                   * static_cast<long double> (value);
    }

  double const meanSquare = static_cast<double> (
      squareSum / static_cast<long double> (config_.windowSamples));
  observation.rms = std::sqrt (std::max (0.0, meanSquare));
  if (observation.rms < config_.minimumRms)
    {
      observation.status = SstvToneStatus::LowSignal;
      observation.snrDb = -120.0;
      ++metrics_.lowSignalWindows;
      return observation;
    }

  std::vector<SpectralPoint> points;
  points.reserve (searchPointUpperBound_);

  auto preferenceDistance = [this] (double offset) noexcept {
    return std::abs (offset - commonOffsetHz_.value_or (0.0));
  };
  auto isBetter = [&preferenceDistance] (Candidate const& candidate,
                                         Candidate const& current) noexcept {
    if (current.energy < 0.0)
      return true;
    double const scale = std::max ({candidate.energy, current.energy, kTiny});
    if (candidate.energy > current.energy + scale * 0.005)
      return true;
    if (current.energy > candidate.energy + scale * 0.005)
      return false;
    double const candidatePreference = preferenceDistance (candidate.offset);
    double const currentPreference = preferenceDistance (current.offset);
    if (!nearlyEqual (candidatePreference, currentPreference, 1.0e-9))
      return candidatePreference < currentPreference;
    if (!nearlyEqual (std::abs (candidate.offset),
                      std::abs (current.offset), 1.0e-9))
      return std::abs (candidate.offset) < std::abs (current.offset);
    return candidate.nominal < current.nominal;
  };

  Candidate best;
  for (double nominal : config_.nominalFrequenciesHz)
    {
      Candidate targetBest;
      auto evaluate = [&] (double offset) {
        offset = std::max (-config_.maximumOffsetHz,
                           std::min (config_.maximumOffsetHz, offset));
        double const frequency = nominal + offset;
        double const energy = spectralEnergy (windowed, config_.sampleRateHz,
                                              frequency);
        points.push_back ({frequency, energy});
        Candidate const candidate {nominal, frequency, offset, energy};
        if (isBetter (candidate, targetBest))
          targetBest = candidate;
      };

      long double const span =
          (2.0L * static_cast<long double> (config_.maximumOffsetHz))
          / static_cast<long double> (config_.searchStepHz);
      std::size_t const fullSteps =
          static_cast<std::size_t> (std::floor (span));
      for (std::size_t step = 0; step <= fullSteps; ++step)
        {
          double const offset =
              -config_.maximumOffsetHz
              + static_cast<double> (step) * config_.searchStepHz;
          evaluate (offset);
        }
      double const lastOffset =
          -config_.maximumOffsetHz
          + static_cast<double> (fullSteps) * config_.searchStepHz;
      if (lastOffset < config_.maximumOffsetHz - 1.0e-9)
        evaluate (config_.maximumOffsetHz);
      evaluate (0.0);
      if (commonOffsetHz_)
        evaluate (*commonOffsetHz_);

      if (isBetter (targetBest, best))
        best = targetBest;
    }

  if (best.energy < 0.0 || !std::isfinite (best.energy))
    {
      observation.status = SstvToneStatus::InvalidInput;
      ++metrics_.invalidWindows;
      return observation;
    }

  // Refine the filter-bank maximum with a bounded parabolic interpolation.
  double const lowerFrequency = best.nominal - config_.maximumOffsetHz;
  double const upperFrequency = best.nominal + config_.maximumOffsetHz;
  double const leftFrequency =
      std::max (lowerFrequency, best.frequency - config_.searchStepHz);
  double const rightFrequency =
      std::min (upperFrequency, best.frequency + config_.searchStepHz);
  double refinedFrequency = best.frequency;
  if (leftFrequency < best.frequency && rightFrequency > best.frequency)
    {
      double const left = spectralEnergy (windowed, config_.sampleRateHz,
                                          leftFrequency);
      double const centre = best.energy;
      double const right = spectralEnergy (windowed, config_.sampleRateHz,
                                           rightFrequency);
      double const denominator = left - 2.0 * centre + right;
      if (denominator < -kTiny)
        {
          double const fraction =
              std::max (-1.0,
                        std::min (1.0,
                                  0.5 * (left - right) / denominator));
          refinedFrequency += fraction * config_.searchStepHz;
          refinedFrequency = std::max (
              lowerFrequency, std::min (upperFrequency, refinedFrequency));
        }
    }

  double const resolution = config_.sampleRateHz
                            / static_cast<double> (config_.windowSamples);
  double const exclusionHz = std::max (2.5 * resolution,
                                       2.0 * config_.searchStepHz);
  double secondEnergy = 0.0;
  for (auto const& point : points)
    if (std::abs (point.frequency - refinedFrequency) > exclusionHz)
      secondEnergy = std::max (secondEnergy, point.energy);
  observation.dominanceDb = powerRatioDb (best.energy, secondEnergy);

  // Least-squares sinusoid fit provides an SNR estimate independent of the
  // filter-bank peak scale and rejects broadband noise / dual-tone mixtures.
  long double cosineSquared = 0.0L;
  long double sineSquared = 0.0L;
  long double cross = 0.0L;
  long double signalCosine = 0.0L;
  long double signalSine = 0.0L;
  double const omega = 2.0 * kPi * refinedFrequency / config_.sampleRateHz;
  for (std::size_t index = 0; index < centred.size (); ++index)
    {
      double const phase = omega * static_cast<double> (index);
      double const cosine = std::cos (phase);
      double const sine = std::sin (phase);
      cosineSquared += cosine * cosine;
      sineSquared += sine * sine;
      cross += cosine * sine;
      signalCosine += centred[index] * cosine;
      signalSine += centred[index] * sine;
    }

  long double const determinant =
      cosineSquared * sineSquared - cross * cross;
  double cosineCoefficient = 0.0;
  double sineCoefficient = 0.0;
  if (std::abs (determinant) > 1.0e-18L)
    {
      cosineCoefficient = static_cast<double> (
          (signalCosine * sineSquared - signalSine * cross) / determinant);
      sineCoefficient = static_cast<double> (
          (signalSine * cosineSquared - signalCosine * cross) / determinant);
    }

  long double fittedPower = 0.0L;
  long double residualPower = 0.0L;
  for (std::size_t index = 0; index < centred.size (); ++index)
    {
      double const phase = omega * static_cast<double> (index);
      double const fitted = cosineCoefficient * std::cos (phase)
                            + sineCoefficient * std::sin (phase);
      double const residual = centred[index] - fitted;
      fittedPower += static_cast<long double> (fitted) * fitted;
      residualPower += static_cast<long double> (residual) * residual;
    }

  observation.tonePower = static_cast<double> (
      fittedPower / static_cast<long double> (centred.size ()));
  observation.noisePower = static_cast<double> (
      residualPower / static_cast<long double> (centred.size ()));
  observation.snrDb = powerRatioDb (observation.tonePower,
                                    observation.noisePower);

  double const snrTop = std::max (config_.minimumSnrDb + 12.0, 24.0);
  double const dominanceTop = config_.minimumDominanceDb + 10.0;
  double const snrScore = clamp01 (
      (observation.snrDb - config_.minimumSnrDb)
      / (snrTop - config_.minimumSnrDb));
  double const dominanceScore = clamp01 (
      (observation.dominanceDb - config_.minimumDominanceDb)
      / (dominanceTop - config_.minimumDominanceDb));
  observation.confidence = clamp01 (0.8 * snrScore
                                    + 0.2 * dominanceScore);

  observation.nominalFrequencyHz = best.nominal;
  observation.detectedFrequencyHz = refinedFrequency;
  observation.frequencyOffsetHz = refinedFrequency - best.nominal;

  bool const detected = observation.snrDb >= config_.minimumSnrDb
                        && observation.dominanceDb
                               >= config_.minimumDominanceDb
                        && observation.confidence
                               >= config_.minimumConfidence;
  if (!detected)
    {
      observation.status = SstvToneStatus::Ambiguous;
      ++metrics_.ambiguousWindows;
      observation.commonOffsetHz = commonOffsetHz_.value_or (0.0);
      return observation;
    }

  observation.status = SstvToneStatus::Detected;
  updateOffset (observation.frequencyOffsetHz);
  observation.commonOffsetHz = commonOffsetHz_.value_or (0.0);
  ++metrics_.detections;
  return observation;
}

double SstvToneDetector::spectralEnergy (
    std::vector<double> const& windowed,
    double sampleRateHz,
    double frequencyHz) noexcept
{
  double const omega = 2.0 * kPi * frequencyHz / sampleRateHz;
  double const coefficient = 2.0 * std::cos (omega);
  double previous = 0.0;
  double previousPrevious = 0.0;
  for (double sample : windowed)
    {
      double const current = sample + coefficient * previous
                             - previousPrevious;
      previousPrevious = previous;
      previous = current;
    }
  return std::max (0.0, previous * previous
                            + previousPrevious * previousPrevious
                            - coefficient * previous * previousPrevious);
}

void SstvToneDetector::updateOffset (double offsetHz) noexcept
{
  offsetHz = std::max (-config_.maximumOffsetHz,
                       std::min (config_.maximumOffsetHz, offsetHz));
  if (!commonOffsetHz_)
    commonOffsetHz_ = offsetHz;
  else
    commonOffsetHz_ = (1.0 - config_.offsetSmoothing) * *commonOffsetHz_
                      + config_.offsetSmoothing * offsetHz;
}

void SstvToneDetector::validateConfig (
    SstvToneDetectorConfig const& config)
{
  if (!std::isfinite (config.sampleRateHz)
      || config.sampleRateHz < MinimumSampleRateHz
      || config.sampleRateHz > MaximumSampleRateHz)
    throw std::invalid_argument ("invalid SSTV tone sample rate");
  if (config.windowSamples < 32 || config.hopSamples == 0
      || config.hopSamples > config.windowSamples
      || config.windowSamples > MaximumWindowSamples)
    throw std::invalid_argument ("invalid SSTV tone window/overlap");
  if (config.nominalFrequenciesHz.empty ()
      || config.nominalFrequenciesHz.size () > MaximumNominalFrequencies)
    throw std::invalid_argument ("empty SSTV tone filter bank");
  if (!std::isfinite (config.maximumOffsetHz)
      || config.maximumOffsetHz <= 0.0
      || config.maximumOffsetHz > MaximumConfiguredOffsetHz
      || !std::isfinite (config.searchStepHz) || config.searchStepHz <= 0.0
      || config.searchStepHz > config.maximumOffsetHz)
    throw std::invalid_argument ("invalid SSTV tone offset search");
  if (!std::isfinite (config.minimumRms) || config.minimumRms < 0.0
      || !std::isfinite (config.minimumSnrDb)
      || !std::isfinite (config.minimumDominanceDb)
      || !std::isfinite (config.minimumConfidence)
      || config.minimumConfidence < 0.0 || config.minimumConfidence > 1.0
      || !std::isfinite (config.offsetSmoothing)
      || config.offsetSmoothing <= 0.0 || config.offsetSmoothing > 1.0)
    throw std::invalid_argument ("invalid SSTV tone thresholds");

  double const nyquist = config.sampleRateHz / 2.0;
  for (std::size_t index = 0;
       index < config.nominalFrequenciesHz.size (); ++index)
    {
      double const frequency = config.nominalFrequenciesHz[index];
      if (!std::isfinite (frequency)
          || frequency - config.maximumOffsetHz <= 0.0
          || frequency + config.maximumOffsetHz >= nyquist)
        throw std::invalid_argument ("SSTV tone outside usable bandwidth");
      for (std::size_t earlier = 0; earlier < index; ++earlier)
        if (nearlyEqual (frequency,
                         config.nominalFrequenciesHz[earlier], 1.0e-9))
          throw std::invalid_argument ("duplicate SSTV tone frequency");
    }

  std::size_t const searchPoints = searchPointUpperBound (config);
  if (config.windowSamples
      > MaximumWorkUnitsPerWindow
            / static_cast<std::uint64_t> (searchPoints))
    throw std::invalid_argument ("SSTV tone window search is too costly");
}

} // namespace decodium::sstv
