void DecodiumBridge::setMultiAnswerMode(bool v)
{
    if (v && m_autoCallEnabled) {
        setAutoCallEnabled(false);
    }
    if (m_multiAnswerMode == v) {
        return;
    }

    m_multiAnswerMode = v;
    if (m_multiAnswerMode) {
        clearManualTxHold(QStringLiteral("mam-enabled"));
    } else if (!m_autoCqRepeat) {
        clearCallerQueue();
        clearAutoCqPartnerLock();
        clearPendingAutoLogSnapshot();
        clearLateAutoLogSnapshot();
        clearPendingAutoSeqTx(QStringLiteral("mam-disabled"));
        clearMamPendingTxPayload(QStringLiteral("mam-disabled"));
    }
    bridgeLog(QStringLiteral("MultiAnswerMode: %1").arg(m_multiAnswerMode ? 1 : 0));
    emit multiAnswerModeChanged();
}

