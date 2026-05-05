// test_ft2_engine.cpp — Qt Test coverage for engines/Ft2QsoEngine.
//
// Validates the FT2 single-pipeline state machine behaviors that
// are required by the lazy-forging-wall plan:
//  - Idle -> AwaitingReport on grid response (CQ direct answer)
//  - AwaitingReport -> AwaitingSignoff on R+report (TX3 -> TX4)
//  - Multi-caller queue processed in best-SNR order
//  - Cooldown 30s blocks repeated engagement after 73
//  - Idempotency: duplicate decode does not double-advance state
//  - Bounded queue caps at kCallerQueueCapacity (50) with FIFO evict
//  - doubleClick engages partner regardless of CQ status
//  - Ultra2 (QuickQSO): doubleClick skips TX1; AwaitingReport+R+rep -> TX3+TU

#include "../engines/Ft2QsoEngine.hpp"

#include <QSignalSpy>
#include <QtTest>
#include <chrono>

using decodium::ft2::Ft2QsoEngine;
using decodium::ft2::EngineConfig;
using decodium::ft2::DecodeRow;
using decodium::ft2::Slot;
using decodium::ft2::TimePoint;
using decodium::ft2::Clock;
using namespace std::chrono_literals;

namespace state = decodium::ft2::state;

namespace {

EngineConfig makeCfg(QString me = "IK8TWX", QString grid = "JN70") {
    EngineConfig c;
    c.myCall          = me;
    c.myBaseCall      = me;
    c.myGrid          = grid;
    c.multiAnswerMode = true;
    c.localSlot       = Slot::Async;
    return c;
}

DecodeRow makeRow(QString from, QString message, int snr = -10, int hz = 1500) {
    DecodeRow r;
    r.timeToken = "120000";
    r.snr       = snr;
    r.dt        = 0.5;
    r.df        = hz;
    r.audioHz   = hz;
    r.message   = message;
    r.fromCall  = from;
    return r;
}

} // namespace

class Ft2EngineTest : public QObject {
    Q_OBJECT
private slots:

    // ----------------------------------------------------------------
    // Idle -> AwaitingReport: a CQ-direct answer (TX1) engages the QSO.
    // ----------------------------------------------------------------
    void cqEngageOnDirectAnswer() {
        Ft2QsoEngine eng(makeCfg(), nullptr);
        QSignalSpy currentTxSpy(&eng, &Ft2QsoEngine::currentTxChanged);
        QSignalSpy dxSpy(&eng, &Ft2QsoEngine::dxCallChanged);

        eng.enableTx(true);
        QVERIFY(std::holds_alternative<state::CallingCq>(eng.state()));

        // Partner sends "IK8TWX K1ABC FN42" (TX1, grid).
        std::vector<DecodeRow> rows{ makeRow("K1ABC", "IK8TWX K1ABC FN42") };
        eng.feedParsed(rows);

        QVERIFY(std::holds_alternative<state::AwaitingReport>(eng.state()));
        QCOMPARE(eng.dxCall(), QStringLiteral("K1ABC"));
        QCOMPARE(eng.currentTx(), 2);
        QVERIFY(currentTxSpy.count() >= 1);
        QVERIFY(dxSpy.count() >= 1);
    }

    // ----------------------------------------------------------------
    // Reversed-order reply: some stations transmit "<FROM> <TO> <PAYLOAD>"
    // instead of canonical "<TO> <FROM> <PAYLOAD>". Decodium 3.0 accepts
    // both; the engine must too. See Ft2QsoEngine.cpp enrichDecodeMeta.
    // ----------------------------------------------------------------
    void cqEngageOnReversedAnswer() {
        Ft2QsoEngine eng(makeCfg(), nullptr);
        eng.enableTx(true);
        QVERIFY(std::holds_alternative<state::CallingCq>(eng.state()));

        // Reversed form: our call IK8TWX is in slot 1, partner K1ABC in slot 0.
        eng.feedParsed({ makeRow("K1ABC", "K1ABC IK8TWX FN42") });

        QVERIFY(std::holds_alternative<state::AwaitingReport>(eng.state()));
        QCOMPARE(eng.dxCall(), QStringLiteral("K1ABC"));
        QCOMPARE(eng.currentTx(), 2);
    }

