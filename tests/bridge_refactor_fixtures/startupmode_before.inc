QString DecodiumBridge::startupModeForFrequency(double dialFrequency) const
{
    if (dialFrequency <= 0.0 || !m_bandManager) {
        return {};
    }
    return m_bandManager->detectModeForFrequency(dialFrequency);
}

void DecodiumBridge::maybeApplyStartupModeFromRigFrequency(double dialFrequency,
                                                           bool authoritativeRigFrequency)
{
    if (dialFrequency <= 0.0) {
        return;
    }

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    bool const startupWindowExpired = m_startupModeAutoUntilMs > 0 && nowMs > m_startupModeAutoUntilMs;
    if (startupWindowExpired) {
        m_startupModeAutoPending = false;
        return;
    }

    bool const canApplyStartupMode =
        m_startupModeAutoPending
        || (authoritativeRigFrequency && !m_startupModeAutoAuthoritativeApplied);
    if (!canApplyStartupMode) {
        return;
    }

    QString const startupMode = startupModeForFrequency(dialFrequency);
    if (startupMode.isEmpty()) {
        return;
    }

    m_startupModeAutoPending = false;
    if (authoritativeRigFrequency) {
        m_startupModeAutoAuthoritativeApplied = true;
    }
    if (startupMode == m_mode) {
        return;
    }
    if (authoritativeRigFrequency && (m_transmitting || m_tuning)) {
        bridgeLog(QStringLiteral("CAT auto-mode deferred during TX/tune: %1 Hz -> %2 (current %3)")
                      .arg(QString::number(dialFrequency, 'f', 0), startupMode, m_mode));
        return;
    }

    bridgeLog(QStringLiteral("startup auto-mode%1: %2 Hz -> %3 (was %4)")
                  .arg(authoritativeRigFrequency ? QStringLiteral(" from CAT") : QString(),
                       QString::number(dialFrequency, 'f', 0), startupMode, m_mode));

    bool const previousPreserveFrequency = m_preserveFrequencyOnModeChange;
    m_preserveFrequencyOnModeChange = true;
    setMode(startupMode);
    m_preserveFrequencyOnModeChange = previousPreserveFrequency;
}

