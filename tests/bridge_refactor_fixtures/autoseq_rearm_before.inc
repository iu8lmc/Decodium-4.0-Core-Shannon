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
    bridgeLog(QStringLiteral("FT2 async abort cleanup (%1): active=%2 progress=%3 tx=%4 retry=%5")
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

    bridgeLog(QStringLiteral("txRearm reset stale state (%1): tx=%2 last=%3 retry=%4 nTx73=%5 logAfter=%6 deferred=%7 lastMsg=[%8]")
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
