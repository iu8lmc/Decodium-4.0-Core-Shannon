#include "DecodiumBridge.h"
#include "Radio.hpp"
#include "Sequencer/MessageTokenRules.hpp"

using decodium::seq::messageContainsCallToken;
void decodiumBridgeDecodeFlowLog(const QString&);

void DecodiumBridge::clearPendingAutoSeqTx(const QString& reason)
{
    if (m_pendingAutoSeqTxAfterActiveTx <= 0
        && m_pendingAutoSeqPartnerBase.isEmpty()
        && m_pendingAutoSeqMessage.isEmpty()
        && m_pendingAutoSeqMode.isEmpty()) {
        return;
    }

    decodiumBridgeDecodeFlowLog(QStringLiteral("auto-seq: clearing pending TX%1 (%2) partner=%3 msg=[%4]")
                  .arg(m_pendingAutoSeqTxAfterActiveTx)
                  .arg(reason.trimmed().isEmpty() ? QStringLiteral("state reset") : reason.trimmed(),
                       m_pendingAutoSeqPartnerBase.trimmed().isEmpty()
                           ? QStringLiteral("<none>")
                           : m_pendingAutoSeqPartnerBase.trimmed(),
                       m_pendingAutoSeqMessage.trimmed()));
    m_pendingAutoSeqTxAfterActiveTx = 0;
    m_pendingAutoSeqPartnerBase.clear();
    m_pendingAutoSeqMessage.clear();
    m_pendingAutoSeqMode.clear();
}

