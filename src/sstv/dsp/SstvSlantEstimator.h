// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace decodium::sstv
{

struct SstvSlantEstimatorConfig
{
  // All timing is supplied by the selected mode.  Zero is intentionally not
  // a useful default and is rejected by the constructor.
  std::uint64_t nominalLinePeriodSamples {0u};
  std::size_t windowLines {32u};
  std::size_t minimumLines {4u};
  double minimumConfidence {0.25};
  double outlierToleranceSamples {8.0};
  double warningClockErrorPpm {300.0};
  double maximumClockErrorPpm {5'000.0};
};

struct SstvSlantObservation
{
  std::uint64_t lineIndex {0u};
  std::uint64_t syncStartSample {0u};
  double confidence {0.0};
  bool predicted {false};
};

enum class SstvSlantStatus : std::uint8_t
{
  InsufficientData,
  Valid,
  BeyondConfiguredTolerance,
  OutlierRejected,
  PredictedSyncIgnored,
  Discontinuity,
  InvalidInput,
  RejectedCall
};

struct SstvSlantMetrics
{
  std::uint64_t observationsReceived {0u};
  std::uint64_t acceptedObservations {0u};
  std::uint64_t invalidObservations {0u};
  std::uint64_t outliersRejected {0u};
  std::uint64_t predictedSyncsIgnored {0u};
  std::uint64_t discontinuities {0u};
  std::uint64_t modelRecomputations {0u};
  std::uint64_t pairwiseWorkUnits {0u};
  std::uint64_t rejectedInputCalls {0u};
  std::uint64_t rejectedOversizeCalls {0u};
  std::uint64_t rejectedObservations {0u};
  std::size_t windowOccupancy {0u};
  std::size_t peakWindowOccupancy {0u};
};

struct SstvSlantEstimate
{
  SstvSlantStatus status {SstvSlantStatus::InsufficientData};
  bool valid {false};
  std::size_t observationCount {0u};
  std::uint64_t anchorLineIndex {0u};
  std::uint64_t anchorSyncSample {0u};
  std::uint64_t lastLineIndex {0u};
  double estimatedLinePeriodSamples {0.0};
  double errorSamplesPerLine {0.0};
  double correctionSamplesPerLine {0.0};
  double clockErrorPpm {0.0};
  double accumulatedSlantSamples {0.0};
  double medianAbsoluteResidualSamples {0.0};
  double confidence {0.0};
  SstvSlantMetrics metrics;
};

struct SstvSlantUpdate
{
  SstvSlantStatus status {SstvSlantStatus::InvalidInput};
  bool accepted {false};
  SstvSlantObservation observation;
  SstvSlantEstimate estimate;
};

// Robust, hard-bounded line-clock estimator.  Only observed syncs enter the
// model; predictions are intentionally excluded to prevent self-confirmation.
// Methods are synchronized so a UI/logger can safely request snapshots.
class SstvSlantEstimator final
{
public:
  // Keep a hostile but valid call suitable for a realtime audio path.  At a
  // full window the public bulk API performs at most 1,032,192 pair slopes.
  static constexpr std::size_t MaximumWindowLines = 64u;
  static constexpr std::size_t MaximumObservationsPerConsume = 512u;
  static constexpr std::uint64_t MaximumPairwiseWorkUnits = 2'016u;
  static constexpr std::uint64_t MaximumPairwiseWorkUnitsPerConsume =
      MaximumPairwiseWorkUnits * MaximumObservationsPerConsume;
  static constexpr std::uint64_t MaximumLinePeriodSamples = 12'000'000u;
  static constexpr double MaximumAllowedClockErrorPpm = 100'000.0;

  explicit SstvSlantEstimator (SstvSlantEstimatorConfig config);

  SstvSlantUpdate observe (SstvSlantObservation observation);
  std::vector<SstvSlantUpdate> consume (SstvSlantObservation const* observations,
                                       std::size_t count);
  std::vector<SstvSlantUpdate> consume (
      std::vector<SstvSlantObservation> const& observations);

  // Clears only the regression window while preserving lifetime diagnostics.
  // reset() clears both window and metrics.
  void notifyDiscontinuity ();
  void reset ();

  SstvSlantEstimate snapshot () const;
  SstvSlantEstimatorConfig const& config () const noexcept;
  std::size_t bufferedObservationCount () const;
  std::size_t maximumBufferedObservationCount () const noexcept;

private:
  SstvSlantUpdate observeUnlocked (SstvSlantObservation observation);
  void clearWindowUnlocked () noexcept;
  void recomputeModelUnlocked ();
  bool isOutlierUnlocked (SstvSlantObservation const& observation) const;
  SstvSlantEstimate snapshotUnlocked () const;
  static void validateConfig (SstvSlantEstimatorConfig const& config);
  static void saturatingAdd (std::uint64_t& value,
                             std::uint64_t increment) noexcept;
  static double medianInPlace (std::vector<double>& values);
  static double clamp (double value, double minimum, double maximum) noexcept;

  SstvSlantEstimatorConfig config_;
  mutable std::mutex mutex_;
  std::vector<SstvSlantObservation> window_;
  std::vector<double> slopeScratch_;
  std::vector<double> valueScratch_;
  SstvSlantMetrics metrics_;
  bool modelValid_ {false};
  double estimatedPeriodSamples_ {0.0};
  double anchorOffsetSamples_ {0.0};
  double medianResidualSamples_ {0.0};
  double modelConfidence_ {0.0};
};

} // namespace decodium::sstv
