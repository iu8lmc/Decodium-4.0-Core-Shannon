#include "DecodiumBridge.h"
#include "BridgeConfigConstants.h"
#include "DecodeSignalReport.h"
#include "DecodeValidationRules.h"
#include "DecodeUtcTime.h"
#include "DecodiumLogging.hpp"
#include "DecodiumLegacyBackend.h"
#include "FtxMessageHelpers.h"
#include "Sequencer/MessageTokenRules.hpp"
#include "Radio.hpp"
#include <QDateTime>

using namespace decodium::seq;
using namespace decodium::decode_validation;
void decodiumBridgeDecodeFlowLog(const QString&);
QString decodiumBridgeDiagnosticOneLine(QString);
int decodiumBridgeReplyTxPeriodForClickedDecode(const QString&, const QString&, int, int);

void DecodiumBridge::engageDxClusterSpot(const QString& call, int audioFreqHz)
{
    QString const cleaned = call.trimmed().toUpper();
    if (cleaned.isEmpty()) {
        decodiumBridgeDecodeFlowLog("engageDxClusterSpot: callsign vuoto, ignorato");
        return;
    }
    decodiumBridgeDecodeFlowLog(QStringLiteral("engageDxClusterSpot: call=%1 audioHz=%2 mode=%3")
              .arg(cleaned).arg(audioFreqHz).arg(m_mode));

    // A clicked spot/decode with no plausible audio offset must not start TX on
    // whatever TX audio frequency happened to be selected previously.
    if (audioFreqHz < 100 || audioFreqHz > 5000) {
        decodiumBridgeDecodeFlowLog(QStringLiteral("engageDxClusterSpot: audioHz=%1 fuori range [100,5000], click ignorato")
                  .arg(audioFreqHz));
        emit statusMessage(QStringLiteral("Spot ignorato: frequenza audio non valida"));
        return;
    }

    // MAM multi-stream ON: lo spot diventa uno slot (IO chiamo da TX1) senza
    // spegnere il MAM ne' attivare il single-QSO. Con MAM OFF e' un no-op.
    if (mamMultiStreamSequencerActive()) {
        mamEnqueueClickedStation(cleaned, audioFreqHz);
        return;
    }

    QString const previousDx = m_dxCall.trimmed();
    bool const newQso = Radio::base_callsign(previousDx).trimmed().compare(
                              Radio::base_callsign(cleaned).trimmed(), Qt::CaseInsensitive) != 0;
    clearPendingAutoSeqTx(QStringLiteral("dx-cluster manual selection"));
    if (newQso) {
        forgetPartnerMemoryForCall(previousDx,
                                   QStringLiteral("dx-cluster takeover -> %1").arg(cleaned));
    }
    setDxCall(cleaned);
    setDxGrid(QString {});  // grid non noto, sarà aggiornato dal primo decode

    setTxFrequency(audioFreqHz);

    // Forza TX1 (chiamata: <them> <me> <my-grid>)
    if (!selectCurrentTxIfAllowed(1, QStringLiteral("engageDxClusterSpot"))) {
        return;
    }

    armExplicitManualTxRequest(1, QStringLiteral("DX cluster spot"));
    if (!m_txEnabled) {
        setTxEnabled(true);
    }
    startTx();
}