void DecodiumBridge::forgetPartnerMemoryForCall(const QString& call, const QString& reason)
{
    QString const targetBase = Radio::base_callsign(call).trimmed().toUpper();
    if (targetBase.isEmpty() || m_partnerMemory.isEmpty()) {
        return;
    }

    int removed = 0;
    for (auto it = m_partnerMemory.begin(); it != m_partnerMemory.end(); ) {
        QString const entryBase = Radio::base_callsign(it.key()).trimmed().toUpper();
        if (entryBase == targetBase) {
            it = m_partnerMemory.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    if (removed > 0) {
        decodiumBridgeDecodeFlowLog(QStringLiteral("[FT2WS-F] forgot partner memory %1 entries=%2 (%3)")
                      .arg(targetBase)
                      .arg(removed)
                      .arg(reason.trimmed().isEmpty() ? QStringLiteral("manual reset") : reason.trimmed()));
    }
}

bool DecodiumBridge::shouldPreserveDeferredAutoSeqTxForRearm(QString* staleReason) const
{
    if (staleReason) {
        staleReason->clear();
    }

    int const deferredTx = m_pendingAutoSeqTxAfterActiveTx;
    bool const ft2AutoCqFirstReply =
        deferredTx == 2
        && m_autoCqRepeat
        && m_mode == QStringLiteral("FT2")
        && m_asyncTxEnabled;
    bool const deferredFinal73 = (deferredTx == 5);
    if (deferredTx != 3 && deferredTx != 4 && !deferredFinal73 && !ft2AutoCqFirstReply) {
        if (staleReason && deferredTx > 0) {
            *staleReason = QStringLiteral("deferred TX%1 is not a preserved FT2 AutoCQ/report/RR73/73 step")
                               .arg(deferredTx);
        }
        return false;
    }

    if (!ft2AutoCqFirstReply && !deferredFinal73 && !m_ft2PartnerMemoryEnabled) {
        if (staleReason) {
            *staleReason = QStringLiteral("PartnerMemory disabled");
        }
        return false;
    }

    QString const activeMode = m_mode.trimmed().toUpper();
    if (!m_pendingAutoSeqMode.isEmpty() && m_pendingAutoSeqMode != activeMode) {
        if (staleReason) {
            *staleReason = QStringLiteral("mode changed %1->%2").arg(m_pendingAutoSeqMode, activeMode);
        }
        return false;
    }

    QString const activePartnerBase = Radio::base_callsign(m_dxCall).trimmed().toUpper();
    if (activePartnerBase.isEmpty()) {
        if (staleReason) {
            *staleReason = QStringLiteral("active partner missing");
        }
        return false;
    }

    if (!m_pendingAutoSeqPartnerBase.isEmpty()
        && m_pendingAutoSeqPartnerBase != activePartnerBase) {
        if (staleReason) {
            *staleReason = QStringLiteral("partner changed %1->%2")
                .arg(m_pendingAutoSeqPartnerBase, activePartnerBase);
        }
        return false;
    }

    QString pendingMessage = m_pendingAutoSeqMessage.trimmed();
    if (pendingMessage.isEmpty()) {
        pendingMessage = txMessage(deferredTx).trimmed();
    }
    if (!pendingMessage.isEmpty()
        && !messageContainsCallToken(pendingMessage, m_dxCall, activePartnerBase)) {
        if (staleReason) {
            *staleReason = QStringLiteral("pending message does not mention active partner");
        }
        return false;
    }

    return true;
}

bool DecodiumBridge::applyDeferredAutoSeqTxForRearm(const QString& reason)
{
    QString staleReason;
    if (!shouldPreserveDeferredAutoSeqTxForRearm(&staleReason)) {
        if (m_pendingAutoSeqTxAfterActiveTx > 0 && !staleReason.isEmpty()) {
            decodiumBridgeDecodeFlowLog(QStringLiteral("txRearm: discard stale deferred TX%1 (%2): %3 pendingMsg=[%4]")
                          .arg(m_pendingAutoSeqTxAfterActiveTx)
                          .arg(reason, staleReason, m_pendingAutoSeqMessage.trimmed()));
        }
        return false;
    }

    int const deferredTx = m_pendingAutoSeqTxAfterActiveTx;
    QString const pendingMessage = m_pendingAutoSeqMessage.trimmed();
    decodiumBridgeDecodeFlowLog(QStringLiteral("txRearm: preserving deferred TX%1 (%2) pendingMsg=[%3]")
                  .arg(deferredTx)
                  .arg(reason, pendingMessage.isEmpty() ? txMessage(deferredTx).trimmed() : pendingMessage));

    m_pendingAutoSeqTxAfterActiveTx = 0;
    m_pendingAutoSeqPartnerBase.clear();
    m_pendingAutoSeqMessage.clear();
    m_pendingAutoSeqMode.clear();

    if (deferredTx != m_currentTx) {
        if (!advanceQsoState(deferredTx)) {
            return false;
        }
    }
    if (deferredTx == 5) {
        m_logAfterOwn73 = true;
        m_ft2DeferredLogPending = false;
    }
    updateAutoCqPartnerLock();
    return true;
}

void DecodiumBridge::clearFt2AsyncAbortQsoState(const QString& reason)
{
    if (m_mode != QStringLiteral("FT2") || !m_asyncTxEnabled) {
        return;
    }

    bool const hadQsoState =
        m_qsoProgress > 1
        || !m_dxCall.trimmed().isEmpty()
        || m_pendingAutoSeqTxAfterActiveTx != 0
        || !m_pendingAutoSeqMessage.trimmed().isEmpty()
        || !m_lastTransmittedMessage.trimmed().isEmpty()
        || m_lastNtx != -1
        || m_txRetryCount != 0
        || m_nTx73 != 0
        || m_logAfterOwn73
        || m_ft2DeferredLogPending;
    if (!hadQsoState) {
        return;
    }

    QString const activeCall = m_dxCall.trimmed();
    decodiumBridgeDecodeFlowLog(QStringLiteral("FT2 async abort cleanup (%1): active=%2 progress=%3 tx=%4 retry=%5")
                  .arg(reason,
                       activeCall.isEmpty() ? QStringLiteral("<none>") : activeCall)
                  .arg(m_qsoProgress)
                  .arg(m_currentTx)
                  .arg(m_txRetryCount));

    clearDeferredManualSyncTx(reason);
    clearPendingAutoSeqTx(reason);
    clearPendingAutoLogSnapshot();
    clearLateAutoLogSnapshot();
    clearAutoCqPartnerLock();

    m_txRetryCount = 0;
    m_lastNtx = -1;
    m_lastCqPidx = -1;
    m_nTx73 = 0;
    m_txWatchdogTicks = 0;
    m_autoCQPeriodsMissed = 0;
    m_autoSeqRogerReportBase.clear();
    m_logAfterOwn73 = false;
    m_ft2DeferredLogPending = false;
    m_quickPeerSignaled = false;
    m_qsoLogged = false;
    m_lastTransmittedMessage.clear();
    m_activeTxNumber = 0;
    m_activeTxMessage.clear();
    m_qsoStartedOn = QDateTime {};
    m_qsoFirstReplyOn = QDateTime {};
    m_asyncLastTxEndMs = 0;
    m_ft2AsyncLastDecodeMs = 0;
    m_ft2AsyncFirstDecodeMs = 0;
    m_ft2AsyncAudioQuietRuns = 0;
    m_ft2AsyncSmartTxPending = false;
    ++m_ft2AsyncSmartTxSerial;
    m_ft2AsyncSmartTxDeadlineMs = 0;
    m_ft2AsyncSmartTxRetries = 0;

    setDxCall(QString());
    setDxGrid(QString());
    if (m_qsoProgress != 0) {
        m_qsoProgress = 0;
        emit qsoProgressChanged();
    }
    setCurrentTx(m_tx6.trimmed().isEmpty() ? 1 : 6);
}

void DecodiumBridge::resetManualTxRearmState(const QString& reason)
{
    bool const hadStaleState =
        m_txRetryCount != 0
        || m_lastNtx != -1
        || m_lastCqPidx != -1
        || m_nTx73 != 0
        || m_txWatchdogTicks != 0
        || m_autoCQPeriodsMissed != 0
        || m_logAfterOwn73
        || m_ft2DeferredLogPending
        || m_pendingAutoSeqTxAfterActiveTx != 0
        || m_quickPeerSignaled
        || m_qsoLogged
        || m_asyncLastTxEndMs > 0
        || !m_lastTransmittedMessage.trimmed().isEmpty();
    if (!hadStaleState) {
        return;
    }

    bool const appliedDeferredAutoSeq = applyDeferredAutoSeqTxForRearm(reason);

    decodiumBridgeDecodeFlowLog(QStringLiteral("txRearm reset stale state (%1): tx=%2 last=%3 retry=%4 nTx73=%5 logAfter=%6 deferred=%7 lastMsg=[%8]")
                  .arg(reason)
                  .arg(m_currentTx)
                  .arg(m_lastNtx)
                  .arg(m_txRetryCount)
                  .arg(m_nTx73)
                  .arg(m_logAfterOwn73 ? 1 : 0)
                  .arg(m_ft2DeferredLogPending ? 1 : 0)
                  .arg(m_lastTransmittedMessage.trimmed()));

    m_txRetryCount = 0;
    m_txWatchdogTicks = 0;
    m_autoCQPeriodsMissed = 0;
    m_nTx73 = 0;
    if (!appliedDeferredAutoSeq) {
        m_autoSeqRogerReportBase.clear();
    }
    m_lastNtx = -1;
    m_lastCqPidx = -1;
    if (!appliedDeferredAutoSeq || m_currentTx != 5) {
        m_logAfterOwn73 = false;
        m_ft2DeferredLogPending = false;
    }
    if (!appliedDeferredAutoSeq) {
        m_pendingAutoSeqTxAfterActiveTx = 0;
        m_pendingAutoSeqPartnerBase.clear();
        m_pendingAutoSeqMessage.clear();
        m_pendingAutoSeqMode.clear();
    }
    if (!appliedDeferredAutoSeq || m_currentTx != 4) {
        m_quickPeerSignaled = false;
    }
    m_qsoLogged = false;
    m_asyncLastTxEndMs = 0;
    m_lastTransmittedMessage.clear();
}
