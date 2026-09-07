// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvVisDetector.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace decodium::sstv
{
namespace
{

constexpr double kLeaderHz = 1'900.0;
constexpr double kSeparatorHz = 1'200.0;
constexpr double kOneHz = 1'100.0;
constexpr double kZeroHz = 1'300.0;

bool containsError (SstvVisDecodeResult const& result,
                    SstvVisError error) noexcept
{
  return std::find (result.errors.begin (), result.errors.end (), error)
         != result.errors.end ();
}

double clamp01 (double value) noexcept
{
  return std::max (0.0, std::min (1.0, value));
}

} // namespace

SstvVisDetector::SstvVisDetector (SstvVisDetectorConfig config)
    : config_ (std::move (config)),
      clockPeriodUs_ (static_cast<double> (config_.symbolDurationUs))
{
  validateConfig (config_);
  rawEvents_.reserve (MaximumRawEventsPerFrame);
  symbols_.reserve (MaximumSymbolsPerFrame);
}

std::vector<SstvVisDetection>
SstvVisDetector::consume (SstvVisToneEvent const* events, std::size_t count)
{
  if (!events && count != 0)
    throw std::invalid_argument ("null VIS tone event buffer");
  preflightConsume (count);

  if (state_ == SstvVisDetectorState::Cancelled)
    {
      metrics_.observationsConsumed += static_cast<std::uint64_t> (count);
      return {};
    }

  std::vector<SstvVisDetection> results;
  results.reserve (count);
  for (std::size_t index = 0; index < count; ++index)
    {
      saturatingIncrement (metrics_.observationsConsumed);
      SstvVisDetection invalid;
      auto event = normalize (events[index], invalid);
      if (!event)
        {
          if (invalid.cause != SstvVisDetectionCause::None)
            {
              bool const retryAsNewFrame =
                  invalid.cause == SstvVisDetectionCause::GapExceeded
                  && frameActive ();
              if (frameActive ())
                {
                  std::uint64_t const endedAt = hasTimeline_
                                                    ? std::max (
                                                          lastTimelineEndUs_,
                                                          events[index]
                                                              .startTimeUs)
                                                    : events[index]
                                                          .startTimeUs;
                  emitFailure (results,
                               SstvVisDetectionStatus::InvalidInput,
                               invalid.cause,
                               endedAt,
                               invalid.detail.c_str (),
                               true);
                }
              else
                {
                  saturatingIncrement (metrics_.invalidInputs);
                  results.push_back (std::move (invalid));
                }
              if (retryAsNewFrame)
                {
                  // The gap terminates the stale frame, but the current
                  // observation may itself be the leader of a new frame.  It
                  // has already passed hostile-input validation, so retry it
                  // exactly once after reset without recounting the input.
                  SstvVisDetection retryInvalid;
                  auto retry = normalize (events[index], retryInvalid);
                  if (retry)
                    {
                      processEvent (*retry, results);
                      lastTimelineEndUs_ = std::max (lastTimelineEndUs_,
                                                     retry->end);
                      hasTimeline_ = true;
                    }
                }
            }
          continue;
        }

      processEvent (*event, results);
      lastTimelineEndUs_ = std::max (lastTimelineEndUs_, event->end);
      hasTimeline_ = true;
    }
  return results;
}

std::vector<SstvVisDetection>
SstvVisDetector::consume (std::vector<SstvVisToneEvent> const& events)
{
  return consume (events.data (), events.size ());
}

std::vector<SstvVisDetection> SstvVisDetector::consumeClassified (
    SstvVisClassifiedEvent const* events, std::size_t count)
{
  if (!events && count != 0)
    throw std::invalid_argument ("null classified VIS event buffer");
  preflightConsume (count);

  if (state_ == SstvVisDetectorState::Cancelled)
    {
      metrics_.observationsConsumed += static_cast<std::uint64_t> (count);
      return {};
    }

  std::vector<SstvVisDetection> results;
  results.reserve (count);
  for (std::size_t index = 0; index < count; ++index)
    {
      saturatingIncrement (metrics_.observationsConsumed);
      SstvVisDetection invalid;
      auto event = normalize (events[index], invalid);
      if (!event)
        {
          if (invalid.cause != SstvVisDetectionCause::None)
            {
              bool const retryAsNewFrame =
                  invalid.cause == SstvVisDetectionCause::GapExceeded
                  && frameActive ();
              if (frameActive ())
                {
                  std::uint64_t const endedAt = hasTimeline_
                                                    ? std::max (
                                                          lastTimelineEndUs_,
                                                          events[index]
                                                              .startTimeUs)
                                                    : events[index]
                                                          .startTimeUs;
                  emitFailure (results,
                               SstvVisDetectionStatus::InvalidInput,
                               invalid.cause,
                               endedAt,
                               invalid.detail.c_str (),
                               true);
                }
              else
                {
                  saturatingIncrement (metrics_.invalidInputs);
                  results.push_back (std::move (invalid));
                }
              if (retryAsNewFrame)
                {
                  // Match the raw-observation path: one bounded retry lets a
                  // classified leader start the next frame after the stale
                  // frame has been rejected.
                  SstvVisDetection retryInvalid;
                  auto retry = normalize (events[index], retryInvalid);
                  if (retry)
                    {
                      processEvent (*retry, results);
                      lastTimelineEndUs_ = std::max (lastTimelineEndUs_,
                                                     retry->end);
                      hasTimeline_ = true;
                    }
                }
            }
          continue;
        }

      processEvent (*event, results);
      lastTimelineEndUs_ = std::max (lastTimelineEndUs_, event->end);
      hasTimeline_ = true;
    }
  return results;
}

std::vector<SstvVisDetection>
SstvVisDetector::consumeClassified (
    std::vector<SstvVisClassifiedEvent> const& events)
{
  return consumeClassified (events.data (), events.size ());
}

std::vector<SstvVisDetection> SstvVisDetector::tick (std::uint64_t nowUs)
{
  std::vector<SstvVisDetection> results;
  if (hasTimeline_ && nowUs < lastTimelineEndUs_)
    {
      if (frameActive ())
        emitFailure (results, SstvVisDetectionStatus::InvalidInput,
                     SstvVisDetectionCause::TimestampRegression,
                     lastTimelineEndUs_, "VIS tick moved backwards", true);
      else
        {
          saturatingIncrement (metrics_.invalidInputs);
          results.push_back (makeResult (
              SstvVisDetectionStatus::InvalidInput,
              SstvVisDetectionCause::TimestampRegression,
              lastTimelineEndUs_, "VIS tick moved backwards", false));
        }
      return results;
    }

  lastTimelineEndUs_ = nowUs;
  hasTimeline_ = true;
  if (!frameActive ())
    return results;

  if (nowUs - frameStartedAtUs_ >= config_.maximumFrameDurationUs)
    {
      emitFailure (results, SstvVisDetectionStatus::TimedOut,
                   SstvVisDetectionCause::FrameDurationExceeded, nowUs,
                   "VIS frame duration exceeded", true);
      return results;
    }

  std::uint64_t deadline = frameStartedAtUs_;
  auto headerDeadline = [&] (std::uint64_t startedAt,
                             std::uint64_t nominal) {
    double const tolerated = std::ceil (
        static_cast<double> (nominal)
        * (1.0 + config_.headerDurationTolerance));
    if (!std::isfinite (tolerated) || tolerated < 0.0
        || tolerated
               > static_cast<double> (
                   std::numeric_limits<std::uint64_t>::max ()))
      return false;
    std::uint64_t afterTone = 0;
    return checkedAdd (startedAt,
                       static_cast<std::uint64_t> (tolerated),
                       afterTone)
           && checkedAdd (afterTone, config_.maximumGapUs, deadline);
  };
  bool deadlineValid = true;
  switch (state_)
    {
    case SstvVisDetectorState::FirstLeader:
    case SstvVisDetectorState::SecondLeader:
      deadlineValid = headerDeadline (run_.start,
                                      config_.leaderDurationUs);
      break;
    case SstvVisDetectorState::Break:
      deadlineValid = headerDeadline (run_.start,
                                      config_.breakDurationUs);
      break;
    case SstvVisDetectorState::StartSeparator:
      deadlineValid = headerDeadline (run_.start,
                                      config_.symbolDurationUs);
      break;
    case SstvVisDetectorState::ReadingBits:
    case SstvVisDetectorState::AwaitingStop:
      {
        std::uint64_t slotEnd = 0;
        deadlineValid = slotBoundary (slotIndex_ + 1, slotEnd)
                        && checkedAdd (slotEnd,
                                       config_.maximumGapUs,
                                       deadline);
      }
      break;
    default: return results;
    }

  if (!deadlineValid)
    {
      emitFailure (results, SstvVisDetectionStatus::InvalidInput,
                   SstvVisDetectionCause::CounterOverflow, nowUs,
                   "VIS phase deadline overflowed", true);
      return results;
    }

  if (nowUs >= deadline)
    emitFailure (results, SstvVisDetectionStatus::TimedOut,
                 SstvVisDetectionCause::Timeout, nowUs,
                 "VIS phase timed out", true);
  return results;
}

std::optional<SstvVisDetection>
SstvVisDetector::finish (std::uint64_t nowUs)
{
  if (!frameActive ())
    return std::nullopt;
  if (hasTimeline_ && nowUs < lastTimelineEndUs_)
    {
      SstvVisDetection result = makeResult (
          SstvVisDetectionStatus::InvalidInput,
          SstvVisDetectionCause::TimestampRegression,
          lastTimelineEndUs_, "VIS finish timestamp moved backwards", true);
      saturatingIncrement (metrics_.invalidInputs);
      saturatingIncrement (metrics_.framesRejected);
      resetFrame (true);
      return result;
    }
  if (nowUs - frameStartedAtUs_ >= config_.maximumFrameDurationUs)
    {
      SstvVisDetection result = makeResult (
          SstvVisDetectionStatus::TimedOut,
          SstvVisDetectionCause::FrameDurationExceeded,
          nowUs,
          "VIS frame duration exceeded",
          true);
      saturatingIncrement (metrics_.timeouts);
      saturatingIncrement (metrics_.framesTruncated);
      resetFrame (true);
      return result;
    }
  SstvVisDetection result = makeResult (
      SstvVisDetectionStatus::Truncated,
      state_ == SstvVisDetectorState::AwaitingStop
          ? SstvVisDetectionCause::MissingStop
          : SstvVisDetectionCause::EndOfInput,
      nowUs, "VIS input ended inside a frame", true);
  lastTimelineEndUs_ = nowUs;
  hasTimeline_ = true;
  saturatingIncrement (metrics_.framesTruncated);
  resetFrame (true);
  return result;
}

std::optional<SstvVisDetection>
SstvVisDetector::cancel (std::uint64_t nowUs)
{
  std::uint64_t const endedAtUs =
      hasTimeline_ ? std::max (nowUs, lastTimelineEndUs_) : nowUs;
  SstvVisDetection result = makeResult (
      SstvVisDetectionStatus::Cancelled,
      SstvVisDetectionCause::Cancelled, endedAtUs,
      "VIS detection cancelled", frameActive ());
  state_ = SstvVisDetectorState::Cancelled;
  run_ = {};
  slot_ = {};
  rawEvents_.clear ();
  symbols_.clear ();
  frequencyOffsetHz_.reset ();
  metrics_.currentFrameId = 0;
  return result;
}

void SstvVisDetector::reset (bool clearMetrics) noexcept
{
  if (clearMetrics)
    metrics_ = {};
  state_ = SstvVisDetectorState::SearchingLeader;
  run_ = {};
  slot_ = {};
  rawEvents_.clear ();
  symbols_.clear ();
  frequencyOffsetHz_.reset ();
  clockPeriodUs_ = static_cast<double> (config_.symbolDurationUs);
  clockDriftPpm_ = 0.0;
  frameStartedAtUs_ = 0;
  symbolEpochUs_ = 0;
  lastTimelineEndUs_ = 0;
  slotIndex_ = 0;
  targetRawBitCount_ = SstvVisCodec::StandardRawBitCount;
  minimumHeaderConfidence_ = 1.0;
  acceptedHeaderPhases_ = 0;
  hasTimeline_ = false;
  metrics_.currentFrameId = 0;
  if (clearMetrics)
    {
      nextRawSequence_ = 0;
      nextFrameId_ = 0;
    }
}

SstvVisDetectorState SstvVisDetector::state () const noexcept
{
  return state_;
}

SstvVisDetectorConfig const& SstvVisDetector::config () const noexcept
{
  return config_;
}

SstvVisDetectorMetrics const& SstvVisDetector::metrics () const noexcept
{
  return metrics_;
}

bool SstvVisDetector::frameActive () const noexcept
{
  return state_ != SstvVisDetectorState::SearchingLeader
         && state_ != SstvVisDetectorState::Cancelled;
}

std::size_t SstvVisDetector::bufferedRawEventCount () const noexcept
{
  return rawEvents_.size ();
}

std::size_t SstvVisDetector::bufferedSymbolCount () const noexcept
{
  return symbols_.size ();
}

char const* SstvVisDetector::stateName (SstvVisDetectorState state) noexcept
{
  switch (state)
    {
    case SstvVisDetectorState::SearchingLeader: return "SearchingLeader";
    case SstvVisDetectorState::FirstLeader: return "FirstLeader";
    case SstvVisDetectorState::Break: return "Break";
    case SstvVisDetectorState::SecondLeader: return "SecondLeader";
    case SstvVisDetectorState::StartSeparator: return "StartSeparator";
    case SstvVisDetectorState::ReadingBits: return "ReadingBits";
    case SstvVisDetectorState::AwaitingStop: return "AwaitingStop";
    case SstvVisDetectorState::Cancelled: return "Cancelled";
    }
  return "Unknown";
}

char const* SstvVisDetector::causeName (SstvVisDetectionCause cause) noexcept
{
  switch (cause)
    {
    case SstvVisDetectionCause::None: return "none";
    case SstvVisDetectionCause::LeaderTiming: return "leader-timing";
    case SstvVisDetectionCause::BreakTiming: return "break-timing";
    case SstvVisDetectionCause::SecondLeaderTiming:
      return "second-leader-timing";
    case SstvVisDetectionCause::StartTiming: return "start-timing";
    case SstvVisDetectionCause::UnexpectedTone: return "unexpected-tone";
    case SstvVisDetectionCause::LowConfidence: return "low-confidence";
    case SstvVisDetectionCause::SlotCoverage: return "slot-coverage";
    case SstvVisDetectionCause::SymbolAmbiguous: return "symbol-ambiguous";
    case SstvVisDetectionCause::EarlyStop: return "early-stop";
    case SstvVisDetectionCause::MissingStop: return "missing-stop";
    case SstvVisDetectionCause::ParityMismatch: return "parity-mismatch";
    case SstvVisDetectionCause::InvalidExtendedMarker:
      return "invalid-extended-marker";
    case SstvVisDetectionCause::CodecRejected: return "codec-rejected";
    case SstvVisDetectionCause::EndOfInput: return "end-of-input";
    case SstvVisDetectionCause::Timeout: return "timeout";
    case SstvVisDetectionCause::Cancelled: return "cancelled";
    case SstvVisDetectionCause::TimestampRegression:
      return "timestamp-regression";
    case SstvVisDetectionCause::GapExceeded: return "gap-exceeded";
    case SstvVisDetectionCause::InvalidObservation:
      return "invalid-observation";
    case SstvVisDetectionCause::FrequencyOffsetOutOfRange:
      return "frequency-offset-out-of-range";
    case SstvVisDetectionCause::FrameDurationExceeded:
      return "frame-duration-exceeded";
    case SstvVisDetectionCause::RawEventLimit: return "raw-event-limit";
    case SstvVisDetectionCause::CounterOverflow: return "counter-overflow";
    }
  return "unknown";
}

std::optional<SstvVisDetector::NormalizedEvent>
SstvVisDetector::normalize (SstvVisToneEvent const& event,
                            SstvVisDetection& invalidResult)
{
  if (event.durationUs == 0
      || event.durationUs > config_.maximumEventDurationUs
      || addWouldOverflow (event.startTimeUs, event.durationUs)
      || !std::isfinite (event.frequencyHz) || event.frequencyHz <= 0.0
      || event.frequencyHz > 100'000.0
      || !std::isfinite (event.confidence) || event.confidence < 0.0
      || event.confidence > 1.0)
    {
      invalidResult = makeResult (
          SstvVisDetectionStatus::InvalidInput,
          SstvVisDetectionCause::InvalidObservation, event.startTimeUs,
          "invalid VIS tone observation", false);
      return std::nullopt;
    }

  NormalizedEvent normalized;
  normalized.start = event.startTimeUs;
  normalized.end = event.startTimeUs + event.durationUs;
  normalized.frequency = event.frequencyHz;
  normalized.confidence = event.confidence;

  if (hasTimeline_ && normalized.start < lastTimelineEndUs_)
    {
      std::uint64_t const overlap = lastTimelineEndUs_ - normalized.start;
      if (overlap > config_.maximumOverlapUs)
        {
          invalidResult = makeResult (
              SstvVisDetectionStatus::InvalidInput,
              SstvVisDetectionCause::TimestampRegression,
              event.startTimeUs, "VIS event timestamp regressed", false);
          return std::nullopt;
        }
      normalized.start = lastTimelineEndUs_;
      if (normalized.end <= normalized.start)
        return std::nullopt;
    }
  if (frameActive () && hasTimeline_
      && normalized.start > lastTimelineEndUs_
      && normalized.start - lastTimelineEndUs_ > config_.maximumGapUs)
    {
      invalidResult = makeResult (
          SstvVisDetectionStatus::InvalidInput,
          SstvVisDetectionCause::GapExceeded,
          normalized.start,
          "VIS event gap exceeds the configured bound",
          false);
      return std::nullopt;
    }

  if (!frequencyOffsetHz_)
    {
      double const candidateOffset = event.frequencyHz - kLeaderHz;
      if (event.confidence >= config_.minimumObservationConfidence
          && std::abs (candidateOffset)
                 <= config_.maximumFrequencyOffsetHz)
        {
          normalized.tone = SstvVisToneKind::Leader;
          normalized.offset = candidateOffset;
        }
      else
        {
          normalized.tone = SstvVisToneKind::Unknown;
        }
    }
  else
    {
      normalized.tone = classifyFrequency (event.frequencyHz,
                                           event.confidence);
      normalized.offset = normalized.tone == SstvVisToneKind::Unknown
                              ? 0.0
                              : event.frequencyHz
                                    - nominalFrequency (normalized.tone);
    }
  return normalized;
}

std::optional<SstvVisDetector::NormalizedEvent>
SstvVisDetector::normalize (SstvVisClassifiedEvent const& event,
                            SstvVisDetection& invalidResult)
{
  bool const validTone = event.tone == SstvVisToneKind::Unknown
                         || event.tone == SstvVisToneKind::Leader
                         || event.tone == SstvVisToneKind::Separator
                         || event.tone == SstvVisToneKind::BitOne
                         || event.tone == SstvVisToneKind::BitZero;
  if (event.durationUs == 0
      || event.durationUs > config_.maximumEventDurationUs
      || addWouldOverflow (event.startTimeUs, event.durationUs)
      || !validTone || !std::isfinite (event.frequencyOffsetHz)
      || !std::isfinite (event.confidence) || event.confidence < 0.0
      || event.confidence > 1.0)
    {
      invalidResult = makeResult (
          SstvVisDetectionStatus::InvalidInput,
          SstvVisDetectionCause::InvalidObservation, event.startTimeUs,
          "invalid classified VIS observation", false);
      return std::nullopt;
    }
  if (event.tone != SstvVisToneKind::Unknown
      && std::abs (event.frequencyOffsetHz)
             > config_.maximumFrequencyOffsetHz)
    {
      invalidResult = makeResult (
          SstvVisDetectionStatus::InvalidInput,
          SstvVisDetectionCause::FrequencyOffsetOutOfRange,
          event.startTimeUs, "classified VIS offset exceeds acquisition bound",
          false);
      return std::nullopt;
    }

  NormalizedEvent normalized;
  normalized.start = event.startTimeUs;
  normalized.end = event.startTimeUs + event.durationUs;
  normalized.tone = event.confidence >= config_.minimumObservationConfidence
                        ? event.tone
                        : SstvVisToneKind::Unknown;
  normalized.offset = event.frequencyOffsetHz;
  normalized.frequency = normalized.tone == SstvVisToneKind::Unknown
                             ? 0.0
                             : nominalFrequency (normalized.tone)
                                   + event.frequencyOffsetHz;
  normalized.confidence = event.confidence;

  if (hasTimeline_ && normalized.start < lastTimelineEndUs_)
    {
      std::uint64_t const overlap = lastTimelineEndUs_ - normalized.start;
      if (overlap > config_.maximumOverlapUs)
        {
          invalidResult = makeResult (
              SstvVisDetectionStatus::InvalidInput,
              SstvVisDetectionCause::TimestampRegression,
              event.startTimeUs, "classified VIS timestamp regressed", false);
          return std::nullopt;
        }
      normalized.start = lastTimelineEndUs_;
      if (normalized.end <= normalized.start)
        return std::nullopt;
    }
  if (frameActive () && hasTimeline_
      && normalized.start > lastTimelineEndUs_
      && normalized.start - lastTimelineEndUs_ > config_.maximumGapUs)
    {
      invalidResult = makeResult (
          SstvVisDetectionStatus::InvalidInput,
          SstvVisDetectionCause::GapExceeded,
          normalized.start,
          "classified VIS event gap exceeds the configured bound",
          false);
      return std::nullopt;
    }
  return normalized;
}

void SstvVisDetector::processEvent (
    NormalizedEvent const& event, std::vector<SstvVisDetection>& results)
{
  if (state_ == SstvVisDetectorState::Cancelled)
    return;

  if (state_ == SstvVisDetectorState::SearchingLeader)
    {
      if (event.tone != SstvVisToneKind::Leader)
        {
          saturatingIncrement (metrics_.noiseEventsDiscarded);
          return;
        }
      if (event.start
          > std::numeric_limits<std::uint64_t>::max ()
                - config_.maximumFrameDurationUs)
        {
          saturatingIncrement (metrics_.invalidInputs);
          results.push_back (makeResult (
              SstvVisDetectionStatus::InvalidInput,
              SstvVisDetectionCause::InvalidObservation, event.start,
              "VIS frame timestamp leaves no bounded horizon", false));
          return;
        }
      beginFrame (event);
      if (!appendRawEvent (event, results))
        return;
      startRun (event);
      return;
    }

  if (event.end - frameStartedAtUs_ > config_.maximumFrameDurationUs)
    {
      emitFailure (results, SstvVisDetectionStatus::TimedOut,
                   SstvVisDetectionCause::FrameDurationExceeded, event.start,
                   "VIS frame duration exceeded", true);
      if (event.tone == SstvVisToneKind::Leader)
        processEvent (event, results);
      return;
    }

  if (state_ == SstvVisDetectorState::ReadingBits
      || state_ == SstvVisDetectorState::AwaitingStop)
    {
      if (appendRawEvent (event, results))
        processDataInterval (event, results);
    }
  else
    processHeaderEvent (event, results);
}

void SstvVisDetector::processHeaderEvent (
    NormalizedEvent const& event,
    std::vector<SstvVisDetection>& results)
{
  if (run_.active && event.tone == run_.tone
      && event.start <= run_.end + config_.maximumGapUs)
    {
      if (!appendRawEvent (event, results))
        return;
      extendRun (event);
      std::uint64_t nominal = config_.symbolDurationUs;
      if (state_ == SstvVisDetectorState::FirstLeader
          || state_ == SstvVisDetectorState::SecondLeader)
        nominal = config_.leaderDurationUs;
      else if (state_ == SstvVisDetectorState::Break)
        nominal = config_.breakDurationUs;
      double const maximum = static_cast<double> (nominal)
                             * (1.0
                                + config_.headerDurationTolerance);
      if (static_cast<double> (run_.end - run_.start) > maximum)
        finalizeRun (event.end, results);
      return;
    }

  if (run_.active && !finalizeRun (event.start, results))
    {
      if (event.tone == SstvVisToneKind::Leader
          && state_ == SstvVisDetectorState::SearchingLeader)
        {
          beginFrame (event);
          if (!appendRawEvent (event, results))
            return;
          startRun (event);
        }
      return;
    }

  SstvVisToneKind expected = SstvVisToneKind::Unknown;
  switch (state_)
    {
    case SstvVisDetectorState::Break:
    case SstvVisDetectorState::StartSeparator:
      expected = SstvVisToneKind::Separator;
      break;
    case SstvVisDetectorState::SecondLeader:
      expected = SstvVisToneKind::Leader;
      break;
    case SstvVisDetectorState::ReadingBits:
      if (appendRawEvent (event, results))
        processDataInterval (event, results);
      return;
    default: break;
    }

  if (event.tone != expected)
    {
      if (!appendRawEvent (event, results))
        return;
      emitFailure (results, SstvVisDetectionStatus::Rejected,
                   event.confidence < config_.minimumObservationConfidence
                       ? SstvVisDetectionCause::LowConfidence
                       : SstvVisDetectionCause::UnexpectedTone,
                   event.start, "unexpected tone in VIS header", false);
      if (event.tone == SstvVisToneKind::Leader)
        {
          beginFrame (event);
          if (!appendRawEvent (event, results))
            return;
          startRun (event);
        }
      return;
    }
  if (!appendRawEvent (event, results))
    return;
  startRun (event);
}

void SstvVisDetector::processDataInterval (
    NormalizedEvent const& event,
    std::vector<SstvVisDetection>& results)
{
  if (event.end <= symbolEpochUs_)
    return;
  std::uint64_t cursor = std::max (event.start, symbolEpochUs_);
  std::size_t slotsProcessed = 0;

  while (cursor < event.end && frameActive ()
         && (state_ == SstvVisDetectorState::ReadingBits
             || state_ == SstvVisDetectorState::AwaitingStop))
    {
      if (++slotsProcessed > MaximumSlotsPerEvent)
        {
          emitFailure (results, SstvVisDetectionStatus::BoundsExceeded,
                       SstvVisDetectionCause::RawEventLimit, cursor,
                       "VIS event crossed too many symbol slots", true);
          return;
        }

      std::uint64_t slotStart = 0;
      std::uint64_t slotEnd = 0;
      if (!slotBoundary (slotIndex_, slotStart)
          || !slotBoundary (slotIndex_ + 1, slotEnd))
        {
          emitFailure (results, SstvVisDetectionStatus::InvalidInput,
                       SstvVisDetectionCause::CounterOverflow, cursor,
                       "VIS symbol boundary overflowed", true);
          return;
        }
      if (cursor >= slotEnd)
        {
          if (!finalizeSlot (slotEnd, results))
            return;
          continue;
        }
      if (event.end <= slotStart)
        return;

      std::uint64_t const contributionStart = std::max (cursor, slotStart);
      std::uint64_t const contributionEnd = std::min (event.end, slotEnd);
      std::uint64_t const duration = contributionEnd - contributionStart;
      switch (event.tone)
        {
        case SstvVisToneKind::BitOne:
          slot_.oneUs += duration;
          slot_.oneConfidenceTime +=
              static_cast<long double> (duration) * event.confidence;
          break;
        case SstvVisToneKind::BitZero:
          slot_.zeroUs += duration;
          slot_.zeroConfidenceTime +=
              static_cast<long double> (duration) * event.confidence;
          break;
        case SstvVisToneKind::Separator:
          slot_.separatorUs += duration;
          slot_.separatorConfidenceTime +=
              static_cast<long double> (duration) * event.confidence;
          break;
        default: slot_.unknownUs += duration; break;
        }
      cursor = contributionEnd;
      if (contributionEnd >= slotEnd
          && !finalizeSlot (slotEnd, results))
        return;
    }
}

bool SstvVisDetector::finalizeRun (
    std::uint64_t atUs, std::vector<SstvVisDetection>& results)
{
  if (!run_.active)
    return true;
  std::uint64_t const span = run_.end - run_.start;
  double const coverage = span == 0
                              ? 0.0
                              : static_cast<double> (run_.coveredUs)
                                    / static_cast<double> (span);
  if (coverage < config_.minimumSlotCoverage
      || runConfidence () < config_.minimumObservationConfidence)
    {
      emitFailure (results, SstvVisDetectionStatus::Rejected,
                   SstvVisDetectionCause::LowConfidence, atUs,
                   "VIS header run has insufficient confidence", false);
      return false;
    }

  SstvVisDetectionCause timingCause = SstvVisDetectionCause::StartTiming;
  std::uint64_t nominal = config_.symbolDurationUs;
  switch (state_)
    {
    case SstvVisDetectorState::FirstLeader:
      timingCause = SstvVisDetectionCause::LeaderTiming;
      nominal = config_.leaderDurationUs;
      break;
    case SstvVisDetectorState::Break:
      timingCause = SstvVisDetectionCause::BreakTiming;
      nominal = config_.breakDurationUs;
      break;
    case SstvVisDetectorState::SecondLeader:
      timingCause = SstvVisDetectionCause::SecondLeaderTiming;
      nominal = config_.leaderDurationUs;
      break;
    case SstvVisDetectorState::StartSeparator:
      timingCause = SstvVisDetectionCause::StartTiming;
      nominal = config_.symbolDurationUs;
      break;
    default:
      emitFailure (results, SstvVisDetectionStatus::Rejected,
                   SstvVisDetectionCause::UnexpectedTone, atUs,
                   "VIS header state/run mismatch", false);
      return false;
    }

  if (!durationMatches (span, nominal))
    {
      emitFailure (results, SstvVisDetectionStatus::Rejected, timingCause,
                   atUs, "VIS header duration outside tolerance", false);
      return false;
    }

  double const phaseConfidence = clamp01 (runConfidence () * coverage);
  minimumHeaderConfidence_ = std::min (minimumHeaderConfidence_,
                                       phaseConfidence);
  ++acceptedHeaderPhases_;

  if (state_ == SstvVisDetectorState::FirstLeader)
    {
      double const measured = static_cast<double> (
          run_.frequencyTime / static_cast<long double> (run_.coveredUs));
      frequencyOffsetHz_ = measured - kLeaderHz;
      if (std::abs (*frequencyOffsetHz_)
          > config_.maximumFrequencyOffsetHz)
        {
          emitFailure (results, SstvVisDetectionStatus::Rejected,
                       SstvVisDetectionCause::FrequencyOffsetOutOfRange,
                       atUs, "VIS leader offset exceeds acquisition bound",
                       false);
          return false;
        }
      state_ = SstvVisDetectorState::Break;
    }
  else if (state_ == SstvVisDetectorState::Break)
    state_ = SstvVisDetectorState::SecondLeader;
  else if (state_ == SstvVisDetectorState::SecondLeader)
    {
      double const measured = static_cast<double> (
          run_.frequencyTime / static_cast<long double> (run_.coveredUs));
      double const secondOffset = measured - kLeaderHz;
      frequencyOffsetHz_ = 0.5 * frequencyOffsetHz_.value_or (secondOffset)
                           + 0.5 * secondOffset;
      if (std::abs (*frequencyOffsetHz_)
          > config_.maximumFrequencyOffsetHz)
        {
          emitFailure (results, SstvVisDetectionStatus::Rejected,
                       SstvVisDetectionCause::FrequencyOffsetOutOfRange,
                       atUs,
                       "VIS second leader offset exceeds acquisition bound",
                       false);
          return false;
        }
      state_ = SstvVisDetectorState::StartSeparator;
    }
  else
    {
      setClockFromStartRun ();
      if (std::abs (clockDriftPpm_) > config_.maximumClockDriftPpm)
        {
          emitFailure (results, SstvVisDetectionStatus::Rejected,
                       SstvVisDetectionCause::StartTiming, atUs,
                       "VIS symbol clock drift exceeds bound", false);
          return false;
        }
      symbolEpochUs_ = run_.end;
      state_ = SstvVisDetectorState::ReadingBits;
      slotIndex_ = 0;
      targetRawBitCount_ = SstvVisCodec::StandardRawBitCount;
      symbols_.clear ();
      symbols_.push_back (
          {SstvVisSymbol::Separator, phaseConfidence});
    }
  run_ = {};
  return true;
}

bool SstvVisDetector::finalizeSlot (
    std::uint64_t atUs, std::vector<SstvVisDetection>& results)
{
  std::uint64_t slotStart = 0;
  std::uint64_t slotEnd = 0;
  if (!slotBoundary (slotIndex_, slotStart)
      || !slotBoundary (slotIndex_ + 1, slotEnd)
      || slotEnd < slotStart)
    {
      emitFailure (results, SstvVisDetectionStatus::InvalidInput,
                   SstvVisDetectionCause::CounterOverflow, atUs,
                   "VIS symbol clock overflowed", true);
      return false;
    }
  std::uint64_t const slotDuration = slotEnd - slotStart;
  if (slotDuration == 0)
    {
      emitFailure (results, SstvVisDetectionStatus::InvalidInput,
                   SstvVisDetectionCause::CounterOverflow, atUs,
                   "VIS symbol clock produced an empty slot", true);
      return false;
    }

  if (state_ == SstvVisDetectorState::ReadingBits)
    {
      std::uint64_t const bitCoverage = slot_.oneUs + slot_.zeroUs;
      std::uint64_t const winner = std::max (slot_.oneUs, slot_.zeroUs);
      double const coverage = static_cast<double> (bitCoverage)
                              / static_cast<double> (slotDuration);
      double const dominance = bitCoverage == 0
                                   ? 0.0
                                   : static_cast<double> (winner)
                                         / static_cast<double> (bitCoverage);
      if (slot_.separatorUs
              >= static_cast<std::uint64_t> (
                  std::ceil (config_.minimumSlotCoverage
                             * static_cast<double> (slotDuration)))
          && bitCoverage < slot_.separatorUs)
        {
          emitFailure (results, SstvVisDetectionStatus::Truncated,
                       SstvVisDetectionCause::EarlyStop, atUs,
                       "VIS stop arrived before all required bits", true);
          return false;
        }
      if (coverage < config_.minimumSlotCoverage)
        {
          emitFailure (results, SstvVisDetectionStatus::Rejected,
                       SstvVisDetectionCause::SlotCoverage, atUs,
                       "VIS bit slot coverage is insufficient", true);
          return false;
        }
      if (dominance < config_.minimumBitDominance
          || slot_.oneUs == slot_.zeroUs)
        {
          emitFailure (results, SstvVisDetectionStatus::Rejected,
                       SstvVisDetectionCause::SymbolAmbiguous, atUs,
                       "VIS bit slot is ambiguous", true);
          return false;
        }

      bool const one = slot_.oneUs > slot_.zeroUs;
      std::uint64_t const confidenceDuration = one ? slot_.oneUs : slot_.zeroUs;
      long double const confidenceTime = one ? slot_.oneConfidenceTime
                                             : slot_.zeroConfidenceTime;
      double const observedConfidence = confidenceDuration == 0
                                            ? 0.0
                                            : static_cast<double> (
                                                  confidenceTime
                                                  / static_cast<long double> (
                                                      confidenceDuration));
      double const confidence = clamp01 (observedConfidence
                                         * coverage
                                         * dominance);
      if (symbols_.size () >= MaximumSymbolsPerFrame)
        {
          emitFailure (results, SstvVisDetectionStatus::BoundsExceeded,
                       SstvVisDetectionCause::RawEventLimit, atUs,
                       "VIS symbol buffer limit reached", true);
          return false;
        }
      symbols_.push_back ({one ? SstvVisSymbol::One : SstvVisSymbol::Zero,
                           confidence});
      ++slotIndex_;
      slot_ = {};

      std::size_t const rawBitCount = symbols_.size () - 1;
      if (rawBitCount == SstvVisCodec::StandardRawBitCount)
        {
          SstvVisDecodeResult const probe =
              SstvVisCodec::decodeFrame (symbols_);
          bool const wideExtended =
              probe.format == SstvVisFormat::Extended
              && probe.primary.rawOctetKnown && probe.primary.parityValid
              && probe.primary.rawOctet
                     == SstvVisCodec::ExtendedMarkerRawOctet;
          targetRawBitCount_ = wideExtended
                                   ? SstvVisCodec::ExtendedRawBitCount
                                   : SstvVisCodec::StandardRawBitCount;
        }
      if (rawBitCount == targetRawBitCount_)
        state_ = SstvVisDetectorState::AwaitingStop;
      return true;
    }

  if (state_ != SstvVisDetectorState::AwaitingStop)
    return false;

  double const coverage = static_cast<double> (slot_.separatorUs)
                          / static_cast<double> (slotDuration);
  if (coverage < config_.minimumSlotCoverage)
    {
      emitFailure (results, SstvVisDetectionStatus::Rejected,
                   SstvVisDetectionCause::MissingStop, atUs,
                   "VIS stop separator missing", true);
      return false;
    }
  double const observedConfidence = slot_.separatorUs == 0
                                        ? 0.0
                                        : static_cast<double> (
                                              slot_.separatorConfidenceTime
                                              / static_cast<long double> (
                                                  slot_.separatorUs));
  double const confidence = clamp01 (observedConfidence * coverage);
  if (symbols_.size () >= MaximumSymbolsPerFrame)
    {
      emitFailure (results, SstvVisDetectionStatus::BoundsExceeded,
                   SstvVisDetectionCause::RawEventLimit, atUs,
                   "VIS symbol buffer limit reached", true);
      return false;
    }
  symbols_.push_back ({SstvVisSymbol::Separator, confidence});
  ++slotIndex_;
  slot_ = {};
  completeFrame (atUs, results);
  return false;
}

void SstvVisDetector::startRun (NormalizedEvent const& event) noexcept
{
  run_ = {};
  run_.active = true;
  run_.tone = event.tone;
  run_.start = event.start;
  run_.end = event.end;
  run_.coveredUs = event.end - event.start;
  run_.confidenceTime =
      static_cast<long double> (run_.coveredUs) * event.confidence;
  run_.frequencyTime =
      static_cast<long double> (run_.coveredUs) * event.frequency;
}

void SstvVisDetector::extendRun (NormalizedEvent const& event) noexcept
{
  std::uint64_t const contributionStart = std::max (run_.end, event.start);
  std::uint64_t const contribution = event.end > contributionStart
                                         ? event.end - contributionStart
                                         : 0;
  run_.end = std::max (run_.end, event.end);
  run_.coveredUs += contribution;
  run_.confidenceTime +=
      static_cast<long double> (contribution) * event.confidence;
  run_.frequencyTime +=
      static_cast<long double> (contribution) * event.frequency;
}

void SstvVisDetector::beginFrame (NormalizedEvent const& event)
{
  resetFrame (false);
  state_ = SstvVisDetectorState::FirstLeader;
  frameStartedAtUs_ = event.start;
  frequencyOffsetHz_ = event.offset;
  ++nextFrameId_;
  if (nextFrameId_ == 0)
    ++nextFrameId_;
  metrics_.currentFrameId = nextFrameId_;
  saturatingIncrement (metrics_.framesStarted);
}

bool SstvVisDetector::appendRawEvent (
    NormalizedEvent const& event, std::vector<SstvVisDetection>& results)
{
  if (rawEvents_.size () >= MaximumRawEventsPerFrame)
    {
      emitFailure (results, SstvVisDetectionStatus::BoundsExceeded,
                   SstvVisDetectionCause::RawEventLimit, event.start,
                   "VIS raw event buffer limit reached", true);
      return false;
    }
  rawEvents_.push_back ({nextRawSequence_++,
                         event.start,
                         event.end - event.start,
                         event.frequency,
                         event.offset,
                         event.confidence,
                         event.tone});
  return true;
}

SstvVisToneKind SstvVisDetector::classifyFrequency (
    double frequencyHz, double confidence) const noexcept
{
  if (confidence < config_.minimumObservationConfidence
      || !frequencyOffsetHz_)
    return SstvVisToneKind::Unknown;

  std::array<SstvVisToneKind, 4> const tones {{
      SstvVisToneKind::Leader,
      SstvVisToneKind::Separator,
      SstvVisToneKind::BitOne,
      SstvVisToneKind::BitZero
  }};
  double bestDistance = std::numeric_limits<double>::infinity ();
  double secondDistance = std::numeric_limits<double>::infinity ();
  SstvVisToneKind best = SstvVisToneKind::Unknown;
  for (auto const tone : tones)
    {
      double const expected = nominalFrequency (tone) + *frequencyOffsetHz_;
      double const distance = std::abs (frequencyHz - expected);
      if (distance < bestDistance)
        {
          secondDistance = bestDistance;
          bestDistance = distance;
          best = tone;
        }
      else if (distance < secondDistance)
        secondDistance = distance;
    }
  if (bestDistance > config_.frequencyToleranceHz
      || std::abs (bestDistance - secondDistance) < 1.0e-9)
    return SstvVisToneKind::Unknown;
  return best;
}

double SstvVisDetector::nominalFrequency (SstvVisToneKind tone) const noexcept
{
  switch (tone)
    {
    case SstvVisToneKind::Leader: return kLeaderHz;
    case SstvVisToneKind::Separator: return kSeparatorHz;
    case SstvVisToneKind::BitOne: return kOneHz;
    case SstvVisToneKind::BitZero: return kZeroHz;
    case SstvVisToneKind::Unknown: return 0.0;
    }
  return 0.0;
}

bool SstvVisDetector::slotBoundary (std::size_t slot,
                                    std::uint64_t& boundary) const noexcept
{
  if (slot > MaximumSymbolsPerFrame)
    return false;
  double const relative = std::round (static_cast<double> (slot)
                                      * clockPeriodUs_);
  if (!std::isfinite (relative) || relative < 0.0
      || relative
             > static_cast<double> (
                 std::numeric_limits<std::uint64_t>::max ()))
    return false;
  auto const delta = static_cast<std::uint64_t> (relative);
  return checkedAdd (symbolEpochUs_, delta, boundary);
}

bool SstvVisDetector::durationMatches (std::uint64_t actual,
                                       std::uint64_t nominal) const noexcept
{
  double const minimum = static_cast<double> (nominal)
                         * (1.0 - config_.headerDurationTolerance);
  double const maximum = static_cast<double> (nominal)
                         * (1.0 + config_.headerDurationTolerance);
  return static_cast<double> (actual) >= minimum
         && static_cast<double> (actual) <= maximum;
}

double SstvVisDetector::runConfidence () const noexcept
{
  if (run_.coveredUs == 0)
    return 0.0;
  return clamp01 (static_cast<double> (
      run_.confidenceTime / static_cast<long double> (run_.coveredUs)));
}

double SstvVisDetector::frameConfidence () const noexcept
{
  if (!symbols_.empty ())
    {
      double sum = 0.0;
      for (auto const& symbol : symbols_)
        sum += clamp01 (symbol.confidence);
      double const symbolConfidence =
          sum / static_cast<double> (symbols_.size ());
      return acceptedHeaderPhases_ == 0
                 ? symbolConfidence
                 : std::min (minimumHeaderConfidence_, symbolConfidence);
    }
  if (rawEvents_.empty ())
    return 0.0;
  long double confidenceTime = 0.0L;
  std::uint64_t duration = 0;
  for (auto const& event : rawEvents_)
    {
      confidenceTime += static_cast<long double> (event.durationUs)
                        * event.confidence;
      duration += event.durationUs;
    }
  return duration == 0
             ? 0.0
             : clamp01 (static_cast<double> (
                   confidenceTime / static_cast<long double> (duration)));
}

void SstvVisDetector::setClockFromStartRun ()
{
  clockPeriodUs_ = static_cast<double> (run_.end - run_.start);
  clockDriftPpm_ =
      (clockPeriodUs_ / static_cast<double> (config_.symbolDurationUs) - 1.0)
      * 1'000'000.0;
}

void SstvVisDetector::resetFrame (bool automatic) noexcept
{
  if (automatic)
    saturatingIncrement (metrics_.automaticResets);
  state_ = SstvVisDetectorState::SearchingLeader;
  run_ = {};
  slot_ = {};
  rawEvents_.clear ();
  symbols_.clear ();
  frequencyOffsetHz_.reset ();
  clockPeriodUs_ = static_cast<double> (config_.symbolDurationUs);
  clockDriftPpm_ = 0.0;
  frameStartedAtUs_ = 0;
  symbolEpochUs_ = 0;
  slotIndex_ = 0;
  targetRawBitCount_ = SstvVisCodec::StandardRawBitCount;
  minimumHeaderConfidence_ = 1.0;
  acceptedHeaderPhases_ = 0;
  metrics_.currentFrameId = 0;
}

SstvVisDetection SstvVisDetector::makeResult (
    SstvVisDetectionStatus status,
    SstvVisDetectionCause cause,
    std::uint64_t endedAtUs,
    char const* detail,
    bool includeCodecResult) const
{
  SstvVisDetection result;
  result.status = status;
  result.cause = cause;
  result.rawEvents = rawEvents_;
  result.symbolObservations = symbols_;
  result.frameStartedAtUs = frameStartedAtUs_;
  result.frameEndedAtUs = endedAtUs;
  result.estimatedFrequencyOffsetHz = frequencyOffsetHz_.value_or (0.0);
  result.estimatedClockDriftPpm = clockDriftPpm_;
  result.confidence = frameConfidence ();
  result.detail = detail ? detail : "";
  if (includeCodecResult && !symbols_.empty ())
    result.codecResult = SstvVisCodec::decodeFrame (symbols_);
  return result;
}

void SstvVisDetector::emitFailure (
    std::vector<SstvVisDetection>& results,
    SstvVisDetectionStatus status,
    SstvVisDetectionCause cause,
    std::uint64_t endedAtUs,
    char const* detail,
    bool includeCodecResult)
{
  results.push_back (makeResult (status, cause, endedAtUs, detail,
                                 includeCodecResult));
  switch (status)
    {
    case SstvVisDetectionStatus::Truncated:
      saturatingIncrement (metrics_.framesTruncated);
      break;
    case SstvVisDetectionStatus::TimedOut:
      saturatingIncrement (metrics_.timeouts);
      saturatingIncrement (metrics_.framesTruncated);
      break;
    case SstvVisDetectionStatus::InvalidInput:
      saturatingIncrement (metrics_.invalidInputs);
      saturatingIncrement (metrics_.framesRejected);
      break;
    default: saturatingIncrement (metrics_.framesRejected); break;
    }
  resetFrame (true);
}

void SstvVisDetector::completeFrame (
    std::uint64_t endedAtUs, std::vector<SstvVisDetection>& results)
{
  SstvVisDecodeResult const decoded = SstvVisCodec::decodeFrame (symbols_);
  SstvVisDetectionCause cause = SstvVisDetectionCause::None;
  if (containsError (decoded, SstvVisError::ParityMismatch))
    cause = SstvVisDetectionCause::ParityMismatch;
  else if (containsError (decoded, SstvVisError::InvalidExtendedMarker))
    cause = SstvVisDetectionCause::InvalidExtendedMarker;
  else if (!decoded.valid)
    cause = SstvVisDetectionCause::CodecRejected;
  else if (decoded.confidence < config_.minimumFrameConfidence
           || frameConfidence () < config_.minimumFrameConfidence)
    cause = SstvVisDetectionCause::LowConfidence;

  SstvVisDetection result = makeResult (
      cause == SstvVisDetectionCause::None
          ? SstvVisDetectionStatus::Decoded
          : SstvVisDetectionStatus::Rejected,
      cause, endedAtUs,
      cause == SstvVisDetectionCause::None ? "VIS frame decoded"
                                           : "VIS codec rejected frame",
      true);
  result.codecResult = decoded;
  results.push_back (std::move (result));
  if (cause == SstvVisDetectionCause::None)
    saturatingIncrement (metrics_.framesDecoded);
  else
    saturatingIncrement (metrics_.framesRejected);
  resetFrame (true);
}

bool SstvVisDetector::preflightConsume (std::size_t count) const
{
  if (count > MaximumEventsPerConsume)
    throw std::length_error ("VIS event chunk exceeds public bound");
  std::uint64_t const count64 = static_cast<std::uint64_t> (count);
  std::uint64_t const maximumRawSequenceAdvance = 2 * count64;
  if (addWouldOverflow (metrics_.observationsConsumed, count64)
      || addWouldOverflow (metrics_.framesStarted, count64)
      || addWouldOverflow (metrics_.framesDecoded, count64)
      || addWouldOverflow (metrics_.framesRejected, count64)
      || addWouldOverflow (metrics_.framesTruncated, count64)
      || addWouldOverflow (metrics_.timeouts, count64)
      || addWouldOverflow (metrics_.invalidInputs, count64)
      || addWouldOverflow (metrics_.noiseEventsDiscarded, count64)
      || addWouldOverflow (metrics_.automaticResets, count64)
      || addWouldOverflow (nextRawSequence_, maximumRawSequenceAdvance)
      || addWouldOverflow (nextFrameId_, count64))
    throw std::overflow_error ("VIS detector counter exhausted");
  return true;
}

void SstvVisDetector::validateConfig (SstvVisDetectorConfig const& config)
{
  if (config.leaderDurationUs == 0 || config.breakDurationUs == 0
      || config.symbolDurationUs == 0
      || config.leaderDurationUs > 1'000'000
      || config.breakDurationUs > 100'000
      || config.symbolDurationUs > 100'000)
    throw std::invalid_argument ("invalid VIS protocol durations");
  if (!std::isfinite (config.headerDurationTolerance)
      || config.headerDurationTolerance < 0.0
      || config.headerDurationTolerance > 0.5
      || !std::isfinite (config.maximumClockDriftPpm)
      || config.maximumClockDriftPpm < 0.0
      || config.maximumClockDriftPpm > 200'000.0)
    throw std::invalid_argument ("invalid VIS timing tolerance");
  if (!std::isfinite (config.frequencyToleranceHz)
      || config.frequencyToleranceHz <= 0.0
      || config.frequencyToleranceHz >= 50.0
      || !std::isfinite (config.maximumFrequencyOffsetHz)
      || config.maximumFrequencyOffsetHz < 0.0
      || config.maximumFrequencyOffsetHz > 500.0)
    throw std::invalid_argument ("invalid VIS frequency tolerance");
  if (!std::isfinite (config.minimumObservationConfidence)
      || config.minimumObservationConfidence < 0.0
      || config.minimumObservationConfidence > 1.0
      || !std::isfinite (config.minimumFrameConfidence)
      || config.minimumFrameConfidence < 0.0
      || config.minimumFrameConfidence > 1.0
      || !std::isfinite (config.minimumSlotCoverage)
      || config.minimumSlotCoverage < 0.5
      || config.minimumSlotCoverage > 1.0
      || !std::isfinite (config.minimumBitDominance)
      || config.minimumBitDominance < 0.5
      || config.minimumBitDominance > 1.0)
    throw std::invalid_argument ("invalid VIS confidence policy");
  if (config.maximumGapUs > config.symbolDurationUs
      || config.maximumOverlapUs > config.symbolDurationUs
      || config.maximumEventDurationUs == 0
      || config.maximumEventDurationUs > 10'000'000
      || config.maximumFrameDurationUs == 0
      || config.maximumFrameDurationUs > 10'000'000)
    throw std::invalid_argument ("invalid VIS hostile-input bounds");

  std::uint64_t const minimumFrame =
      2 * config.leaderDurationUs + config.breakDurationUs
      + (2 + SstvVisCodec::ExtendedRawBitCount) * config.symbolDurationUs;
  if (config.maximumFrameDurationUs < minimumFrame
      || config.maximumEventDurationUs < config.leaderDurationUs)
    throw std::invalid_argument ("VIS frame bounds cannot hold wide VIS");
}

bool SstvVisDetector::addWouldOverflow (std::uint64_t value,
                                        std::uint64_t increment) noexcept
{
  return increment > std::numeric_limits<std::uint64_t>::max () - value;
}

bool SstvVisDetector::checkedAdd (std::uint64_t left,
                                  std::uint64_t right,
                                  std::uint64_t& result) noexcept
{
  if (addWouldOverflow (left, right))
    return false;
  result = left + right;
  return true;
}

void SstvVisDetector::saturatingIncrement (std::uint64_t& value) noexcept
{
  if (value != std::numeric_limits<std::uint64_t>::max ())
    ++value;
}

} // namespace decodium::sstv