    // ----------------------------------------------------------------
    // AwaitingReport -> Closing on R+report decode (CQer side: TX4 RR73 is
    // the final TX of the QSO; we do not linger in AwaitingSignoff).
    // ----------------------------------------------------------------
    void awaitingReportToSignoffOnRogerReport() {
        Ft2QsoEngine eng(makeCfg(), nullptr);
        eng.enableTx(true);
        eng.feedParsed({ makeRow("K1ABC", "IK8TWX K1ABC FN42") });
        QVERIFY(std::holds_alternative<state::AwaitingReport>(eng.state()));

        // Partner replies TX3: "IK8TWX K1ABC R-12" — engine sends RR73 once
        // and goes straight to Closing.
        eng.feedParsed({ makeRow("K1ABC", "IK8TWX K1ABC R-12") });
        QVERIFY(std::holds_alternative<state::Closing>(eng.state()));
        QCOMPARE(eng.currentTx(), 4);
    }

    // ----------------------------------------------------------------
    // Multi-caller: highest-SNR caller drained first when queue holds 2.
    // ----------------------------------------------------------------
    void multiCallerProcessedBestSnrFirst() {
        Ft2QsoEngine eng(makeCfg(), nullptr);
        eng.enableTx(true);
        // Two CQ answers at different SNRs (none directed at us yet — they're CQs).
        eng.feedParsed({
            makeRow("K1ABC", "CQ K1ABC FN42", /*snr*/ -18),
            makeRow("W2DEF", "CQ W2DEF FN20", /*snr*/  -5),
        });
        QCOMPARE(eng.callerQueueSize(), std::size_t{2});

        // Partner W2DEF (better SNR) directly answers — engage.
        eng.feedParsed({ makeRow("W2DEF", "IK8TWX W2DEF FN20", -5) });
        QCOMPARE(eng.dxCall(), QStringLiteral("W2DEF"));
        // K1ABC remains queued for next QSO.
        QCOMPARE(eng.callerQueueSize(), std::size_t{1});
    }

    // ----------------------------------------------------------------
    // Idempotency: same decode fed twice does not double-advance.
    // ----------------------------------------------------------------
    void idempotentDuplicateDecode() {
        Ft2QsoEngine eng(makeCfg(), nullptr);
        eng.enableTx(true);

        QSignalSpy txSpy(&eng, &Ft2QsoEngine::currentTxChanged);
        DecodeRow r = makeRow("K1ABC", "IK8TWX K1ABC FN42");
        eng.feedParsed({ r });
        int const firstCount = txSpy.count();

        // Same decode again — must be deduped.
        eng.feedParsed({ r });
        QCOMPARE(txSpy.count(), firstCount);
        QVERIFY(std::holds_alternative<state::AwaitingReport>(eng.state()));
    }

    // ----------------------------------------------------------------
    // Cooldown: after a logged QSO the partner cannot be re-queued
    // until kCooldown73 elapses.
    // ----------------------------------------------------------------
    void cooldownBlocksImmediateRequeue() {
        Ft2QsoEngine eng(makeCfg(), nullptr);
        TimePoint synthetic = Clock::now();
        eng.setClockOverride([&]() { return synthetic; });
        eng.enableTx(true);

        // Drive a partial QSO and force Closing manually via feed sequence:
        eng.feedParsed({ makeRow("K1ABC", "IK8TWX K1ABC FN42") });
        eng.feedParsed({ makeRow("K1ABC", "IK8TWX K1ABC R-12") });
        // We are in AwaitingSignoff. Partner sends 73 -> Closing.
        eng.feedParsed({ makeRow("K1ABC", "IK8TWX K1ABC 73") });
        // tick() finalizes Closing -> Idle and stamps cooldown.
        eng.tick();

        // Try to re-queue same partner CQ immediately.
        eng.feedParsed({ makeRow("K1ABC", "CQ K1ABC FN42") });
        QCOMPARE(eng.callerQueueSize(), std::size_t{0});

        // Advance the synthetic clock past the cooldown window.
        synthetic += std::chrono::seconds(31);
        eng.feedParsed({ makeRow("K1ABC", "CQ K1ABC FN42") });
        QCOMPARE(eng.callerQueueSize(), std::size_t{1});
    }

