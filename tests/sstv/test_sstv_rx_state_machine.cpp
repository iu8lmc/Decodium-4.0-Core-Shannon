// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include "src/sstv/rx/SstvRxStateMachine.h"

#include <limits>
#include <set>
#include <stdexcept>
#include <string>

using namespace decodium::sstv;

namespace
{

bool dispatchTo (SstvRxStateMachine& receiver,
                 std::uint64_t time,
                 SstvRxEvent const& event,
                 SstvRxState expected)
{
  auto const transition = receiver.dispatch (time, event);
  return transition.accepted && receiver.state () == expected
         && receiver.invariantsHold ();
}

bool startAutomaticReception (SstvRxStateMachine& receiver,
                              std::uint64_t start,
                              std::string const& mode = "Martin M1")
{
  return dispatchTo (receiver, start, SstvRxEnable {}, SstvRxState::Idle)
         && dispatchTo (receiver, start + 1, SstvRxStartMonitoring {},
                        SstvRxState::SearchingLeader)
         && dispatchTo (receiver, start + 2,
                        SstvRxLeaderObserved {0.91, 18.0},
                        SstvRxState::LeaderCandidate)
         && dispatchTo (receiver, start + 3, SstvRxLeaderConfirmed {},
                        SstvRxState::ReadingVis)
         && dispatchTo (receiver, start + 4,
                        SstvRxVisDecoded {mode, 0.87},
                        SstvRxState::ModeDetected)
         && dispatchTo (receiver, start + 5, SstvRxModeReady {},
                        SstvRxState::WaitingForSync)
         && dispatchTo (receiver, start + 6, SstvRxSyncObserved {},
                        SstvRxState::Receiving);
}

SstvRxPolicy shortPolicy ()
{
  SstvRxPolicy policy;
  policy.timeouts.leaderCandidateMs = 20;
  policy.timeouts.visMs = 30;
  policy.timeouts.modePreparationMs = 40;
  policy.timeouts.waitingForSyncMs = 50;
  policy.timeouts.syncRecoveryMs = 60;
  policy.timeouts.maxReceptionMs = 1'000;
  policy.timeouts.terminalHoldMs = 0;
  policy.missingSyncBeforeRecovery = 2;
  policy.maxMissingSyncDuringRecovery = 3;
  return policy;
}

} // namespace

class TestSstvRxStateMachine final : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void validatesPolicyAndModeTokens ();
  void enableMonitorStopAndDisableAreDeterministic ();
  void completesVisReceptionAndStartsBackToBack ();
  void rejectsFalseLeaderAndExpiresCandidate ();
  void handlesVisFallbackModeLockAndMismatch ();
  void manualOverrideBypassesAutomaticModeLock ();
  void toleratesMissingSyncThenRecovers ();
  void recoveryExhaustionProducesPartialImage ();
  void phaseAndMaximumTimeoutsTerminate ();
  void inputEndCancelFailureAndResetAreExplicit ();
  void rejectsUnexpectedInvalidAndRegressedEvents ();
  void policyCannotMutateDuringReception ();
  void everyMissionStateIsReachable ();
};

void TestSstvRxStateMachine::validatesPolicyAndModeTokens ()
{
  SstvRxPolicy invalid = shortPolicy ();
  invalid.timeouts.visMs = 0;
  QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                            SstvRxStateMachine {invalid});

  invalid = shortPolicy ();
  invalid.missingSyncBeforeRecovery = 0;
  QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                            SstvRxStateMachine {invalid});

  invalid = shortPolicy ();
  invalid.lockedMode = " leading";
  QVERIFY_THROWS_EXCEPTION (std::invalid_argument,
                            SstvRxStateMachine {invalid});

  SstvRxStateMachine receiver {shortPolicy ()};
  QVERIFY (receiver.invariantsHold ());
  QVERIFY (receiver.setModeLock ("Martin M1"));
  QVERIFY (!receiver.setModeLock ("bad\nmode"));
  QVERIFY (receiver.setNoVisFallbackMode ("Robot 36"));
}

