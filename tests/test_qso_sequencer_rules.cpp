// Fase 1 port mobile — test unitari delle regole pure del sequencer estratte
// in Sequencer/QsoSequencerRules.cpp (step A strangler). Le attese fotografano
// il comportamento storico (1.0.311/315, 1.0.437 weak boost, 1.0.289 quick
// give-up, 1.0.174 QSB fallback): se un refactor le cambia, il test deve
// fallire PRIMA che la regressione arrivi in release.
#include <QtTest>

#include "Sequencer/QsoSequencerRules.hpp"
#include "Sequencer/MessageTokenRules.hpp"
#include "Sequencer/QsoSequencerState.hpp"
#include "Sequencer/ISequencerSink.hpp"
#include "Sequencer/QsoSequencer.hpp"

// Sink finto per i test: registra le chiamate senza dipendenze desktop.
// È l'harness che lo step D userà per validare la logica estratta.
class FakeSink final : public decodium::seq::ISequencerSink
{
public:
    int txCount {0}, haltCount {0}, logCount {0}, stateCount {0}, diagCount {0}, scheduleCount {0};
    int lastTxNum {-1}, lastAudioFreq {-1}, lastProgress {-1}, lastScheduleMs {-1};
    QString lastTxMessage, lastHaltReason, lastDxCall, lastDiag;

    void requestTransmit(int txNum, const QString& message, int audioFreqHz) override
    { ++txCount; lastTxNum = txNum; lastTxMessage = message; lastAudioFreq = audioFreqHz; }
    void requestHaltTx(const QString& reason) override
    { ++haltCount; lastHaltReason = reason; }
    void requestLogQso(decodium::seq::LogQsoReason, bool) override { ++logCount; }
    void stateChanged(int, int qsoProgress, const QString& dxCall) override
    { ++stateCount; lastProgress = qsoProgress; lastDxCall = dxCall; }
    void diag(const QString& line) override { ++diagCount; lastDiag = line; }
    void scheduleCallback(int delayMs, int) override { ++scheduleCount; lastScheduleMs = delayMs; }
};

using decodium::seq::deferredSignoffRetryCapForMode;
using decodium::seq::remapRequestedTxStep;
using decodium::seq::qsoProgressForTxStep;
using decodium::seq::isTxStepDisabledInMask;
using decodium::seq::TxStepRemapReason;

class TestQsoSequencerRules final : public QObject
{
  Q_OBJECT

private slots:
  void ftxUserCapIsAbsolute ()
  {
    // FT2/FT4/FT8: lo spinbox utente è un valore assoluto, conservative NON lo gonfia
    QCOMPARE (deferredSignoffRetryCapForMode ("FT2", 10, 127, false, false, 4, 4, 3), 4);
    QCOMPARE (deferredSignoffRetryCapForMode ("FT4", 10, 127, false, false, 4, 4, 3), 4);
    QCOMPARE (deferredSignoffRetryCapForMode ("FT8", 10, 127, false, false, 4, 4, 3), 3);
    QCOMPARE (deferredSignoffRetryCapForMode ("ft8 ", 10, 127, false, false, 4, 4, 3), 3); // trim+upper
    QCOMPARE (deferredSignoffRetryCapForMode ("FT2", 10, -20, true, false, 4, 4, 3), 4);   // conservative ignorato
  }

  void ftxUserCapClamped ()
  {
    QCOMPARE (deferredSignoffRetryCapForMode ("FT2", 10, 127, false, false, 0, 4, 3), 1);   // min 1
    QCOMPARE (deferredSignoffRetryCapForMode ("FT2", 10, 127, false, false, 99, 4, 3), 8);  // max 8
  }

  void weakBoostOptIn ()
  {
    // 1.0.437: weakBoost ON + partner <= soglia -> cap + bonus (clampato a 8)
    QCOMPARE (deferredSignoffRetryCapForMode ("FT8", 10, -18, false, false, 4, 4, 3, true, -15, 3), 6);
    QCOMPARE (deferredSignoffRetryCapForMode ("FT8", 10, -14, false, false, 4, 4, 3, true, -15, 3), 3);  // sopra soglia
    QCOMPARE (deferredSignoffRetryCapForMode ("FT8", 10, 127, false, false, 4, 4, 3, true, -15, 3), 3);  // SNR ignoto
    QCOMPARE (deferredSignoffRetryCapForMode ("FT2", 10, -20, false, false, 7, 4, 3, true, -15, 6), 8);  // clamp 8
  }

