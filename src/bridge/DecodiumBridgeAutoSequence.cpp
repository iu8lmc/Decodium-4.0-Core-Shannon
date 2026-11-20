#include "DecodiumBridge.h"
#include "Radio.hpp"
#include "DecodeUtcTime.h"
#include "DecodeSignalReport.h"
#include "DecodiumLegacyBackend.h"
#include "DecodeValidationRules.h"
#include "DecodeUnresolved.h"
#include "DecodeMessageNormalization.h"
#include "DecodeUserFilterConfig.h"
#include "Sequencer/QsoSequencerRules.hpp"
#include "Sequencer/MessageTokenRules.hpp"
#include <QDateTime>
#include <QTimer>

using namespace decodium::decode_validation;
using namespace decodium::seq;
using decodium::decode_message::canonicalDecodeMessage;
void decodiumBridgeDecodeFlowLog(const QString&);
QString decodiumBridgeInferPartnerForAutolog(const QString&, const QString&, const QString&);
qint64 decodiumBridgeQsoCooldownWindowMs();
int decodiumBridgeReplyTxPeriodForClickedDecode(const QString&, const QString&, int, int);
#define replyTxPeriodForClickedDecode decodiumBridgeReplyTxPeriodForClickedDecode
#define bridgeLog decodiumBridgeDecodeFlowLog
#define inferPartnerFromDirectedMessage decodiumBridgeInferPartnerForAutolog
#define kQsoCooldownWindowMs (decodiumBridgeQsoCooldownWindowMs())
static constexpr int kFt2AsyncAutoSeqDedupeSeconds = 9;
static constexpr int kFt2AsyncAutoSeqDedupeMs = kFt2AsyncAutoSeqDedupeSeconds * 1000;
static int autoSeqTxRank(int txNum)
{
    switch (txNum) {
    case 5: return 5;
    case 4: return 4;
    case 3: return 3;
    case 2: return 2;
    case 1: return 1;
    case 6: return 1;
    default: return 0;
    }
}