void TestSstvRxStateMachine::enableMonitorStopAndDisableAreDeterministic ()
{
  SstvRxStateMachine receiver {shortPolicy ()};
  QCOMPARE (receiver.state (), SstvRxState::Disabled);

  auto rejected = receiver.dispatch (0, SstvRxStartMonitoring {});
  QVERIFY (!rejected.accepted);
  QCOMPARE (rejected.cause, SstvRxCause::UnexpectedEvent);

  QVERIFY (dispatchTo (receiver, 1, SstvRxEnable {}, SstvRxState::Idle));
  QVERIFY (receiver.enabled ());
  QVERIFY (!receiver.monitoring ());
  QVERIFY (dispatchTo (receiver, 2, SstvRxStartMonitoring {},
                       SstvRxState::SearchingLeader));
  QVERIFY (receiver.monitoring ());
  QVERIFY (dispatchTo (receiver, 3, SstvRxStopMonitoring {},
                       SstvRxState::Idle));
  QVERIFY (dispatchTo (receiver, 4, SstvRxDisable {},
                       SstvRxState::Disabled));
  QVERIFY (!receiver.enabled ());
  QCOMPARE (receiver.metrics ().transitionCount, std::uint64_t {4});
  QCOMPARE (receiver.metrics ().rejectedEventCount, std::uint64_t {1});
}

void TestSstvRxStateMachine::completesVisReceptionAndStartsBackToBack ()
{
  SstvRxStateMachine receiver {shortPolicy ()};
  QVERIFY (startAutomaticReception (receiver, 10));
  QCOMPARE (receiver.metrics ().selectedMode, std::string ("Martin M1"));
  QCOMPARE (receiver.metrics ().modeSource, SstvRxModeSource::Vis);
  QCOMPARE (receiver.metrics ().validVisFrames, std::uint64_t {1});

  QVERIFY (dispatchTo (receiver, 17, SstvRxLineObservation {true, 12},
                       SstvRxState::Receiving));
  QVERIFY (dispatchTo (receiver, 18, SstvRxFrameCompleted {4},
                       SstvRxState::Completed));
  QCOMPARE (receiver.metrics ().decodedLines, std::uint32_t {16});
  QCOMPARE (receiver.metrics ().sessionsCompleted, std::uint64_t {1});
  std::uint64_t const firstSession = receiver.metrics ().currentSessionId;

  auto const next = receiver.dispatch (
      19, SstvRxLeaderObserved {0.8, -12.0});
  QVERIFY (next.accepted);
  QVERIFY (next.automaticReset);
  QCOMPARE (next.before, SstvRxState::Completed);
  QCOMPARE (next.after, SstvRxState::LeaderCandidate);
  QVERIFY (receiver.metrics ().currentSessionId > firstSession);
  QCOMPARE (receiver.metrics ().decodedLines, std::uint32_t {0});
  QVERIFY (receiver.invariantsHold ());
}

void TestSstvRxStateMachine::rejectsFalseLeaderAndExpiresCandidate ()
{
  SstvRxStateMachine receiver {shortPolicy ()};
  QVERIFY (dispatchTo (receiver, 0, SstvRxEnable {}, SstvRxState::Idle));
  QVERIFY (dispatchTo (receiver, 1, SstvRxStartMonitoring {},
                       SstvRxState::SearchingLeader));
  QVERIFY (dispatchTo (receiver, 2, SstvRxLeaderObserved {0.6, 0.0},
                       SstvRxState::LeaderCandidate));
  QVERIFY (dispatchTo (receiver, 3, SstvRxLeaderRejected {},
                       SstvRxState::SearchingLeader));
  QCOMPARE (receiver.metrics ().falseLeaders, std::uint64_t {1});

  QVERIFY (dispatchTo (receiver, 4, SstvRxLeaderObserved {0.7, 0.0},
                       SstvRxState::LeaderCandidate));
  auto const expired = receiver.dispatch (24, SstvRxTick {});
  QVERIFY (expired.accepted);
  QVERIFY (expired.timedOut);
  QCOMPARE (expired.cause, SstvRxCause::LeaderCandidateTimeout);
  QCOMPARE (receiver.state (), SstvRxState::SearchingLeader);
  QCOMPARE (receiver.metrics ().falseLeaders, std::uint64_t {2});
  QVERIFY (receiver.invariantsHold ());
}

