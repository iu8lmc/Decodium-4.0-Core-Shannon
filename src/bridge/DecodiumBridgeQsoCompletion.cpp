#include "DecodiumBridge.h"
#include "Radio.hpp"

void decodiumBridgeAudioLevelLog(const QString&);

void DecodiumBridge::clearCompletedQsoTxFields(const QString& completedCall, const QString& reason)
{
    QString const completedBase = Radio::base_callsign(completedCall).trimmed().toUpper();
    QString const activeBase = Radio::base_callsign(m_dxCall).trimmed().toUpper();
    if (!completedBase.isEmpty()
        && !activeBase.isEmpty()
        && completedBase != activeBase) {
        decodiumBridgeAudioLevelLog(QStringLiteral("QSO complete: preserving TX fields for active %1 after completed %2 (%3)")
                      .arg(m_dxCall.trimmed(), completedCall.trimmed(), reason));
        return;
    }

    QString const loggedCall = completedCall.trimmed().isEmpty()
        ? m_dxCall.trimmed()
        : completedCall.trimmed();
    decodiumBridgeAudioLevelLog(QStringLiteral("QSO complete: clearing completed TX fields after %1 (%2)")
                  .arg(loggedCall, reason));

    m_lastTransmittedMessage.clear();
    m_activeTxNumber = 0;
    m_activeTxMessage.clear();
    m_pendingAutoSeqTxAfterActiveTx = 0;
    m_pendingAutoSeqPartnerBase.clear();
    m_pendingAutoSeqMessage.clear();
    m_pendingAutoSeqMode.clear();
    clearAutoCqPartnerLock();

    if (m_reportSent != QStringLiteral("-10")) {
        m_reportSent = QStringLiteral("-10");
        emit reportSentChanged();
    }
    if (m_reportReceived != QStringLiteral("-10")) {
        m_reportReceived = QStringLiteral("-10"); m_reportReceivedReal = false;
        emit reportReceivedChanged();
    }

    setDxCall(QString());
    setDxGrid(QString());
    setTx1(QString());
    setTx2(QString());
    setTx3(QString());
    setTx4(QString());
    setTx5(QString());
    setCurrentTx(m_tx6.trimmed().isEmpty() ? 1 : 6);
}

void DecodiumBridge::resetCompletedQsoLatchesForCq(const QString& reason)
{
    bool const hadState = m_logAfterOwn73
        || m_ft2DeferredLogPending
        || m_pendingAutoSeqTxAfterActiveTx != 0
        || !m_pendingAutoSeqPartnerBase.isEmpty()
        || !m_pendingAutoSeqMessage.isEmpty()
        || !m_pendingAutoSeqMode.isEmpty()
        || !m_autoCqLockedCall.trimmed().isEmpty()
        || !m_autoSeqRogerReportBase.isEmpty()
        || m_quickPeerSignaled
        || m_nTx73 != 0
        || m_lastNtx != -1
        || m_txRetryCount != 0
        || m_txWatchdogTicks != 0
        || m_autoCQPeriodsMissed != 0;

    if (hadState) {
        decodiumBridgeAudioLevelLog(QStringLiteral("CQ clean-slate: clearing completed/signoff latches (%1)").arg(reason));
    }

    m_logAfterOwn73 = false;
    m_ft2DeferredLogPending = false;
    m_pendingAutoSeqTxAfterActiveTx = 0;
    m_pendingAutoSeqPartnerBase.clear();
    m_pendingAutoSeqMessage.clear();
    m_pendingAutoSeqMode.clear();
    clearAutoCqPartnerLock();
    m_autoSeqRogerReportBase.clear();
    m_quickPeerSignaled = false;
    m_nTx73 = 0;
    m_lastNtx = -1;
    m_txRetryCount = 0;
    m_txWatchdogTicks = 0;
    m_autoCQPeriodsMissed = 0;

    if (m_dxCall.trimmed().isEmpty()) {
        m_lastTransmittedMessage.clear();
        m_activeTxNumber = 0;
        m_activeTxMessage.clear();
    }
}

void DecodiumBridge::clearTxArmedAfterCompletedQso(const QString& completedCall, const QString& reason)
{
    markWorldMapQsoClosed(completedCall, reason);

    bool const continueQueuedCaller = (m_multiAnswerMode || m_autoCqRepeat) && !m_callerQueue.isEmpty();
    bool const continueAutoCq = autoCqCanRestartTx6();

    QString const completedBase = Radio::base_callsign(completedCall).trimmed().toUpper();
    QString const activeBase = Radio::base_callsign(m_dxCall).trimmed().toUpper();
    bool const activeDifferentQso = !completedBase.isEmpty()
        && !activeBase.isEmpty()
        && activeBase != completedBase;
    bool const preserveActiveDifferentQso = activeDifferentQso
        && (m_transmitting || m_tuning);

    if (continueQueuedCaller || continueAutoCq || preserveActiveDifferentQso) {
        if (continueQueuedCaller || continueAutoCq) {
            invalidateDeferredTxRequests(QStringLiteral("QSO complete; continuing automatic queue"));
        }
        if ((m_multiAnswerMode || m_autoCqRepeat) && !completedBase.isEmpty()) {
            removeCallerFromQueue(completedBase);
        }
        decodiumBridgeAudioLevelLog(QStringLiteral("QSO complete: preserving TX enable (%1, queued=%2 autoCQ=%3 activeDifferent=%4)")
                      .arg(reason)
                      .arg(continueQueuedCaller ? 1 : 0)
                      .arg(continueAutoCq ? 1 : 0)
                      .arg(preserveActiveDifferentQso ? 1 : 0));
        return;
    }

    if ((m_multiAnswerMode || m_autoCqRepeat) && !completedBase.isEmpty()) {
        removeCallerFromQueue(completedBase);
    }
    invalidateDeferredTxRequests(QStringLiteral("QSO complete; TX disarmed"));
    clearDeferredManualSyncTx(reason);
    m_logAfterOwn73 = false;
    m_ft2DeferredLogPending = false;
    clearPendingAutoSeqTx(reason);
    m_quickPeerSignaled = false;
    m_nTx73 = 0;
    m_autoSeqRogerReportBase.clear();
    m_txRetryCount = 0;
    m_txWatchdogTicks = 0;
    m_autoCQPeriodsMissed = 0;
    m_lastNtx = -1;
    m_lastCqPidx = -1;

    if (m_qsoProgress != 6) {
        m_qsoProgress = 6;
        emit qsoProgressChanged();
    }

    if (m_txEnabled) {
        decodiumBridgeAudioLevelLog(QStringLiteral("QSO complete: clearing visual TX enable (%1)").arg(reason));
        setTxEnabled(false);
    }
    clearCompletedQsoTxFields(completedCall, reason);
}