    // ----------------------------------------------------------------
    // Bounded queue: pushing > kCallerQueueCapacity entries evicts oldest.
    // ----------------------------------------------------------------
    void boundedQueueEvictsOldest() {
        Ft2QsoEngine eng(makeCfg(), nullptr);
        TimePoint synthetic = Clock::now();
        eng.setClockOverride([&]() { return synthetic; });
        eng.enableTx(false); // don't auto-engage, keep them all queued

        // We never enableTx -> stays Idle, queues each CQ.
        for (int i = 0; i < 60; ++i) {
            QString call = QStringLiteral("K%1ABC").arg(i);
            QString msg  = QStringLiteral("CQ %1 FN42").arg(call);
            synthetic += std::chrono::milliseconds(10); // ensure unique enqueue times
            eng.feedParsed({ makeRow(call, msg, -15 - i) });
        }
        QCOMPARE(eng.callerQueueSize(), decodium::ft2::kCallerQueueCapacity);
        // Oldest 10 (K0ABC..K9ABC) must have been evicted.
        auto snap = eng.callerQueueSnapshot();
        for (auto const& e : snap) {
            QVERIFY(e.call != QStringLiteral("K0ABC"));
            QVERIFY(e.call != QStringLiteral("K9ABC"));
        }
    }

    // ----------------------------------------------------------------
    // doubleClick engages a CQ-ing station and emits TX request.
    // ----------------------------------------------------------------
    void doubleClickEngages() {
        Ft2QsoEngine eng(makeCfg(), nullptr);
        QSignalSpy txSpy(&eng, &Ft2QsoEngine::txMessageRequested);
        QSignalSpy dxSpy(&eng, &Ft2QsoEngine::dxCallChanged);

        // doubleClick implies enableTx(true) inside the engine.
        eng.doubleClick("K1ABC", "FN42", 1500, -8);
        QVERIFY(std::holds_alternative<state::ReplyingTx1>(eng.state()));
        QCOMPARE(eng.dxCall(), QStringLiteral("K1ABC"));
        QVERIFY(txSpy.count() >= 1);
        QVERIFY(dxSpy.count() >= 1);
        // First emitted tx should be a real message containing the partner.
        auto args = txSpy.takeFirst();
        QString msg = args.value(1).toString();
        QVERIFY(msg.contains("K1ABC"));
        QVERIFY(msg.contains("IK8TWX"));
    }

    void doubleClickHonorsRequestedTx() {
        Ft2QsoEngine eng(makeCfg(), nullptr);
        QSignalSpy txSpy(&eng, &Ft2QsoEngine::txMessageRequested);

        eng.doubleClick("K1ABC", "FN42", 1500, -8, 1);

        QCOMPARE(eng.currentTx(), 1);
        QVERIFY(eng.isTxEnabled());
        QVERIFY(std::holds_alternative<state::ReplyingTx1>(eng.state()));
        QVERIFY(txSpy.count() >= 1);
        auto args = txSpy.takeFirst();
        QCOMPARE(args.value(0).toInt(), 1);
        QCOMPARE(args.value(1).toString(), QStringLiteral("K1ABC IK8TWX JN70"));
    }