void TestSstvRxStateMachine::handlesVisFallbackModeLockAndMismatch ()
{
  SstvRxStateMachine unavailable {shortPolicy ()};
  QVERIFY (dispatchTo (unavailable, 0, SstvRxEnable {}, SstvRxState::Idle));
  QVERIFY (dispatchTo (unavailable, 1, SstvRxStartMonitoring {},
                       SstvRxState::SearchingLeader));
  QVERIFY (dispatchTo (unavailable, 2,
                       SstvRxLeaderObserved {0.8, 5.0},
                       SstvRxState::LeaderCandidate));
  QVERIFY (dispatchTo (unavailable, 3, SstvRxLeaderConfirmed {},
                       SstvRxState::ReadingVis));
  auto const unavailableResult = unavailable.dispatch (
      4, SstvRxVisUnavailable {});
  QCOMPARE (unavailableResult.cause, SstvRxCause::VisUnavailable);
  QCOMPARE (unavailable.state (), SstvRxState::SearchingLeader);

  SstvRxPolicy fallback = shortPolicy ();
  fallback.noVisFallbackMode = "Scottie S1";
  SstvRxStateMachine noVis {fallback};
  QVERIFY (dispatchTo (noVis, 0, SstvRxEnable {}, SstvRxState::Idle));
  QVERIFY (dispatchTo (noVis, 1, SstvRxStartMonitoring {},
                       SstvRxState::SearchingLeader));
  QVERIFY (dispatchTo (noVis, 2, SstvRxLeaderObserved {0.8, 5.0},
                       SstvRxState::LeaderCandidate));
  QVERIFY (dispatchTo (noVis, 3, SstvRxLeaderConfirmed {},
                       SstvRxState::ReadingVis));
  auto rejected = noVis.dispatch (4, SstvRxVisRejected {});
  QVERIFY (rejected.accepted);
  QCOMPARE (rejected.cause, SstvRxCause::NoVisFallback);
  QCOMPARE (noVis.state (), SstvRxState::ModeDetected);
  QCOMPARE (noVis.metrics ().selectedMode, std::string ("Scottie S1"));
  QCOMPARE (noVis.metrics ().modeSource, SstvRxModeSource::NoVisFallback);

  SstvRxPolicy locked = shortPolicy ();
  locked.lockedMode = "Robot 36";
  SstvRxStateMachine lockFallback {locked};
  QVERIFY (dispatchTo (lockFallback, 0, SstvRxEnable {}, SstvRxState::Idle));
  QVERIFY (dispatchTo (lockFallback, 1, SstvRxStartMonitoring {},
                       SstvRxState::SearchingLeader));
  QVERIFY (dispatchTo (lockFallback, 2,
                       SstvRxLeaderObserved {0.9, 0.0},
                       SstvRxState::LeaderCandidate));
  QVERIFY (dispatchTo (lockFallback, 3, SstvRxLeaderConfirmed {},
                       SstvRxState::ReadingVis));
  QVERIFY (dispatchTo (lockFallback, 4, SstvRxVisUnavailable {},
                       SstvRxState::ModeDetected));
  QCOMPARE (lockFallback.metrics ().selectedMode,
            std::string ("Robot 36"));
  QCOMPARE (lockFallback.metrics ().modeSource,
            SstvRxModeSource::ModeLockWithoutVis);

  SstvRxStateMachine mismatch {locked};
  QVERIFY (dispatchTo (mismatch, 0, SstvRxEnable {}, SstvRxState::Idle));
  QVERIFY (dispatchTo (mismatch, 1, SstvRxStartMonitoring {},
                       SstvRxState::SearchingLeader));
  QVERIFY (dispatchTo (mismatch, 2, SstvRxLeaderObserved {0.9, 0.0},
                       SstvRxState::LeaderCandidate));
  QVERIFY (dispatchTo (mismatch, 3, SstvRxLeaderConfirmed {},
                       SstvRxState::ReadingVis));
  auto const wrong = mismatch.dispatch (
      4, SstvRxVisDecoded {"Martin M1", 0.9});
  QCOMPARE (wrong.cause, SstvRxCause::ModeLockMismatch);
  QCOMPARE (mismatch.state (), SstvRxState::SearchingLeader);
  QCOMPARE (mismatch.metrics ().modeLockMismatches, std::uint64_t {1});
  QVERIFY (mismatch.invariantsHold ());
}

