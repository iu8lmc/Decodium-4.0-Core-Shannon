void DecodiumBridge::clearTxMessages()
{
    bridgeLog("clearTxMessages: reset QSO/TX state");
    // 1.0.364+ MAM multi-stream nativo (FASE 2): reset slot multi-QSO.
    m_mamSlots.clear();
    m_mamLateReportRecoveries.clear();
    m_mamMessages.clear();
    m_mamF0sHz.clear();
    m_mamGains.clear();
    emit mamTxStreamsChanged();
    clearDeferredManualSyncTx(QStringLiteral("clear-tx-messages"));

    if (m_transmitting || m_tuning)
        halt();

    setTxEnabled(false);
    m_qsoCooldown.clear();
    m_txRetryCount = 0;
    m_nTx73 = 0;
    m_autoSeqRogerReportBase.clear();
    m_lastNtx = -1;
    m_activeTxNumber = 0;
    m_activeTxMessage.clear();
    clearPendingAutoSeqTx(QStringLiteral("clear-tx-messages"));

    if (m_qsoProgress != 0) {
        m_qsoProgress = 0;
        emit qsoProgressChanged();
    }
    if (m_reportSent != "-10") {
        m_reportSent = "-10";
        emit reportSentChanged();
    }
    if (m_reportReceived != "-10") {
        m_reportReceived = "-10"; m_reportReceivedReal = false;
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

