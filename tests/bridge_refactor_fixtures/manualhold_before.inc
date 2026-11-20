void DecodiumBridge::engageManualTxHold(const QString& reason, bool clearQueue)
{
    if (!m_manualTxHold) {
        bridgeLog(QStringLiteral("manualTxHold: engaged (%1)").arg(reason));
    }
    m_manualTxHold = true;
    // 1.0.304 (#9) — resume-on-reply: arma il partner PRIMA dei wipe sottostanti
    // (clearFt2AsyncAbortQsoState in FT2-async azzera m_dxCall). Solo se toggle ON e c'è un
    // partner ingaggiato. NON azzero nell'else: l'arm persiste fino a resume/timeout/altro
    // ingaggio (vedi tryResumeQsoOnReply, processDecodeDoubleClick, setResumeQsoOnReply).
    QString const resumeMode = m_mode.trimmed().toUpper();
    QString const resumeTarget = normalizedBaseCall(m_dxCall.trimmed().toUpper());
    bool const resumeModeAllowed =
        resumeMode == QStringLiteral("FT8")
        || resumeMode == QStringLiteral("FT4")
        || resumeMode == QStringLiteral("FT2");
    bool const resumeActiveQso =
        !resumeTarget.isEmpty()
        && m_currentTx >= 1
        && m_currentTx <= 5
        && (m_transmitting
            || m_txEnabled
            || m_deferredManualSyncTx
            || m_pendingAutoSeqTxAfterActiveTx > 0
            || (m_qsoProgress > 1 && m_qsoProgress < 6));
    if (m_resumeQsoOnReply && resumeModeAllowed && resumeActiveQso) {
        m_resumeTargetCall = resumeTarget;
        m_resumeArmedMs = QDateTime::currentMSecsSinceEpoch();
        m_resumeArmedMode = resumeMode;
        m_resumeArmedTx = m_currentTx;
        m_resumeArmedProgress = m_qsoProgress;
        bridgeLog(QStringLiteral("[Resume] armato su %1 mode=%2 tx=%3 progress=%4 (Halt) — riprende solo su reply diretto entro 2 min")
                      .arg(m_resumeTargetCall, m_resumeArmedMode)
                      .arg(m_resumeArmedTx)
                      .arg(m_resumeArmedProgress));
    } else if (m_resumeQsoOnReply) {
        clearResumeQsoOnReplyArm(QStringLiteral("manual hold without active resumable QSO"));
    }
    clearAutoCqPartnerLock();
    clearPendingAutoLogSnapshot();
    clearLateAutoLogSnapshot();
    m_txRetryCount = 0;
    m_lastNtx = -1;
    m_txWatchdogTicks = 0;
    m_autoCQPeriodsMissed = 0;
    m_logAfterOwn73 = false;
    m_ft2DeferredLogPending = false;
    if (m_mode == QStringLiteral("FT2") && m_asyncTxEnabled) {
        clearFt2AsyncAbortQsoState(QStringLiteral("manualTxHold: %1").arg(reason));
    }
    QString staleDeferredReason;
    bool const keepDeferredAutoSeq = shouldPreserveDeferredAutoSeqTxForRearm(&staleDeferredReason);
    if (keepDeferredAutoSeq) {
        bridgeLog(QStringLiteral("manualTxHold: keeping deferred TX%1 for rearm (%2)")
                      .arg(m_pendingAutoSeqTxAfterActiveTx)
                      .arg(reason));
    } else {
        if (m_pendingAutoSeqTxAfterActiveTx > 0 && !staleDeferredReason.isEmpty()) {
            bridgeLog(QStringLiteral("manualTxHold: clearing stale deferred TX%1 (%2): %3")
                          .arg(m_pendingAutoSeqTxAfterActiveTx)
                          .arg(reason, staleDeferredReason));
        }
        m_pendingAutoSeqTxAfterActiveTx = 0;
        m_pendingAutoSeqPartnerBase.clear();
        m_pendingAutoSeqMessage.clear();
        m_pendingAutoSeqMode.clear();
        m_autoSeqRogerReportBase.clear();
    }
    if (!keepDeferredAutoSeq || m_pendingAutoSeqTxAfterActiveTx != 4) {
        m_quickPeerSignaled = false;
    }
    if (clearQueue) {
        clearCallerQueue();
    }
}

void DecodiumBridge::clearManualTxHold(const QString& reason)
{
    if (!m_manualTxHold) {
        return;
    }
    m_manualTxHold = false;
    bridgeLog(QStringLiteral("manualTxHold: cleared (%1)").arg(reason));
}