void TestSstvRxStateMachine::manualOverrideBypassesAutomaticModeLock ()
{
  SstvRxPolicy policy = shortPolicy ();
  policy.lockedMode = "Robot 36";
  SstvRxStateMachine receiver {policy};
  QVERIFY (dispatchTo (receiver, 0, SstvRxEnable {}, SstvRxState::Idle));
  QVERIFY (dispatchTo (receiver, 1, SstvRxManualMode {"Martin M1"},
                       SstvRxState::ModeDetected));
  QCOMPARE (receiver.metrics ().selectedMode, std::string ("Martin M1"));
  QCOMPARE (receiver.metrics ().modeSource,
            SstvRxModeSource::ManualOverride);
  QVERIFY (receiver.monitoring ());
}

void TestSstvRxStateMachine::toleratesMissingSyncThenRecovers ()
{
  SstvRxStateMachine receiver {shortPolicy ()};
  QVERIFY (startAutomaticReception (receiver, 0));
  QVERIFY (dispatchTo (receiver, 7, SstvRxLineObservation {true, 1},
                       SstvRxState::Receiving));
  auto firstMiss = receiver.dispatch (
      8, SstvRxLineObservation {false, 0});
  QCOMPARE (firstMiss.cause, SstvRxCause::MissingSyncTolerated);
  QCOMPARE (receiver.state (), SstvRxState::Receiving);

  auto secondMiss = receiver.dispatch (
      9, SstvRxLineObservation {false, 0});
  QCOMPARE (secondMiss.cause, SstvRxCause::SyncLost);
  QCOMPARE (receiver.state (), SstvRxState::RecoveringSync);
  QCOMPARE (receiver.metrics ().syncLosses, std::uint64_t {1});

  QVERIFY (dispatchTo (receiver, 10, SstvRxLineObservation {true, 1},
                       SstvRxState::Receiving));
  QCOMPARE (receiver.metrics ().syncRecoveries, std::uint64_t {1});
  QCOMPARE (receiver.metrics ().missingSyncLines, std::uint64_t {2});
  QCOMPARE (receiver.metrics ().decodedLines, std::uint32_t {2});
  QVERIFY (receiver.invariantsHold ());
}

void TestSstvRxStateMachine::recoveryExhaustionProducesPartialImage ()
{
  SstvRxPolicy policy = shortPolicy ();
  policy.missingSyncBeforeRecovery = 1;
  policy.maxMissingSyncDuringRecovery = 2;
  SstvRxStateMachine receiver {policy};
  QVERIFY (startAutomaticReception (receiver, 0));
  QVERIFY (dispatchTo (receiver, 7, SstvRxLineObservation {true, 3},
                       SstvRxState::Receiving));
  QVERIFY (dispatchTo (receiver, 8, SstvRxLineObservation {false, 0},
                       SstvRxState::RecoveringSync));
  QVERIFY (dispatchTo (receiver, 9, SstvRxLineObservation {false, 0},
                       SstvRxState::RecoveringSync));
  auto const exhausted = receiver.dispatch (
      10, SstvRxLineObservation {false, 0});
  QCOMPARE (exhausted.cause, SstvRxCause::SyncRecoveryExhausted);
  QCOMPARE (receiver.state (), SstvRxState::Partial);
  QCOMPARE (receiver.metrics ().sessionsPartial, std::uint64_t {1});
  QCOMPARE (receiver.metrics ().decodedLines, std::uint32_t {3});
  QVERIFY (receiver.invariantsHold ());
}

