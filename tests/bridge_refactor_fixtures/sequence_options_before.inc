void DecodiumBridge::setAutoSeq(bool v)
{
    if (m_autoSeq != v) {
        m_autoSeq = v;
        emit autoSeqChanged();
        if (usingLegacyBackendForTx()) {
            m_legacyBackend->setAutoSeq(v);
        }
    }
}

void DecodiumBridge::setWaitPounceActive(bool v)
{
    if (m_waitPounceActive == v) {
        return;
    }

    m_waitPounceActive = v;
    if (!m_waitPounceActive) {
        m_lastWaitPounceKey.clear();
        m_lastWaitPounceMs = 0;
    } else if (!m_autoSeq) {
        // Wait & Pounce is only useful when the response can continue through
        // the normal report/RR73/73 sequence.
        setAutoSeq(true);
    }

    QSettings s(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(s);
    s.setValue(QStringLiteral("WaitPounceActive"), m_waitPounceActive);
    s.sync();

    emit waitPounceActiveChanged();
    bridgeLog(QStringLiteral("Wait & Pounce %1")
                  .arg(m_waitPounceActive ? QStringLiteral("enabled")
                                           : QStringLiteral("disabled")));
    emit statusMessage(m_waitPounceActive
                       ? QStringLiteral("Wait & Pounce attivo")
                       : QStringLiteral("Wait & Pounce disattivato"));
}

void DecodiumBridge::setHoldTxFreq(bool v)
{
    if (m_holdTxFreq != v) {
        m_holdTxFreq = v;
        emit holdTxFreqChanged();
        if (m_legacyBackend) {
            m_legacyBackend->setHoldTxFreq(v);
        }
        QSettings s(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
        decodium::beginActiveSettingsProfile(s);
        s.setValue(QStringLiteral("HoldTxFreq"), m_holdTxFreq);
        s.setValue(QStringLiteral("holdTxFreq"), m_holdTxFreq);
        s.sync();
    }
}

extern "C" void ftx_ft2_set_accumulo_enabled_c(int on);

void DecodiumBridge::setFt2AccumuloEnabled(bool v)
{
    if (m_ft2AccumuloEnabled != v) {
        m_ft2AccumuloEnabled = v;
        ftx_ft2_set_accumulo_enabled_c(v ? 1 : 0);
        emit ft2AccumuloEnabledChanged();
        bridgeLog(QStringLiteral("FT2 accumulo fra slot: %1").arg(v ? QStringLiteral("acceso") : QStringLiteral("spento")));
    }
}