    // ----------------------------------------------------------------
    // promoteCaller: user-chosen station engages immediately, regardless
    // of best-SNR order. Implies enableTx and removes the entry from queue.
    // ----------------------------------------------------------------
    void promoteCallerEngagesImmediately() {
        Ft2QsoEngine eng(makeCfg(), nullptr);
        // Two queued callers: K1ABC at -18 and W2DEF at -5 (W2DEF has best SNR).
        eng.feedParsed({
            makeRow("K1ABC", "CQ K1ABC FN42", -18),
            makeRow("W2DEF", "CQ W2DEF FN20",  -5),
        });
        QCOMPARE(eng.callerQueueSize(), std::size_t{2});
        QVERIFY(std::holds_alternative<state::Idle>(eng.state()));

        // User promotes K1ABC (the lower-SNR one) — engine engages it now.
        eng.promoteCaller("K1ABC");

        QVERIFY(eng.isTxEnabled());
        QVERIFY(std::holds_alternative<state::ReplyingTx1>(eng.state()));
        QCOMPARE(eng.dxCall(), QStringLiteral("K1ABC"));
        // W2DEF must remain queued for later.
        QCOMPARE(eng.callerQueueSize(), std::size_t{1});
    }

    // ----------------------------------------------------------------
    // skipCaller: removes the entry AND short-cooldowns it so a follow-up
    // CQ decode does not re-enqueue the same station immediately.
    // ----------------------------------------------------------------
    void skipCallerRemovesAndCooldowns() {
        Ft2QsoEngine eng(makeCfg(), nullptr);
        TimePoint synthetic = Clock::now();
        eng.setClockOverride([&]() { return synthetic; });

        eng.feedParsed({ makeRow("K1ABC", "CQ K1ABC FN42", -10) });
        QCOMPARE(eng.callerQueueSize(), std::size_t{1});

        eng.skipCaller("K1ABC");
        QCOMPARE(eng.callerQueueSize(), std::size_t{0});

        // Same caller's CQ within the cooldown window must NOT re-enqueue.
        synthetic += std::chrono::seconds(5);
        eng.feedParsed({ makeRow("K1ABC", "CQ K1ABC FN42", -10) });
        QCOMPARE(eng.callerQueueSize(), std::size_t{0});

        // Past the cooldown the caller is allowed back in.
        synthetic += std::chrono::seconds(31);
        eng.feedParsed({ makeRow("K1ABC", "CQ K1ABC FN42", -10) });
        QCOMPARE(eng.callerQueueSize(), std::size_t{1});
    }

    // ----------------------------------------------------------------
    // cancelPendingTx: emits txCancelRequested without touching state.
    // The Bridge slot is responsible for dropping the next outbound TX.
    // ----------------------------------------------------------------
    void cancelPendingTxEmitsSignal() {
        Ft2QsoEngine eng(makeCfg(), nullptr);
        QSignalSpy cancelSpy(&eng, &Ft2QsoEngine::txCancelRequested);

        eng.enableTx(true);
        QVERIFY(std::holds_alternative<state::CallingCq>(eng.state()));
        auto const stateBefore = eng.state().index();

        eng.cancelPendingTx();

        QCOMPARE(cancelSpy.count(), 1);
        // Cancel must not mutate engine state — only the Bridge intercepts TX.
        QCOMPARE(eng.state().index(), stateBefore);
    }

    // ----------------------------------------------------------------
    // TX latency profiling: feed-driven engagement records last/max/avg
    // microseconds; resetTxLatencyStats clears all counters and emits a
    // zero-sample.
    // ----------------------------------------------------------------
    void txLatencyProfilingRecorded() {
        Ft2QsoEngine eng(makeCfg(), nullptr);
        QSignalSpy latSpy(&eng, &Ft2QsoEngine::txLatencyMeasured);
        eng.enableTx(true);
        // enableTx -> CQ went through requestTx but with no
        // m_currentDispatchStart set (not decode-driven), so no sample.
        QCOMPARE(eng.lastTxLatencyUs(), qint64{0});
        QCOMPARE(latSpy.count(), 0);

        // Decode-driven engagement: feedParsed brackets dispatchDecodeToState
        // so requestTx samples the elapsed time.
        eng.feedParsed({ makeRow("K1ABC", "IK8TWX K1ABC FN42") });

        QVERIFY(latSpy.count() >= 1);
        QVERIFY(eng.lastTxLatencyUs() > 0);
        QVERIFY(eng.maxTxLatencyUs() >= eng.lastTxLatencyUs());
        QVERIFY(eng.avgTxLatencyUs() > 0);

        eng.resetTxLatencyStats();
        QCOMPARE(eng.lastTxLatencyUs(), qint64{0});
        QCOMPARE(eng.maxTxLatencyUs(),  qint64{0});
        QCOMPARE(eng.avgTxLatencyUs(),  qint64{0});
    }

