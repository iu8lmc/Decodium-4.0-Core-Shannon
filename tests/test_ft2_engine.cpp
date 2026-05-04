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
};

QTEST_MAIN(Ft2EngineTest)
#include "test_ft2_engine.moc"
