#include "DecodiumBridge.h"
#include "Sequencer/QsoSequencerRules.hpp"
#include <QDateTime>
#include <QFile>
#include <QTextStream>

void decodiumBridgeQsoTransitionLog(const QString&);

bool DecodiumBridge::canAdvance(int fromTx, int toTx) const
{
    if (toTx == 6) return true;                                              // ripartenza CQ sempre
    if ((fromTx == 0 || fromTx == 6) && toTx >= 1 && toTx <= 5) return true; // reply + deferred-after-CQ (1.0.359)
    if (toTx == 2 && (fromTx <= 2 || fromTx == 4 || fromTx == 5)) return true; // report; coda(5->2); recovery(4/5->2)
    if (toTx == 3 && fromTx >= 1 && fromTx <= 5) return true;                 // roger-report + recovery backward(4/5->3)
    if ((toTx == 4 || toTx == 5) && fromTx >= 2 && fromTx <= 5) return true;  // signoff incl. fast(2->4) e ritrasmissioni
    return false;
}

bool DecodiumBridge::advanceQsoState(int txNum)
{
    // 1.0.447 - Fondamenta Fase 1 (CENSIMENTO, solo-logging, opt-in default OFF): registra ogni
    // transizione richiesta (from=m_currentTx, to=txNum, progress, quick) per costruire EMPIRICAMENTE
    // la matrice reale dei salti su TUTTI gli scenari (standard/QuickQSO/Ultra2/Fast3/coda/recovery/
    // parity) prima di scrivere a mano canAdvance/enforcement. Gated FT2+async: 0 impatto FT8/FT4,
    // byte-identico col toggle OFF. NESSUN rifiuto, NESSUN cambio comportamento.
    if (m_ft2TransitionCensus && m_mode == QStringLiteral("FT2") && m_asyncTxEnabled) {
        QString const censusLine = QStringLiteral("[STATE-CENSUS] from=%1 to=%2 progress=%3 quick=%4 lastNtx=%5 retry=%6 call=%7")
                      .arg(m_currentTx).arg(txNum).arg(m_qsoProgress)
                      .arg(m_quickQsoEnabled ? 1 : 0).arg(m_lastNtx).arg(m_txRetryCount).arg(m_callsign.trimmed());
        decodiumBridgeQsoTransitionLog(censusLine);   // visibilita' live monitor (log diagnostico, puo' ruotare a 5MB)
        // 1.0.447 - file dedicato NO-ROTATE: sotto loopback 2-istanze il diagnostico si tronca e perde
        // le righe census. Append durevole in %LOCALAPPDATA%\decodium_census.log (call= distingue ALPHA/BRAVO).
        QString const censusDir = qEnvironmentVariable("LOCALAPPDATA");
        if (!censusDir.isEmpty()) {
            QFile cf(censusDir + QStringLiteral("/decodium_census.log"));
            if (cf.open(QIODevice::Append | QIODevice::Text)) {
                QTextStream ts(&cf);
                ts << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << ' ' << censusLine << '\n';
                cf.close();
            }
        }
    }
    // Step A2 strangler: regole di remap estratte in Sequencer/QsoSequencerRules
    // (QuickQSO Ultra2 salta TX1; 1.0.379 fallback Tx1-disabled). Log invariati.
    {
        auto const remap = decodium::seq::remapRequestedTxStep(
            txNum, m_quickQsoEnabled, m_txDisabledMask);
        if (remap.reason == decodium::seq::TxStepRemapReason::QuickQsoSkipTx1) {
            decodiumBridgeQsoTransitionLog("advanceQsoState: QuickQSO attivo → salta TX1, va a TX2 (Ultra2)");
        } else if (remap.reason == decodium::seq::TxStepRemapReason::Tx1DisabledFallback) {
            decodiumBridgeQsoTransitionLog("advanceQsoState: TX1 user-disabled -> fallback a TX2 (reply-from-Tx2)");
        }
        txNum = remap.txNum;
    }

    // Quick QSO (Ultra2): TX3 contiene "R+report TU" → dopo TX3 il QSO e' finito
    // Salta TX4/TX5 e va diretto a SIGNOFF con log automatico
    if (txNum == 3 && m_quickQsoEnabled) {
        decodiumBridgeQsoTransitionLog("advanceQsoState: QuickQSO TX3 con TU → SIGNOFF diretto");
        if (!selectCurrentTxIfAllowed(txNum, QStringLiteral("advanceQsoState quick-qso"))) {
            return false;
        }
        m_qsoProgress = 5; // SIGNOFF
        emit qsoProgressChanged();
        m_txWatchdogTicks = 0;
        return true;
    }

    // 1.0.447 - Fase 2 (WOULD-REJECT, LOG-ONLY, NON blocca nulla): qui txNum e' gia' EFFETTIVO
    // (post-remap QuickQSO/Tx1-disabled). Se non e' nella matrice canAdvance "best knowledge",
    // logga l'eccezione nel census durevole per raffinare la matrice. Censimento con predicato:
    // quando resta silenzioso su molti QSO -> matrice completa -> si potra' promuovere a guard.
    if (m_ft2TransitionCensus && m_mode == QStringLiteral("FT2") && m_asyncTxEnabled
        && !canAdvance(m_currentTx, txNum)) {
        QString const wrLine = QStringLiteral("[STATE] WOULD-REJECT from=%1 to=%2 progress=%3 quick=%4 lastNtx=%5 (matrice non copre - LOG ONLY)")
                      .arg(m_currentTx).arg(txNum).arg(m_qsoProgress).arg(m_quickQsoEnabled ? 1 : 0).arg(m_lastNtx);
        decodiumBridgeQsoTransitionLog(wrLine);
        QString const wrDir = qEnvironmentVariable("LOCALAPPDATA");
        if (!wrDir.isEmpty()) {
            QFile wf(wrDir + QStringLiteral("/decodium_census.log"));
            if (wf.open(QIODevice::Append | QIODevice::Text)) {
                QTextStream wts(&wf);
                wts << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << ' ' << wrLine << '\n';
                wf.close();
            }
        }
    }
    // Step A2 strangler: mappa txNum→progress estratta (qsoProgressForTxStep).
    int const progress = decodium::seq::qsoProgressForTxStep(txNum);
    if (progress < 0) {
        return false;
    }
    if (!selectCurrentTxIfAllowed(txNum, QStringLiteral("advanceQsoState"))) {
        return false;
    }
    if (m_qsoProgress != progress) { m_qsoProgress = progress; emit qsoProgressChanged(); }
    m_txWatchdogTicks = 0;  // reset watchdog ad ogni avanzamento di stato
    decodiumBridgeQsoTransitionLog("advanceQsoState: TX" + QString::number(txNum) +
              " → progress=" + QString::number(progress));
    // 1.0.187 — FT2 Pack F v2: ricorda lo stato corrente del QSO nella cache
    // partner-memory (gate stretto + log immediato dentro helper).
    rememberPartnerStateV2(QStringLiteral("advanceQsoState"));
    prunePartnerMemoryIfDueV2();
    return true;
}

