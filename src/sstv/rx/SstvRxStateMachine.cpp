// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvRxStateMachine.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace decodium::sstv
{
namespace
{

bool isAcquisitionState (SstvRxState state) noexcept
{
  switch (state)
    {
    case SstvRxState::LeaderCandidate:
    case SstvRxState::ReadingVis:
    case SstvRxState::ModeDetected:
    case SstvRxState::WaitingForSync:
    case SstvRxState::Receiving:
    case SstvRxState::RecoveringSync:
      return true;
    default:
      return false;
    }
}

std::uint64_t elapsed (std::uint64_t now, std::uint64_t then) noexcept
{
  return now >= then ? now - then : 0;
}

void saturatingAdd (std::uint32_t& destination,
                    std::uint32_t value) noexcept
{
  auto const maximum = std::numeric_limits<std::uint32_t>::max ();
  if (value > maximum - destination)
    destination = maximum;
  else
    destination += value;
}

} // namespace

SstvRxStateMachine::SstvRxStateMachine (SstvRxPolicy policy)
    : policy_ (std::move (policy))
{
  validatePolicy (policy_);
  metrics_.state = state_;
}

SstvRxTransition
SstvRxStateMachine::dispatch (std::uint64_t nowMs,
                              SstvRxEvent const& event)
{
  SstvRxState const before = state_;

  if (hasClock_ && nowMs < metrics_.lastEventAtMs)
    {
      metrics_.lastErrorCode = SstvRxErrorCode::ClockRegression;
      metrics_.lastErrorDetail = "non-monotonic receiver timestamp";
      if (activeSession_)
        finishSession (SstvRxState::Error, SstvRxCause::ClockRegression,
                       metrics_.lastEventAtMs);
      else
        transitionTo (SstvRxState::Error, SstvRxCause::ClockRegression,
                      metrics_.lastEventAtMs);
      ++metrics_.rejectedEventCount;
      return result (before, SstvRxCause::ClockRegression, false);
    }

  if (!hasClock_)
    {
      hasClock_ = true;
      metrics_.stateEnteredAtMs = nowMs;
    }
  metrics_.lastEventAtMs = nowMs;
  updateElapsed (nowMs);

  // Safety/lifecycle commands take precedence over an elapsed phase timeout.
  if (std::holds_alternative<SstvRxDisable> (event))
    {
      if (state_ == SstvRxState::Disabled)
        return reject (before, SstvRxCause::UnexpectedEvent);

      if (activeSession_)
        {
          ++metrics_.sessionsAborted;
          activeSession_ = false;
        }
      enabled_ = false;
      monitoring_ = false;
      clearSessionData ();
      transitionTo (SstvRxState::Disabled, SstvRxCause::Disabled, nowMs);
      return result (before, SstvRxCause::Disabled, true);
    }

  if (auto const* failure = std::get_if<SstvRxFailure> (&event))
    {
      metrics_.lastErrorCode = failure->code == SstvRxErrorCode::None
                                   ? SstvRxErrorCode::InternalFailure
                                   : failure->code;
      metrics_.lastErrorDetail = boundedDetail (failure->detail);
      if (activeSession_)
        finishSession (SstvRxState::Error, SstvRxCause::Failure, nowMs);
      else
        transitionTo (SstvRxState::Error, SstvRxCause::Failure, nowMs);
      return result (before, SstvRxCause::Failure, true);
    }

  if (std::holds_alternative<SstvRxCancel> (event))
    {
      if (!activeSession_ || !isAcquisitionState (state_))
        return reject (before, SstvRxCause::UnexpectedEvent);
      finishSession (SstvRxState::Aborted, SstvRxCause::Cancelled, nowMs);
      return result (before, SstvRxCause::Cancelled, true);
    }

  if (std::holds_alternative<SstvRxReset> (event))
    {
      if (activeSession_)
        {
          ++metrics_.sessionsAborted;
          activeSession_ = false;
        }
      metrics_.lastErrorCode = SstvRxErrorCode::None;
      metrics_.lastErrorDetail.clear ();
      resetToListeningState (nowMs, SstvRxCause::Reset);
      return result (before, SstvRxCause::Reset, true);
    }

  SstvRxTransition timeoutTransition;
  if (expireBeforeEvent (nowMs, timeoutTransition))
    {
      timeoutTransition.before = before;
      timeoutTransition.after = state_;
      timeoutTransition.stateChanged = before != state_;
      timeoutTransition.sessionId = metrics_.currentSessionId;
      return timeoutTransition;
    }

  bool automaticReset = false;
  bool const startsBackToBack =
      std::holds_alternative<SstvRxLeaderObserved> (event)
      || std::holds_alternative<SstvRxManualMode> (event);
  if (isTerminal (state_) && startsBackToBack)
    {
      resetToListeningState (nowMs, SstvRxCause::AutoReset);
      automaticReset = true;
    }

  if (std::holds_alternative<SstvRxEnable> (event))
    {
      if (state_ != SstvRxState::Disabled)
        return reject (before, SstvRxCause::UnexpectedEvent);
      enabled_ = true;
      monitoring_ = false;
      transitionTo (SstvRxState::Idle, SstvRxCause::Enabled, nowMs);
      return result (before, SstvRxCause::Enabled, true);
    }

  if (std::holds_alternative<SstvRxStartMonitoring> (event))
    {
      if (!enabled_ || activeSession_)
        return reject (before, SstvRxCause::UnexpectedEvent);
      monitoring_ = true;
      clearSessionData ();
      transitionTo (SstvRxState::SearchingLeader,
                    SstvRxCause::MonitoringStarted, nowMs);
      return result (before, SstvRxCause::MonitoringStarted, true);
    }

  if (std::holds_alternative<SstvRxStopMonitoring> (event))
    {
      if (!enabled_ || state_ == SstvRxState::Idle)
        return reject (before, SstvRxCause::UnexpectedEvent);
      if (activeSession_)
        {
          ++metrics_.sessionsAborted;
          activeSession_ = false;
        }
      monitoring_ = false;
      clearSessionData ();
      transitionTo (SstvRxState::Idle, SstvRxCause::MonitoringStopped, nowMs);
      return result (before, SstvRxCause::MonitoringStopped, true);
    }

  if (std::holds_alternative<SstvRxTick> (event))
    {
      if (isTerminal (state_)
          && metrics_.stateElapsedMs >= policy_.timeouts.terminalHoldMs)
        {
          resetToListeningState (nowMs, SstvRxCause::AutoReset);
          return result (before, SstvRxCause::AutoReset, true, false, true);
        }
      return result (before, SstvRxCause::None, true);
    }

  if (auto const* leader = std::get_if<SstvRxLeaderObserved> (&event))
    {
      if (state_ != SstvRxState::SearchingLeader)
        return reject (before, SstvRxCause::UnexpectedEvent);
      if (!validConfidence (leader->confidence)
          || !std::isfinite (leader->frequencyOffsetHz))
        return reject (before, SstvRxCause::InvalidInput);

      beginSession (nowMs);
      ++metrics_.leaderCandidates;
      metrics_.lastLeaderConfidence = leader->confidence;
      metrics_.lastLeaderOffsetHz = leader->frequencyOffsetHz;
      transitionTo (SstvRxState::LeaderCandidate,
                    SstvRxCause::LeaderCandidateObserved, nowMs);
      return result (before, SstvRxCause::LeaderCandidateObserved, true,
                     false, automaticReset);
    }

  if (std::holds_alternative<SstvRxLeaderConfirmed> (event))
    {
      if (state_ != SstvRxState::LeaderCandidate)
        return reject (before, SstvRxCause::UnexpectedEvent);
      ++metrics_.visAttempts;
      transitionTo (SstvRxState::ReadingVis, SstvRxCause::LeaderConfirmed,
                    nowMs);
      return result (before, SstvRxCause::LeaderConfirmed, true);
    }

  if (std::holds_alternative<SstvRxLeaderRejected> (event))
    {
      if (state_ != SstvRxState::LeaderCandidate)
        return reject (before, SstvRxCause::UnexpectedEvent);
      ++metrics_.falseLeaders;
      activeSession_ = false;
      transitionTo (SstvRxState::SearchingLeader,
                    SstvRxCause::LeaderRejected, nowMs);
      return result (before, SstvRxCause::LeaderRejected, true);
    }

  if (auto const* vis = std::get_if<SstvRxVisDecoded> (&event))
    {
      if (state_ != SstvRxState::ReadingVis)
        return reject (before, SstvRxCause::UnexpectedEvent);
      if (!validModeName (vis->mode) || !validConfidence (vis->confidence))
        return reject (before, SstvRxCause::InvalidInput);

      if (policy_.lockedMode && *policy_.lockedMode != vis->mode)
        {
          ++metrics_.invalidVisFrames;
          ++metrics_.modeLockMismatches;
          activeSession_ = false;
          transitionTo (SstvRxState::SearchingLeader,
                        SstvRxCause::ModeLockMismatch, nowMs);
          return result (before, SstvRxCause::ModeLockMismatch, true);
        }

      ++metrics_.validVisFrames;
      SstvRxModeSource const source = policy_.lockedMode
                                          ? SstvRxModeSource::VisWithModeLock
                                          : SstvRxModeSource::Vis;
      selectMode (vis->mode, source, vis->confidence, nowMs,
                  SstvRxCause::VisDecoded);
      return result (before, SstvRxCause::VisDecoded, true);
    }

  if (std::holds_alternative<SstvRxVisRejected> (event)
      || std::holds_alternative<SstvRxVisUnavailable> (event))
    {
      if (state_ != SstvRxState::ReadingVis)
        return reject (before, SstvRxCause::UnexpectedEvent);
      ++metrics_.invalidVisFrames;
      SstvRxCause const failureCause =
          std::holds_alternative<SstvRxVisRejected> (event)
              ? SstvRxCause::VisRejected
              : SstvRxCause::VisUnavailable;
      selectWithoutVis (nowMs, failureCause);
      return result (before, metrics_.lastCause, true);
    }

  if (auto const* manual = std::get_if<SstvRxManualMode> (&event))
    {
      if (!enabled_ || (state_ != SstvRxState::Idle
                        && state_ != SstvRxState::SearchingLeader))
        return reject (before, SstvRxCause::UnexpectedEvent);
      if (!validModeName (manual->mode))
        return reject (before, SstvRxCause::InvalidInput);
      monitoring_ = true;
      beginSession (nowMs);
      selectMode (manual->mode, SstvRxModeSource::ManualOverride, 0.0,
                  nowMs, SstvRxCause::ManualOverride);
      return result (before, SstvRxCause::ManualOverride, true, false,
                     automaticReset);
    }

  if (std::holds_alternative<SstvRxModeReady> (event))
    {
      if (state_ != SstvRxState::ModeDetected)
        return reject (before, SstvRxCause::UnexpectedEvent);
      transitionTo (SstvRxState::WaitingForSync, SstvRxCause::ModeReady,
                    nowMs);
      return result (before, SstvRxCause::ModeReady, true);
    }

  if (std::holds_alternative<SstvRxSyncObserved> (event))
    {
      if (state_ == SstvRxState::WaitingForSync)
        {
          metrics_.receptionStartedAtMs = nowMs;
          metrics_.consecutiveMissingSync = 0;
          metrics_.recoveryMissingSync = 0;
          transitionTo (SstvRxState::Receiving, SstvRxCause::SyncAcquired,
                        nowMs);
          return result (before, SstvRxCause::SyncAcquired, true);
        }
      if (state_ == SstvRxState::RecoveringSync)
        {
          ++metrics_.syncRecoveries;
          metrics_.consecutiveMissingSync = 0;
          metrics_.recoveryMissingSync = 0;
          transitionTo (SstvRxState::Receiving, SstvRxCause::SyncRecovered,
                        nowMs);
          return result (before, SstvRxCause::SyncRecovered, true);
        }
      return reject (before, SstvRxCause::UnexpectedEvent);
    }

  if (std::holds_alternative<SstvRxSyncLost> (event))
    {
      if (state_ != SstvRxState::Receiving)
        return reject (before, SstvRxCause::UnexpectedEvent);
      ++metrics_.syncLosses;
      metrics_.recoveryMissingSync = 0;
      transitionTo (SstvRxState::RecoveringSync, SstvRxCause::SyncLost,
                    nowMs);
      return result (before, SstvRxCause::SyncLost, true);
    }

  if (auto const* line = std::get_if<SstvRxLineObservation> (&event))
    {
      if (state_ != SstvRxState::Receiving
          && state_ != SstvRxState::RecoveringSync)
        return reject (before, SstvRxCause::UnexpectedEvent);

      if (line->syncObserved)
        {
          saturatingAdd (metrics_.decodedLines, line->decodedLines);
          metrics_.consecutiveMissingSync = 0;
          if (state_ == SstvRxState::RecoveringSync)
            {
              ++metrics_.syncRecoveries;
              metrics_.recoveryMissingSync = 0;
              transitionTo (SstvRxState::Receiving,
                            SstvRxCause::SyncRecovered, nowMs);
              return result (before, SstvRxCause::SyncRecovered, true);
            }
          metrics_.lastCause = SstvRxCause::LineReceived;
          return result (before, SstvRxCause::LineReceived, true);
        }

      ++metrics_.missingSyncLines;
      if (metrics_.consecutiveMissingSync
          < std::numeric_limits<std::uint32_t>::max ())
        ++metrics_.consecutiveMissingSync;

      if (state_ == SstvRxState::Receiving)
        {
          if (metrics_.consecutiveMissingSync
              >= policy_.missingSyncBeforeRecovery)
            {
              ++metrics_.syncLosses;
              metrics_.recoveryMissingSync = 0;
              transitionTo (SstvRxState::RecoveringSync,
                            SstvRxCause::SyncLost, nowMs);
              return result (before, SstvRxCause::SyncLost, true);
            }
          metrics_.lastCause = SstvRxCause::MissingSyncTolerated;
          return result (before, SstvRxCause::MissingSyncTolerated, true);
        }

      if (metrics_.recoveryMissingSync
          < std::numeric_limits<std::uint32_t>::max ())
        ++metrics_.recoveryMissingSync;
      if (metrics_.recoveryMissingSync
          >= policy_.maxMissingSyncDuringRecovery)
        {
          SstvRxState const outcome = metrics_.decodedLines > 0
                                          ? SstvRxState::Partial
                                          : SstvRxState::Aborted;
          finishSession (outcome, SstvRxCause::SyncRecoveryExhausted,
                         nowMs);
          return result (before, SstvRxCause::SyncRecoveryExhausted, true);
        }
      metrics_.lastCause = SstvRxCause::MissingSyncTolerated;
      return result (before, SstvRxCause::MissingSyncTolerated, true);
    }

  if (auto const* completed = std::get_if<SstvRxFrameCompleted> (&event))
    {
      if (state_ != SstvRxState::Receiving
          && state_ != SstvRxState::RecoveringSync)
        return reject (before, SstvRxCause::UnexpectedEvent);
      saturatingAdd (metrics_.decodedLines, completed->decodedLines);
      if (metrics_.decodedLines == 0)
        return reject (before, SstvRxCause::InvalidInput);
      finishSession (SstvRxState::Completed, SstvRxCause::FrameComplete,
                     nowMs);
      return result (before, SstvRxCause::FrameComplete, true);
    }

  if (auto const* ended = std::get_if<SstvRxInputEnded> (&event))
    {
      if (!activeSession_ || !isAcquisitionState (state_))
        return reject (before, SstvRxCause::UnexpectedEvent);
      bool const usable = ended->usablePartialImage
                          || metrics_.decodedLines > 0;
      finishSession (usable ? SstvRxState::Partial : SstvRxState::Aborted,
                     SstvRxCause::EndOfInput, nowMs);
      return result (before, SstvRxCause::EndOfInput, true);
    }

  return reject (before, SstvRxCause::UnexpectedEvent);
}

SstvRxState SstvRxStateMachine::state () const noexcept
{
  return state_;
}

SstvRxPolicy const& SstvRxStateMachine::policy () const noexcept
{
  return policy_;
}

SstvRxMetrics const& SstvRxStateMachine::metrics () const noexcept
{
  return metrics_;
}

bool SstvRxStateMachine::enabled () const noexcept
{
  return enabled_;
}

bool SstvRxStateMachine::monitoring () const noexcept
{
  return monitoring_;
}

bool SstvRxStateMachine::hasActiveSession () const noexcept
{
  return activeSession_;
}

bool SstvRxStateMachine::setModeLock (std::optional<std::string> mode)
{
  if (!canChangePolicy () || (mode && !validModeName (*mode)))
    return false;
  policy_.lockedMode = std::move (mode);
  return true;
}

bool
SstvRxStateMachine::setNoVisFallbackMode (std::optional<std::string> mode)
{
  if (!canChangePolicy () || (mode && !validModeName (*mode)))
    return false;
  policy_.noVisFallbackMode = std::move (mode);
  return true;
}

bool SstvRxStateMachine::invariantsHold () const noexcept
{
  if (metrics_.state != state_)
    return false;
  if (state_ == SstvRxState::Disabled && (enabled_ || monitoring_))
    return false;
  if (state_ == SstvRxState::Idle && (!enabled_ || monitoring_))
    return false;
  if (state_ == SstvRxState::SearchingLeader
      && (!enabled_ || !monitoring_ || activeSession_))
    return false;
  if (isAcquisitionState (state_)
      && (!enabled_ || !monitoring_ || !activeSession_))
    return false;
  if (isTerminal (state_) && activeSession_)
    return false;
  if (activeSession_
      && (metrics_.currentSessionId == 0
          || !metrics_.sessionStartedAtMs.has_value ()))
    return false;

  bool const needsMode = state_ == SstvRxState::ModeDetected
                         || state_ == SstvRxState::WaitingForSync
                         || state_ == SstvRxState::Receiving
                         || state_ == SstvRxState::RecoveringSync
                         || state_ == SstvRxState::Completed
                         || state_ == SstvRxState::Partial;
  if (needsMode
      && (metrics_.selectedMode.empty ()
          || metrics_.modeSource == SstvRxModeSource::None))
    return false;
  if (metrics_.selectedMode.empty ()
      != (metrics_.modeSource == SstvRxModeSource::None))
    return false;
  return true;
}

bool SstvRxStateMachine::isTerminal (SstvRxState state) noexcept
{
  return state == SstvRxState::Completed || state == SstvRxState::Partial
         || state == SstvRxState::Aborted || state == SstvRxState::Error;
}

char const* SstvRxStateMachine::stateName (SstvRxState state) noexcept
{
  switch (state)
    {
    case SstvRxState::Disabled: return "Disabled";
    case SstvRxState::Idle: return "Idle";
    case SstvRxState::SearchingLeader: return "SearchingLeader";
    case SstvRxState::LeaderCandidate: return "LeaderCandidate";
    case SstvRxState::ReadingVis: return "ReadingVis";
    case SstvRxState::ModeDetected: return "ModeDetected";
    case SstvRxState::WaitingForSync: return "WaitingForSync";
    case SstvRxState::Receiving: return "Receiving";
    case SstvRxState::RecoveringSync: return "RecoveringSync";
    case SstvRxState::Completed: return "Completed";
    case SstvRxState::Partial: return "Partial";
    case SstvRxState::Aborted: return "Aborted";
    case SstvRxState::Error: return "Error";
    }
  return "Unknown";
}

char const* SstvRxStateMachine::causeName (SstvRxCause cause) noexcept
{
  switch (cause)
    {
    case SstvRxCause::None: return "None";
    case SstvRxCause::Enabled: return "Enabled";
    case SstvRxCause::Disabled: return "Disabled";
    case SstvRxCause::MonitoringStarted: return "MonitoringStarted";
    case SstvRxCause::MonitoringStopped: return "MonitoringStopped";
    case SstvRxCause::LeaderCandidateObserved: return "LeaderCandidateObserved";
    case SstvRxCause::LeaderConfirmed: return "LeaderConfirmed";
    case SstvRxCause::LeaderRejected: return "LeaderRejected";
    case SstvRxCause::LeaderCandidateTimeout: return "LeaderCandidateTimeout";
    case SstvRxCause::VisDecoded: return "VisDecoded";
    case SstvRxCause::VisRejected: return "VisRejected";
    case SstvRxCause::VisUnavailable: return "VisUnavailable";
    case SstvRxCause::VisTimeout: return "VisTimeout";
    case SstvRxCause::ModeLockMismatch: return "ModeLockMismatch";
    case SstvRxCause::LockedModeFallback: return "LockedModeFallback";
    case SstvRxCause::NoVisFallback: return "NoVisFallback";
    case SstvRxCause::ManualOverride: return "ManualOverride";
    case SstvRxCause::ModeReady: return "ModeReady";
    case SstvRxCause::ModePreparationTimeout: return "ModePreparationTimeout";
    case SstvRxCause::SyncAcquired: return "SyncAcquired";
    case SstvRxCause::SyncWaitTimeout: return "SyncWaitTimeout";
    case SstvRxCause::LineReceived: return "LineReceived";
    case SstvRxCause::MissingSyncTolerated: return "MissingSyncTolerated";
    case SstvRxCause::SyncLost: return "SyncLost";
    case SstvRxCause::SyncRecovered: return "SyncRecovered";
    case SstvRxCause::SyncRecoveryExhausted: return "SyncRecoveryExhausted";
    case SstvRxCause::SyncRecoveryTimeout: return "SyncRecoveryTimeout";
    case SstvRxCause::FrameComplete: return "FrameComplete";
    case SstvRxCause::EndOfInput: return "EndOfInput";
    case SstvRxCause::MaxReceptionExceeded: return "MaxReceptionExceeded";
    case SstvRxCause::Cancelled: return "Cancelled";
    case SstvRxCause::Failure: return "Failure";
    case SstvRxCause::Reset: return "Reset";
    case SstvRxCause::AutoReset: return "AutoReset";
    case SstvRxCause::ClockRegression: return "ClockRegression";
    case SstvRxCause::InvalidInput: return "InvalidInput";
    case SstvRxCause::UnexpectedEvent: return "UnexpectedEvent";
    }
  return "Unknown";
}

SstvRxTransition
SstvRxStateMachine::reject (SstvRxState before, SstvRxCause cause) noexcept
{
  ++metrics_.rejectedEventCount;
  metrics_.lastCause = cause;
  return result (before, cause, false);
}

SstvRxTransition
SstvRxStateMachine::result (SstvRxState before,
                            SstvRxCause cause,
                            bool accepted,
                            bool timedOut,
                            bool automaticReset) const noexcept
{
  return {before,
          state_,
          cause,
          metrics_.currentSessionId,
          accepted,
          before != state_,
          timedOut,
          automaticReset};
}

void SstvRxStateMachine::transitionTo (SstvRxState next,
                                       SstvRxCause cause,
                                       std::uint64_t nowMs)
{
  if (state_ != next)
    {
      state_ = next;
      metrics_.state = next;
      metrics_.stateEnteredAtMs = nowMs;
      metrics_.stateElapsedMs = 0;
      ++metrics_.transitionCount;
    }
  metrics_.lastCause = cause;
}

void SstvRxStateMachine::beginSession (std::uint64_t nowMs)
{
  clearSessionData ();
  activeSession_ = true;
  ++nextSessionId_;
  if (nextSessionId_ == 0)
    ++nextSessionId_;
  metrics_.currentSessionId = nextSessionId_;
  metrics_.sessionStartedAtMs = nowMs;
  metrics_.sessionElapsedMs = 0;
  ++metrics_.sessionsStarted;
}

void SstvRxStateMachine::clearSessionData () noexcept
{
  activeSession_ = false;
  metrics_.currentSessionId = 0;
  metrics_.selectedMode.clear ();
  metrics_.modeSource = SstvRxModeSource::None;
  metrics_.decodedLines = 0;
  metrics_.consecutiveMissingSync = 0;
  metrics_.recoveryMissingSync = 0;
  metrics_.lastLeaderConfidence = 0.0;
  metrics_.lastLeaderOffsetHz = 0.0;
  metrics_.lastVisConfidence = 0.0;
  metrics_.sessionStartedAtMs.reset ();
  metrics_.receptionStartedAtMs.reset ();
  metrics_.sessionElapsedMs = 0;
}

void SstvRxStateMachine::finishSession (SstvRxState terminal,
                                        SstvRxCause cause,
                                        std::uint64_t nowMs)
{
  if (activeSession_)
    {
      switch (terminal)
        {
        case SstvRxState::Completed: ++metrics_.sessionsCompleted; break;
        case SstvRxState::Partial: ++metrics_.sessionsPartial; break;
        case SstvRxState::Aborted: ++metrics_.sessionsAborted; break;
        case SstvRxState::Error: ++metrics_.sessionsErrored; break;
        default: break;
        }
    }
  activeSession_ = false;
  transitionTo (terminal, cause, nowMs);
}

bool SstvRxStateMachine::expireBeforeEvent (
    std::uint64_t nowMs, SstvRxTransition& transition)
{
  if (!activeSession_ || !metrics_.sessionStartedAtMs)
    return false;

  if (elapsed (nowMs, *metrics_.sessionStartedAtMs)
      >= policy_.timeouts.maxReceptionMs)
    {
      SstvRxState const outcome = metrics_.decodedLines > 0
                                      ? SstvRxState::Partial
                                      : SstvRxState::Aborted;
      finishSession (outcome, SstvRxCause::MaxReceptionExceeded, nowMs);
      transition = result (state_, SstvRxCause::MaxReceptionExceeded, true,
                           true);
      return true;
    }

  std::uint64_t const stateAge = elapsed (nowMs, metrics_.stateEnteredAtMs);
  switch (state_)
    {
    case SstvRxState::LeaderCandidate:
      if (stateAge < policy_.timeouts.leaderCandidateMs)
        return false;
      ++metrics_.falseLeaders;
      activeSession_ = false;
      transitionTo (SstvRxState::SearchingLeader,
                    SstvRxCause::LeaderCandidateTimeout, nowMs);
      transition = result (SstvRxState::LeaderCandidate,
                           SstvRxCause::LeaderCandidateTimeout, true, true);
      return true;

    case SstvRxState::ReadingVis:
      if (stateAge < policy_.timeouts.visMs)
        return false;
      ++metrics_.invalidVisFrames;
      selectWithoutVis (nowMs, SstvRxCause::VisTimeout);
      transition = result (SstvRxState::ReadingVis, metrics_.lastCause, true,
                           true);
      return true;

    case SstvRxState::ModeDetected:
      if (stateAge < policy_.timeouts.modePreparationMs)
        return false;
      finishSession (SstvRxState::Aborted,
                     SstvRxCause::ModePreparationTimeout, nowMs);
      transition = result (SstvRxState::ModeDetected,
                           SstvRxCause::ModePreparationTimeout, true, true);
      return true;

    case SstvRxState::WaitingForSync:
      if (stateAge < policy_.timeouts.waitingForSyncMs)
        return false;
      finishSession (SstvRxState::Aborted, SstvRxCause::SyncWaitTimeout,
                     nowMs);
      transition = result (SstvRxState::WaitingForSync,
                           SstvRxCause::SyncWaitTimeout, true, true);
      return true;

    case SstvRxState::RecoveringSync:
      if (stateAge < policy_.timeouts.syncRecoveryMs)
        return false;
      finishSession (metrics_.decodedLines > 0 ? SstvRxState::Partial
                                              : SstvRxState::Aborted,
                     SstvRxCause::SyncRecoveryTimeout, nowMs);
      transition = result (SstvRxState::RecoveringSync,
                           SstvRxCause::SyncRecoveryTimeout, true, true);
      return true;

    default: return false;
    }
}

bool SstvRxStateMachine::selectWithoutVis (std::uint64_t nowMs,
                                           SstvRxCause failureCause)
{
  if (policy_.lockedMode && policy_.useLockedModeWithoutVis)
    return selectMode (*policy_.lockedMode,
                       SstvRxModeSource::ModeLockWithoutVis, 0.0, nowMs,
                       SstvRxCause::LockedModeFallback);
  if (policy_.noVisFallbackMode)
    return selectMode (*policy_.noVisFallbackMode,
                       SstvRxModeSource::NoVisFallback, 0.0, nowMs,
                       SstvRxCause::NoVisFallback);

  activeSession_ = false;
  transitionTo (SstvRxState::SearchingLeader, failureCause, nowMs);
  return false;
}

bool SstvRxStateMachine::selectMode (std::string const& mode,
                                    SstvRxModeSource source,
                                    double visConfidence,
                                    std::uint64_t nowMs,
                                    SstvRxCause cause)
{
  if (!validModeName (mode))
    return false;
  metrics_.selectedMode = mode;
  metrics_.modeSource = source;
  metrics_.lastVisConfidence = visConfidence;
  transitionTo (SstvRxState::ModeDetected, cause, nowMs);
  return true;
}

void SstvRxStateMachine::resetToListeningState (std::uint64_t nowMs,
                                                SstvRxCause cause)
{
  clearSessionData ();
  SstvRxState const next = !enabled_ ? SstvRxState::Disabled
                           : monitoring_ ? SstvRxState::SearchingLeader
                                         : SstvRxState::Idle;
  transitionTo (next, cause, nowMs);
}

void SstvRxStateMachine::updateElapsed (std::uint64_t nowMs) noexcept
{
  metrics_.stateElapsedMs = elapsed (nowMs, metrics_.stateEnteredAtMs);
  metrics_.sessionElapsedMs = metrics_.sessionStartedAtMs
                                  ? elapsed (nowMs,
                                             *metrics_.sessionStartedAtMs)
                                  : 0;
}

bool SstvRxStateMachine::canChangePolicy () const noexcept
{
  return !activeSession_
         && (state_ == SstvRxState::Disabled
             || state_ == SstvRxState::Idle
             || state_ == SstvRxState::SearchingLeader
             || isTerminal (state_));
}

bool SstvRxStateMachine::validModeName (std::string const& mode) noexcept
{
  if (mode.empty () || mode.size () > 64 || mode.front () == ' '
      || mode.back () == ' ')
    return false;
  return std::all_of (mode.begin (), mode.end (), [] (unsigned char ch) {
    return ch >= 0x20u && ch <= 0x7eu;
  });
}

bool SstvRxStateMachine::validConfidence (double confidence) noexcept
{
  return std::isfinite (confidence) && confidence >= 0.0
         && confidence <= 1.0;
}

std::string SstvRxStateMachine::boundedDetail (std::string const& detail)
{
  std::string output;
  output.reserve (std::min<std::size_t> (detail.size (), 256));
  for (char raw : detail)
    {
      if (output.size () == 256)
        break;
      auto const ch = static_cast<unsigned char> (raw);
      output.push_back (ch >= 0x20u && ch <= 0x7eu
                            ? static_cast<char> (ch)
                            : '?');
    }
  return output;
}

void SstvRxStateMachine::validatePolicy (SstvRxPolicy const& policy)
{
  auto const& timeout = policy.timeouts;
  if (timeout.leaderCandidateMs == 0 || timeout.visMs == 0
      || timeout.modePreparationMs == 0 || timeout.waitingForSyncMs == 0
      || timeout.syncRecoveryMs == 0 || timeout.maxReceptionMs == 0)
    throw std::invalid_argument ("SSTV RX timeouts must be non-zero");
  if (policy.missingSyncBeforeRecovery == 0
      || policy.maxMissingSyncDuringRecovery == 0)
    throw std::invalid_argument ("SSTV RX sync limits must be non-zero");
  if ((policy.lockedMode && !validModeName (*policy.lockedMode))
      || (policy.noVisFallbackMode
          && !validModeName (*policy.noVisFallbackMode)))
    throw std::invalid_argument ("invalid SSTV RX mode policy");
}

} // namespace decodium::sstv
