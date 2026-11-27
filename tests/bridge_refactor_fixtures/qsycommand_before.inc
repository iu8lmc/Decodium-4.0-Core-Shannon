void DecodiumBridge::qsyTo(double freqHz, const QString& newMode)
{
    setFrequency(freqHz);
    if (!newMode.isEmpty() && newMode != m_mode) {
        bool const previousPreserveFrequency = m_preserveFrequencyOnModeChange;
        m_preserveFrequencyOnModeChange = true;
        setMode(newMode);
        m_preserveFrequencyOnModeChange = previousPreserveFrequency;
    }
    requestRigFrequencyFromBridge(freqHz, QStringLiteral("qsy"));
    emit statusMessage(QString("QSY → %1 MHz (%2)")
        .arg(freqHz / 1e6, 0, 'f', 6)
        .arg(newMode));
}

