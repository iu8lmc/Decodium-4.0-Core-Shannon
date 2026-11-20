// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/tx/SstvTxStateMachine.h"

#include <QtTest/QtTest>

#include <cstdint>
#include <limits>
#include <string>
#include <utility>

using namespace decodium::sstv;

namespace {

template<typename Event>
SstvTxTransition send(SstvTxStateMachine& machine,
                      std::uint64_t nowMs,
                      Event event)
{
    const SstvTxTransition transition = machine.dispatch(
        nowMs, SstvTxEvent {std::move(event)});
    Q_ASSERT(machine.invariantsHold());
    return transition;
}

void prepareReady(SstvTxStateMachine& machine,
                  std::uint64_t& nowMs,
                  bool fskId = false)
{
    QVERIFY(send(machine, nowMs++, SstvTxEnable {}).accepted);
    QVERIFY(send(machine,
                 nowMs++,
                 SstvTxPrepare {"martin-m1", 320U, 256U}).accepted);
    QVERIFY(send(machine, nowMs++, SstvTxImagePrepared {}).accepted);
    QVERIFY(send(machine,
                 nowMs++,
                 SstvTxEncodingComplete {1'200'000U, 12'000U, fskId})
                .accepted);
    QCOMPARE(machine.state(), SstvTxState::Ready);
}

void enterHeader(SstvTxStateMachine& machine,
                 std::uint64_t& nowMs,
                 bool releaseRequired = true,
                 bool fskId = false)
{
    prepareReady(machine, nowMs, fskId);
    QVERIFY(send(machine, nowMs++, SstvTxRequest {}).accepted);
    QVERIFY(send(machine,
                 nowMs++,
                 SstvTxPttRequestDispatched {releaseRequired}).accepted);
    QVERIFY(send(machine, nowMs++, SstvTxPttConfirmed {}).accepted);
    QVERIFY(send(machine, nowMs++, SstvTxLeadElapsed {}).accepted);
    QCOMPARE(machine.state(), SstvTxState::TransmittingHeader);
}

void finishFromHeader(SstvTxStateMachine& machine,
                      std::uint64_t& nowMs,
                      bool fskId)
{
    QVERIFY(send(machine, nowMs++, SstvTxHeaderComplete {}).accepted);
    QVERIFY(send(machine, nowMs++, SstvTxImageComplete {}).accepted);
    QCOMPARE(machine.state(), fskId
                                 ? SstvTxState::TransmittingFskId
                                 : SstvTxState::TailDelay);
    if (fskId) {
        QVERIFY(send(machine, nowMs++, SstvTxFskIdComplete {}).accepted);
    }
    QVERIFY(send(machine, nowMs++, SstvTxTailElapsed {}).accepted);
    QCOMPARE(machine.state(), SstvTxState::ReleasingPtt);
    QVERIFY(send(machine, nowMs++, SstvTxPttReleased {}).accepted);
    QCOMPARE(machine.state(), SstvTxState::Completed);
}

} // namespace

class TestSstvTxStateMachine final : public QObject
{
    Q_OBJECT

private slots:
    void happyPathRequiresConfirmedPttAndRelease();
    void fskAndVoxPathRemainExplicit();
    void cancellationCannotBypassPttRelease();
    void failureAndDisableCannotBypassPttRelease();
    void watchdogsFailClosedAndLateReleaseIsAccepted();
    void clockRegressionFailsClosed();
    void invalidInputsAndUnexpectedEventsAreTransactional();
    void completedSessionAutoResetsForNextImage();
    void hostileEventSequencePreservesFailSafeInvariants();
};

void TestSstvTxStateMachine::happyPathRequiresConfirmedPttAndRelease()
{
    SstvTxStateMachine machine;
    QCOMPARE(machine.state(), SstvTxState::Disabled);
    QVERIFY(machine.invariantsHold());

    std::uint64_t nowMs = 10U;
    prepareReady(machine, nowMs, false);
    QCOMPARE(machine.metrics().encodedDurationMs, std::uint64_t {100'000U});
    QCOMPARE(machine.metrics().sessionsStarted, std::uint64_t {1U});

    QVERIFY(send(machine, nowMs++, SstvTxRequest {}).accepted);
    QCOMPARE(machine.state(), SstvTxState::RequestingPtt);
    QVERIFY(!send(machine, nowMs++, SstvTxLeadElapsed {}).accepted);
    QVERIFY(!machine.metrics().audioStarted);

    QVERIFY(send(machine,
                 nowMs++,
                 SstvTxPttRequestDispatched {true}).accepted);
    QVERIFY(machine.releaseRequired());
    QVERIFY(!send(machine, nowMs++, SstvTxLeadElapsed {}).accepted);
    QVERIFY(!machine.metrics().audioStarted);
    QVERIFY(send(machine, nowMs++, SstvTxPttConfirmed {}).accepted);
    QVERIFY(machine.metrics().pttConfirmed);
    QVERIFY(send(machine, nowMs++, SstvTxLeadElapsed {}).accepted);
    QVERIFY(machine.metrics().audioStarted);

    finishFromHeader(machine, nowMs, false);
    QCOMPARE(machine.metrics().sessionsCompleted, std::uint64_t {1U});
    QCOMPARE(machine.metrics().pttRequestsDispatched, std::uint64_t {1U});
    QCOMPARE(machine.metrics().pttConfirmations, std::uint64_t {1U});
    QCOMPARE(machine.metrics().pttReleaseRequests, std::uint64_t {1U});
    QCOMPARE(machine.metrics().pttReleases, std::uint64_t {1U});
    QVERIFY(!machine.releaseRequired());
    QVERIFY(!machine.metrics().audioStarted);
}

void TestSstvTxStateMachine::fskAndVoxPathRemainExplicit()
{
    SstvTxStateMachine machine;
    std::uint64_t nowMs = 0U;
    enterHeader(machine, nowMs, false, true);
    QVERIFY(!machine.releaseRequired());

    QVERIFY(send(machine, nowMs++, SstvTxHeaderComplete {}).accepted);
    QVERIFY(send(machine, nowMs++, SstvTxImageComplete {}).accepted);
    QCOMPARE(machine.state(), SstvTxState::TransmittingFskId);
    QVERIFY(send(machine, nowMs++, SstvTxFskIdComplete {}).accepted);
    QCOMPARE(machine.state(), SstvTxState::TailDelay);
    QVERIFY(send(machine, nowMs++, SstvTxTailElapsed {}).accepted);
    // Even VOX/audio-only completion has an explicit coordinator release
    // acknowledgement; it simply carries no CAT PTT-off obligation.
    QCOMPARE(machine.state(), SstvTxState::ReleasingPtt);
    QVERIFY(!machine.releaseRequired());
    QVERIFY(send(machine, nowMs++, SstvTxPttReleased {}).accepted);
    QCOMPARE(machine.state(), SstvTxState::Completed);
}

void TestSstvTxStateMachine::cancellationCannotBypassPttRelease()
{
    {
        SstvTxStateMachine machine;
        std::uint64_t nowMs = 0U;
        prepareReady(machine, nowMs);
        const auto cancelled = send(machine, nowMs++, SstvTxCancel {});
        QVERIFY(cancelled.accepted);
        QCOMPARE(machine.state(), SstvTxState::Cancelled);
        QCOMPARE(machine.metrics().sessionsCancelled, std::uint64_t {1U});
        QCOMPARE(machine.metrics().pttReleaseRequests, std::uint64_t {0U});
    }

    const SstvTxState cancelStates[] {
        SstvTxState::WaitingForPtt,
        SstvTxState::TransmittingHeader,
        SstvTxState::TransmittingImage,
        SstvTxState::TransmittingFskId,
        SstvTxState::TailDelay,
    };
    for (const SstvTxState target : cancelStates) {
        SstvTxStateMachine machine;
        std::uint64_t nowMs = 100U;
        prepareReady(machine, nowMs, true);
        QVERIFY(send(machine, nowMs++, SstvTxRequest {}).accepted);
        QVERIFY(send(machine,
                     nowMs++,
                     SstvTxPttRequestDispatched {true}).accepted);
        if (target != SstvTxState::WaitingForPtt) {
            QVERIFY(send(machine, nowMs++, SstvTxPttConfirmed {}).accepted);
            QVERIFY(send(machine, nowMs++, SstvTxLeadElapsed {}).accepted);
        }
        if (target == SstvTxState::TransmittingImage
            || target == SstvTxState::TransmittingFskId
            || target == SstvTxState::TailDelay) {
            QVERIFY(send(machine, nowMs++, SstvTxHeaderComplete {}).accepted);
        }
        if (target == SstvTxState::TransmittingFskId
            || target == SstvTxState::TailDelay) {
            QVERIFY(send(machine, nowMs++, SstvTxImageComplete {}).accepted);
        }
        if (target == SstvTxState::TailDelay) {
            QVERIFY(send(machine, nowMs++, SstvTxFskIdComplete {}).accepted);
        }
        QCOMPARE(machine.state(), target);

        QVERIFY(send(machine, nowMs++, SstvTxCancel {}).accepted);
        QCOMPARE(machine.state(), SstvTxState::ReleasingPtt);
        QVERIFY(machine.releaseRequired());
        QCOMPARE(machine.metrics().sessionsCancelled, std::uint64_t {0U});
        QVERIFY(send(machine, nowMs++, SstvTxPttReleased {}).accepted);
        QCOMPARE(machine.state(), SstvTxState::Cancelled);
        QCOMPARE(machine.metrics().sessionsCancelled, std::uint64_t {1U});
    }
}

void TestSstvTxStateMachine::failureAndDisableCannotBypassPttRelease()
{
    {
        SstvTxStateMachine machine;
        std::uint64_t nowMs = 0U;
        enterHeader(machine, nowMs, true);
        QVERIFY(send(machine, nowMs++, SstvTxHeaderComplete {}).accepted);
        const std::string longDetail(900U, 'x');
        QVERIFY(send(machine,
                     nowMs++,
                     SstvTxFailure {SstvTxErrorCode::AudioUnderrun,
                                    longDetail}).accepted);
        QCOMPARE(machine.state(), SstvTxState::ReleasingPtt);
        QCOMPARE(machine.metrics().sessionsFailed, std::uint64_t {0U});
        QVERIFY(send(machine, nowMs++, SstvTxPttReleased {}).accepted);
        QCOMPARE(machine.state(), SstvTxState::Error);
        QCOMPARE(machine.metrics().lastErrorCode,
                 SstvTxErrorCode::AudioUnderrun);
        QCOMPARE(machine.metrics().lastErrorDetail.size(),
                 machine.policy().maximumErrorCharacters);
        QCOMPARE(machine.metrics().sessionsFailed, std::uint64_t {1U});
    }

    {
        SstvTxStateMachine machine;
        std::uint64_t nowMs = 0U;
        enterHeader(machine, nowMs, true);
        QVERIFY(send(machine, nowMs++, SstvTxDisable {}).accepted);
        QCOMPARE(machine.state(), SstvTxState::ReleasingPtt);
        QVERIFY(!machine.enabled());
        QVERIFY(send(machine,
                     nowMs++,
                     SstvTxFailure {SstvTxErrorCode::AudioDeviceLoss,
                                    "late shutdown callback"}).accepted);
        QCOMPARE(machine.state(), SstvTxState::ReleasingPtt);
        QVERIFY(send(machine, nowMs++, SstvTxPttReleased {}).accepted);
        QCOMPARE(machine.state(), SstvTxState::Disabled);
        QCOMPARE(machine.metrics().currentSessionId, std::uint64_t {0U});
        QVERIFY(!machine.enabled());
    }
}

void TestSstvTxStateMachine::watchdogsFailClosedAndLateReleaseIsAccepted()
{
    SstvTxPolicy policy;
    policy.timeouts.requestingPttMs = 5U;
    policy.timeouts.waitingForPttMs = 7U;
    policy.timeouts.releasingPttMs = 3U;

    {
        SstvTxStateMachine machine(policy);
        std::uint64_t nowMs = 0U;
        prepareReady(machine, nowMs);
        QVERIFY(send(machine, 10U, SstvTxRequest {}).accepted);
        QVERIFY(send(machine, 15U, SstvTxTick {}).accepted);
        QCOMPARE(machine.state(), SstvTxState::Error);
        QCOMPARE(machine.metrics().lastErrorCode,
                 SstvTxErrorCode::PttTimeout);
        QVERIFY(!machine.releaseRequired());
    }

    {
        SstvTxStateMachine machine(policy);
        std::uint64_t nowMs = 0U;
        prepareReady(machine, nowMs);
        QVERIFY(send(machine, 10U, SstvTxRequest {}).accepted);
        QVERIFY(send(machine,
                     11U,
                     SstvTxPttRequestDispatched {true}).accepted);
        QVERIFY(send(machine, 18U, SstvTxTick {}).accepted);
        QCOMPARE(machine.state(), SstvTxState::ReleasingPtt);
        QVERIFY(machine.releaseRequired());
        QCOMPARE(machine.metrics().lastErrorCode, SstvTxErrorCode::None);

        // Release watchdog expiry never fabricates a PTT-off confirmation or
        // escapes the barrier. A real late acknowledgement remains accepted.
        QVERIFY(send(machine, 21U, SstvTxTick {}).accepted);
        QCOMPARE(machine.state(), SstvTxState::ReleasingPtt);
        QVERIFY(machine.releaseRequired());
        QVERIFY(send(machine, 100U, SstvTxPttReleased {}).accepted);
        QCOMPARE(machine.state(), SstvTxState::Error);
        QCOMPARE(machine.metrics().lastErrorCode,
                 SstvTxErrorCode::WatchdogExpired);
    }
}

void TestSstvTxStateMachine::clockRegressionFailsClosed()
{
    SstvTxStateMachine machine;
    std::uint64_t nowMs = 100U;
    enterHeader(machine, nowMs, true);
    const auto regressed = send(machine, nowMs - 2U, SstvTxTick {});
    QVERIFY(!regressed.accepted);
    QCOMPARE(regressed.cause, SstvTxCause::ClockRegression);
    QCOMPARE(machine.state(), SstvTxState::ReleasingPtt);
    QVERIFY(machine.releaseRequired());
    QVERIFY(send(machine, nowMs++, SstvTxPttReleased {}).accepted);
    QCOMPARE(machine.state(), SstvTxState::Error);
    QCOMPARE(machine.metrics().lastErrorCode,
             SstvTxErrorCode::ClockRegression);
}

void TestSstvTxStateMachine::invalidInputsAndUnexpectedEventsAreTransactional()
{
    SstvTxStateMachine machine;
    QVERIFY(send(machine, 0U, SstvTxEnable {}).accepted);
    QCOMPARE(machine.state(), SstvTxState::Idle);

    QVERIFY(!send(machine,
                  1U,
                  SstvTxPrepare {"", 320U, 256U}).accepted);
    QVERIFY(!send(machine,
                  2U,
                  SstvTxPrepare {"martin-m1", 0U, 256U}).accepted);
    QVERIFY(!send(machine,
                  3U,
                  SstvTxPrepare {std::string(65U, 'm'), 320U, 256U})
                 .accepted);
    std::string embeddedNul("m1\0bad", 6U);
    QVERIFY(!send(machine,
                  4U,
                  SstvTxPrepare {embeddedNul, 320U, 256U}).accepted);
    QCOMPARE(machine.state(), SstvTxState::Idle);
    QVERIFY(!send(machine,
                  5U,
                  SstvTxFailure {SstvTxErrorCode::InternalFailure, "idle"})
                 .accepted);
    QCOMPARE(machine.state(), SstvTxState::Idle);

    QVERIFY(send(machine,
                 6U,
                 SstvTxPrepare {"martin-m1", 320U, 256U}).accepted);
    QVERIFY(send(machine, 7U, SstvTxImagePrepared {}).accepted);
    QVERIFY(!send(machine,
                  8U,
                  SstvTxEncodingComplete {0U, 12'000U, false}).accepted);
    QVERIFY(!send(machine,
                  9U,
                  SstvTxEncodingComplete {100U, 7'999U, false}).accepted);
    QCOMPARE(machine.state(), SstvTxState::Encoding);
    QCOMPARE(machine.metrics().encodedSamples, std::uint64_t {0U});
    QVERIFY(machine.metrics().rejectedEvents >= 7U);

    SstvTxPolicy invalid;
    invalid.timeouts.releasingPttMs = 0U;
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                             SstvTxStateMachine bad(invalid));
}

void TestSstvTxStateMachine::completedSessionAutoResetsForNextImage()
{
    SstvTxStateMachine machine;
    std::uint64_t nowMs = 0U;
    enterHeader(machine, nowMs, false, false);
    finishFromHeader(machine, nowMs, false);
    const std::uint64_t firstSession = machine.metrics().currentSessionId;
    QCOMPARE(machine.state(), SstvTxState::Completed);

    const auto next = send(
        machine,
        nowMs++,
        SstvTxPrepare {"scottie-s1", 320U, 256U});
    QVERIFY(next.accepted);
    QVERIFY(next.automaticReset);
    QCOMPARE(next.before, SstvTxState::Completed);
    QCOMPARE(next.after, SstvTxState::PreparingImage);
    QVERIFY(machine.metrics().currentSessionId > firstSession);
    QCOMPARE(machine.metrics().mode, std::string("scottie-s1"));
    QCOMPARE(machine.metrics().sessionsStarted, std::uint64_t {2U});
    QCOMPARE(machine.metrics().sessionsCompleted, std::uint64_t {1U});
}

void TestSstvTxStateMachine::hostileEventSequencePreservesFailSafeInvariants()
{
    SstvTxStateMachine machine;
    std::uint64_t clock = 0U;
    std::uint32_t random = 0x5a17c3e1U;

    for (std::size_t iteration = 0U; iteration < 20'000U; ++iteration) {
        random = random * 1'664'525U + 1'013'904'223U;
        clock += static_cast<std::uint64_t>((random >> 28U) & 0x03U);
        const std::uint32_t selector = (random >> 8U) % 19U;
        SstvTxEvent event = SstvTxTick {};
        switch (selector) {
        case 0U: event = SstvTxEnable {}; break;
        case 1U: event = SstvTxDisable {}; break;
        case 2U: event = SstvTxReset {}; break;
        case 3U:
            event = SstvTxPrepare {
                (random & 1U) != 0U ? "martin-m1" : std::string {},
                (random & 2U) != 0U ? 320U : 0U,
                256U};
            break;
        case 4U: event = SstvTxImagePrepared {}; break;
        case 5U:
            event = SstvTxEncodingComplete {
                (random & 4U) != 0U ? 120'000U : 0U,
                (random & 8U) != 0U ? 12'000U : 1U,
                (random & 16U) != 0U};
            break;
        case 6U: event = SstvTxRequest {}; break;
        case 7U:
            event = SstvTxPttRequestDispatched {(random & 32U) != 0U};
            break;
        case 8U: event = SstvTxPttConfirmed {}; break;
        case 9U: event = SstvTxLeadElapsed {}; break;
        case 10U: event = SstvTxHeaderComplete {}; break;
        case 11U: event = SstvTxImageComplete {}; break;
        case 12U: event = SstvTxFskIdComplete {}; break;
        case 13U: event = SstvTxTailElapsed {}; break;
        case 14U: event = SstvTxPttReleased {}; break;
        case 15U: event = SstvTxCancel {}; break;
        case 16U:
            event = SstvTxFailure {
                SstvTxErrorCode::AudioDeviceLoss,
                std::string(700U, 'e')};
            break;
        case 17U:
            // A hostile timestamp regression is deliberate and must still
            // preserve the release barrier.
            if (clock > 0U) {
                --clock;
            }
            event = SstvTxTick {};
            break;
        case 18U: event = SstvTxTick {}; break;
        default: Q_UNREACHABLE();
        }

        static_cast<void>(machine.dispatch(clock, event));
        QVERIFY2(machine.invariantsHold(),
                 SstvTxStateMachine::stateName(machine.state()));
        if (machine.releaseRequired()) {
            QVERIFY(!SstvTxStateMachine::isTerminal(machine.state()));
            QVERIFY(machine.metrics().pttRequestDispatched);
        }
        if (machine.metrics().audioStarted) {
            QVERIFY(machine.metrics().pttConfirmed);
        }
    }
}

QTEST_GUILESS_MAIN(TestSstvTxStateMachine)
#include "test_sstv_tx_state_machine.moc"
