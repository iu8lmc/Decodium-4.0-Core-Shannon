bool DecodiumBridge::noteTxPlaybackFinished(const QString& reason, bool error)
{
    int const finishedTx = m_activeTxNumber;
    QString const finishedMessage = m_activeTxMessage;
    bool const finishedPureAutoCqCq = m_activeTxWasPureAutoCqCq;
    m_activeTxNumber = 0;
    m_activeTxMessage.clear();
    m_activeTxWasPureAutoCqCq = false;

    if (finishedTx <= 0) {
        return false;
    }

    // Count a burst only after the app-owned audio playback completed.  The
    // pure-CQ marker was captured at TX start so a caller decoded meanwhile
    // cannot turn this CQ into a different operation retroactively.
    noteAutoCqBurstCqCompleted(finishedPureAutoCqCq, reason, error);

    if (error) {
        bridgeLog(QStringLiteral("tx-playback-finished: TX%1 error, not counting signoff (%2)")
                      .arg(finishedTx)
                      .arg(reason));
        return false;
    }

    bool const signoffPayload =
        (finishedTx == 5) || messageCarries73Payload(finishedMessage);
    if (!signoffPayload) {
        return false;
    }

    ++m_nTx73;
    bridgeLog(QStringLiteral("tx-playback-finished: completed signoff TX%1 count=%2 reason=%3 msg=[%4]")
                  .arg(finishedTx)
                  .arg(m_nTx73)
                  .arg(reason, finishedMessage));

    QString const normalizedMode = m_mode.trimmed().toUpper();
    if (finishedTx == 5
        && (normalizedMode == QStringLiteral("FT2")
            || normalizedMode == QStringLiteral("FT4")
            || normalizedMode == QStringLiteral("FT8"))) {
        m_logAfterOwn73 = true;
        m_ft2DeferredLogPending = false;
        m_lastNtx = 5;
        m_lastTransmittedMessage = finishedMessage;
        finishAutoSequenceQsoFromCompletedSignoff(
            QStringLiteral("tx-playback-finished: completed final TX5 73 once -> QSO complete"),
            true);
        return true;
    }

    // The embedded legacy FT4/FT8 sequencer can continue to request its TX4
    // RR73 after bridge state has already scheduled the normal completion
    // check.  That path used to bypass the caller-retry limit entirely: the
    // playback counter visibly reached 11 for YC2ULK with MaxCallerRetries=3.
    // Enforce the same definition used by the normal retry path here: one
    // first TX plus at most MaxCallerRetries repeats.  A received report has
    // already completed the exchange, so at the limit we log and release the
    // partner instead of transmitting RR73 indefinitely.
    if (finishedTx == 4
        && (normalizedMode == QStringLiteral("FT2")
            || normalizedMode == QStringLiteral("FT4")
            || normalizedMode == QStringLiteral("FT8"))
        && m_autoCqRepeat
        && m_nTx73 > m_maxCallerRetries) {
        capturePendingAutoLogSnapshot();
        m_ft2DeferredLogPending = false;
        bridgeLog(QStringLiteral("tx-playback-finished: TX4 RR73 retry cap reached %1/%2 -> log and release partner")
                      .arg(m_nTx73)
                      .arg(m_maxCallerRetries + 1));
        finishAutoSequenceQsoFromCompletedSignoff(
            QStringLiteral("tx-playback-finished: TX4 RR73 retry cap reached -> QSO complete"),
            true);
        return true;
    }

    // 1.0.225 — guard reentry (BUG #8 agente-tx). Senza guard, al fine TX
    // FT2 venivano scheduate 3 singleShot in coda verso
    // checkAndStartPeriodicTx (qui + 2 da completeTxPlayback) -> 3
    // chiamate consecutive, ognuna 12+ bridgeLog flush.
    if (!m_periodicTxCheckScheduled) {
        m_periodicTxCheckScheduled = true;
        quint64 const requestGeneration = m_txRequestGeneration;
        QTimer::singleShot(0, this, [this, requestGeneration]() {
            m_periodicTxCheckScheduled = false;
            if (requestGeneration == m_txRequestGeneration
                && !m_transmitting && !m_tuning) {
                checkAndStartPeriodicTx();
            }
        });
    }
    return false;
}

