void DecodiumBridge::setRxFrequency(int f)
{
    f = qBound(0, f, 5000);
    m_localRxFrequencyTargetHz = f;
    m_localRxFrequencyGuardUntilMs = QDateTime::currentMSecsSinceEpoch() + 2500;
    if (m_rxFrequency != f) {
        m_rxFrequency = f;
        emit rxFrequencyChanged();
        if (legacyBackendAvailable()) {
            m_legacyBackend->setRxFrequency(f);
        }
        if (!usingLegacyBackendForTx()) {
            rebuildRxDecodeList();
        }
        QSettings s(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
        decodium::beginActiveSettingsProfile(s);
        s.setValue(QStringLiteral("rxFrequency"), m_rxFrequency);
    }
}

void DecodiumBridge::setTxFrequency(int f)
{
    f = qBound(0, f, 5000);
    m_localTxFrequencyTargetHz = f;
    m_localTxFrequencyGuardUntilMs = QDateTime::currentMSecsSinceEpoch() + 2500;
    if (m_txFrequency != f) {
        m_txFrequency = f;
        emit txFrequencyChanged();
        invalidateTxAudioCache();
        if (legacyBackendAvailable()) {
            m_legacyBackend->setTxFrequency(f);
        }
        QSettings s(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
        decodium::beginActiveSettingsProfile(s);
        s.setValue(QStringLiteral("txFrequency"), m_txFrequency);
        s.setValue(QStringLiteral("TXFrequency"), m_txFrequency);
        syncActiveCatTxSplitFrequency(QStringLiteral("tx-audio-change"));
        scheduleTxAudioPrecompute();
    }
}

