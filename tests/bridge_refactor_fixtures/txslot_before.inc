int DecodiumBridge::currentTx() const { return m_currentTx; }
bool DecodiumBridge::selectCurrentTxIfAllowed(int txNum, const QString& reason)
{
    QString const source = reason.trimmed().isEmpty() ? QStringLiteral("setCurrentTx") : reason.trimmed();
    if (txNum < 1 || txNum > 6) {
        bridgeLog(QStringLiteral("%1: invalid TX%2 selection rejected currentTx=%3")
                      .arg(source)
                      .arg(txNum)
                      .arg(m_currentTx));
        return false;
    }
    if (isTxDisabled(txNum)) {
        bridgeLog(QStringLiteral("%1: TX%2 user-disabled, selection rejected currentTx=%3")
                      .arg(source)
                      .arg(txNum)
                      .arg(m_currentTx));
        emit statusMessage(QStringLiteral("TX%1 disabilitato").arg(txNum));
        return false;
    }
    if (m_explicitManualTxRequest > 0 && m_explicitManualTxRequest != txNum) {
        m_explicitManualTxRequest = 0;
    }
    if (txNum == 6) {
        resetCompletedQsoLatchesForCq(QStringLiteral("set-current-tx6"));
    }
    bool const txStepChanged = (m_currentTx != txNum);
    if (txStepChanged) {
        if (m_mode == QStringLiteral("FT2") && m_asyncTxEnabled) {
            // Any delayed FT2 retry belongs to the TX step that scheduled it.
            // A valid partner decode can advance TX1->TX3 while a safe-slot
            // timer is still pending; invalidate that old timer so it cannot
            // replay the stale locator.
            m_ft2AsyncSmartTxPending = false;
            ++m_ft2AsyncSmartTxSerial;
            m_ft2AsyncSmartTxDeadlineMs = 0;
            m_ft2AsyncSmartTxRetries = 0;
            m_periodicTxCheckScheduled = false;
        }
        if (txNum != 1) {
            m_ft2ManualClickTx1BypassPeriodOnce = false;
        }
        m_currentTx = txNum;
        emit currentTxChanged();
        emit currentTxMessageChanged();
        if (usingLegacyBackendForTx()) m_legacyBackend->selectTxMessage(txNum);
        invalidateTxAudioCache();
        scheduleTxAudioPrecompute();
    }
    return true;
}

bool DecodiumBridge::ensureCurrentTxCanTransmit(const QString& reason)
{
    QString const source = reason.trimmed().isEmpty() ? QStringLiteral("TX") : reason.trimmed();
    if (m_currentTx < 1 || m_currentTx > 6) {
        bridgeLog(QStringLiteral("%1: blocked invalid currentTx=%2").arg(source).arg(m_currentTx));
        emit statusMessage(QStringLiteral("TX bloccato: slot non valido"));
        return false;
    }
    if (isTxDisabled(m_currentTx)) {
        bridgeLog(QStringLiteral("%1: blocked TX%2 because it is user-disabled").arg(source).arg(m_currentTx));
        emit statusMessage(QStringLiteral("TX%1 disabilitato").arg(m_currentTx));
        return false;
    }
    return true;
}

void DecodiumBridge::setCurrentTx(int v) {
    (void)selectCurrentTxIfAllowed(v, QStringLiteral("setCurrentTx"));
}
