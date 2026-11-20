void DecodiumBridge::armExplicitManualTxRequest(int txNumber, const QString& reason)
{
    if (txNumber < 1 || txNumber > 6) {
        m_explicitManualTxRequest = 0;
        return;
    }

    m_explicitManualTxRequest = txNumber;
    bridgeLog(QStringLiteral("TX guard: explicit manual TX%1 armed (%2)")
                  .arg(txNumber)
                  .arg(reason.trimmed().isEmpty()
                           ? QStringLiteral("manual request")
                           : reason.trimmed()));
}

bool DecodiumBridge::txDispatchGuardAllows(const QString& reason)
{
    bool const explicitManual =
        m_explicitManualTxRequest > 0
        && m_explicitManualTxRequest == m_currentTx;

    // A request for another slot is stale as soon as the sequencer advances.
    // Do not let it authorise a later TX6.
    if (m_explicitManualTxRequest > 0 && !explicitManual) {
        bridgeLog(QStringLiteral("TX guard: dropping stale explicit TX%1 request at current TX%2 (%3)")
                      .arg(m_explicitManualTxRequest)
                      .arg(m_currentTx)
                      .arg(reason));
        m_explicitManualTxRequest = 0;
    }

    if (m_manualTxHold && !explicitManual) {
        bridgeLog(QStringLiteral("TX guard: blocked TX%1 during manual hold (%2)")
                      .arg(m_currentTx)
                      .arg(reason));
        return false;
    }

    if (m_startupTxGuard && !explicitManual) {
        bridgeLog(QStringLiteral("TX guard: blocked automatic startup TX (%1)").arg(reason));
        return false;
    }

    // TX6 is the CQ slot.  It may be started automatically only while
    // AutoCQ or Multi-Answer is active.  A direct TX6 button/double-click is
    // explicit and remains valid even with AutoCQ disabled.
    if (m_currentTx == 6
        && !m_autoCqRepeat
        && !m_multiAnswerMode
        && !explicitManual) {
        // Logged at most once a minute: with AutoCQ off the periodic check hits this every 15 s
        // (188 lines in one session).
        static qint64 lastStaleTx6LogMs = 0;
        static int suppressedStaleTx6 = 0;
        qint64 const nowStaleMs = QDateTime::currentMSecsSinceEpoch();
        if (nowStaleMs - lastStaleTx6LogMs >= 60000) {
            bridgeLog(QStringLiteral("TX guard: blocked stale TX6 because AutoCQ is off (%1)%2")
                          .arg(reason,
                               suppressedStaleTx6 > 0
                                   ? QStringLiteral(", +%1 repeats").arg(suppressedStaleTx6)
                                   : QString()));
            lastStaleTx6LogMs = nowStaleMs;
            suppressedStaleTx6 = 0;
        } else {
            ++suppressedStaleTx6;
        }
        if (m_txEnabled) {
            m_txEnabled = false;
            emit txEnabledChanged();
        }
        return false;
    }

    return true;
}

void DecodiumBridge::invalidateDeferredTxRequests(const QString& reason)
{
    const bool hadDeferredState =
        m_periodicTxCheckScheduled
        || m_syncTxRetryScheduled
        || m_deferredManualSyncTx
        || m_ft2AsyncSmartTxPending
        || m_pendingAutoSeqTxAfterActiveTx > 0
        || m_explicitManualTxRequest > 0;

    ++m_txRequestGeneration;
    m_ft8AutoTxDecodeGate.resetKeepingSpeed();
    m_periodicTxCheckScheduled = false;
    m_syncTxRetryScheduled = false;
    ++m_syncTxRetrySerial;
    m_deferredManualSyncTx = false;
    m_ft2AsyncSmartTxPending = false;
    ++m_ft2AsyncSmartTxSerial;
    m_ft2AsyncSmartTxDeadlineMs = 0;
    m_ft2AsyncSmartTxRetries = 0;
    m_explicitManualTxRequest = 0;

    if (hadDeferredState) {
        bridgeLog(QStringLiteral("TX guard: invalidated deferred TX requests generation=%1 (%2)")
                      .arg(m_txRequestGeneration)
                      .arg(reason.trimmed().isEmpty()
                               ? QStringLiteral("state transition")
                               : reason.trimmed()));
    }
}

