#include <QtTest>
#include <QFile>

#include "src/bridge/Ft8AutoTxDecodeGate.h"

using Gate = decodium::Ft8AutoTxDecodeGate;
using Decision = Gate::Decision;

class TestFt8AutoTxDecodeGate : public QObject
{
    Q_OBJECT
private slots:
    void diagnosticReplay_data()
    {
        QTest::addColumn<int>("oldCommitMs");
        QTest::addColumn<int>("responseMs");
        QTest::addColumn<QString>("oldPayload");
        QTest::addColumn<QString>("newPayload");
        // EA5IL's diagnostic (21): actual auto-sequence processing offsets.
        QTest::newRow("SQ7KMT-RR73") << 384 << 637
            << QString("SQ7KMT EA5IL R-04") << QString("SQ7KMT EA5IL 73");
        QTest::newRow("YO2LOG-report") << 393 << 674
            << QString("YO2LOG EA5IL IM99") << QString("YO2LOG EA5IL R-01");
        QTest::newRow("YO2LOG-RR73") << 384 << 560
            << QString("YO2LOG EA5IL R+01") << QString("YO2LOG EA5IL 73");
    }

    void diagnosticReplay()
    {
        QFETCH(int, oldCommitMs);
        QFETCH(int, responseMs);
        QFETCH(QString, oldPayload);
        QFETCH(QString, newPayload);
        Gate gate;
        Gate::Context const context {12345, 2, 7};
        QString selected = oldPayload;
        QStringList transmitted;
        auto dispatch = [&](int elapsed) {
            if (gate.decision(context, elapsed) == Decision::Ready) {
                transmitted.append(selected);
                gate.committed(context);
            }
        };
        QVERIFY(gate.armTimer(context));
        QCOMPARE(gate.decision(context, oldCommitMs), Decision::Wait);
        dispatch(oldCommitMs); // Old unconditional fallback MUST NOT key PTT.
        QVERIFY(transmitted.isEmpty());
        selected = newPayload; // Auto-seq applies the received report/signoff first.
        gate.decoded(context, responseMs);
        dispatch(responseMs);
        QCOMPARE(transmitted, QStringList {newPayload});
        dispatch(1200); // Stale fallback cannot send a second time.
        gate.decoded(context, responseMs + 100);
        dispatch(responseMs + 100); // Late decode cannot replace an active frame.
        QCOMPARE(transmitted.size(), 1);
    }

    void noResultSkipsSlotButCompletedEmptyDecodeAllowsRetry()
    {
        Gate gate;
        Gate::Context const context {12345, 2, 7};
        QVERIFY(gate.armTimer(context));
        QVERIFY(!gate.armTimer(context));
        QCOMPARE(gate.decision(context, 1199), Decision::Wait);
        QCOMPARE(gate.decision(context, 1200), Decision::SkipSlot);
        gate.decoded(context, 1201);
        QCOMPARE(gate.decision(context, 1201), Decision::SkipSlot);
        // A stalled decoder must not hold all future slots.
        Gate::Context const next {12347, 2, 7};
        QVERIFY(gate.armTimer(next));
        gate.decoded(next, 520); // The final pass completed with zero rows.
        QCOMPARE(gate.decision(next, 520), Decision::Ready);
    }

    void deadlinesAreAbsoluteAndBounded()
    {
        Gate gate;
        Gate::Context const context {12345, 2, 7};
        gate.decoded(context, 1199);
        QCOMPARE(gate.decision(context, 1399), Decision::Ready);
        QCOMPARE(gate.decision(context, 1400), Decision::SkipSlot);
        QCOMPARE(gate.decision(context, 5000), Decision::SkipSlot);
        QCOMPARE(gate.decision(context, -1), Decision::SkipSlot);
        QVERIFY(gate.armTimer(context)); // Scheduling cannot clear a ready result.
        QCOMPARE(gate.decision(context, 1200), Decision::Ready);
    }

    void slotSessionAndHaltGenerationCannotReuseReadyState()
    {
        Gate::Context const context {12345, 2, 7};
        for (auto const other : {Gate::Context {12346, 2, 7},
                                Gate::Context {12344, 2, 7}, // clock moved backwards
                                Gate::Context {12345, 3, 7}, // monitoring/mode restart
                                Gate::Context {12345, 2, 8}}) { // HALT/reset QSO
            Gate gate;
            gate.decoded(context, 600);
            QVERIFY(!gate.matches(other));
            QCOMPARE(gate.decision(other, 610), Decision::Wait);
            QVERIFY(gate.armTimer(other));
            QVERIFY(!gate.matches(context)); // old timer must be discarded
            gate.reset();
            QCOMPARE(gate.decision(other, 610), Decision::Wait);
        }
    }