void TestSstvRxStateMachine::phaseAndMaximumTimeoutsTerminate ()
{
  SstvRxPolicy policy = shortPolicy ();
  SstvRxStateMachine waiting {policy};
  QVERIFY (dispatchTo (waiting, 0, SstvRxEnable {}, SstvRxState::Idle));
  QVERIFY (dispatchTo (waiting, 1, SstvRxManualMode {"PD 120"},
                       SstvRxState::ModeDetected));
  QVERIFY (dispatchTo (waiting, 2, SstvRxModeReady {},
                       SstvRxState::WaitingForSync));
  auto waitTimeout = waiting.dispatch (52, SstvRxTick {});
  QVERIFY (waitTimeout.timedOut);
  QCOMPARE (waitTimeout.cause, SstvRxCause::SyncWaitTimeout);
  QCOMPARE (waiting.state (), SstvRxState::Aborted);

  SstvRxStateMachine recovery {policy};
  QVERIFY (startAutomaticReception (recovery, 100));
  QVERIFY (dispatchTo (recovery, 107, SstvRxLineObservation {true, 2},
                       SstvRxState::Receiving));
  QVERIFY (dispatchTo (recovery, 108, SstvRxSyncLost {},
                       SstvRxState::RecoveringSync));
  auto recoveryTimeout = recovery.dispatch (168, SstvRxTick {});
  QVERIFY (recoveryTimeout.timedOut);
  QCOMPARE (recoveryTimeout.cause, SstvRxCause::SyncRecoveryTimeout);
  QCOMPARE (recovery.state (), SstvRxState::Partial);

  policy.timeouts.maxReceptionMs = 10;
  SstvRxStateMachine capped {policy};
  QVERIFY (startAutomaticReception (capped, 200));
  QVERIFY (dispatchTo (capped, 207, SstvRxLineObservation {true, 1},
                       SstvRxState::Receiving));
  auto maximum = capped.dispatch (212, SstvRxTick {});
  QVERIFY (maximum.timedOut);
  QCOMPARE (maximum.cause, SstvRxCause::MaxReceptionExceeded);
  QCOMPARE (capped.state (), SstvRxState::Partial);
}

void TestSstvRxStateMachine::inputEndCancelFailureAndResetAreExplicit ()
{
  SstvRxStateMachine partial {shortPolicy ()};
  QVERIFY (startAutomaticReception (partial, 0));
  QVERIFY (dispatchTo (partial, 7, SstvRxLineObservation {true, 2},
                       SstvRxState::Receiving));
  QVERIFY (dispatchTo (partial, 8, SstvRxInputEnded {false},
                       SstvRxState::Partial));

  SstvRxStateMachine aborted {shortPolicy ()};
  QVERIFY (dispatchTo (aborted, 0, SstvRxEnable {}, SstvRxState::Idle));
  QVERIFY (dispatchTo (aborted, 1, SstvRxManualMode {"Robot 36"},
                       SstvRxState::ModeDetected));
  QVERIFY (dispatchTo (aborted, 2, SstvRxCancel {},
                       SstvRxState::Aborted));
  QCOMPARE (aborted.metrics ().sessionsAborted, std::uint64_t {1});

  SstvRxStateMachine failed {shortPolicy ()};
  QVERIFY (startAutomaticReception (failed, 0));
  std::string detail (300, 'x');
  detail[2] = '\n';
  QVERIFY (dispatchTo (failed, 7,
                       SstvRxFailure {SstvRxErrorCode::DspFailure, detail},
                       SstvRxState::Error));
  QCOMPARE (failed.metrics ().lastErrorCode,
            SstvRxErrorCode::DspFailure);
  QCOMPARE (failed.metrics ().lastErrorDetail.size (), std::size_t {256});
  QCOMPARE (failed.metrics ().lastErrorDetail[2], '?');
  QVERIFY (dispatchTo (failed, 8, SstvRxReset {},
                       SstvRxState::SearchingLeader));
  QCOMPARE (failed.metrics ().lastErrorCode, SstvRxErrorCode::None);
}

