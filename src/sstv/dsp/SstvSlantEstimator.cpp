// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvSlantEstimator.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace decodium::sstv
{
namespace
{
constexpr double MinimumToleranceSamples = 0.01;
}

SstvSlantEstimator::SstvSlantEstimator (SstvSlantEstimatorConfig config)
    : config_ {[&config] {
        validateConfig (config);
        return config;
      } ()}
{
  window_.reserve (config_.windowLines);
  slopeScratch_.reserve (config_.windowLines * (config_.windowLines - 1u)
                         / 2u);
  valueScratch_.reserve (config_.windowLines);
}

SstvSlantUpdate SstvSlantEstimator::observe (
    SstvSlantObservation observation)
{
  std::lock_guard<std::mutex> const lock {mutex_};
  return observeUnlocked (observation);
}

std::vector<SstvSlantUpdate> SstvSlantEstimator::consume (
    SstvSlantObservation const* observations,
    std::size_t count)
{
  std::lock_guard<std::mutex> const lock {mutex_};
  std::vector<SstvSlantUpdate> updates;
  if (count == 0u)
    {
      return updates;
    }
  if (observations == nullptr || count > MaximumObservationsPerConsume)
    {
      saturatingAdd (metrics_.rejectedInputCalls, 1u);
      saturatingAdd (metrics_.rejectedObservations,
                     static_cast<std::uint64_t> (count));
      if (count > MaximumObservationsPerConsume)
        {
          saturatingAdd (metrics_.rejectedOversizeCalls, 1u);
        }
      return updates;
    }

  updates.reserve (count);
  for (std::size_t index = 0u; index < count; ++index)
    {
      updates.push_back (observeUnlocked (observations[index]));
    }
  return updates;
}

std::vector<SstvSlantUpdate> SstvSlantEstimator::consume (
    std::vector<SstvSlantObservation> const& observations)
{
  return consume (observations.data (), observations.size ());
}

void SstvSlantEstimator::notifyDiscontinuity ()
{
  std::lock_guard<std::mutex> const lock {mutex_};
  saturatingAdd (metrics_.discontinuities, 1u);
  clearWindowUnlocked ();
}

void SstvSlantEstimator::reset ()
{
  std::lock_guard<std::mutex> const lock {mutex_};
  metrics_ = {};
  clearWindowUnlocked ();
}

SstvSlantEstimate SstvSlantEstimator::snapshot () const
{
  std::lock_guard<std::mutex> const lock {mutex_};
  return snapshotUnlocked ();
}

SstvSlantEstimatorConfig const& SstvSlantEstimator::config () const noexcept
{
  return config_;
}

std::size_t SstvSlantEstimator::bufferedObservationCount () const
{
  std::lock_guard<std::mutex> const lock {mutex_};
  return window_.size ();
}

std::size_t
SstvSlantEstimator::maximumBufferedObservationCount () const noexcept
{
  return config_.windowLines;
}

SstvSlantUpdate SstvSlantEstimator::observeUnlocked (
    SstvSlantObservation observation)
{
  SstvSlantUpdate update;
  update.observation = observation;
  saturatingAdd (metrics_.observationsReceived, 1u);

  if (!std::isfinite (observation.confidence)
      || observation.confidence < 0.0 || observation.confidence > 1.0)
    {
      saturatingAdd (metrics_.invalidObservations, 1u);
      update.status = SstvSlantStatus::InvalidInput;
      update.estimate = snapshotUnlocked ();
      update.estimate.status = update.status;
      return update;
    }
  if (observation.predicted)
    {
      saturatingAdd (metrics_.predictedSyncsIgnored, 1u);
      update.status = SstvSlantStatus::PredictedSyncIgnored;
      update.estimate = snapshotUnlocked ();
      update.estimate.status = update.status;
      return update;
    }
  if (observation.confidence < config_.minimumConfidence)
    {
      saturatingAdd (metrics_.invalidObservations, 1u);
      update.status = SstvSlantStatus::InvalidInput;
      update.estimate = snapshotUnlocked ();
      update.estimate.status = update.status;
      return update;
    }

  bool discontinuity = false;
  if (!window_.empty ()
      && (observation.lineIndex <= window_.back ().lineIndex
          || observation.syncStartSample <= window_.back ().syncStartSample))
    {
      saturatingAdd (metrics_.discontinuities, 1u);
      clearWindowUnlocked ();
      discontinuity = true;
    }

  if (!window_.empty () && isOutlierUnlocked (observation))
    {
      saturatingAdd (metrics_.outliersRejected, 1u);
      update.status = SstvSlantStatus::OutlierRejected;
      update.estimate = snapshotUnlocked ();
      update.estimate.status = update.status;
      return update;
    }

  if (window_.size () == config_.windowLines)
    {
      window_.erase (window_.begin ());
    }
  window_.push_back (observation);
  saturatingAdd (metrics_.acceptedObservations, 1u);
  metrics_.peakWindowOccupancy = std::max (metrics_.peakWindowOccupancy,
                                           window_.size ());
  recomputeModelUnlocked ();
  update.accepted = true;
  update.estimate = snapshotUnlocked ();
  update.status = discontinuity ? SstvSlantStatus::Discontinuity
                                : update.estimate.status;
  update.estimate.status = update.status;
  return update;
}

void SstvSlantEstimator::clearWindowUnlocked () noexcept
{
  window_.clear ();
  slopeScratch_.clear ();
  valueScratch_.clear ();
  modelValid_ = false;
  estimatedPeriodSamples_ = 0.0;
  anchorOffsetSamples_ = 0.0;
  medianResidualSamples_ = 0.0;
  modelConfidence_ = 0.0;
}

void SstvSlantEstimator::recomputeModelUnlocked ()
{
  modelValid_ = false;
  if (window_.size () < 2u)
    {
      return;
    }

  slopeScratch_.clear ();
  for (std::size_t left = 0u; left + 1u < window_.size (); ++left)
    {
      for (std::size_t right = left + 1u; right < window_.size (); ++right)
        {
          std::uint64_t const lineDelta = window_[right].lineIndex
                                          - window_[left].lineIndex;
          std::uint64_t const sampleDelta = window_[right].syncStartSample
                                            - window_[left].syncStartSample;
          if (lineDelta == 0u)
            {
              continue;
            }
          slopeScratch_.push_back (
              static_cast<double> (sampleDelta)
              / static_cast<double> (lineDelta));
        }
    }
  saturatingAdd (metrics_.pairwiseWorkUnits,
                 static_cast<std::uint64_t> (slopeScratch_.size ()));
  if (slopeScratch_.empty ())
    {
      return;
    }

  estimatedPeriodSamples_ = medianInPlace (slopeScratch_);
  SstvSlantObservation const& anchor = window_.front ();
  valueScratch_.clear ();
  for (SstvSlantObservation const& observation : window_)
    {
      double const lineDelta = static_cast<double> (
          observation.lineIndex - anchor.lineIndex);
      double const sampleDelta = static_cast<double> (
          observation.syncStartSample - anchor.syncStartSample);
      valueScratch_.push_back (sampleDelta
                               - estimatedPeriodSamples_ * lineDelta);
    }
  anchorOffsetSamples_ = medianInPlace (valueScratch_);

  valueScratch_.clear ();
  double confidenceSum = 0.0;
  for (SstvSlantObservation const& observation : window_)
    {
      double const lineDelta = static_cast<double> (
          observation.lineIndex - anchor.lineIndex);
      double const sampleDelta = static_cast<double> (
          observation.syncStartSample - anchor.syncStartSample);
      double const residual = sampleDelta - anchorOffsetSamples_
                              - estimatedPeriodSamples_ * lineDelta;
      valueScratch_.push_back (std::abs (residual));
      confidenceSum += observation.confidence;
    }
  medianResidualSamples_ = medianInPlace (valueScratch_);
  double const coverage = std::min (
      1.0,
      static_cast<double> (window_.size ())
          / static_cast<double> (config_.minimumLines));
  double const residualScore = 1.0
                               / (1.0 + medianResidualSamples_
                                      / config_.outlierToleranceSamples);
  modelConfidence_ = clamp (
      confidenceSum / static_cast<double> (window_.size ())
          * coverage * residualScore,
      0.0,
      1.0);
  modelValid_ = window_.size () >= config_.minimumLines
                && std::isfinite (estimatedPeriodSamples_)
                && estimatedPeriodSamples_ > 0.0;
  saturatingAdd (metrics_.modelRecomputations, 1u);
}

bool SstvSlantEstimator::isOutlierUnlocked (
    SstvSlantObservation const& observation) const
{
  SstvSlantObservation const& last = window_.back ();
  std::uint64_t const lineDelta = observation.lineIndex - last.lineIndex;
  std::uint64_t const sampleDelta = observation.syncStartSample
                                    - last.syncStartSample;
  if (lineDelta == 0u)
    {
      return true;
    }

  double const nominal = static_cast<double> (
      config_.nominalLinePeriodSamples);
  double const allowedPeriodError = nominal
                                    * config_.maximumClockErrorPpm
                                    / 1'000'000.0;
  double const observedPeriod = static_cast<double> (sampleDelta)
                                / static_cast<double> (lineDelta);
  if (std::abs (observedPeriod - nominal)
      > allowedPeriodError
            + config_.outlierToleranceSamples
                  / static_cast<double> (lineDelta))
    {
      return true;
    }
  if (!modelValid_)
    {
      return false;
    }

  SstvSlantObservation const& anchor = window_.front ();
  double const lineFromAnchor = static_cast<double> (
      observation.lineIndex - anchor.lineIndex);
  double const observedFromAnchor = static_cast<double> (
      observation.syncStartSample - anchor.syncStartSample);
  double const predictedFromAnchor = anchorOffsetSamples_
                                     + estimatedPeriodSamples_
                                           * lineFromAnchor;
  return std::abs (observedFromAnchor - predictedFromAnchor)
         > config_.outlierToleranceSamples;
}

SstvSlantEstimate SstvSlantEstimator::snapshotUnlocked () const
{
  SstvSlantEstimate result;
  result.observationCount = window_.size ();
  result.metrics = metrics_;
  result.metrics.windowOccupancy = window_.size ();
  if (window_.empty ())
    {
      return result;
    }

  result.anchorLineIndex = window_.front ().lineIndex;
  result.anchorSyncSample = window_.front ().syncStartSample;
  result.lastLineIndex = window_.back ().lineIndex;
  if (!modelValid_)
    {
      return result;
    }

  double const nominal = static_cast<double> (
      config_.nominalLinePeriodSamples);
  result.estimatedLinePeriodSamples = estimatedPeriodSamples_;
  result.errorSamplesPerLine = estimatedPeriodSamples_ - nominal;
  result.correctionSamplesPerLine = -result.errorSamplesPerLine;
  result.clockErrorPpm = result.errorSamplesPerLine / nominal * 1'000'000.0;
  double const lineSpan = static_cast<double> (result.lastLineIndex
                                               - result.anchorLineIndex);
  result.accumulatedSlantSamples = result.errorSamplesPerLine * lineSpan;
  result.medianAbsoluteResidualSamples = medianResidualSamples_;
  result.confidence = modelConfidence_;
  result.valid = std::isfinite (result.estimatedLinePeriodSamples)
                 && std::isfinite (result.clockErrorPpm)
                 && std::isfinite (result.accumulatedSlantSamples)
                 && std::isfinite (result.confidence);
  if (!result.valid)
    {
      result.status = SstvSlantStatus::InvalidInput;
    }
  else if (std::abs (result.clockErrorPpm)
           > config_.warningClockErrorPpm)
    {
      result.status = SstvSlantStatus::BeyondConfiguredTolerance;
    }
  else
    {
      result.status = SstvSlantStatus::Valid;
    }
  return result;
}

void SstvSlantEstimator::validateConfig (
    SstvSlantEstimatorConfig const& config)
{
  if (config.nominalLinePeriodSamples == 0u
      || config.nominalLinePeriodSamples > MaximumLinePeriodSamples
      || config.windowLines < 2u || config.windowLines > MaximumWindowLines
      || config.minimumLines < 2u
      || config.minimumLines > config.windowLines
      || !std::isfinite (config.minimumConfidence)
      || config.minimumConfidence < 0.0 || config.minimumConfidence > 1.0
      || !std::isfinite (config.outlierToleranceSamples)
      || config.outlierToleranceSamples < MinimumToleranceSamples
      || config.outlierToleranceSamples
             > static_cast<double> (config.nominalLinePeriodSamples)
      || !std::isfinite (config.warningClockErrorPpm)
      || config.warningClockErrorPpm < 0.0
      || !std::isfinite (config.maximumClockErrorPpm)
      || config.maximumClockErrorPpm < config.warningClockErrorPpm
      || config.maximumClockErrorPpm > MaximumAllowedClockErrorPpm
      || (config.windowLines * (config.windowLines - 1u) / 2u)
             > MaximumPairwiseWorkUnits)
    {
      throw std::invalid_argument {"invalid SSTV slant-estimator configuration"};
    }
}

void SstvSlantEstimator::saturatingAdd (std::uint64_t& value,
                                        std::uint64_t increment) noexcept
{
  std::uint64_t const maximum = std::numeric_limits<std::uint64_t>::max ();
  value = increment > maximum - value ? maximum : value + increment;
}

double SstvSlantEstimator::medianInPlace (std::vector<double>& values)
{
  if (values.empty ())
    {
      return 0.0;
    }
  std::size_t const middleIndex = values.size () / 2u;
  auto const middle = values.begin ()
                      + static_cast<std::ptrdiff_t> (middleIndex);
  std::nth_element (values.begin (), middle, values.end ());
  double upper = *middle;
  if ((values.size () % 2u) != 0u)
    {
      return upper;
    }
  auto const lower = std::max_element (values.begin (), middle);
  return (*lower + upper) / 2.0;
}

double SstvSlantEstimator::clamp (double value,
                                  double minimum,
                                  double maximum) noexcept
{
  if (!std::isfinite (value))
    {
      return minimum;
    }
  return std::max (minimum, std::min (maximum, value));
}

} // namespace decodium::sstv