    // ----------------------------------------------------------------
    // Ultra2 (QuickQSO) — caller side: doubleClick with default tx skips
    // the grid exchange (TX1) and opens directly with TX2 (report).
    // ----------------------------------------------------------------
    void quickQsoDoubleClickSkipsTx1() {
        EngineConfig cfg = makeCfg();
        cfg.quickQsoEnabled = true;
        Ft2QsoEngine eng(cfg, nullptr);
        QSignalSpy txMsgSpy(&eng, &Ft2QsoEngine::txMessageRequested);
        eng.enableTx(true);

        // requestedTx omitted -> engine should choose TX2 (Ultra2 entry).
        eng.doubleClick("K1ABC", "FN42", 1500, -10, 0);

        QVERIFY(std::holds_alternative<state::AwaitingReport>(eng.state()));
        QCOMPARE(eng.currentTx(), 2);
        // txMsgSpy holds (txNum, msg) per emission; final TX must be TX2.
        QVERIFY(txMsgSpy.count() >= 1);
        auto const last = txMsgSpy.last();
        QCOMPARE(last.at(0).toInt(), 2);
    }

    // ----------------------------------------------------------------
    // Ultra2 (QuickQSO) — CQer side: AwaitingReport sees R+report ->
    // closes via TX3+TU (NOT TX4 RR73). Confirms 2-message exchange.
    // ----------------------------------------------------------------
    void quickQsoSendsTx3WithTuOnRogerReport() {
        EngineConfig cfg = makeCfg();
        cfg.quickQsoEnabled = true;
        Ft2QsoEngine eng(cfg, nullptr);
        QSignalSpy txMsgSpy(&eng, &Ft2QsoEngine::txMessageRequested);
        eng.enableTx(true);

        eng.feedParsed({ makeRow("K1ABC", "IK8TWX K1ABC FN42") });
        QVERIFY(std::holds_alternative<state::AwaitingReport>(eng.state()));

        eng.feedParsed({ makeRow("K1ABC", "IK8TWX K1ABC R-12") });

        QVERIFY(std::holds_alternative<state::Closing>(eng.state()));
        QCOMPARE(eng.currentTx(), 3);
        QVERIFY(txMsgSpy.count() >= 1);
        QString const lastMsg = txMsgSpy.last().at(1).toString();
        QVERIFY2(lastMsg.endsWith(QStringLiteral(" TU")),
                 qPrintable(QStringLiteral("expected TX3 to end with TU, got: %1").arg(lastMsg)));
        QVERIFY2(lastMsg.contains(QStringLiteral("R-12")),
                 qPrintable(QStringLiteral("expected TX3 to echo received report, got: %1").arg(lastMsg)));
    }