    void aLateDecoderDoesNotStarveTheSequencer()
    {
        // eldiablo.log, 26/10/2026: the decode took 2.3-2.9 s against a 1.2 s deadline,
        // so TX5 (73 to R6ACD) was skipped slot after slot and the QSO never closed.
        Gate gate;
        Gate::Context const rx {1000, 2, 7};
        Gate::Context const tx {1001, 2, 7};
        gate.decoded(rx, 800);                              // healthy decoder
        QCOMPARE(gate.lateStreak(), 0);
        QVERIFY(gate.armTimer(tx));
        QCOMPARE(gate.decision(tx, 1100, true), Decision::Wait);
        gate.deadlineExpired(tx);                           // first late slot
        QCOMPARE(gate.lateStreak(), 1);
        QCOMPARE(gate.decision(tx, 1200, true), Decision::Ready);   // 73/RR73 goes out
        QCOMPARE(gate.decision(tx, 1200, false), Decision::SkipSlot); // others still wait
        QCOMPARE(gate.decision(tx, 1400, true), Decision::SkipSlot);  // never past the commit deadline
        gate.decoded(tx, 2450);                             // the late result of the same slot
        QCOMPARE(gate.lateStreak(), 1);                     // one miss per slot
        gate.committed(tx);
        QCOMPARE(gate.decision(tx, 1250, true), Decision::SkipSlot);  // no second frame
        Gate::Context const rx2 {1002, 2, 7};
        gate.decoded(rx2, 2268);                            // second late slot
        QCOMPARE(gate.lateStreak(), 2);
        Gate::Context const tx2 {1003, 2, 7};
        QCOMPARE(gate.decision(tx2, 100, false), Decision::Ready);   // now any message
        gate.decoded(tx2, 800);                             // the decoder recovers
        QCOMPARE(gate.lateStreak(), 0);
        Gate::Context const tx3 {1005, 2, 7};
        QCOMPARE(gate.decision(tx3, 100, false), Decision::Wait);
        QCOMPARE(gate.decision(tx3, 100, true), Decision::Wait);
    }

    void aDecoderHoveringAroundTheDeadlineKeepsTheCycle()
    {
        // mattei.log, 27/10/2026: results at 1.1-1.7 s against a 1.2 s deadline lost
        // every other TX cycle to a few milliseconds ("waits 4 turns to call again").
        Gate gate;
        int const results[] = {1225, 1678, 1155, 1101, 2181, 1357};
        std::int64_t slot = 1000;
        for (int elapsed : results) gate.decoded(Gate::Context {slot++, 2, 7}, elapsed);
        QVERIFY(gate.slowDecoder());
        QCOMPARE(gate.decodeDeadline(), 1400);
        QCOMPARE(gate.commitDeadline(), 1800);
        Gate::Context const tx {slot + 1, 2, 7};
        QVERIFY(gate.armTimer(tx));
        QCOMPARE(gate.decision(tx, 1250, false), Decision::Wait);   // used to be SkipSlot
        gate.decoded(tx, 1300);                                     // result in the extended window
        QVERIFY(gate.resultReady(tx));
        QCOMPARE(gate.decision(tx, 1300, false), Decision::Ready);
        // no result at all: send at the extended deadline rather than lose the cycle
        Gate::Context const tx2 {slot + 3, 2, 7};
        QCOMPARE(gate.decision(tx2, 1399, false), Decision::Wait);
        QCOMPARE(gate.decision(tx2, 1400, false), Decision::Ready);
        QVERIFY(!gate.resultReady(tx2));
        QCOMPARE(gate.decision(tx2, 1800, false), Decision::SkipSlot);  // never past the commit deadline
    }

    void aNewSlotOrQsoDoesNotForgetHowSlowTheDecoderIs()
    {
        // IT9MRM, 721: after every double click, HALT or finished QSO the learned speed was
        // wiped and the next two cycles were skipped again before it was learned anew.
        Gate gate;
        std::int64_t slot = 3000;
        for (int elapsed : {1167, 1228, 1153, 1257, 1202, 1317})
            gate.decoded(Gate::Context {slot++, 2, 7}, elapsed);
        QVERIFY(gate.slowDecoder());
        gate.resetKeepingSpeed();                       // what a new QSO or a HALT now does
        QVERIFY(gate.slowDecoder());
        Gate::Context const tx {slot + 2, 2, 7};
        QCOMPARE(gate.decision(tx, 1250, false), Decision::Wait);   // not SkipSlot
        QCOMPARE(gate.decision(tx, 1400, false), Decision::Ready);  // and it sends, not skips
        gate.reset();                                   // a full reset still forgets everything
        QVERIFY(!gate.slowDecoder());
        QCOMPARE(gate.decision(tx, 1250, false), Decision::SkipSlot);
    }

    void aHealthyDecoderKeepsTheOriginalDeadlines()
    {
        Gate gate;
        std::int64_t slot = 2000;
        for (int elapsed : {430, 620, 510, 1300, 480, 590, 700}) // one late sample only
            gate.decoded(Gate::Context {slot++, 2, 7}, elapsed);
        QVERIFY(!gate.slowDecoder());
        QCOMPARE(gate.decodeDeadline(), 1200);
        QCOMPARE(gate.commitDeadline(), 1400);
        Gate::Context const tx {slot + 1, 2, 7};
        QCOMPARE(gate.decision(tx, 1200, false), Decision::SkipSlot);
        QCOMPARE(gate.decision(tx, 1000, false), Decision::Wait);
    }