void DecodiumBridge::autoSequenceStep(const QStringList& f)
{
    // f = [time, snr, dt, df, message, aptype, quality, freq]
    // Corrisponde a GitHub processDecodedMessage()
    if (f.size() < 5) return;

    QString msg = f[4].trimmed();
    // A low-confidence decode ('?') can keep an exchange moving but can never
    // close one: a phantom 73/RR73/RRR/TU made by the a-priori memory from the
    // tail of our own signal or from an old message would log a QSO that did
    // not happen (T77RN: PA0BTR closed on a "73" he never sent).
    if (msg.endsWith(QLatin1Char('?')) && !decodium::seq::mayFeedRescuedDecodeToSequencer(msg)) {
        bridgeLog(QStringLiteral("autoSequenceStep: low-confidence signoff ignored, it cannot close the QSO: %1").arg(msg));
        return;
    }
    QString const autoSeqDecodeIdentity =
        QStringLiteral("%1|%2").arg(normalizeUtcDisplayToken(f.value(0)), msg);
    QString localHashPartner;
    bool const directedToLocalHashPrecheck =
        isDirectedToLocalHashFromActivePartner(msg, &localHashPartner);
    QString semanticRejectReason;
    if (!shouldAcceptDecodedMessage(msg,
                                    &semanticRejectReason,
                                    directedToLocalHashPrecheck)) {
        bridgeLog(QStringLiteral("autoSequenceStep: semantic reject (%1): %2")
                      .arg(semanticRejectReason, msg));
        return;
    }
    QStringList parts = msg.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() < 2) return;

    bool const filterIsCQ = msg.startsWith(QStringLiteral("CQ "), Qt::CaseInsensitive)
                         || msg.compare(QStringLiteral("CQ"), Qt::CaseInsensitive) == 0
                         || msg.startsWith(QStringLiteral("QRZ "), Qt::CaseInsensitive);
    QVariantMap filterEntry;
    filterEntry[QStringLiteral("message")] = msg;
    filterEntry[QStringLiteral("mode")] = m_mode;
    filterEntry[QStringLiteral("isTx")] = false;
    filterEntry[QStringLiteral("isCQ")] = filterIsCQ;
    filterEntry[QStringLiteral("fromCall")] = extractDecodedCallsign(msg, filterIsCQ);
    enrichDecodeEntry(filterEntry);
    QString userFilterRejectReason;
    DecodeUserFilterConfig const userFilterConfig = readDecodeUserFilterConfig();
    if (!shouldAcceptDecodeEntryByUserFilters(filterEntry,
                                              userFilterConfig,
                                              true,
                                              &userFilterRejectReason)) {
        bridgeLog(QStringLiteral("autoSequenceStep: user filter reject (%1): %2")
                      .arg(userFilterRejectReason, msg));
        return;
    }

    QString const myCallUpper = m_callsign.trimmed().toUpper();
    QString const myBaseUpper = normalizedBaseCall(myCallUpper);
    auto const tokenIsMine = [&](int index) {
        return index >= 0
            && index < parts.size()
            && tokenMatchesCall(parts.at(index), myCallUpper, myBaseUpper);
    };
    bool const directedToMe = tokenIsMine(0) || tokenIsMine(1);
    bool const directedToLocalHash = !directedToMe && directedToLocalHashPrecheck;
    bool const directedToMeEffective = directedToMe || directedToLocalHash;
    // FT2/FT4/FT8: i decoder scrivono sempre "destinatario mittente". Un messaggio
    // col nostro nominativo al SECONDO posto e' quindi DA noi: l'eco della nostra
    // trasmissione, o una sua rilettura tardiva (a7/a8) che il confronto esatto
    // con l'ultimo TX qui sotto non riconosce. Contato come messaggio della
    // controparte puo' avanzare o chiudere il QSO. L'ordine invertito resta
    // ammesso solo per gli altri modi.
    if ((m_mode == QStringLiteral("FT2") || m_mode == QStringLiteral("FT4")
         || m_mode == QStringLiteral("FT8"))
        && messageIsSentByCall(msg, myCallUpper, myBaseUpper)) {
        bridgeLog(QStringLiteral("autoSequenceStep: ignore echo of own transmission: %1")
                      .arg(msg));
        return;
    }
    if (directedToMe
        && shouldSuppressDirectedGhostDecode(f, QStringLiteral("auto-seq"))) {
        return;
    }
    // FT2: l'AP e la ricerca della risposta attesa devono puntare sul segnale del
    // corrispondente, che in AutoCQ risponde a una frequenza diversa dalla
    // nostra ricezione. Si tiene la frequenza dell'ultimo suo messaggio per noi.
    if (m_mode == QStringLiteral("FT2") && directedToMe && parts.size() > 1
        && !m_dxCall.trimmed().isEmpty()) {
        QString const dxUpper = m_dxCall.trimmed().toUpper();
        bool dfOk = false;
        int const df = f.value(3).toInt(&dfOk);
        if (dfOk && df > 0
            && tokenMatchesCall(parts.at(1), dxUpper, normalizedBaseCall(dxUpper))) {
            m_qsoPartnerFreqHz = df;
            m_qsoPartnerFreqMs = QDateTime::currentMSecsSinceEpoch();
        }
    }

    // Gate self-echo. Con nominativi speciali/non standard la forma corretta
    // puo' mantenere lo stesso ordine dei primi due token tra TX e RX
    // (es. "<II9MESC> KF9UG -05" seguito da "<II9MESC> KF9UG EN71").
    // Scartare solo per i due token iniziali blocca l'autosequenza; l'eco
    // locale e' il messaggio completo identico al nostro ultimo TX.
    if (!m_lastTransmittedMessage.isEmpty()) {
        QString const canonicalMessage = canonicalDecodeMessage(msg).toUpper();
        QString const canonicalLastTx = canonicalDecodeMessage(m_lastTransmittedMessage).toUpper();
        if (!canonicalMessage.isEmpty()
            && canonicalMessage == canonicalLastTx) {
            if (m_mode == QStringLiteral("FT2")) {
                QString localTxEchoReason;
                if (shouldSuppressRecentLocalTxEchoDecode(msg,
                                                          QStringLiteral("auto-seq"),
                                                          &localTxEchoReason)) {
                    return;
                }
            } else if (m_transmitting || m_tuning) {
                bridgeLog(QStringLiteral("autoSequenceStep: skip self-echo (exact active local TX): %1")
                          .arg(msg));
                return;
            }
        }
    }

    // Decodium3/legacy behavior: during FT8/FT4 sync TX, we still have to arm
    // auto-reply as soon as a directed call to us is decoded from our CQ.
    // Keep blocking unrelated/self decodes while TX is still winding down.
    if (m_transmitting && !(m_mode == "FT2" && m_asyncTxEnabled) && !directedToMeEffective) {
        bridgeLog("autoSequenceStep: skipped during TX (not directed): " + msg);
        return;
    }

    // Pulizia periodica cooldown map (previene memory leak su sessioni lunghe)
    {
        qint64 nowPurge = QDateTime::currentMSecsSinceEpoch();
        for (auto it = m_qsoCooldown.begin(); it != m_qsoCooldown.end(); ) {
            if (nowPurge - it.value() > kQsoCooldownWindowMs) {
                m_postLogReengageCount.remove(it.key());  // 1.0.446 - pulisci contatore ri-aggancio
                it = m_qsoCooldown.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Anti-collisione: deduplicazione — ignora decode identici processati di recente.
    // FT2 async: il time cambia ad ogni decode, quindi deduplicare solo sul messaggio.
    // FT8 sync: time+message per permettere stesso messaggio in periodi diversi.
    {
        bool isFt2Async = (m_mode == "FT2" && m_asyncTxEnabled);
        QString dedupeKey = isFt2Async ? f[4] : (f[0] + "|" + f[4]);
        int dedupeWindowMs = isFt2Async ? kFt2AsyncAutoSeqDedupeMs : 4000;
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (m_lastAutoSeqKey == dedupeKey && (now - m_lastAutoSeqMs) < dedupeWindowMs) {
            bridgeLog("autoSequenceStep: dedupe skip: " + f[4]);
            return;
        }
        m_lastAutoSeqKey = dedupeKey;
        m_lastAutoSeqMs = now;
    }
    QString reportConflictReason;
    if (shouldSuppressConflictingDirectedReportDecode(f,
                                                      QStringLiteral("auto-seq"),
                                                      &reportConflictReason)) {
        return;
    }
    if (m_manualTxHold) return;

    auto finishAutoSequenceQso = [&](QString const& reason) {
        if (releaseCourtesySignoffIfActive(reason)) {
            return;
        }
        bridgeLog(reason);
        QString completedPartner = inferredPartnerForAutolog();
        capturePendingAutoLogSnapshot();
        if (completedPartner.trimmed().isEmpty() && m_pendingAutoLogValid) {
            completedPartner = m_pendingAutoLogCall;
        }
        markWorldMapQsoClosed(completedPartner, reason);
        rememberCompletedAutoCqPartner(completedPartner, true, reason);
        logQso();
        bool const continueQueuedCaller = (m_multiAnswerMode || m_autoCqRepeat) && !m_callerQueue.isEmpty();
        bool const continueAutoCq = autoCqCanRestartTx6();
        bool const preserveAutoTx = continueQueuedCaller || continueAutoCq;
        resetQsoStateForNextSequence(preserveAutoTx);
        if (applyDeferredSpecialOperationActivityIfReady()) {
            return;
        }
        if (continueQueuedCaller) {
            processNextInQueue();
        } else if (continueAutoCq) {
            if (!advanceQsoState(6)) {
                return;
            }
            if (!m_txEnabled) {
                setTxEnabled(true);
            }
            // 1.0.256 fix BUG #1 FT8/FT4: parity check
            checkAndStartPeriodicTx();
        }
    };

    // Estrai il mittente: TO_CALL FROM_CALL ...
    QString from;
    if (directedToLocalHash) {
        from = localHashPartner;
        bridgeLog(QStringLiteral("autoSeq: local hash placeholder resolved to active partner %1 for msg=%2")
                      .arg(localHashPartner, msg));
    } else if (tokenIsMine(0) && parts.size() >= 2) {
        from = parts[1];
    } else if (tokenIsMine(1)) {
        from = parts[0];
    } else {
        from = parts[0];
    }
    QString const directedPartner = inferPartnerFromDirectedMessage(msg, myCallUpper, myBaseUpper);
    QString fallbackDirectedPartner = directedToLocalHash ? localHashPartner : directedPartner;
    if (fallbackDirectedPartner.isEmpty()
        && !directedToLocalHash
        && directedToMe
        && parts.size() >= 2
        && isPlaceholderCallToken(tokenIsMine(0) ? parts[1] : parts[0])
        && !m_dxCall.trimmed().isEmpty()) {
        fallbackDirectedPartner = m_dxCall.trimmed();
        bridgeLog(QStringLiteral("autoSeq: placeholder partner resolved to active DX %1 for msg=%2")
                      .arg(fallbackDirectedPartner, msg));
    }
    QString const messagePartner = fallbackDirectedPartner.isEmpty() ? from : fallbackDirectedPartner;
    QString const messagePartnerBase = Radio::base_callsign(messagePartner).trimmed().toUpper();

    // 1.0.174 — Aggiorna SNR del partner corrente. Usato da ghost filter
    // e retry cap/wait adattivi (FT2 weak-signal pack). Memoria singolo
    // valore (no storage costoso); 127 = sentinel "no data".
    if (!m_dxCall.isEmpty() && !messagePartnerBase.isEmpty()) {
        QString const dxUp = m_dxCall.trimmed().toUpper();
        QString const dxBase = Radio::base_callsign(dxUp).trimmed().toUpper();
        if (messagePartnerBase == dxBase || messagePartnerBase == dxUp) {
            bool snrOk = false;
            int const snrVal = f[1].trimmed().toInt(&snrOk);
            if (snrOk) {
                m_currentPartnerSnrDb = snrVal;
            }
        }
    }
    if (!m_dxCall.isEmpty()
        && (m_currentTx == 1 || m_currentTx == 2)
        && filterIsCQ) {
        QString const cqCall = extractDecodedCallsign(msg, true).trimmed().toUpper();
        QString const cqBase = Radio::base_callsign(cqCall).trimmed().toUpper();
        QString const dxUp = m_dxCall.trimmed().toUpper();
        QString const dxBase = Radio::base_callsign(dxUp).trimmed().toUpper();
        if (!cqBase.isEmpty()
            && (!dxBase.isEmpty() || !dxUp.isEmpty())
            && (cqBase == dxBase || cqBase == dxUp)) {
            bool snrOk = false;
            int const snrVal = f.value(1).trimmed().toInt(&snrOk);
            if (snrOk) {
                QString const nextReport = formatSignalReport(snrVal);
                if (m_reportSent != nextReport) {
                    setReportSent(nextReport);
                    bridgeLog(QStringLiteral("autoSeq: CQ ripetuto dal target %1 -> report aggiornato %2")
                                  .arg(cqCall.isEmpty() ? cqBase : cqCall,
                                       m_reportSent));
                }
            }
        }
    }
    QString const currentTxPayloadAtAutoSeq = buildCurrentTxMessage().trimmed();
    bool const currentPayloadMentionsMessagePartner =
        !messagePartnerBase.isEmpty()
        && messageContainsCallToken(currentTxPayloadAtAutoSeq, messagePartner, messagePartnerBase);
    bool const lastPayloadMentionsMessagePartner =
        !messagePartnerBase.isEmpty()
        && messageContainsCallToken(m_lastTransmittedMessage, messagePartner, messagePartnerBase);
    bool const pendingPayloadMentionsMessagePartner =
        !messagePartnerBase.isEmpty()
        && messageContainsCallToken(m_pendingAutoSeqMessage, messagePartner, messagePartnerBase);
    bool const messagePartnerMatchesCurrentQso =
        !messagePartnerBase.isEmpty()
        && (currentPayloadMentionsMessagePartner
            || lastPayloadMentionsMessagePartner
            || pendingPayloadMentionsMessagePartner
            || (!m_ft2AutoCqAwaitingPartnerBase.isEmpty()
                && messagePartnerBase == m_ft2AutoCqAwaitingPartnerBase));
    auto const currentPayloadMentionsMessagePartnerNow = [&]() {
        return messagePartnerBase.isEmpty()
            || messageContainsCallToken(buildCurrentTxMessage(), messagePartner, messagePartnerBase);
    };
    auto const lastPayloadMentionsMessagePartnerNow = [&]() {
        return messagePartnerBase.isEmpty()
            || messageContainsCallToken(m_lastTransmittedMessage, messagePartner, messagePartnerBase);
    };
    auto const currentSignoffForMessagePartnerNow = [&]() {
        QString const payload = buildCurrentTxMessage().trimmed();
        bool const currentTx5ForPartner =
            (m_currentTx == 5) && currentPayloadMentionsMessagePartnerNow();
        return currentTx5ForPartner
            || messageCarries73PayloadForCall(payload, messagePartner, messagePartnerBase);
    };
    auto const lastSignoffForMessagePartnerNow = [&]() {
        return messageCarries73PayloadForCall(m_lastTransmittedMessage, messagePartner, messagePartnerBase);
    };
    auto const signoffStateForMessagePartnerNow = [&]() {
        bool const currentMentionsPartner = currentPayloadMentionsMessagePartnerNow();
        bool const currentStepForPartner =
            (m_currentTx == 4 || m_currentTx == 5) && currentMentionsPartner;
        bool const currentSignoffForPartner = currentSignoffForMessagePartnerNow();
        bool const lastSignoffForPartner = lastSignoffForMessagePartnerNow();
        bool const progressForPartner =
            m_qsoProgress >= 5
            && (currentStepForPartner || currentSignoffForPartner || lastSignoffForPartner);
        return progressForPartner
            || currentStepForPartner
            || currentSignoffForPartner
            || lastSignoffForPartner;
    };

    QString last = parts.last();
    int nextTx = -1;

    if (m_mode == QStringLiteral("FT2") && filterIsCQ) {
        QString activeSignoffPartner = m_dxCall.trimmed();
        if (activeSignoffPartner.isEmpty()) {
            activeSignoffPartner = m_autoCqLockedCall.trimmed();
        }
        if (activeSignoffPartner.isEmpty()) {
            activeSignoffPartner = inferredPartnerForAutolog().trimmed();
        }
        QString const activeSignoffBase = normalizedBaseCall(activeSignoffPartner);
        QString const cqCall = extractDecodedCallsign(msg, true).trimmed();
        QString const cqBase = normalizedBaseCall(cqCall);
        bool const cqFromActivePartner =
            !cqBase.isEmpty()
            && !activeSignoffBase.isEmpty()
            && cqBase == activeSignoffBase;
        bool const lastSignoffForActivePartner =
            messageCarries73PayloadForCall(m_lastTransmittedMessage,
                                           activeSignoffPartner,
                                           activeSignoffBase);
        bool const localSignoffWasActuallySent =
            cqFromActivePartner
            && lastSignoffForActivePartner
            && (m_nTx73 > 0
                || m_lastNtx == 4
                || m_lastNtx == 5
                || m_ft2DeferredLogPending
                || m_logAfterOwn73);
        if (localSignoffWasActuallySent) {
            m_qsoCooldown[cqBase] = QDateTime::currentMSecsSinceEpoch();
            finishAutoSequenceQso(QStringLiteral("autoSeq: CQ from active FT2 partner %1 after our signoff TX%2 -> QSO complete, partner left")
                                      .arg(cqCall.isEmpty() ? cqBase : cqCall,
                                           QString::number(m_lastNtx)));
            return;
        }
    }

    // QSO cooldown: ignora 73/RR73 ripetuti da stazione con cui il QSO è chiuso
    // Previene loop su decode ripetuti dello stesso messaggio finale
    QString last_word = last;
    bool is_73 = (last_word.compare("73",   Qt::CaseInsensitive) == 0 ||
                  last_word.compare("RR73",  Qt::CaseInsensitive) == 0 ||
                  last_word.compare("RRR",   Qt::CaseInsensitive) == 0);
    bool const directedResponseToMe =
        directedToMeEffective
        && !messagePartnerBase.isEmpty()
        && messagePartnerBase != myBaseUpper
        && (is_73 || isRogerSignalReportToken(last_word));
    bool const liveFt2AutoCqDirectedGrid =
        m_autoCqRepeat
        && m_mode == QStringLiteral("FT2")
        && m_asyncTxEnabled
        && directedToMeEffective
        && !messagePartnerBase.isEmpty()
        && messagePartnerBase != myBaseUpper
        && isGridTokenStrict(last_word);
    bool const ft2AutoCqKnownExchangeSignoff =
        m_autoCqRepeat
        && m_mode == QStringLiteral("FT2")
        && m_asyncTxEnabled
        && directedResponseToMe
        && is_73
        && (messagePartnerMatchesCurrentQso
            || isDirectedActivePartnerSignoffDecode(f));
    QString const cooldownKey = !messagePartnerBase.isEmpty()
        ? messagePartnerBase
        : Radio::base_callsign(from).trimmed().toUpper();

    // A late final 73 is a recovery path, not a new QSO.  It must be checked
    // before the generic cooldown/recent-worked gates: those gates correctly
    // suppress ordinary duplicate traffic, but used to discard a valid 73
    // when AutoCQ had already left a retry-capped RR73 exchange.
    QString const latePartnerBase = Radio::base_callsign(m_lateAutoLogCall).trimmed().toUpper();
    bool const latePartnerWindowOpen =
        m_lateAutoLogValid
        && m_lateAutoLogExpires.isValid()
        && QDateTime::currentDateTimeUtc() <= m_lateAutoLogExpires
        && !latePartnerBase.isEmpty();
    bool const fromLatePartner =
        latePartnerWindowOpen
        && !messagePartnerBase.isEmpty()
        && messagePartnerBase == latePartnerBase;
    if (fromLatePartner && is_73) {
        m_pendingAutoLogValid = true;
        m_pendingAutoLogCall = m_lateAutoLogCall;
        m_pendingAutoLogGrid = m_lateAutoLogGrid;
        m_pendingAutoLogRptSent = m_lateAutoLogRptSent;
        m_pendingAutoLogRptRcvd = m_lateAutoLogRptRcvd;
        m_pendingAutoLogOn = m_lateAutoLogOn;
        m_pendingAutoLogOff = QDateTime::currentDateTimeUtc();
        if (m_pendingAutoLogOn.isValid() && m_pendingAutoLogOff < m_pendingAutoLogOn) {
            m_pendingAutoLogOff = m_pendingAutoLogOn;
        }
        m_pendingAutoLogDialFreq = m_lateAutoLogDialFreq;
        removeCallerFromQueue(latePartnerBase);
        bridgeLog("late-signoff-log: " + latePartnerBase + " msg=" + msg);
        clearLateAutoLogSnapshot();
        m_qsoLogged = false;
        logQso();
        return;
    }
    if (fromLatePartner) {
        removeCallerFromQueue(latePartnerBase);
        bridgeLog("late-signoff-skip: " + latePartnerBase + " msg=" + msg);
        return;
    }

    if (is_73 && !cooldownKey.isEmpty()) {
        qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        qint64 const cooldownMs = kQsoCooldownWindowMs;
        // purge entries scadute
        for (auto it = m_qsoCooldown.begin(); it != m_qsoCooldown.end(); ) {
            if (nowMs - it.value() > cooldownMs) it = m_qsoCooldown.erase(it);
            else ++it;
        }
        // FIX 1.0.360 (9H1SR): un 73/RR73 da un nominativo APPENA LOGGATO/lavorato non
        // deve riaprire un QSO (nuova chiamata/TX1). m_qsoCooldown viene azzerato da
        // vari reset di stato (clearTxMessages, resetStartupTransientQsoState, cambio
        // modo) e l'entry appena inserita sul log puo' sparire -> il repeat RR73 sfuggiva
        // e ripartiva un TX1. Aggiungo il check resiliente isRecentAutoCqWorkedOrLogged
        // Duplicate (QHash call|band|mode popolato sul log, age-pruned, NON azzerato dai
        // reset di stato).
        if (m_qsoCooldown.contains(cooldownKey)
            || isRecentAutoCqWorkedOrLoggedDuplicate(cooldownKey, m_frequency, m_mode)) {
            bridgeLog("QSO cooldown: ignoro " + last_word + " da " + cooldownKey
                      + " (recently worked/logged)");
            return;
        }
    }

    QString duplicateActivePartner = m_dxCall.trimmed();
    if (duplicateActivePartner.isEmpty()) {
        duplicateActivePartner = m_autoCqLockedCall.trimmed();
    }
    if (duplicateActivePartner.isEmpty()) {
        duplicateActivePartner = inferredPartnerForAutolog().trimmed();
    }
    QString const duplicateActiveBase = normalizedBaseCall(duplicateActivePartner);
    bool const finalSignoffFromActivePartner =
        is_73
        && !messagePartnerBase.isEmpty()
        && !duplicateActiveBase.isEmpty()
        && messagePartnerBase == duplicateActiveBase;
    bool const finalSignoffFromActiveExchange =
        finalSignoffFromActivePartner
        && m_currentTx != 6
        && m_qsoProgress > 1
        && m_qsoProgress < 6;
    if (m_autoCqRepeat && !messagePartnerBase.isEmpty()
        && !finalSignoffFromActiveExchange) {
        if (liveFt2AutoCqDirectedGrid) {
            if (isRecentAutoCqWorkedOrLoggedDuplicate(messagePartnerBase, m_frequency, m_mode)) {
                bridgeLog(QStringLiteral("autoSeq: skip recently worked/logged AutoCQ partner %1 msg=%2")
                              .arg(messagePartnerBase, msg));
                touchRecentAutoCqWorked(messagePartnerBase, m_frequency, m_mode);
                removeCallerFromQueue(messagePartnerBase);
                if (directedToMe) {
                    maybeStartCourtesySignoff(messagePartner, messagePartnerBase, msg);
                }
                return;
            }
            clearRecentAutoCqAbandoned(messagePartnerBase,
                                       m_frequency,
                                       m_mode,
                                       QStringLiteral("live FT2 directed grid"));
        } else if (isRecentAutoCqDuplicate(messagePartnerBase, m_frequency, m_mode)) {
            bridgeLog(QStringLiteral("autoSeq: skip recently completed AutoCQ partner %1 msg=%2")
                          .arg(messagePartnerBase, msg));
            removeCallerFromQueue(messagePartnerBase);
            if (directedToMe) {
                maybeStartCourtesySignoff(messagePartner, messagePartnerBase, msg);
            }
            return;
        }
    }

    if (m_autoCqRepeat
        && m_qsoProgress > 1
        && m_currentTx != 6
        && m_dxCall.trimmed().isEmpty()
        && !m_autoCqLockedCall.trimmed().isEmpty()) {
        restoreAutoCqPartnerLock();
    }

    bool const autoCqCallingCq = m_autoCqRepeat && m_currentTx == 6;
    bool const autoCqExchangeInProgress =
        m_autoCqRepeat
        && !autoCqCallingCq
        && !duplicateActiveBase.isEmpty()
        && (m_qsoProgress > 1
            || (m_txEnabled && m_currentTx >= 1 && m_currentTx <= 5)
            || (!m_autoCqLockedCall.trimmed().isEmpty() && m_currentTx != 6));
    bool const differentAutoCqCallerDuringActiveExchange =
        autoCqExchangeInProgress
        && !messagePartnerBase.isEmpty()
        && messagePartnerBase != duplicateActiveBase;
    if (differentAutoCqCallerDuringActiveExchange) {
        if (directedToMe && !is_73) {
            bool freqOk = false;
            int const freq = f.size() >= 8 ? f.value(7).toInt(&freqOk) : m_rxFrequency;
            int const snr = f.size() >= 2 ? decodeSnrOrDefault(f.value(1), -99) : -99;
            enqueueCallerInternal(messagePartner,
                                  freqOk ? freq : m_rxFrequency,
                                  snr,
                                  true);
        }
        bridgeLog(QStringLiteral("autoSeq: keep active AutoCQ partner %1; queue/ignore %2 msg=%3")
                      .arg(duplicateActiveBase,
                           messagePartnerBase,
                           msg));
        return;
    }

    // Shannon (riga 9699-9720): filtro messaggi — solo quelli rilevanti per noi
    // CQ mode (m_bCallingCQ && m_bAutoReply): accetta solo chi ci chiama (parts[0]==m_callsign)
    // Non CQ mode: accetta solo dalla stazione DX corrente; se dxCall vuoto → non rispondere
    // Manual TX6 arm behaves like CQ mode too: after a completed QSO the user can
    // re-arm TX by hand and still expect directed callers to start a new QSO.
    bool const manualCqArmed = m_txEnabled && m_currentTx == 6;
    bool inCqMode = (m_autoCqRepeat || manualCqArmed)
                    && (m_currentTx == 6 || m_dxCall.isEmpty() || m_qsoProgress <= 1)
                    && !ft2AutoCqKnownExchangeSignoff;
    bool cqModeAcceptedFreshCaller = false;
    if (inCqMode) {
        // Rispondo solo a messaggi che coinvolgono il mio call. Alcuni decoder
        // espongono i due call nell'ordine DX/MY invece di MY/DX.
        if (!directedToMe) return;
        QString staleCqReason;
        if (!decodeIsFreshForCqAutoReply(f.value(0), &staleCqReason)) {
            bridgeLog(QStringLiteral("autoSeq: CQ auto-reply ignored stale directed decode (%1): %2")
                          .arg(staleCqReason, msg));
            return;
        }
        if (normalizedUsableCallToken(messagePartner).isEmpty()) {
            bridgeLog(QStringLiteral("autoSeq: directed caller unresolved, stopping CQ: %1")
                          .arg(msg));
            if (m_autoCqRepeat) {
                setAutoCqRepeat(false);
            }
            if (m_txEnabled) {
                setTxEnabled(false);
            }
            if (m_qsoProgress != 0) {
                m_qsoProgress = 0;
                emit qsoProgressChanged();
            }
            m_txRetryCount = 0;
            m_txWatchdogTicks = 0;
            m_autoCQPeriodsMissed = 0;
            m_lastNtx = -1;
            m_lastCqPidx = -1;
            clearAutoCqPartnerLock();
            emit statusMessage(QStringLiteral("AutoCQ fermato: chiamata diretta con nominativo compresso"));
            emit warningRaised(tr("AutoCQ stopped"),
                               tr("Direct call received, but the callsign is not resolved"),
                               msg);
            return;
        }
        if (is_73) {
            bridgeLog(QStringLiteral("autoSeq: ignore final payload from CQ caller without active exchange: %1")
                          .arg(msg));
            return;
        }
        if (m_multiAnswerMode) {
            if (!messagePartnerBase.isEmpty()) {
                bool freqOk = false;
                int const freq = f.size() >= 8 ? f.value(7).toInt(&freqOk) : m_rxFrequency;
                int const snr = f.size() >= 2 ? decodeSnrOrDefault(f.value(1), -99) : -99;
                enqueueCallerInternal(messagePartner,
                                      freqOk ? freq : m_rxFrequency,
                                      snr,
                                      true);
            }
            processNextInQueue();
            return;
        }
    } else {
        // Non in CQ mode: nessun dxCall → non rispondere automaticamente (Shannon: clearDX)
        if (m_dxCall.isEmpty()) {
            restoreAutoCqPartnerLock();
        }
        if (m_dxCall.isEmpty()
            && messagePartnerMatchesCurrentQso
            && !messagePartner.trimmed().isEmpty()) {
            setDxCall(messagePartner);
        }
        if (m_dxCall.isEmpty()) return;
        QString const activeDxBase = Radio::base_callsign(m_dxCall).trimmed().toUpper();
        // Rispondo solo dalla stazione DX corrente
        if (messagePartnerBase.isEmpty()
            || activeDxBase.isEmpty()
            || messagePartnerBase != activeDxBase) {
            // Stazione diversa: in multi-answer mode, aggiunge alla coda se ci sta chiamando
            // (parts[0] == m_callsign indica che il messaggio è indirizzato a noi)
            if ((m_multiAnswerMode || m_autoCqRepeat) && !messagePartner.isEmpty() && !myCallUpper.isEmpty() &&
                tokenMatchesCall(parts[0], myCallUpper, myBaseUpper)) {
                int freq = f.size() >= 8 ? f[7].toInt() : m_rxFrequency;
                int snr = f.size() >= 2 ? f[1].toInt() : -99;
                if (enqueueCallerInternal(messagePartner, freq, snr, true)) {
                    bridgeLog("autoSeq: multi-answer: enqueue " + messagePartner + " (in QSO con " + m_dxCall + ")");
                }
            } else if (directedToMe && !messagePartnerBase.isEmpty() && !activeDxBase.isEmpty()) {
                bridgeLog(QStringLiteral("autoSeq: ignore directed decode from non-active partner %1 while active=%2 msg=%3")
                              .arg(messagePartnerBase, activeDxBase, msg));
            }
            return;
        }

        // Any directed payload from the active partner means the QSO is still
        // alive for watchdog/CQ-missed purposes. Do not reset m_txRetryCount
        // here, though: repeated FT4 grid/report payloads can keep the same TX
        // step active forever and would otherwise bypass "Caller retries".
        // The retry counter is reset below only when the sequence actually
        // advances to a different TX step or a fresh partner is accepted.
        m_txWatchdogTicks = 0;
        m_autoCQPeriodsMissed = 0;
    }

    QString activeSignoffPartner = m_dxCall.trimmed();
    if (activeSignoffPartner.isEmpty()) {
        activeSignoffPartner = m_autoCqLockedCall.trimmed();
    }
    if (activeSignoffPartner.isEmpty()) {
        activeSignoffPartner = inferredPartnerForAutolog().trimmed();
    }
    QString const activeSignoffBase = normalizedBaseCall(activeSignoffPartner);
    bool const signoffFromActivePartner =
        !messagePartnerBase.isEmpty()
        && !activeSignoffBase.isEmpty()
        && messagePartnerBase == activeSignoffBase;
    bool const localSignoffStateForMessagePartner = signoffStateForMessagePartnerNow();
    bool const lastTransmittedSignoffForMessagePartner = lastSignoffForMessagePartnerNow();
    bool const localSignoffAlreadySentForMessagePartner =
        lastTransmittedSignoffForMessagePartner
        && (m_nTx73 > 0 || m_lastNtx == 4 || m_lastNtx == 5);
    bool const localFinal73AlreadyArmedOrSentForMessagePartner =
        ((m_currentTx == 5) && currentPayloadMentionsMessagePartnerNow())
        || ((m_lastNtx == 5) && lastPayloadMentionsMessagePartnerNow());
    bool const localRogerReportForMessagePartner =
        !messagePartnerBase.isEmpty()
        && ((m_currentTx == 3 && currentPayloadMentionsMessagePartnerNow())
            || (m_lastNtx == 3 && lastPayloadMentionsMessagePartnerNow())
            || (!m_autoSeqRogerReportBase.isEmpty()
                && m_autoSeqRogerReportBase == messagePartnerBase));
    bool const signoffAfterOurRogerReportToActivePartner =
        directedToMeEffective
        && signoffFromActivePartner
        && localRogerReportForMessagePartner;
    bool const signoffEligibleForActivePartner =
        m_qsoProgress >= 4 || signoffAfterOurRogerReportToActivePartner;
    bool const partnerSignoff73 =
        (last.compare(QStringLiteral("73"), Qt::CaseInsensitive) == 0
         && signoffEligibleForActivePartner);
    bool const partnerSignoffRR73 =
        (last.compare(QStringLiteral("RR73"), Qt::CaseInsensitive) == 0
         && signoffEligibleForActivePartner);
    bool const partnerSignoffRRR =
        (last.compare(QStringLiteral("RRR"), Qt::CaseInsensitive) == 0
         && signoffEligibleForActivePartner);
    bool const partnerAnySignoff = partnerSignoff73 || partnerSignoffRR73 || partnerSignoffRRR;
    if (partnerAnySignoff && signoffAfterOurRogerReportToActivePartner && m_qsoProgress < 4) {
        bridgeLog(QStringLiteral("autoSeq: signoff %1 from active partner after TX3/report -> force final 73")
                      .arg(last));
    }
    if (partnerAnySignoff && !directedToMeEffective && !signoffFromActivePartner) {
        bridgeLog(QStringLiteral("autoSeq: ignore unrelated signoff from %1 while active=%2 msg=%3")
                      .arg(messagePartnerBase.isEmpty() ? from : messagePartnerBase,
                           activeSignoffBase.isEmpty() ? QStringLiteral("<none>") : activeSignoffBase,
                           msg));
        return;
    }
    bool const profileNeedsOwn73BeforeLog =
        ((m_mode == QStringLiteral("FT2") && !m_quickQsoEnabled)
         || m_mode == QStringLiteral("FT8")
         || m_mode == QStringLiteral("FT4")
         || m_mode == QStringLiteral("MSK144"));
    bool const partnerSignoffNeedsOwn73 =
        partnerAnySignoff
        && !localFinal73AlreadyArmedOrSentForMessagePartner
        && profileNeedsOwn73BeforeLog;
    bool const partnerFinalAck =
        partnerAnySignoff && !partnerSignoffNeedsOwn73;

    bool const partnerSignoffShouldTriggerExplicitFinal73 =
        (partnerSignoffRR73 || partnerSignoffRRR)
        && profileNeedsOwn73BeforeLog;

    if (partnerAnySignoff
        && localSignoffAlreadySentForMessagePartner
        && !partnerSignoffShouldTriggerExplicitFinal73) {
        if (!cooldownKey.isEmpty()) {
            m_qsoCooldown[cooldownKey] = QDateTime::currentMSecsSinceEpoch();
        }
        finishAutoSequenceQso(QStringLiteral("autoSeq: ricevuto %1 da %2 dopo nostro signoff TX%3 -> QSO completo")
                                  .arg(last,
                                       messagePartner.isEmpty() ? from : messagePartner,
                                       QString::number(m_lastNtx)));
        return;
    }

    if (m_transmitting
        && m_mode == QStringLiteral("FT2")
        && m_asyncTxEnabled
        && m_pendingAutoSeqTxAfterActiveTx > 0) {
        int incomingTx = 0;
        if (partnerAnySignoff) {
            incomingTx = 5;
        } else if (isRogerSignalReportToken(last)) {
            incomingTx = 4;
        } else if (last.compare(QStringLiteral("TU"), Qt::CaseInsensitive) == 0
                   && parts.size() >= 4
                   && isPlainSignalReportToken(parts.at(parts.size() - 2))) {
            incomingTx = 4;
        } else if (isPlainSignalReportToken(last)) {
            incomingTx = 3;
        } else if (isGridTokenStrict(last)) {
            incomingTx = 2;
        }

        if (incomingTx > 0
            && autoSeqTxRank(incomingTx) < autoSeqTxRank(m_pendingAutoSeqTxAfterActiveTx)) {
            bridgeLog(QStringLiteral("autoSeq: ignore stale TX%1 payload while TX%2 is active; pending TX%3 wins: %4")
                          .arg(incomingTx)
                          .arg(m_currentTx)
                          .arg(m_pendingAutoSeqTxAfterActiveTx)
                          .arg(msg));
            return;
        }
    }

    bool const localSignoffStageActive =
        localSignoffStateForMessagePartner
        || ((m_ft2DeferredLogPending || m_logAfterOwn73)
            && (currentPayloadMentionsMessagePartnerNow()
                || lastPayloadMentionsMessagePartnerNow()));
    bool const stalePreSignoffPayload =
        !partnerAnySignoff
        && (isGridTokenStrict(last)
            || isPlainSignalReportToken(last)
            || isRogerSignalReportToken(last)
            || last.compare(QStringLiteral("TU"), Qt::CaseInsensitive) == 0);
    if (localSignoffStageActive && stalePreSignoffPayload) {
        int const keepTx = (m_currentTx == 5 || m_logAfterOwn73) ? 5 : 4;
        if (m_currentTx != keepTx) {
            if (!advanceQsoState(keepTx)) {
                return;
            }
        }
        setTxEnabled(true);
        updateAutoCqPartnerLock();
        bridgeLog(QStringLiteral("autoSeq: signoff monotonic guard keeps TX%1, ignoring stale payload from %2: %3")
                      .arg(keepTx)
                      .arg(messagePartner.isEmpty() ? from : messagePartner)
                      .arg(msg));
        return;
    }

    if (m_logAfterOwn73 && !partnerAnySignoff) {
        bridgeLog("autoSeq: own 73 pending, ignore non-final payload from " +
                  (messagePartner.isEmpty() ? from : messagePartner) + ": " + msg);
        return;
    }

    if (inCqMode && !messagePartner.isEmpty()) {
        QString const callerBase = messagePartnerBase;
        if (callerBase.isEmpty()) {
            return;
        }

        if (m_dxCall.trimmed().isEmpty()) {
            if (liveFt2AutoCqDirectedGrid) {
                if (isRecentAutoCqWorkedOrLoggedDuplicate(callerBase, m_frequency, m_mode)) {
                    bridgeLog("autoSeq: skip recently worked/logged AutoCQ caller " + callerBase);
                    return;
                }
                clearRecentAutoCqAbandoned(callerBase,
                                           m_frequency,
                                           m_mode,
                                           QStringLiteral("CQ-mode live FT2 directed grid"));
            } else if (isRecentAutoCqDuplicate(callerBase, m_frequency, m_mode)) {
                bridgeLog("autoSeq: skip recent AutoCQ caller " + callerBase);
                return;
            }
        }

        QString const previousDxBase = Radio::base_callsign(m_dxCall).trimmed().toUpper();
        bool const existingExchangeForCaller =
            !previousDxBase.isEmpty()
            && previousDxBase == callerBase
            && m_qsoProgress > 1
            && m_currentTx != 6;
        cqModeAcceptedFreshCaller = !existingExchangeForCaller;

        if (Radio::base_callsign(m_dxCall).trimmed().compare(callerBase, Qt::CaseInsensitive) != 0) {
            setReportReceivedPlaceholder(QStringLiteral("-10"));
            setDxCall(messagePartner);
            m_qsoStartedOn = QDateTime::currentDateTimeUtc();
            m_qsoFirstReplyOn = QDateTime {};
            m_lastTransmittedMessage.clear();
            if (!isGridTokenStrict(last)) {
                setDxGrid(QString());
            }
        } else if (!m_qsoStartedOn.isValid()) {
            m_qsoStartedOn = QDateTime::currentDateTimeUtc();
        }

        m_txRetryCount = 0;
        m_lastNtx = -1;
        m_nTx73 = 0;
        m_autoSeqRogerReportBase.clear();
        m_logAfterOwn73 = false;
        m_ft2DeferredLogPending = false;
        m_quickPeerSignaled = false;
        m_lastCqPidx = -1;
        m_autoCQPeriodsMissed = 0;
        m_txWatchdogTicks = 0;
        if (m_autoCqRepeat && cqModeAcceptedFreshCaller) {
            clearFt2AutoCqAwaitingPartnerDecode(QStringLiteral("AutoCQ fresh caller"));
            resetAutoCqBurstCadence(QStringLiteral("valid AutoCQ caller accepted"));
        }
        setReportSent(QString::number(decodeSnrOrDefault(f.value(1), -10)));

        if (isGridTokenStrict(last)) {
            setDxGrid(last.toUpper());
        }

        if (m_autoCqRepeat
            && m_mode == QStringLiteral("FT2")
            && m_asyncTxEnabled
            && cqModeAcceptedFreshCaller
            && isGridTokenStrict(last)) {
            m_autoCqLockedCall = messagePartner.trimmed();
            m_autoCqLockedGrid = last.toUpper();
            m_autoCqLockedNtx = 2;
            m_autoCqLockedProgress = 3;
            m_asyncLastTxEndMs = 0;
            bridgeLog(QStringLiteral("FT2 AutoCQ latch: caller=%1 grid=%2 -> TX2")
                          .arg(callerBase, m_autoCqLockedGrid));
        }

        bridgeLog("autoSeq: start AutoCQ partner " + callerBase + " msg=" + msg);
    }

    if (partnerSignoff73) {
        if (m_mode == "FT2" && m_asyncTxEnabled && !cooldownKey.isEmpty())
            m_qsoCooldown[cooldownKey] = QDateTime::currentMSecsSinceEpoch();
        if (partnerSignoffNeedsOwn73) {
            bridgeLog("autoSeq: ricevuto 73 da " + (messagePartner.isEmpty() ? from : messagePartner) + " → invio nostro 73 prima del log");
            // Preserve the confirmed exchange before scheduling TX5.  The
            // audio-complete callback is asynchronous and AutoCQ may have
            // already re-armed CQ by the time it asks for the log partner.
            capturePendingAutoLogSnapshot();
            if (m_autoCqRepeat) {
                armLateAutoLogSnapshot();
            }
            m_ft2DeferredLogPending = false;
            m_nTx73 = 0;
            m_autoSeqRogerReportBase.clear();
            m_logAfterOwn73 = true;
            nextTx = 5;
        } else {
            finishAutoSequenceQso("autoSeq: ricevuto 73 da " + (messagePartner.isEmpty() ? from : messagePartner) + " → QSO completo (IDLE_QSO)");
            return;
        }
    } else if (last.compare("RR73", Qt::CaseInsensitive) == 0 ||
               last.compare("RRR",  Qt::CaseInsensitive) == 0) {
        // Se abbiamo appena mandato TX4 (RR73/RRR), non liberare ancora il
        // partner: arma TX5 73. Altrimenti un chiamante nello stesso batch di
        // decode puo' prendere il QSO prima del nostro 73 finale.
        if (m_currentTx == 5
            && m_logAfterOwn73
            && (m_transmitting || m_lastNtx != 5 || m_nTx73 == 0)) {
            bridgeLog("autoSeq: TX5 73 gia' armato per " +
                      (messagePartner.isEmpty() ? from : messagePartner) +
                      ", ignoro signoff ripetuto: " + msg);
            return;
        }
        if (m_qsoProgress >= 5 && m_nTx73 >= 1) {
            if (partnerSignoffShouldTriggerExplicitFinal73
                && m_currentTx != 5
                && !m_tx5.trimmed().isEmpty()
                && !isTxDisabled(5)) {
                bridgeLog("autoSeq: ricevuto " + last + " da " +
                          (messagePartner.isEmpty() ? from : messagePartner) +
                          " dopo nostro TX4 -> TX5 (73) prima del log");
                m_ft2DeferredLogPending = false;
                m_logAfterOwn73 = true;
                m_nTx73 = 0;
                m_autoSeqRogerReportBase.clear();
                nextTx = 5;
            } else {
                if (!cooldownKey.isEmpty())
                    m_qsoCooldown[cooldownKey] = QDateTime::currentMSecsSinceEpoch();
                finishAutoSequenceQso("autoSeq: ricevuto RR73 ma già in SIGNOFF con nTx73=" + QString::number(m_nTx73) + " → QSO completo");
                return;
            }
        } else {
            if (!cooldownKey.isEmpty())
                m_qsoCooldown[cooldownKey] = QDateTime::currentMSecsSinceEpoch();
            if (partnerSignoffNeedsOwn73) {
                bridgeLog("autoSeq: ricevuto RR73 da " + (messagePartner.isEmpty() ? from : messagePartner) + " → invio nostro 73 prima del log");
                // A received RR73 is the partner's final acknowledgement.
                // Snapshot it before TX5 so PD2WL-style QSOs cannot lose the
                // log identity if AutoCQ changes state during audio teardown.
                capturePendingAutoLogSnapshot();
                // The partner can repeat RR73 after our final 73 (IU1WKA
                // case).  Arm recovery now, rather than only at the retry
                // cap, so that repeat can repair a missed TX5 completion log.
                if (m_autoCqRepeat) {
                    armLateAutoLogSnapshot();
                }
                m_ft2DeferredLogPending = false;
                m_nTx73 = 0;
                m_autoSeqRogerReportBase.clear();
                m_logAfterOwn73 = true;
                nextTx = 5;
            } else if (partnerFinalAck) {
                finishAutoSequenceQso("autoSeq: ricevuto RR73 finale da " + (messagePartner.isEmpty() ? from : messagePartner) + " → QSO completo");
                return;
            } else {
                bridgeLog("autoSeq: ricevuto " + last + " → TX5 (73)");
                nextTx = 5;
            }
        }
    } else if (last.compare("TU", Qt::CaseInsensitive) == 0 && parts.size() >= 4) {
        // Quick QSO (Ultra2): peer ha inviato "R+NN TU" o "+NN TU"
        // Significa che il peer considera il QSO completo → rispondiamo con RR73 (TX4)
        QString reportToken = parts[parts.size() - 2];
        static QRegularExpression const rptRx(R"(^(?:R)?[+-]\d{2}$)");
        if (rptRx.match(reportToken).hasMatch()) {
            bridgeLog("autoSeq: ricevuto Quick QSO TU da " + from + " (report=" + reportToken + ") → TX4 (RR73)");
            setReportReceived(reportToken.startsWith('R') ? reportToken.mid(1) : reportToken);
            if (!m_dxCall.isEmpty()) genStdMsgs(m_dxCall, m_dxGrid);
            m_quickPeerSignaled = true;
            m_autoSeqRogerReportBase = messagePartnerBase;
            nextTx = 4;
        } else {
            bridgeLog("autoSeq: TU senza report valido, ignoro");
            return;
        }
    } else if (last.length() >= 2 && last[0] == 'R' &&
               (last[1] == '-' || last[1] == '+' || last[1].isDigit())) {
        // Salva il report ricevuto dal partner: la forma "R<report>" è una
        // conferma + report del partner verso di noi. Se l'utente ha saltato
        // lo scambio del report nudo, questo è l'unico punto in cui il valore
        // viene memorizzato — senza, l'ADIF loggerebbe RST_RCVD vuoto/stale.
        // (Parallelismo con i rami report-nudo e Quick-QSO TU qui sotto.)
        QString const rptValue = last.mid(1);
        bool const ownReportWasSentToSamePartner =
            !messagePartnerBase.isEmpty()
            && (m_lastNtx == 2 || m_lastNtx == 3)
            && lastPayloadMentionsMessagePartner;
        // A R+report cannot be trusted as a continuation merely because a
        // stale TX number/message survived a prior QSO.  The current partner
        // must already have produced an accepted reply in this QSO; otherwise
        // this is its first useful decode and we restart conservatively at TX3.
        bool const priorReportSentToPartner =
            decodium::seq::mayAdvanceRogerReportToRr73(
                cqModeAcceptedFreshCaller,
                ownReportWasSentToSamePartner,
                m_qsoFirstReplyOn.isValid());
        if (!priorReportSentToPartner) {
            bridgeLog(QStringLiteral("autoSeq: R+report %1 from fresh/unsent caller %2 -> TX3, not RR73")
                          .arg(last,
                               messagePartner.isEmpty() ? from : messagePartner));
            setReportSent(QString::number(decodeSnrOrDefault(f.value(1), -10)));
            setReportReceived(rptValue);
            if (!m_dxCall.isEmpty()) genStdMsgs(m_dxCall, m_dxGrid);
            m_autoSeqRogerReportBase.clear();
            nextTx = 3;
        } else {
            // 1.0.446 - opt-in guard "post-log re-engage" (default OFF): se il partner e' stato
            // APPENA loggato (entro m_qsoCooldown=30s) e ha gia' ottenuto m_ft2PostLogReengageMax
            // RRR di cortesia, NON ri-avanzare a TX4/RRR -> evita le "troppe richiamate" quando il
            // partner non sente il nostro RRR e continua a mandare R+report (caso 9H1SR). Il gate
            // cooldown is_73 non copre i payload R+report; ri-rispondere 1-2 volte e' corretto.
            if (m_ft2PostLogReengageGuard
                && m_mode == QStringLiteral("FT2")
                && !messagePartnerBase.isEmpty()
                && m_qsoCooldown.contains(messagePartnerBase)) {
                int &reengN = m_postLogReengageCount[messagePartnerBase];
                if (reengN >= m_ft2PostLogReengageMax) {
                    bridgeLog(QStringLiteral("autoSeq: post-log reengage guard: %1 gia' loggato, ignoro R+report ripetuto (%2/%3)")
                                  .arg(messagePartnerBase).arg(reengN).arg(m_ft2PostLogReengageMax));
                    return;
                }
                ++reengN;
            }
            bridgeLog("autoSeq: ricevuto R+report " + last + " (rpt=" + rptValue + ") → TX4 (RR73)");
            setReportReceived(rptValue);
            if (!m_dxCall.isEmpty()) genStdMsgs(m_dxCall, m_dxGrid);
            m_autoSeqRogerReportBase = messagePartnerBase;
            nextTx = 4;
        }
    } else if (last.length() >= 2 &&
               (last[0] == '-' || last[0] == '+') && last[1].isDigit()) {
        bridgeLog("autoSeq: ricevuto report " + last + " → TX3 (R + nostro report)");
        // Salva il report ricevuto dal partner per log/ADIF, ma TX3 deve usare
        // il report che noi inviamo al partner, cioe' il suo dB ricevuto qui.
        setReportSent(QString::number(decodeSnrOrDefault(f.value(1), -10)));
        setReportReceived(last);
        if (!m_dxCall.isEmpty()) genStdMsgs(m_dxCall, m_dxGrid);
        m_autoSeqRogerReportBase.clear();
        nextTx = 3;
    } else if (isGridTokenStrict(last)) {
        // Grid ricevuto:
        // - Stazione nuova o grid nuovo -> TX2 (invia il nostro report)
        // - Stessa stazione con stesso grid -> ripeti TX2. Non avanzare a TX3:
        //   il partner sta ritrasmettendo il proprio grid perche' non ha ancora
        //   copiato il nostro report.
        bool sameGrid = (!m_dxGrid.isEmpty() &&
                         last.compare(m_dxGrid, Qt::CaseInsensitive) == 0 &&
                         Radio::base_callsign(m_dxCall).trimmed().compare(messagePartnerBase, Qt::CaseInsensitive) == 0 &&
                         m_qsoProgress >= 3);  // già in REPORT state (enum 3=REPORT)
        m_autoSeqRogerReportBase.clear();
        if (sameGrid) {
            setReportSent(QString::number(decodeSnrOrDefault(f.value(1), -10)));
            if (!m_dxCall.isEmpty()) genStdMsgs(m_dxCall, m_dxGrid);
            bridgeLog("autoSeq: stesso grid da " + (messagePartner.isEmpty() ? from : messagePartner) + " → resta su TX2 (report aggiornato)");
            nextTx = 2;
        } else {
            bridgeLog("autoSeq: grid da " + (messagePartner.isEmpty() ? from : messagePartner) + " → TX2 (report)");
            if (messagePartnerBase.isEmpty()) {
                return;
            }
            if (Radio::base_callsign(m_dxCall).trimmed().compare(messagePartnerBase, Qt::CaseInsensitive) != 0) {
                setReportReceivedPlaceholder(QStringLiteral("-10"));
                setDxCall(messagePartner);
                m_qsoStartedOn = QDateTime::currentDateTimeUtc();
                m_qsoFirstReplyOn = QDateTime {};
                m_lastTransmittedMessage.clear();
            }
            setReportSent(QString::number(decodeSnrOrDefault(f.value(1), -10)));
            setDxGrid(last);
            genStdMsgs(messagePartner, last);
            nextTx = 2;
        }
    } else {
        return;
    }

    // ADIF/LoTW: the QSO start is the first valid reply from the DX station,
    // not the time at which the operator first armed the call.  This is done
    // after the message has been accepted by the sequencer and after any new
    // partner has been installed in m_dxCall, so a queued AutoCQ caller is
    // timestamped correctly as well.
    if (directedToMeEffective
        && m_qsoFirstReplyOn.isNull()
        && !messagePartnerBase.isEmpty()) {
        QString const activePartnerBase =
            Radio::base_callsign(m_dxCall).trimmed().toUpper();
        if (activePartnerBase.isEmpty() || activePartnerBase == messagePartnerBase) {
            int const decodeSeconds = secondsFromUtcDisplayToken(f.value(0));
            if (decodeSeconds >= 0) {
                QDateTime const decodeUtc = approxUtcDateTimeForDisplayToken(f.value(0));
                bool const notStaleBeforeQsoStart =
                    !m_qsoStartedOn.isValid()
                    || decodeUtc >= m_qsoStartedOn.toUTC().addSecs(-30);
                if (decodeUtc.isValid() && notStaleBeforeQsoStart) {
                    m_qsoFirstReplyOn = decodeUtc.toUTC();
                    bridgeLog(QStringLiteral("qso-log: first valid DX reply %1 at %2 -> TIME_ON")
                                  .arg(messagePartnerBase,
                                       m_qsoFirstReplyOn.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss UTC"))));
                }
            }
        }
    }

    if (nextTx > 0) {
        // Safety barrier: never send RR73/73 for a caller unless this
        // caller has actually received our numeric report.  AutoCQ/MAM can
        // carry a stale closing state across callers; a grid decode from the
        // new caller must then restart at TX2 instead of inheriting TX5.
        if (nextTx == 5 && !messagePartnerBase.isEmpty()) {
            // 1.0.718: our own RR73/RRR/73 also counts. A partner who answers our RR73 with
            // RR73 (LY5A, 27/10/2026) was restarted at TX2 three times and never logged.
            bool const reportWasSentToThisPartner =
                decodium::seq::lastTxJustifiesSignoff(m_lastTransmittedMessage)
                && messageContainsCallToken(m_lastTransmittedMessage,
                                             messagePartner,
                                             messagePartnerBase);
            if (!reportWasSentToThisPartner) {
                bridgeLog(QStringLiteral("autoSeq: blocked stale TX5/RR73 for %1; last TX was [%2], restarting at TX2")
                              .arg(messagePartnerBase, m_lastTransmittedMessage));
                nextTx = 2;
            }
        }
        bool const ft2AutomatedExchange =
            m_mode == QStringLiteral("FT2")
            && m_asyncTxEnabled
            && (m_autoCqRepeat || m_multiAnswerMode || m_waitPounceActive);
        QString const activeBaseForNextTx = Radio::base_callsign(m_dxCall).trimmed().toUpper();
        if (nextTx >= 1 && nextTx <= 5
            && !messagePartnerBase.isEmpty()
            && (activeBaseForNextTx.isEmpty() || messagePartnerBase != activeBaseForNextTx)) {
            bridgeLog(QStringLiteral("autoSeq: discard TX%1 for non-current partner %2 active=%3 msg=%4")
                          .arg(nextTx)
                          .arg(messagePartnerBase,
                               activeBaseForNextTx.isEmpty() ? QStringLiteral("<none>") : activeBaseForNextTx,
                               msg));
            return;
        }
        bool const sameTxFt2AutoCqWait =
            m_autoCqRepeat
            && m_mode == QStringLiteral("FT2")
            && m_asyncTxEnabled
            && m_ft2AutoCqAwaitingPartnerTx > 0
            && m_currentTx == m_ft2AutoCqAwaitingPartnerTx
            && nextTx == m_currentTx
            && !messagePartnerBase.isEmpty()
            && messagePartnerBase == m_ft2AutoCqAwaitingPartnerBase;
        if (sameTxFt2AutoCqWait && m_ft2AutoCqAwaitingPartnerSinceMs > 0) {
            if (!m_ft2AutoCqAwaitingPartnerDecodeIdentity.isEmpty()
                && autoSeqDecodeIdentity == m_ft2AutoCqAwaitingPartnerDecodeIdentity) {
                bridgeLog(QStringLiteral("FT2 AutoCQ one-shot: ignore replayed same-TX%1 decode from %2 identity=%3")
                              .arg(nextTx)
                              .arg(messagePartnerBase, autoSeqDecodeIdentity));
                return;
            }
            int const decodeSeconds = secondsFromUtcDisplayToken(f.value(0));
            if (decodeSeconds >= 0) {
                int const waitStartSeconds =
                    QDateTime::fromMSecsSinceEpoch(m_ft2AutoCqAwaitingPartnerSinceMs, Qt::UTC)
                        .time()
                        .msecsSinceStartOfDay() / 1000;
                int const afterWaitSeconds = signedUtcSecondDelta(waitStartSeconds, decodeSeconds);
                if (afterWaitSeconds < -2) {
                    bridgeLog(QStringLiteral("FT2 AutoCQ one-shot: ignore stale same-TX%1 decode from %2 time=%3 waitDelta=%4s msg=%5")
                                  .arg(nextTx)
                                  .arg(messagePartnerBase,
                                       f.value(0),
                                       QString::number(afterWaitSeconds),
                                       msg));
                    return;
                }
            }
        }
        // Deduplicazione: se stiamo già trasmettendo o abbiamo già impostato
        // lo stesso TX step → non avanzare di nuovo (evita loop in FT2 async)
        if (nextTx == m_currentTx && (m_transmitting || m_txEnabled)) {
            if (m_autoCqRepeat
                && m_mode == QStringLiteral("FT2")
                && m_asyncTxEnabled
                && !messagePartnerBase.isEmpty()
                && messagePartnerBase == activeBaseForNextTx) {
                if (sameTxFt2AutoCqWait) {
                    m_lastAutoSeqDecodeIdentity = autoSeqDecodeIdentity;
                    clearFt2AutoCqAwaitingPartnerDecode(QStringLiteral("fresh partner same-TX%1 retry").arg(nextTx));
                }
            }
            m_txWatchdogTicks = 0;
            m_autoCQPeriodsMissed = 0;
            bridgeLog("autoSeq: nextTx=" + QString::number(nextTx) +
                      " già attivo (transmitting=" + QString::number(m_transmitting) +
                      " txEnabled=" + QString::number(m_txEnabled) + "), keep alive");
            return;
        }
        // Resetta contatori PRIMA di avanzare stato (evita race condition)
        m_txRetryCount = 0;
        m_txWatchdogTicks = 0;
        m_qsoLogged = false;  // nuovo step → permetti log
        if (nextTx <= 4) {
            m_logAfterOwn73 = false;
            m_ft2DeferredLogPending = false;
            if (nextTx <= 3) {
                m_quickPeerSignaled = false;
            }
        }
        bool const deferAutoSeqUntilTxComplete =
            m_transmitting
            && nextTx != m_currentTx
            && (isTimeSyncDecodeMode(m_mode)
                || (m_mode == QStringLiteral("FT2") && m_asyncTxEnabled));
        if (deferAutoSeqUntilTxComplete) {
            m_lastAutoSeqDecodeIdentity = autoSeqDecodeIdentity;
            clearFt2AutoCqAwaitingPartnerDecode(QStringLiteral("auto-seq defer TX%1").arg(nextTx));
            int const pendingRank = autoSeqTxRank(m_pendingAutoSeqTxAfterActiveTx);
            int const nextRank = autoSeqTxRank(nextTx);
            if (m_pendingAutoSeqTxAfterActiveTx <= 0 || nextRank >= pendingRank) {
                m_pendingAutoSeqTxAfterActiveTx = nextTx;
                m_pendingAutoSeqPartnerBase = activeBaseForNextTx;
                m_pendingAutoSeqMessage = txMessage(nextTx).trimmed();
                m_pendingAutoSeqMode = m_mode.trimmed().toUpper();
            }
            if (m_autoCqRepeat
                && nextTx >= 1 && nextTx <= 5
                && !m_dxCall.trimmed().isEmpty()) {
                m_autoCqLockedCall = m_dxCall.trimmed();
                m_autoCqLockedGrid = m_dxGrid.trimmed();
                m_autoCqLockedNtx = nextTx;
                m_autoCqLockedProgress = qMax(2, m_qsoProgress);
                if (nextTx == 2) {
                    m_autoCqLockedProgress = qMax(3, m_autoCqLockedProgress);
                }
            }
            if (!m_txEnabled) {
                setTxEnabled(true);
            }
            updateAutoCqPartnerLock();
            bridgeLog(QStringLiteral("auto-seq: defer TX%1 while active TX%2 is still playing (pending TX%3)")
                          .arg(nextTx)
                          .arg(m_currentTx)
                          .arg(m_pendingAutoSeqTxAfterActiveTx));
            return;
        }
        // 1.0.130: skip TX marcati come disabled (right-click toggle UI).
        // 1.0.134: TX disabled → NON trasmettere (return), no advance arbitrario.
        // Avanzare TX1→TX2 quando TX1 è disabled significherebbe mandare un
        // report senza prima aver chiamato il partner: rompe il QSO. Meglio
        // semplicemente non trasmettere e lasciar passare il periodo.
        if (nextTx >= 1 && nextTx <= 6 && isTxDisabled(nextTx)) {
            bridgeLog(QStringLiteral("autoSeq: TX%1 user-disabled, skip transmission")
                          .arg(nextTx));
            if (nextTx == 5) {
                // TX5 disabled → finalize QSO subito (skip 73 finale, comune)
                finishAutoSequenceQso(
                    QStringLiteral("autoSeq: TX5 user-disabled -> log immediato"));
            }
            // TX1-TX4 + TX6 disabled: semplicemente non trasmette (l'auto-seq
            // riprenderà al prossimo periodo). Per TX6 questo ferma il
            // re-arm CQ ciclico (= "non auto-CQ").
            return;
        }
        // Avanza stato QSO
        m_lastAutoSeqDecodeIdentity = autoSeqDecodeIdentity;
        clearFt2AutoCqAwaitingPartnerDecode(QStringLiteral("auto-seq TX%1").arg(nextTx));
        if (ft2AutomatedExchange && nextTx >= 1 && nextTx <= 5) {
            int const previousTxPeriod = m_txPeriod;
            int const replyTxPeriod = replyTxPeriodForClickedDecode(
                m_mode, f.value(0), m_txPeriod, effectivePeriodMsForMode(m_mode));
            if (replyTxPeriod != m_txPeriod) {
                setTxPeriod(replyTxPeriod);
                bridgeLog(QStringLiteral("autoSeq: FT2 automated TX period adjusted %1 -> %2 from partner decode time=%3")
                              .arg(previousTxPeriod)
                              .arg(m_txPeriod)
                              .arg(normalizeUtcDisplayToken(f.value(0)).isEmpty()
                                       ? QStringLiteral("<none>")
                                       : normalizeUtcDisplayToken(f.value(0))));
            }
        }
        if (!advanceQsoState(nextTx)) {
            return;
        }
        if (ft2AutomatedExchange) {
            // In AutoCQ/Wait&Pounce il prossimo step e' armato, ma deve partire
            // dal prossimo slot sicuro. Il vecchio path "ritrasmetti subito"
            // poteva creare doppie trasmissioni incrociate nel loopback.
            m_asyncLastTxEndMs = QDateTime::currentMSecsSinceEpoch();
        } else {
            // Resetta guard FT2: risposta ricevuta -> ritrasmetti subito
            m_asyncLastTxEndMs = 0;
        }
        // Shannon: auto_tx_mode(true) — abilita TX dopo risposta valida da DX
        setTxEnabled(true);
        updateAutoCqPartnerLock();
        if (m_mode == QStringLiteral("FT2")
            && m_asyncTxEnabled
            && m_txEnabled
            && !m_transmitting
            && !m_tuning
            && !ft2AutomatedExchange) {
            quint64 const autoSeqSerial = m_ft2AsyncSmartTxSerial;
            quint64 const requestGeneration = m_txRequestGeneration;
            int const autoSeqTx = nextTx;
            QTimer::singleShot(0, this, [this, autoSeqSerial, requestGeneration, autoSeqTx]() {
                if (autoSeqSerial != m_ft2AsyncSmartTxSerial
                    || requestGeneration != m_txRequestGeneration
                    || m_mode != QStringLiteral("FT2")
                    || !m_asyncTxEnabled
                    || !m_txEnabled
                    || m_transmitting
                    || m_tuning
                    || m_currentTx != autoSeqTx) {
                    return;
                }
                checkAndStartPeriodicTx();
            });
        }
        bridgeLog(QStringLiteral("autoSeq: TX%1 %2")
                      .arg(nextTx)
                      .arg(ft2AutomatedExchange
                               ? QStringLiteral("armed for next safe slot")
                               : QStringLiteral("abilitato")));
    }
}