    // ----------------------------------------------------------------
    // Ultra2 (QuickQSO): runtime config flip via setConfig propagates
    // to subsequent QSOs without rebuilding the engine.
    // ----------------------------------------------------------------
    void quickQsoConfigFlipTakesEffect() {
        EngineConfig cfg = makeCfg();
        cfg.quickQsoEnabled = false;
        Ft2QsoEngine eng(cfg, nullptr);
        QSignalSpy txMsgSpy(&eng, &Ft2QsoEngine::txMessageRequested);
        eng.enableTx(true);

        // Standard mode QSO: AwaitingReport -> R+rep -> TX4 RR73.
        eng.feedParsed({ makeRow("K1ABC", "IK8TWX K1ABC FN42") });
        eng.feedParsed({ makeRow("K1ABC", "IK8TWX K1ABC R-12") });
        QCOMPARE(eng.currentTx(), 4);

        // Flip to Ultra2 mid-session, run a fresh QSO with K2DEF.
        cfg.quickQsoEnabled = true;
        eng.setConfig(cfg);
        // Drain Closing -> Idle by ticking past kCloseDrainAfter (8s).
        TimePoint synthetic = Clock::now() + std::chrono::seconds(20);
        eng.setClockOverride([&]() { return synthetic; });
        eng.tick();
        eng.tick();

        eng.feedParsed({ makeRow("K2DEF", "IK8TWX K2DEF FN30") });
        QVERIFY(std::holds_alternative<state::AwaitingReport>(eng.state()));
        eng.feedParsed({ makeRow("K2DEF", "IK8TWX K2DEF R-08") });

        QVERIFY(std::holds_alternative<state::Closing>(eng.state()));
        QCOMPARE(eng.currentTx(), 3);
        QString const lastMsg = txMsgSpy.last().at(1).toString();
        QVERIFY(lastMsg.endsWith(QStringLiteral(" TU")));
    }

    // ----------------------------------------------------------------
    // Real-signal report formatting: the SNR we report on TX2 must match
    // WSJT-X conventions (sign + zero-padded 2 digits). Positive SNR must
    // not be flipped to negative; small magnitudes must be padded.
    //   snr=+5  -> "+05"
    //   snr=-3  -> "-03"
    // Regression: prior code used QString::arg(int) which dropped the sign
    // and padding, then fmtReport prepended "-" to anything sign-less,
    // turning "+5" into "-5".
    // ----------------------------------------------------------------
    void txReportEncodesRealSnrSignAndPadding() {
        Ft2QsoEngine eng(makeCfg(), nullptr);
        QSignalSpy txMsgSpy(&eng, &Ft2QsoEngine::txMessageRequested);
        eng.enableTx(true);

        // CQer side: partner sends grid; engine answers TX2 with their SNR.
        eng.feedParsed({ makeRow("K1ABC", "IK8TWX K1ABC FN42", /*snr=*/5) });
        QCOMPARE(eng.currentTx(), 2);
        QString const tx2 = txMsgSpy.last().at(1).toString();
        QVERIFY2(tx2.contains(QStringLiteral("+05")),
                 qPrintable(QStringLiteral("expected '+05' in TX2 for snr=+5, got: %1").arg(tx2)));
        QVERIFY2(!tx2.contains(QStringLiteral("-05")),
                 qPrintable(QStringLiteral("sign flipped: TX2 contains -05 for snr=+5, got: %1").arg(tx2)));

        // Fresh engine for a single-digit negative SNR -> -03 padded.
        Ft2QsoEngine eng2(makeCfg("IK1XYZ", "JN35"), nullptr);
        QSignalSpy spy2(&eng2, &Ft2QsoEngine::txMessageRequested);
        eng2.enableTx(true);
        eng2.feedParsed({ makeRow("K2DEF", "IK1XYZ K2DEF FN30", /*snr=*/-3) });
        QString const tx2neg = spy2.last().at(1).toString();
        QVERIFY2(tx2neg.contains(QStringLiteral("-03")),
                 qPrintable(QStringLiteral("expected '-03' in TX2 for snr=-3, got: %1").arg(tx2neg)));
    }

    // ----------------------------------------------------------------
    // Watchdog: state stuck beyond budget falls back to Idle/CallingCq.
    // ----------------------------------------------------------------
    void watchdogFallback() {
        Ft2QsoEngine eng(makeCfg(), nullptr);
        TimePoint synthetic = Clock::now();
        eng.setClockOverride([&]() { return synthetic; });
        eng.enableTx(true);

        eng.feedParsed({ makeRow("K1ABC", "IK8TWX K1ABC FN42") });
        QVERIFY(std::holds_alternative<state::AwaitingReport>(eng.state()));

        // Advance well past kWatchdogPerStateMax (60s).
        synthetic += std::chrono::seconds(75);
        eng.tick();

        // TX still enabled -> we should fall back to CallingCq (CQ TX6).
        QVERIFY(std::holds_alternative<state::CallingCq>(eng.state()));
    }