void DecodiumBridge::processDecodeDoubleClick(const QString& message,
                                              const QString& timeStr,
                                              const QString& db,
                                              int audioFreq)
{
#if DECODIUM_HAS_SSTV
    // External UDP Reply packets use this same path. They must not turn a
    // decode from the suspended dashboard mode into a QSO/TX action while the
    // operator is working inside SSTV.
    if (m_sstvWorkspaceActive) {
        decodiumBridgeDecodeFlowLog(QStringLiteral(
            "decode selection ignored while SSTV workspace is active"));
        return;
    }
#endif
    if (message.isEmpty()) return;

    // A manual double-click is an explicit operator takeover. The automatic
    // candidate path sets m_autoCallStartingCandidate while calling this same
    // routine, so it remains armed for the next CQ only in that case.
    if (m_autoCallEnabled && !m_autoCallStartingCandidate) {
        setAutoCallEnabled(false);
    }

    // Aggiorna il report che invieremo in TX2 basandoci sull'SNR del segnale decodificato
    if (!db.isEmpty()) {
        bool ok = false;
        int snr = db.trimmed().toInt(&ok);
        if (ok) {
            setReportSent(formatSignalReport(snr));
        }
    }

    QStringList parts = message.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.isEmpty()) return;

    QString hisCall, hisGrid;
    int newCurrentTx = 1;
    bool clickedSignoffMessage = false;
    QString const myCallUpper = m_callsign.trimmed().toUpper();
    QString const myBaseUpper = normalizedBaseCall(myCallUpper);
    QString const activeDxBaseBeforeClick =
        normalizedBaseCall(m_dxCall.trimmed().toUpper());
    bool const txArmedOrActiveBeforeClick = m_txEnabled || m_transmitting;
    auto const tokenIsMine = [&](int index) {
        return index >= 0
            && index < parts.size()
            && tokenMatchesCall(parts.at(index), myCallUpper, myBaseUpper);
    };
    auto const callAt = [&](int index) {
        return index >= 0 && index < parts.size()
            ? normalizedUsableCallToken(parts.at(index))
            : QString {};
    };
    auto const firstUsablePeerCall = [&]() {
        for (QString const& part : parts) {
            QString const candidate = normalizedUsableCallToken(part);
            if (!candidate.isEmpty()
                && !tokenMatchesCall(candidate, myCallUpper, myBaseUpper)) {
                return candidate;
            }
        }
        return QString {};
    };
    bool const incomingDirectedToMe = tokenIsMine(0);
    bool const outgoingFromMe = tokenIsMine(1);

    if (parts[0].compare("CQ", Qt::CaseInsensitive) == 0) {
        // CQ [modifier] CALL GRID
        int callIdx = 1;
        if (parts.size() > 2) {
            QString const mod = parts[1].trimmed().toUpper();
            if (isDirectedCqModifierToken(mod))
                callIdx = 2;
        }
        if (callIdx < parts.size()) hisCall = callAt(callIdx);
        if (hisCall.isEmpty()) hisCall = firstUsablePeerCall();
        if (callIdx + 1 < parts.size() && isGridTokenStrict(parts[callIdx + 1])) {
            hisGrid = parts[callIdx + 1].trimmed().toUpper();
        }
        newCurrentTx = 1; // risposta CQ con callsign+grid
    } else if (parts.size() >= 2) {
        if (tokenIsMine(0)) {
            hisCall = callAt(1);
        } else if (tokenIsMine(1)) {
            hisCall = callAt(0);
        } else {
            // Per traffico non diretto a noi, mantieni la semantica storica:
            // il secondo token è il mittente del messaggio decodificato. Se però
            // è il placeholder FT compresso "<...>", usa l'altro nominativo reale.
            hisCall = callAt(1);
            if (hisCall.isEmpty()) {
                hisCall = callAt(0);
            }
        }
        if (hisCall.isEmpty()) hisCall = firstUsablePeerCall();
        if (parts.size() >= 3) {
            QString const last = normalizeCallToken(parts.last()).trimmed().toUpper();
            clickedSignoffMessage = isQsoSignoffToken(last);
            if (isGridTokenStrict(last)) {
                hisGrid = last;
                if (incomingDirectedToMe) newCurrentTx = 2; // ricevuto grid -> mandiamo report
            } else if (last.compare("RR73", Qt::CaseInsensitive) == 0 ||
                       last.compare("RRR",  Qt::CaseInsensitive) == 0) {
                if (incomingDirectedToMe) newCurrentTx = 5; // ricevuto RR73 -> mandiamo 73
            } else if (last.compare("73", Qt::CaseInsensitive) == 0) {
                if (incomingDirectedToMe) newCurrentTx = 6; // QSO finito
            } else if (last.length() >= 2 &&
                       last[0] == 'R' &&
                       (last[1] == '-' || last[1] == '+' || last[1].isDigit())) {
                if (incomingDirectedToMe) {
                    newCurrentTx = 4; // ricevuto R+report -> mandiamo RR73
                    // 1.0.726: the report we clicked is the partner's real report: record it, or the
                    // final-signoff rule refuses the QSO as "never sent a report" (paul log, FT4, 28/10)
                    setReportReceived(last.startsWith(QLatin1Char('R')) ? last.mid(1) : last);
                }
            } else if (last.length() >= 2 &&
                       (last[0] == '-' || last[0] == '+') && last[1].isDigit()) {
                if (incomingDirectedToMe) {
                    newCurrentTx = 3; // ricevuto report -> mandiamo R + nostro report al DX
                    setReportReceived(last); // 1.0.726: a plain report from the partner is real too
                }
            }
            bool const qsoPayloadWouldAdvance =
                isGridTokenStrict(last) ||
                last.compare("RR73", Qt::CaseInsensitive) == 0 ||
                last.compare("RRR",  Qt::CaseInsensitive) == 0 ||
                last.compare("73", Qt::CaseInsensitive) == 0 ||
                (last.length() >= 2 &&
                 last[0] == 'R' &&
                 (last[1] == '-' || last[1] == '+' || last[1].isDigit())) ||
                (last.length() >= 2 &&
                 (last[0] == '-' || last[0] == '+') && last[1].isDigit());
            if (qsoPayloadWouldAdvance && !incomingDirectedToMe && !outgoingFromMe) {
                decodiumBridgeDecodeFlowLog(QStringLiteral("processDecodeDoubleClick: third-party decode '%1' starts new call -> TX1")
                              .arg(message.trimmed()));
            }
        }
    }

    hisCall = normalizedUsableCallToken(hisCall);
    if (hisCall.isEmpty()) {
        DIAG_WARN(QStringLiteral("processDecodeDoubleClick ignored: unresolved compressed callsign msg=\"%1\"")
                      .arg(decodiumBridgeDiagnosticOneLine(message)));
        emit warningRaised(tr("Transmission not started"),
                           tr("Compressed callsign not resolved"),
                           message);
        return;
    }

    // MAM multi-stream ON: il doppio-click aggiunge la stazione alla coda/slot
    // (IO la chiamo da TX1) invece di spegnere il MAM e aprire un single-QSO.
    // Con MAM OFF (default) e' un no-op: il path single-QSO sottostante e'
    // byte-identico.
    if (mamMultiStreamSequencerActive()) {
        mamEnqueueClickedStation(hisCall, audioFreq);
        return;
    }

    if (clickedSignoffMessage) {
        QString const clickedCallBase =
            normalizedBaseCall(hisCall.trimmed().toUpper());
        bool const sameActivePartner =
            !activeDxBaseBeforeClick.isEmpty()
            && !clickedCallBase.isEmpty()
            && activeDxBaseBeforeClick == clickedCallBase;
        bool const activeQsoCanUseSignoff =
            sameActivePartner
            && txArmedOrActiveBeforeClick
            && m_qsoProgress > 1
            && m_qsoProgress < 5;
        if (!activeQsoCanUseSignoff) {
            decodiumBridgeDecodeFlowLog(QStringLiteral("processDecodeDoubleClick: final decode '%1' treated as new call -> TX1 (active=%2 progress=%3)")
                          .arg(message.trimmed(),
                               activeDxBaseBeforeClick.isEmpty() ? QStringLiteral("<none>") : activeDxBaseBeforeClick)
                          .arg(m_qsoProgress));
            newCurrentTx = 1;
            hisGrid.clear();
        }
    }

    QString const clickedCallBase = normalizedBaseCall(hisCall.trimmed().toUpper());
    bool const manualTakeoverFromManagedQso =
        (m_multiAnswerMode || m_autoCqRepeat)
        && !activeDxBaseBeforeClick.isEmpty()
        && !clickedCallBase.isEmpty()
        && activeDxBaseBeforeClick != clickedCallBase
        && txArmedOrActiveBeforeClick
        && m_qsoProgress > 1
        && m_qsoProgress < 6;
    if (manualTakeoverFromManagedQso) {
        QString const takeoverReason =
            QStringLiteral("manual double-click takeover -> %1").arg(clickedCallBase);
        rememberCompletedAutoCqPartner(activeDxBaseBeforeClick, false, takeoverReason);
    }

    if (m_multiAnswerMode) {
        setMultiAnswerMode(false);
    }
    if (m_autoCqRepeat) {
        setAutoCqRepeat(false);
    }
    clearCallerQueue();
    clearAutoCqPartnerLock();
    clearManualTxHold(QStringLiteral("decode-double-click"));

    // Nuovo QSO: resetta contatori retry/watchdog (GitHub handleDoubleClick)
    bool newQso = (hisCall.compare(m_dxCall, Qt::CaseInsensitive) != 0);
    clearPendingAutoSeqTx(QStringLiteral("decode-double-click manual selection"));
    if (newQso) {
        forgetPartnerMemoryForCall(activeDxBaseBeforeClick,
                                   QStringLiteral("decode-double-click takeover -> %1")
                                       .arg(clickedCallBase.isEmpty() ? hisCall : clickedCallBase));
    }
    setDxCall(hisCall);
    if (!hisGrid.isEmpty()) setDxGrid(hisGrid);
    else if (newQso) setDxGrid(QString());
    else if (!m_dxGrid.isEmpty()) hisGrid = m_dxGrid;

    // Sempre resettare contatori su double-click (anche se stessa stazione)
    m_nTx73 = 0;
    m_autoSeqRogerReportBase.clear();
    m_txRetryCount = 0;
    m_lastNtx = -1;
    m_lastCqPidx = -1;
    m_txWatchdogTicks = 0;
    m_autoCQPeriodsMissed = 0;
    m_logAfterOwn73 = false;
    m_ft2DeferredLogPending = false;
    clearPendingAutoSeqTx(QStringLiteral("decode-double-click state reset"));
    m_quickPeerSignaled = false;
    m_qsoLogged = false;
    m_lastTransmittedMessage.clear();
    m_lastAutoSeqKey.clear();
    m_lastAutoSeqMs = 0;
    m_ft2ManualClickTx1BypassPeriodOnce = false;
    m_qsoStartedOn = QDateTime::currentDateTimeUtc();
    m_qsoFirstReplyOn = QDateTime {};
    clearPendingAutoLogSnapshot();
    if (newQso) {
        clearAutoCqPartnerLock();
    }

    genStdMsgs(hisCall, hisGrid);
    // advanceQsoState imposta currentTx + qsoProgress + resetta watchdog
    if (!advanceQsoState(newCurrentTx)) {
        return;
    }
    armExplicitManualTxRequest(newCurrentTx, QStringLiteral("decode double-click"));
    if (m_mode == QStringLiteral("FT2")
        && m_asyncTxEnabled
        && newCurrentTx == 1
        && !m_autoCqRepeat
        && !m_multiAnswerMode
        && !m_waitPounceActive) {
        // One-shot manual CQ reply: do not require the global "Immediate TX on
        // click" option just to catch the current FT2 reply slot. The late-start
        // guard still enforces the conservative payload window.
        // Wait&Pounce is automated and must stay slot-gated; otherwise the
        // reply can be launched into the caller's receive/settle window.
        m_ft2ManualClickTx1BypassPeriodOnce = true;
    }
    updateAutoCqPartnerLock();

    if (audioFreq > 0) setRxFrequency(audioFreq);

    // Usa il timestamp del decode cliccato per selezionare il periodo opposto
    // al DX. In FT4 i tempi visualizzati (:07/:22/:37/:52) sono slot .5s
    // troncati: gestirli come secondi pieni spostava la TX nello stesso slot
    // del chiamante.
    int const previousTxPeriod = m_txPeriod;
    int const replyTxPeriod = decodiumBridgeReplyTxPeriodForClickedDecode(
        m_mode, timeStr, m_txPeriod, effectivePeriodMsForMode(m_mode));
    if (m_txPeriod != replyTxPeriod) {
        m_txPeriod = replyTxPeriod;
        emit txPeriodChanged();
        if (usingLegacyBackendForTx()) {
            m_legacyBackend->setTxFirst(m_specialOperationActivity == kSpecialOpHound ? false : (m_txPeriod != 0));
        }
        decodiumBridgeDecodeFlowLog(QStringLiteral("processDecodeDoubleClick: txPeriod adjusted %1 -> %2 from clicked decode time=%3 mode=%4")
                      .arg(previousTxPeriod)
                      .arg(m_txPeriod)
                      .arg(normalizeUtcDisplayToken(timeStr).isEmpty() ? QStringLiteral("<none>") : normalizeUtcDisplayToken(timeStr))
                      .arg(m_mode));
    }

    // Abilita TX: doppio click su decode abilita TX automaticamente (quick call behavior)
    // Come Shannon: il doppio click equivale sempre a "Enable TX" + avvio sequenza
    setTxEnabled(true);

    decodiumBridgeDecodeFlowLog("processDecodeDoubleClick: hisCall=" + hisCall +
              " TX=" + QString::number(newCurrentTx) +
              " progress=" + QString::number(m_qsoProgress) +
              " txPeriod=" + QString::number(m_txPeriod));

    // 1.0.304 (#9) — se c'è un resume armato su un partner DIVERSO e l'utente ingaggia
    // manualmente qualcun altro, disarma (ha cambiato target). Il resume stesso azzera
    // m_resumeTargetCall PRIMA di chiamare questo metodo, quindi non si auto-disarma.
    if (!m_resumeTargetCall.isEmpty()
        && normalizedBaseCall(hisCall.trimmed().toUpper()) != m_resumeTargetCall) {
        clearResumeQsoOnReplyArm(QStringLiteral("manual target changed to %1").arg(hisCall));
    }
    emit statusMessage("QSO con " + hisCall + " — TX" + QString::number(newCurrentTx));

    // Decodium3 recalculated m_bTxTime on every GUI tick, so a double-click just
    // after the UTC boundary could still catch the current TX slot. D4 normally
    // checks only when the boundary changes; run the same guarded check now so
    // we do not miss a valid slot that is already in progress.
    checkAndStartPeriodicTx();
}