void TestSstvRxStateMachine::rejectsUnexpectedInvalidAndRegressedEvents ()
{
  SstvRxStateMachine receiver {shortPolicy ()};
  QVERIFY (dispatchTo (receiver, 10, SstvRxEnable {}, SstvRxState::Idle));
  auto unexpected = receiver.dispatch (11, SstvRxSyncObserved {});
  QVERIFY (!unexpected.accepted);
  QCOMPARE (unexpected.cause, SstvRxCause::UnexpectedEvent);
  QCOMPARE (receiver.state (), SstvRxState::Idle);

  QVERIFY (dispatchTo (receiver, 12, SstvRxStartMonitoring {},
                       SstvRxState::SearchingLeader));
  auto invalid = receiver.dispatch (
      13, SstvRxLeaderObserved {
              std::numeric_limits<double>::quiet_NaN (), 0.0});
  QVERIFY (!invalid.accepted);
  QCOMPARE (invalid.cause, SstvRxCause::InvalidInput);
  QCOMPARE (receiver.state (), SstvRxState::SearchingLeader);

  auto regressed = receiver.dispatch (9, SstvRxTick {});
  QVERIFY (!regressed.accepted);
  QCOMPARE (regressed.cause, SstvRxCause::ClockRegression);
  QCOMPARE (receiver.state (), SstvRxState::Error);
  QCOMPARE (receiver.metrics ().lastErrorCode,
            SstvRxErrorCode::ClockRegression);
  QVERIFY (receiver.invariantsHold ());
}

void TestSstvRxStateMachine::policyCannotMutateDuringReception ()
{
  SstvRxStateMachine receiver {shortPolicy ()};
  QVERIFY (dispatchTo (receiver, 0, SstvRxEnable {}, SstvRxState::Idle));
  QVERIFY (receiver.setModeLock ("Martin M1"));
  QVERIFY (dispatchTo (receiver, 1, SstvRxStartMonitoring {},
                       SstvRxState::SearchingLeader));
  QVERIFY (receiver.setNoVisFallbackMode ("Scottie S1"));
  QVERIFY (dispatchTo (receiver, 2, SstvRxLeaderObserved {0.9, 0.0},
                       SstvRxState::LeaderCandidate));
  QVERIFY (!receiver.setModeLock ("Robot 36"));
  QVERIFY (!receiver.setNoVisFallbackMode (std::nullopt));
}

void TestSstvRxStateMachine::everyMissionStateIsReachable ()
{
  std::set<SstvRxState> reached;
  auto record = [&reached] (SstvRxStateMachine const& receiver) {
    reached.insert (receiver.state ());
    return receiver.invariantsHold ();
  };

  SstvRxStateMachine receiver {shortPolicy ()};
  QVERIFY (record (receiver)); // Disabled
  receiver.dispatch (0, SstvRxEnable {}); QVERIFY (record (receiver));
  receiver.dispatch (1, SstvRxStartMonitoring {}); QVERIFY (record (receiver));
  receiver.dispatch (2, SstvRxLeaderObserved {0.9, 0.0}); QVERIFY (record (receiver));
  receiver.dispatch (3, SstvRxLeaderConfirmed {}); QVERIFY (record (receiver));
  receiver.dispatch (4, SstvRxVisDecoded {"Martin M1", 0.9}); QVERIFY (record (receiver));
  receiver.dispatch (5, SstvRxModeReady {}); QVERIFY (record (receiver));
  receiver.dispatch (6, SstvRxSyncObserved {}); QVERIFY (record (receiver));
  receiver.dispatch (7, SstvRxSyncLost {}); QVERIFY (record (receiver));
  receiver.dispatch (8, SstvRxSyncObserved {});
  receiver.dispatch (9, SstvRxLineObservation {true, 1});
  receiver.dispatch (10, SstvRxFrameCompleted {}); QVERIFY (record (receiver));
  receiver.dispatch (11, SstvRxLeaderObserved {0.9, 0.0});
  receiver.dispatch (12, SstvRxCancel {}); QVERIFY (record (receiver));
  receiver.dispatch (13, SstvRxReset {});
  receiver.dispatch (14, SstvRxManualMode {"Martin M1"});
  receiver.dispatch (15, SstvRxModeReady {});
  receiver.dispatch (16, SstvRxSyncObserved {});
  receiver.dispatch (17, SstvRxLineObservation {true, 1});
  receiver.dispatch (18, SstvRxInputEnded {}); QVERIFY (record (receiver));
  receiver.dispatch (19, SstvRxFailure {SstvRxErrorCode::InternalFailure,
                                        "test"});
  QVERIFY (record (receiver));

  QCOMPARE (reached.size (), std::size_t {13});
}

QTEST_APPLESS_MAIN (TestSstvRxStateMachine)

#include "test_sstv_rx_state_machine.moc"