    // ----------------------------------------------------------------
    // Closing TX-end short-circuit: when the backend signals TX-begin then
    // TX-end inside Closing, the engine drains immediately on the next tick
    // instead of waiting for kCloseDrainAfter (7s safety fallback). This
    // prevents the doubled RR73 caused by the safety timer straddling two
    // FT2 slot boundaries (observed live as RR73 transmitted twice).
    // ----------------------------------------------------------------
    void closingShortCircuitsOnBackendTxEnd() {
        Ft2QsoEngine eng(makeCfg(), nullptr);
        TimePoint synthetic = Clock::now();
        eng.setClockOverride([&]() { return synthetic; });
        eng.enableTx(true);

        // Drive a QSO into Closing: K1ABC sends grid then R+report.
        eng.feedParsed({ makeRow("K1ABC", "IK8TWX K1ABC FN42") });
        QVERIFY(std::holds_alternative<state::AwaitingReport>(eng.state()));
        eng.feedParsed({ makeRow("K1ABC", "IK8TWX K1ABC R-12") });
        QVERIFY(std::holds_alternative<state::Closing>(eng.state()));
        QCOMPARE(eng.currentTx(), 4);

        // Tick once at t=0: Closing should NOT drain (timer not elapsed,
        // backend hasn't signalled TX-begin/end yet).
        synthetic += std::chrono::milliseconds(500);
        eng.tick();
        QVERIFY(std::holds_alternative<state::Closing>(eng.state()));

        // Backend keys TX (slot starts), then unkeys TX (slot ends) — the
        // Bridge would forward both events as the m_transmitting bool flips.
        synthetic += std::chrono::milliseconds(500);
        eng.notifyBackendTxStateChanged(true);   // TX begin
        synthetic += std::chrono::milliseconds(3750); // 1 FT2 slot
        eng.notifyBackendTxStateChanged(false);  // TX end -> short-circuit

        // Next tick should drain immediately, well before kCloseDrainAfter.
        synthetic += std::chrono::milliseconds(100);
        eng.tick();
        QVERIFY2(!std::holds_alternative<state::Closing>(eng.state()),
                 "Closing should drain on backend TX-end, not wait for timer");
    }

    // ----------------------------------------------------------------
    // Spurious TX-state changes outside Closing must be ignored — they
    // belong to a separate QSO (e.g. a manual transmit) and must not
    // affect future Closing states.
    // ----------------------------------------------------------------
    void notifyTxStateIgnoredOutsideClosing() {
        Ft2QsoEngine eng(makeCfg(), nullptr);
        eng.enableTx(true);

        // Fire spurious TX-state events while NOT in Closing (engine is in
        // CallingCq after enableTx(true)). These must be no-ops.
        QVERIFY(!std::holds_alternative<state::Closing>(eng.state()));
        eng.notifyBackendTxStateChanged(true);
        eng.notifyBackendTxStateChanged(false);
        QVERIFY(!std::holds_alternative<state::Closing>(eng.state()));

        // Now drive into Closing. The flags must be fresh (reset on entry),
        // so the next TX-end after entry triggers the short-circuit cleanly
        // without being prematurely armed by the spurious events above.
        TimePoint synthetic = Clock::now();
        eng.setClockOverride([&]() { return synthetic; });
        eng.feedParsed({ makeRow("K1ABC", "IK8TWX K1ABC FN42") });
        eng.feedParsed({ makeRow("K1ABC", "IK8TWX K1ABC R-12") });
        QVERIFY(std::holds_alternative<state::Closing>(eng.state()));

        // A bare TX-end without a preceding TX-begin (inside Closing) must
        // NOT short-circuit — could be the tail of a pre-Closing transmit.
        eng.notifyBackendTxStateChanged(false);
        synthetic += std::chrono::milliseconds(100);
        eng.tick();
        QVERIFY(std::holds_alternative<state::Closing>(eng.state()));
    }
};

QTEST_MAIN(Ft2EngineTest)
#include "test_ft2_engine.moc"