void DecodiumBridge::genStdMsgs(const QString& hisCall, const QString& hisGrid)
{
    if (m_callsign.isEmpty() || hisCall.isEmpty()) return;
    QString mygr = m_grid.left(4); // gridsquare 4 chars
    QString const mode = m_mode.trimmed().toUpper();
    QString rptSent = m_reportSent.isEmpty() ? "-10" : m_reportSent;

    setTx1(standardFtxQsoMessage(1, hisCall, m_callsign, mygr, rptSent,
                                 m_sendRR73, false, mode));           // risposta CQ
    setTx2(standardFtxQsoMessage(2, hisCall, m_callsign, mygr, rptSent,
                                 m_sendRR73, false, mode));           // il nostro report

    // TX3 conferma il report ricevuto e ripete il report che inviamo al DX.
    // Quick QSO (Ultra2): aggiunge "TU" per segnalare QSO completo in 2 messaggi.
    // Formato: "HIS_CALL MY_CALL R+NN TU" — il DX peer risponde con RR73 direttamente.
    setTx3(standardFtxQsoMessage(3, hisCall, m_callsign, mygr, rptSent,
                                 m_sendRR73, m_quickQsoEnabled, mode));

    setTx4(standardFtxQsoMessage(4, hisCall, m_callsign, mygr, rptSent,
                                 m_sendRR73, false, mode));           // RR73 o RRR
    setTx5(standardFtxQsoMessage(5, hisCall, m_callsign, mygr, rptSent,
                                 m_sendRR73, false, mode));           // 73
    if (m_customCqPrefix.isEmpty() && m_customCqMessage.isEmpty()) {
        m_customCqPrefix = customCqPrefixFromMessage(m_tx6, m_callsign);
    }
    setTx6(m_customCqMessage.isEmpty()
        ? ftxCqMessageWithCustomPrefix(
              standardFtxCqMessage(m_callsign, mygr, mode), m_customCqPrefix)
        : m_customCqMessage); // CQ
    Q_UNUSED(hisGrid)
}