  void quickGiveUpStrongOnlyReduces ()
  {
    // 1.0.289: partner forte (SNR>0) -> cap ridotto a 4, mai alzato
    QCOMPARE (deferredSignoffRetryCapForMode ("FT2", 10, 5, false, true, 8, 4, 3), 4);
    QCOMPARE (deferredSignoffRetryCapForMode ("FT8", 10, 5, false, true, 4, 4, 2), 2);   // già sotto 4
    QCOMPARE (deferredSignoffRetryCapForMode ("FT2", 10, -5, false, true, 8, 4, 3), 8);  // partner debole: no
  }

  void legacyFallbackQsbAware ()
  {
    // Modi non-FTX: modeCap 3 + extra QSB (1.0.174) + conservative
    QCOMPARE (deferredSignoffRetryCapForMode ("Q65", 10), 3);
    QCOMPARE (deferredSignoffRetryCapForMode ("Q65", 10, -12), 4);          // -12 -> +1
    QCOMPARE (deferredSignoffRetryCapForMode ("Q65", 10, -18), 7);          // -18 -> +4
    QCOMPARE (deferredSignoffRetryCapForMode ("Q65", 10, -30), 7);          // clamp +4
    QCOMPARE (deferredSignoffRetryCapForMode ("Q65", 10, 127, true), 5);    // conservative +2
    QCOMPARE (deferredSignoffRetryCapForMode ("MSK144", 10, -18, true), 9); // 3+4+2
    QCOMPARE (deferredSignoffRetryCapForMode ("Q65", 10, 5, false, true), 3);  // strong give-up: min(3,4)
  }

  // ---- Step A2: regole advanceQsoState ----

  void txStepRemapQuickQso ()
  {
    auto r = remapRequestedTxStep (1, true, 0);
    QCOMPARE (r.txNum, 2);
    QVERIFY (r.reason == TxStepRemapReason::QuickQsoSkipTx1);
    // QuickQSO ha precedenza sul fallback Tx1-disabled (else-if originale)
    r = remapRequestedTxStep (1, true, 0b000001);
    QVERIFY (r.reason == TxStepRemapReason::QuickQsoSkipTx1);
    // Solo TX1 viene rimappato
    r = remapRequestedTxStep (3, true, 0);
    QCOMPARE (r.txNum, 3);
    QVERIFY (r.reason == TxStepRemapReason::None);
  }

  void txStepRemapTx1Disabled ()
  {
    // 1.0.379: Tx1 off + Tx2 on -> risposta parte da TX2
    auto r = remapRequestedTxStep (1, false, 0b000001);
    QCOMPARE (r.txNum, 2);
    QVERIFY (r.reason == TxStepRemapReason::Tx1DisabledFallback);
    // Tx1 off MA anche Tx2 off -> nessun fallback (comportamento originale)
    r = remapRequestedTxStep (1, false, 0b000011);
    QCOMPARE (r.txNum, 1);
    QVERIFY (r.reason == TxStepRemapReason::None);
    // Tutto abilitato -> invariato
    r = remapRequestedTxStep (1, false, 0);
    QCOMPARE (r.txNum, 1);
    QVERIFY (r.reason == TxStepRemapReason::None);
  }

  void txDisabledMaskBits ()
  {
    QVERIFY (isTxStepDisabledInMask (1, 0b000001));
    QVERIFY (isTxStepDisabledInMask (6, 0b100000));
    QVERIFY (!isTxStepDisabledInMask (2, 0b000001));
    QVERIFY (!isTxStepDisabledInMask (0, 0xFF));   // fuori range
    QVERIFY (!isTxStepDisabledInMask (7, 0xFF));
  }

  void qsoProgressMap ()
  {
    QCOMPARE (qsoProgressForTxStep (1), 2);  // REPLYING
    QCOMPARE (qsoProgressForTxStep (2), 3);  // REPORT
    QCOMPARE (qsoProgressForTxStep (3), 4);  // ROGER_REPORT
    QCOMPARE (qsoProgressForTxStep (4), 5);  // SIGNOFF
    QCOMPARE (qsoProgressForTxStep (5), 5);  // SIGNOFF
    QCOMPARE (qsoProgressForTxStep (6), 1);  // CALLING_CQ
    QCOMPARE (qsoProgressForTxStep (0), -1);
    QCOMPARE (qsoProgressForTxStep (7), -1);
  }

  // ---- Step A3: famiglia parsing token/messaggi ----

  void baseCallNormalization ()
  {
    using decodium::seq::normalizedBaseCall;
    QCOMPARE (normalizedBaseCall ("IU8LMC"), QString ("IU8LMC"));
    QCOMPARE (normalizedBaseCall ("iu8lmc/p"), QString ("IU8LMC"));
    QCOMPARE (normalizedBaseCall ("<IU8LMC>"), QString ("IU8LMC"));
  }

