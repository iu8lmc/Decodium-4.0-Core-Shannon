// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace decodium::sstv
{

// These states intentionally describe only acquisition and framing.  Image
// demodulation and mode-specific line decoding live above this state machine.
enum class SstvRxState : std::uint8_t
{
  Disabled,
  Idle,
  SearchingLeader,
  LeaderCandidate,
  ReadingVis,
  ModeDetected,
  WaitingForSync,
  Receiving,
  RecoveringSync,
  Completed,
  Partial,
  Aborted,
  Error
};

enum class SstvRxModeSource : std::uint8_t
{
  None,
  Vis,
  VisWithModeLock,
  ModeLockWithoutVis,
  NoVisFallback,
  ManualOverride
};

enum class SstvRxCause : std::uint8_t
{
  None,
  Enabled,
  Disabled,
  MonitoringStarted,
  MonitoringStopped,
  LeaderCandidateObserved,
  LeaderConfirmed,
  LeaderRejected,
  LeaderCandidateTimeout,
  VisDecoded,
  VisRejected,
  VisUnavailable,
  VisTimeout,
  ModeLockMismatch,
  LockedModeFallback,
  NoVisFallback,
  ManualOverride,
  ModeReady,
  ModePreparationTimeout,
  SyncAcquired,
  SyncWaitTimeout,
  LineReceived,
  MissingSyncTolerated,
  SyncLost,
  SyncRecovered,
  SyncRecoveryExhausted,
  SyncRecoveryTimeout,
  FrameComplete,
  EndOfInput,
  MaxReceptionExceeded,
  Cancelled,
  Failure,
  Reset,
  AutoReset,
  ClockRegression,
  InvalidInput,
  UnexpectedEvent
};

enum class SstvRxErrorCode : std::uint8_t
{
  None,
  InvalidInput,
  DspFailure,
  ResourceFailure,
  InternalFailure,
  ClockRegression
};

struct SstvRxTimeouts
{
  std::uint64_t leaderCandidateMs {500};
  std::uint64_t visMs {1'500};
  std::uint64_t modePreparationMs {1'000};
  std::uint64_t waitingForSyncMs {5'000};
  std::uint64_t syncRecoveryMs {2'000};
  std::uint64_t maxReceptionMs {600'000};
  std::uint64_t terminalHoldMs {0};
};

struct SstvRxPolicy
{
  SstvRxTimeouts timeouts;

  // A mode lock constrains automatic VIS selection.  Manual override is an
  // explicit operator action and therefore bypasses the automatic lock.
  std::optional<std::string> lockedMode;
  bool useLockedModeWithoutVis {true};
  std::optional<std::string> noVisFallbackMode;

  // Missing line sync is tolerated before entering prediction/recovery.
  std::uint32_t missingSyncBeforeRecovery {2};
  std::uint32_t maxMissingSyncDuringRecovery {12};
};

// Typed events keep DSP observations separate from state-management commands.
struct SstvRxEnable
{
};
struct SstvRxDisable
{
};
struct SstvRxStartMonitoring
{
};
struct SstvRxStopMonitoring
{
};
struct SstvRxReset
{
};
struct SstvRxTick
{
};

struct SstvRxLeaderObserved
{
  double confidence {0.0};
  double frequencyOffsetHz {0.0};
};
struct SstvRxLeaderConfirmed
{
};
struct SstvRxLeaderRejected
{
};

struct SstvRxVisDecoded
{
  std::string mode;
  double confidence {0.0};
};
struct SstvRxVisRejected
{
};
struct SstvRxVisUnavailable
{
};
struct SstvRxModeReady
{
};

struct SstvRxManualMode
{
  std::string mode;
};
struct SstvRxSyncObserved
{
};
struct SstvRxSyncLost
{
};

struct SstvRxLineObservation
{
  bool syncObserved {true};
  std::uint32_t decodedLines {1};
};

struct SstvRxFrameCompleted
{
  std::uint32_t decodedLines {0};
};
struct SstvRxInputEnded
{
  bool usablePartialImage {false};
};
struct SstvRxCancel
{
};
struct SstvRxFailure
{
  SstvRxErrorCode code {SstvRxErrorCode::InternalFailure};
  std::string detail;
};

using SstvRxEvent = std::variant<SstvRxEnable,
                                 SstvRxDisable,
                                 SstvRxStartMonitoring,
                                 SstvRxStopMonitoring,
                                 SstvRxReset,
                                 SstvRxTick,
                                 SstvRxLeaderObserved,
                                 SstvRxLeaderConfirmed,
                                 SstvRxLeaderRejected,
                                 SstvRxVisDecoded,
                                 SstvRxVisRejected,
                                 SstvRxVisUnavailable,
                                 SstvRxModeReady,
                                 SstvRxManualMode,
                                 SstvRxSyncObserved,
                                 SstvRxSyncLost,
                                 SstvRxLineObservation,
                                 SstvRxFrameCompleted,
                                 SstvRxInputEnded,
                                 SstvRxCancel,
                                 SstvRxFailure>;

struct SstvRxMetrics
{
  SstvRxState state {SstvRxState::Disabled};
  SstvRxCause lastCause {SstvRxCause::None};
  SstvRxErrorCode lastErrorCode {SstvRxErrorCode::None};
  std::string lastErrorDetail;

  std::uint64_t transitionCount {0};
  std::uint64_t rejectedEventCount {0};
  std::uint64_t sessionsStarted {0};
  std::uint64_t sessionsCompleted {0};
  std::uint64_t sessionsPartial {0};
  std::uint64_t sessionsAborted {0};
  std::uint64_t sessionsErrored {0};
  std::uint64_t currentSessionId {0};

  std::uint64_t leaderCandidates {0};
  std::uint64_t falseLeaders {0};
  std::uint64_t visAttempts {0};
  std::uint64_t validVisFrames {0};
  std::uint64_t invalidVisFrames {0};
  std::uint64_t modeLockMismatches {0};
  std::uint64_t missingSyncLines {0};
  std::uint64_t syncLosses {0};
  std::uint64_t syncRecoveries {0};

  std::uint32_t decodedLines {0};
  std::uint32_t consecutiveMissingSync {0};
  std::uint32_t recoveryMissingSync {0};

  std::string selectedMode;
  SstvRxModeSource modeSource {SstvRxModeSource::None};
  double lastLeaderConfidence {0.0};
  double lastLeaderOffsetHz {0.0};
  double lastVisConfidence {0.0};

  std::optional<std::uint64_t> sessionStartedAtMs;
  std::optional<std::uint64_t> receptionStartedAtMs;
  std::uint64_t stateEnteredAtMs {0};
  std::uint64_t lastEventAtMs {0};
  std::uint64_t stateElapsedMs {0};
  std::uint64_t sessionElapsedMs {0};
};

struct SstvRxTransition
{
  SstvRxState before {SstvRxState::Disabled};
  SstvRxState after {SstvRxState::Disabled};
  SstvRxCause cause {SstvRxCause::None};
  std::uint64_t sessionId {0};
  bool accepted {false};
  bool stateChanged {false};
  bool timedOut {false};
  bool automaticReset {false};
};

class SstvRxStateMachine final
{
public:
  explicit SstvRxStateMachine (SstvRxPolicy policy = {});

  // nowMs is a caller-owned monotonic clock.  Wall-clock time is never read by
  // this class, making timeout behaviour deterministic in replay and tests.
  SstvRxTransition dispatch (std::uint64_t nowMs,
                             SstvRxEvent const& event);

  SstvRxState state () const noexcept;
  SstvRxPolicy const& policy () const noexcept;
  SstvRxMetrics const& metrics () const noexcept;
  bool enabled () const noexcept;
  bool monitoring () const noexcept;
  bool hasActiveSession () const noexcept;

  // Policy changes are accepted only while no reception is active.
  bool setModeLock (std::optional<std::string> mode);
  bool setNoVisFallbackMode (std::optional<std::string> mode);

  bool invariantsHold () const noexcept;

  static bool isTerminal (SstvRxState state) noexcept;
  static char const* stateName (SstvRxState state) noexcept;
  static char const* causeName (SstvRxCause cause) noexcept;

private:
  SstvRxTransition reject (SstvRxState before,
                           SstvRxCause cause) noexcept;
  SstvRxTransition result (SstvRxState before,
                           SstvRxCause cause,
                           bool accepted,
                           bool timedOut = false,
                           bool automaticReset = false) const noexcept;
  void transitionTo (SstvRxState next,
                     SstvRxCause cause,
                     std::uint64_t nowMs);
  void beginSession (std::uint64_t nowMs);
  void clearSessionData () noexcept;
  void finishSession (SstvRxState terminal,
                      SstvRxCause cause,
                      std::uint64_t nowMs);
  bool expireBeforeEvent (std::uint64_t nowMs,
                          SstvRxTransition& transition);
  bool selectWithoutVis (std::uint64_t nowMs, SstvRxCause failureCause);
  bool selectMode (std::string const& mode,
                   SstvRxModeSource source,
                   double visConfidence,
                   std::uint64_t nowMs,
                   SstvRxCause cause);
  void resetToListeningState (std::uint64_t nowMs, SstvRxCause cause);
  void updateElapsed (std::uint64_t nowMs) noexcept;
  bool canChangePolicy () const noexcept;

  static bool validModeName (std::string const& mode) noexcept;
  static bool validConfidence (double confidence) noexcept;
  static std::string boundedDetail (std::string const& detail);
  static void validatePolicy (SstvRxPolicy const& policy);

  SstvRxPolicy policy_;
  SstvRxMetrics metrics_;
  SstvRxState state_ {SstvRxState::Disabled};
  std::uint64_t nextSessionId_ {0};
  bool enabled_ {false};
  bool monitoring_ {false};
  bool activeSession_ {false};
  bool hasClock_ {false};
};

} // namespace decodium::sstv
