void DecodiumBridge::setTxPeriod(int v)
{
    v = (v != 0) ? 1 : 0;
    if (m_txPeriod != v) {
        m_txPeriod = v;
        emit txPeriodChanged();
        if (usingLegacyBackendForTx()) {
            m_legacyBackend->setTxFirst(m_specialOperationActivity == kSpecialOpHound ? false : (m_txPeriod != 0));
        }
    }
}

void DecodiumBridge::setAlt12Enabled(bool v)
{
    if (m_alt12Enabled != v) {
        m_alt12Enabled = v;
        emit alt12EnabledChanged();
        if (usingLegacyBackendForTx()) {
            m_legacyBackend->setAlt12Enabled(m_specialOperationActivity == kSpecialOpHound ? false : v);
        }
    }
}