  void messageContainsCall ()
  {
    using decodium::seq::messageContainsCallToken;
    QVERIFY (messageContainsCallToken ("IK8OLM IU8LMC JN70", "IU8LMC", "IU8LMC"));
    QVERIFY (messageContainsCallToken ("IK8OLM IU8LMC/P -12", "IU8LMC/P", "IU8LMC"));
    QVERIFY (!messageContainsCallToken ("CQ IZ0ABC JN61", "IU8LMC", "IU8LMC"));
  }

  void signoffPayloadDetection ()
  {
    using decodium::seq::messageCarries73Payload;
    using decodium::seq::messageCarries73PayloadForCall;
    QVERIFY (messageCarries73Payload ("IK8OLM IU8LMC 73"));
    QVERIFY (messageCarries73Payload ("IK8OLM IU8LMC RR73"));
    QVERIFY (!messageCarries73Payload ("IK8OLM IU8LMC -15"));
    QVERIFY (!messageCarries73Payload ("CQ IU8LMC JN70"));
    QVERIFY (messageCarries73PayloadForCall ("IK8OLM IU8LMC RR73", "IK8OLM", "IK8OLM"));
    QVERIFY (!messageCarries73PayloadForCall ("IZ0ABC IW0XYZ RR73", "IK8OLM", "IK8OLM"));
  }

  void directedPeerExtraction ()
  {
    using decodium::seq::directedPeerTokenFromMessage;
    QCOMPARE (directedPeerTokenFromMessage ("IU8LMC IK8OLM -10", "IU8LMC", "IU8LMC"),
              QString ("IK8OLM"));
  }

  void uncommonAndPortableCallsignParsing ()
  {
    using decodium::seq::decodedDxCallToken;
    using decodium::seq::isPlausibleDecodedCallsignToken;

    QVERIFY (isPlausibleDecodedCallsignToken ("8B8FTDM"));
    QVERIFY (isPlausibleDecodedCallsignToken ("8D8DADA"));
    QVERIFY (isPlausibleDecodedCallsignToken ("8A3B"));
    QVERIFY (isPlausibleDecodedCallsignToken ("IZ1ABC/0"));
    QVERIFY (isPlausibleDecodedCallsignToken ("IZ1ABC/1"));
    QVERIFY (isPlausibleDecodedCallsignToken ("8A81JK/LH"));
    QVERIFY (isPlausibleDecodedCallsignToken ("8B81JB/LH"));
    QVERIFY (isPlausibleDecodedCallsignToken ("8A1AA/LH"));
    QVERIFY (isPlausibleDecodedCallsignToken ("8A1AAA/LH"));
    QVERIFY (!isPlausibleDecodedCallsignToken ("ABCDEF12"));

    QCOMPARE (decodedDxCallToken ("CQ 8B8FTDM OI33"), QString ("8B8FTDM"));
    QCOMPARE (decodedDxCallToken ("CQ 8D8DADA OI33"), QString ("8D8DADA"));
    QCOMPARE (decodedDxCallToken ("CQ 8A3B OI62"), QString ("8A3B"));
    QCOMPARE (decodedDxCallToken ("IU8LMC IZ1ABC/0 -10"), QString ("IZ1ABC/0"));
    QCOMPARE (decodedDxCallToken ("IU8LMC IZ1ABC/1 RR73"), QString ("IZ1ABC/1"));
    QCOMPARE (decodedDxCallToken ("BG5JGG RU6AGR RR73"), QString ("RU6AGR"));
    QCOMPARE (decodedDxCallToken ("IZ1JIZ VU33IN RR73"), QString ("VU33IN"));

    // If the station on the right is unknown, never display the left-hand
    // station as its DXCC identity.
    QVERIFY (decodedDxCallToken ("IU8LMC BADTOKEN RR73").isEmpty ());
    QVERIFY (decodedDxCallToken ("IU8LMC <...> -10").isEmpty ());
  }

  // ---- Step C: seam ISequencerSink + QsoSequencer (contratto) ----

  void sequencerStateDefaults ()
  {
    // La struct raggruppata (step B/B2/B3) parte con i default storici.
    decodium::seq::QsoSequencerState st;
    QCOMPARE (st.currentTx, 1);
    QCOMPARE (st.qsoProgress, 0);
    QCOMPARE (st.nTx73, 0);
    QCOMPARE (st.lastNtx, -1);
    QCOMPARE (st.reportReceived, QString ("-10"));
    QVERIFY (st.sendRR73);
    QVERIFY (!st.qsoLogged);
    QVERIFY (st.qsoFirstReplyOn.isNull());
    QVERIFY (!st.ft2DeferredLogPending);
  }