    void completeWaveformStaysInsideSlot()
    {
        constexpr int payloadMs = 79 * 160; // Complete FT8 frame, including Costas.
        for (int elapsed : {500, 674, 900, 1200, 1400, 1800})
            QVERIFY(Gate::fullFrameFits(elapsed, payloadMs));
        QVERIFY(!Gate::fullFrameFits(1801, payloadMs));
        QVERIFY(!Gate::fullFrameFits(15000, payloadMs));
        QVERIFY(!Gate::fullFrameFits(1800, 13000));
        QVERIFY(!Gate::fullFrameFits(500, 0));
    }

    void bridgeWiring()
    {
        QFile file(QStringLiteral(DECODIUM_SOURCE_DIR "/src/bridge/DecodiumBridge.cpp"));
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        QString const cpp = QString::fromUtf8(file.readAll());
        auto body = [&](const char* begin, const char* end) {
            int const from = cpp.indexOf(QLatin1String(begin));
            int const to = cpp.indexOf(QLatin1String(end), from + 1);
            return from >= 0 && to > from ? cpp.mid(from, to - from) : QString();
        };
        for (auto const& entry : {
                 body("void DecodiumBridge::startTx()", "void DecodiumBridge::sendTx(int n)"),
                 body("void DecodiumBridge::checkAndStartPeriodicTx()", "void DecodiumBridge::autoSequenceStep(")})
            QVERIFY(entry.contains(QStringLiteral("deferFt8AutoTxForDecode()")));
        QString const ready = body("void DecodiumBridge::onFt8DecodeReady(", "void DecodiumBridge::onFt2DecodeReady(");
        QVERIFY(ready.contains(QStringLiteral("serial == m_ft8AutoTxFinalDecodeSerial")));
        QVERIFY(ready.contains(QStringLiteral("context.slot - 1")));
        QVERIFY(ready.indexOf(QStringLiteral("autoSequenceStep(fields)"))
                < ready.indexOf(QStringLiteral("m_ft8AutoTxDecodeGate.decoded")));
        QVERIFY(ready.contains(QStringLiteral("autoSeqGotResponse || ft8AutoTxDecodeSettled")));
        QString const defer = body("bool DecodiumBridge::deferFt8AutoTxForDecode()",
                                   "bool DecodiumBridge::shouldDeferAutoTxUntilTimeSyncDecode(");
        QVERIFY(defer.contains(QStringLiteral("m_ft8AutoTxDecodeGate.deadlineExpired(context)")));
        QVERIFY(defer.contains(QStringLiteral("decision(context, elapsedMs, finalMessage)")));
        QVERIFY(defer.contains(QStringLiteral("m_ft8AutoTxDecodeGate.decodeDeadline()")));
        QVERIFY(defer.contains(QStringLiteral("resultReady(context)")));
        // the learned decoder speed survives the reset done at every new QSO, HALT or click
        QVERIFY(cpp.contains(QStringLiteral("m_ft8AutoTxDecodeGate.resetKeepingSpeed();")));
        QVERIFY(!cpp.contains(QStringLiteral("m_ft8AutoTxDecodeGate.reset();")));
        QCOMPARE(cpp.count(QStringLiteral("syncTxCommitDeadlineMs(")), 3); // helper and both guards
        QCOMPARE(cpp.count(QStringLiteral("validateFt8AutoTxAudioStart(txPlaybackMs)")), 3); // macOS, Windows, Linux
        QVERIFY(cpp.contains(QStringLiteral("m_activeFt8AutoTxFullFrame) return 0")));
        // 1.0.722: the gate is off unless explicitly enabled for testing
        QString const uses = body("bool DecodiumBridge::usesFt8AutoTxDecodeGate() const", "decodium::Ft8AutoTxDecodeGate::Context DecodiumBridge::ft8AutoTxDecodeContext()");
        QVERIFY(uses.contains(QStringLiteral("truthyEnvironmentFlag(\"DECODIUM_FT8_TX_GATE\")")));
        QVERIFY(uses.indexOf(QStringLiteral("if (!gateEnabled)")) < uses.indexOf(QStringLiteral("m_mode == QStringLiteral(\"FT8\")")));
        QString const deferred = body("bool DecodiumBridge::tryStartDeferredManualSyncTx()",
                                      "void DecodiumBridge::clearDeferredManualSyncTx(");
        QVERIFY(deferred.contains(QStringLiteral("auto-ft8-rx-rendezvous")));
        QVERIFY(deferred.contains(QStringLiteral("checkAndStartPeriodicTx()")));
    }
};

QTEST_GUILESS_MAIN(TestFt8AutoTxDecodeGate)
#include "test_ft8_auto_tx_decode_gate.moc"
