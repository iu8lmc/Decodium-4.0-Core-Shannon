// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "src/sstv/core/SstvVisCodec.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace decodium::sstv
{

enum class SstvVisToneKind : std::uint8_t
{
  Unknown,
  Leader,
  Separator,
  BitOne,
  BitZero
};

enum class SstvVisDetectorState : std::uint8_t
{
  SearchingLeader,
  FirstLeader,
  Break,
  SecondLeader,
  StartSeparator,
  ReadingBits,
  AwaitingStop,
  Cancelled
};

enum class SstvVisDetectionStatus : std::uint8_t
{
  Decoded,
  Rejected,
  Truncated,
  TimedOut,
  Cancelled,
  InvalidInput,
  BoundsExceeded
};

enum class SstvVisDetectionCause : std::uint8_t
{
  None,
  LeaderTiming,
  BreakTiming,
  SecondLeaderTiming,
  StartTiming,
  UnexpectedTone,
  LowConfidence,
  SlotCoverage,
  SymbolAmbiguous,
  EarlyStop,
  MissingStop,
  ParityMismatch,
  InvalidExtendedMarker,
  CodecRejected,
  EndOfInput,
  Timeout,
  Cancelled,
  TimestampRegression,
  GapExceeded,
  InvalidObservation,
  FrequencyOffsetOutOfRange,
  FrameDurationExceeded,
  RawEventLimit,
  CounterOverflow
};

struct SstvVisDetectorConfig
{
  std::uint64_t leaderDurationUs {300'000};
  std::uint64_t breakDurationUs {10'000};
  std::uint64_t symbolDurationUs {30'000};
  double headerDurationTolerance {0.25};
  double maximumClockDriftPpm {50'000.0};
  double frequencyToleranceHz {45.0};
  double maximumFrequencyOffsetHz {100.0};
  double minimumObservationConfidence {0.35};
  double minimumFrameConfidence {0.45};
  double minimumSlotCoverage {0.70};
  double minimumBitDominance {0.70};
  std::uint64_t maximumGapUs {5'000};
  std::uint64_t maximumOverlapUs {5'000};
  std::uint64_t maximumEventDurationUs {2'000'000};
  std::uint64_t maximumFrameDurationUs {2'000'000};
};

struct SstvVisToneEvent
{
  std::uint64_t startTimeUs {0};
  std::uint64_t durationUs {0};
  double frequencyHz {0.0};
  double confidence {0.0};
};

// This form is useful when a preceding filter bank already made the discrete
// tone decision.  frequencyOffsetHz remains explicit for diagnostics and for
// enforcing the same acquisition bound as unclassified events.
struct SstvVisClassifiedEvent
{
  std::uint64_t startTimeUs {0};
  std::uint64_t durationUs {0};
  SstvVisToneKind tone {SstvVisToneKind::Unknown};
  double frequencyOffsetHz {0.0};
  double confidence {0.0};
};

struct SstvVisRawEvent
{
  std::uint64_t sequence {0};
  std::uint64_t startTimeUs {0};
  std::uint64_t durationUs {0};
  double frequencyHz {0.0};
  double frequencyOffsetHz {0.0};
  double confidence {0.0};
  SstvVisToneKind tone {SstvVisToneKind::Unknown};
};

struct SstvVisDetection
{
  SstvVisDetectionStatus status {SstvVisDetectionStatus::Rejected};
  SstvVisDetectionCause cause {SstvVisDetectionCause::None};
  SstvVisDecodeResult codecResult;
  std::vector<SstvVisRawEvent> rawEvents;
  std::vector<SstvVisObservation> symbolObservations;
  std::uint64_t frameStartedAtUs {0};
  std::uint64_t frameEndedAtUs {0};
  double estimatedFrequencyOffsetHz {0.0};
  double estimatedClockDriftPpm {0.0};
  double confidence {0.0};
  std::string detail;

  bool valid () const noexcept
  {
    return status == SstvVisDetectionStatus::Decoded && codecResult.valid;
  }
};

struct SstvVisDetectorMetrics
{
  std::uint64_t observationsConsumed {0};
  std::uint64_t framesStarted {0};
  std::uint64_t framesDecoded {0};
  std::uint64_t framesRejected {0};
  std::uint64_t framesTruncated {0};
  std::uint64_t timeouts {0};
  std::uint64_t invalidInputs {0};
  std::uint64_t noiseEventsDiscarded {0};
  std::uint64_t automaticResets {0};
  std::uint64_t currentFrameId {0};
};

// Streaming physical-layer VIS detector.  SstvVisCodec remains the sole
// authority for bit framing, parity and standard/wide-extended validity.  No
// mode lookup and no narrow N-VIS interpretation occur in this class.
//
// All methods, including observers, are confined to one RX-worker thread.
// Controllers must publish copied SstvVisDetection/SstvVisDetectorMetrics
// snapshots across a queued connection instead of polling this object from
// the UI or audio-callback thread.
class SstvVisDetector final
{
public:
  static constexpr std::size_t MaximumEventsPerConsume = 256;
  static constexpr std::size_t MaximumRawEventsPerFrame = 64;
  static constexpr std::size_t MaximumSymbolsPerFrame = 18;
  static constexpr std::size_t MaximumSlotsPerEvent = 18;

  explicit SstvVisDetector (SstvVisDetectorConfig config = {});

  std::vector<SstvVisDetection> consume (SstvVisToneEvent const* events,
                                         std::size_t count);
  std::vector<SstvVisDetection> consume (
      std::vector<SstvVisToneEvent> const& events);
  std::vector<SstvVisDetection> consumeClassified (
      SstvVisClassifiedEvent const* events, std::size_t count);
  std::vector<SstvVisDetection> consumeClassified (
      std::vector<SstvVisClassifiedEvent> const& events);

  std::vector<SstvVisDetection> tick (std::uint64_t nowUs);
  std::optional<SstvVisDetection> finish (std::uint64_t nowUs);
  std::optional<SstvVisDetection> cancel (std::uint64_t nowUs);
  void reset (bool clearMetrics = false) noexcept;

  SstvVisDetectorState state () const noexcept;
  SstvVisDetectorConfig const& config () const noexcept;
  SstvVisDetectorMetrics const& metrics () const noexcept;
  bool frameActive () const noexcept;
  std::size_t bufferedRawEventCount () const noexcept;
  std::size_t bufferedSymbolCount () const noexcept;

  static char const* stateName (SstvVisDetectorState state) noexcept;
  static char const* causeName (SstvVisDetectionCause cause) noexcept;

private:
  struct NormalizedEvent
  {
    std::uint64_t start {0};
    std::uint64_t end {0};
    double frequency {0.0};
    double offset {0.0};
    double confidence {0.0};
    SstvVisToneKind tone {SstvVisToneKind::Unknown};
  };

  struct ToneRun
  {
    bool active {false};
    SstvVisToneKind tone {SstvVisToneKind::Unknown};
    std::uint64_t start {0};
    std::uint64_t end {0};
    std::uint64_t coveredUs {0};
    long double confidenceTime {0.0L};
    long double frequencyTime {0.0L};
  };

  struct SlotEvidence
  {
    std::uint64_t oneUs {0};
    std::uint64_t zeroUs {0};
    std::uint64_t separatorUs {0};
    std::uint64_t unknownUs {0};
    long double oneConfidenceTime {0.0L};
    long double zeroConfidenceTime {0.0L};
    long double separatorConfidenceTime {0.0L};
  };

  std::optional<NormalizedEvent> normalize (
      SstvVisToneEvent const& event,
      SstvVisDetection& invalidResult);
  std::optional<NormalizedEvent> normalize (
      SstvVisClassifiedEvent const& event,
      SstvVisDetection& invalidResult);
  void processEvent (NormalizedEvent const& event,
                     std::vector<SstvVisDetection>& results);
  void processHeaderEvent (NormalizedEvent const& event,
                           std::vector<SstvVisDetection>& results);
  void processDataInterval (NormalizedEvent const& event,
                            std::vector<SstvVisDetection>& results);
  bool finalizeRun (std::uint64_t atUs,
                    std::vector<SstvVisDetection>& results);
  bool finalizeSlot (std::uint64_t atUs,
                     std::vector<SstvVisDetection>& results);
  void startRun (NormalizedEvent const& event) noexcept;
  void extendRun (NormalizedEvent const& event) noexcept;
  void beginFrame (NormalizedEvent const& event);
  bool appendRawEvent (NormalizedEvent const& event,
                       std::vector<SstvVisDetection>& results);
  SstvVisToneKind classifyFrequency (double frequencyHz,
                                     double confidence) const noexcept;
  double nominalFrequency (SstvVisToneKind tone) const noexcept;
  bool slotBoundary (std::size_t slot,
                     std::uint64_t& boundary) const noexcept;
  bool durationMatches (std::uint64_t actual,
                        std::uint64_t nominal) const noexcept;
  double runConfidence () const noexcept;
  double frameConfidence () const noexcept;
  void setClockFromStartRun ();
  void resetFrame (bool automatic) noexcept;
  SstvVisDetection makeResult (SstvVisDetectionStatus status,
                               SstvVisDetectionCause cause,
                               std::uint64_t endedAtUs,
                               char const* detail,
                               bool includeCodecResult) const;
  void emitFailure (std::vector<SstvVisDetection>& results,
                    SstvVisDetectionStatus status,
                    SstvVisDetectionCause cause,
                    std::uint64_t endedAtUs,
                    char const* detail,
                    bool includeCodecResult = false);
  void completeFrame (std::uint64_t endedAtUs,
                      std::vector<SstvVisDetection>& results);
  bool preflightConsume (std::size_t count) const;
  static void validateConfig (SstvVisDetectorConfig const& config);
  static bool addWouldOverflow (std::uint64_t value,
                                std::uint64_t increment) noexcept;
  static bool checkedAdd (std::uint64_t left,
                          std::uint64_t right,
                          std::uint64_t& result) noexcept;
  static void saturatingIncrement (std::uint64_t& value) noexcept;

  SstvVisDetectorConfig config_;
  SstvVisDetectorMetrics metrics_;
  SstvVisDetectorState state_ {SstvVisDetectorState::SearchingLeader};
  ToneRun run_;
  SlotEvidence slot_;
  std::vector<SstvVisRawEvent> rawEvents_;
  std::vector<SstvVisObservation> symbols_;
  std::optional<double> frequencyOffsetHz_;
  double clockPeriodUs_ {30'000.0};
  double clockDriftPpm_ {0.0};
  std::uint64_t frameStartedAtUs_ {0};
  std::uint64_t symbolEpochUs_ {0};
  std::uint64_t lastTimelineEndUs_ {0};
  std::uint64_t nextRawSequence_ {0};
  std::uint64_t nextFrameId_ {0};
  std::size_t slotIndex_ {0};
  std::size_t targetRawBitCount_ {SstvVisCodec::StandardRawBitCount};
  double minimumHeaderConfidence_ {1.0};
  std::size_t acceptedHeaderPhases_ {0};
  bool hasTimeline_ {false};
};

} // namespace decodium::sstv