  void qsoLogTimePrefersFirstReply ()
  {
    decodium::seq::QsoSequencerState st;
    QDateTime const initiated {
        QDate(2026, 8, 10), QTime(12, 0, 0), QTimeZone::UTC};
    QDateTime const firstReply {
        QDate(2026, 8, 10), QTime(12, 7, 30), QTimeZone::UTC};

    st.qsoStartedOn = initiated;
    QCOMPARE(st.effectiveQsoLogTimeOnUtc(), initiated);

    st.qsoFirstReplyOn = firstReply;
    QCOMPARE(st.effectiveQsoLogTimeOnUtc(), firstReply);
  }

  void planAdvanceDecision ()
  {
    // step D: planAdvance replica la DECISIONE di advanceQsoState (remap +
    // QuickQSO-TX3-signoff + progress), composta dalle stesse primitive del
    // bridge → equivalente per costruzione. Copre tutti i rami.
    using decodium::seq::QsoSequencer;
    using R = decodium::seq::TxStepRemapReason;

    // Normale: TX2 -> progress REPORT(3)
    auto p = QsoSequencer::planAdvance (2, /*quick*/false, /*mask*/0);
    QCOMPARE (p.effectiveTxNum, 2); QVERIFY (p.remapReason == R::None);
    QVERIFY (!p.quickQsoTx3Signoff); QCOMPARE (p.progress, 3); QVERIFY (p.valid);

    // QuickQSO: TX1 -> remap TX2
    p = QsoSequencer::planAdvance (1, true, 0);
    QCOMPARE (p.effectiveTxNum, 2); QVERIFY (p.remapReason == R::QuickQsoSkipTx1);
    QVERIFY (!p.quickQsoTx3Signoff); QCOMPARE (p.progress, 3);

    // Tx1 disabilitato -> fallback TX2
    p = QsoSequencer::planAdvance (1, false, 0b000001);
    QCOMPARE (p.effectiveTxNum, 2); QVERIFY (p.remapReason == R::Tx1DisabledFallback);

    // QuickQSO TX3 -> SIGNOFF diretto (progress 5)
    p = QsoSequencer::planAdvance (3, true, 0);
    QCOMPARE (p.effectiveTxNum, 3); QVERIFY (p.quickQsoTx3Signoff);
    QCOMPARE (p.progress, 5); QVERIFY (p.valid);

    // TX3 senza QuickQSO -> ROGER_REPORT(4), NON signoff
    p = QsoSequencer::planAdvance (3, false, 0);
    QVERIFY (!p.quickQsoTx3Signoff); QCOMPARE (p.progress, 4);

    // TX4/TX5 -> SIGNOFF(5); TX6 -> CALLING_CQ(1)
    QCOMPARE (QsoSequencer::planAdvance (4, false, 0).progress, 5);
    QCOMPARE (QsoSequencer::planAdvance (5, false, 0).progress, 5);
    QCOMPARE (QsoSequencer::planAdvance (6, false, 0).progress, 1);

    // Step invalido -> valid=false
    p = QsoSequencer::planAdvance (7, false, 0);
    QVERIFY (!p.valid); QCOMPARE (p.progress, -1);
  }

  void sequencerSeamWiring ()
  {
    // Il seam è implementabile e QsoSequencer si costruisce su State& + Sink&.
    decodium::seq::QsoSequencerState st;
    FakeSink sink;
    decodium::seq::QsoSequencer seq (st, sink);

    QCOMPARE (&seq.state (), &st);
    QCOMPARE (&seq.sink (), static_cast<decodium::seq::ISequencerSink*> (&sink));

    // Un sink chiamato via interfaccia registra correttamente (harness step D).
    decodium::seq::ISequencerSink& s = sink;
    s.requestTransmit (2, "IK8OLM IU8LMC -05", 1500);
    s.diag ("test");
    s.scheduleCallback (250, 0);
    QCOMPARE (sink.txCount, 1);
    QCOMPARE (sink.lastTxNum, 2);
    QCOMPARE (sink.lastAudioFreq, 1500);
    QCOMPARE (sink.lastTxMessage, QString ("IK8OLM IU8LMC -05"));
    QCOMPARE (sink.diagCount, 1);
    QCOMPARE (sink.lastScheduleMs, 250);
  }
};

QTEST_APPLESS_MAIN (TestQsoSequencerRules)
#include "test_qso_sequencer_rules.moc"
