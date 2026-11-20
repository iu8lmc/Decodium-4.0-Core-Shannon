    connect(this, &DecodiumBridge::filterMyCallOnlyChanged, this, [this]() {
        if (usingLegacyBackendForTx()) {
            m_legacyBandActivityRevision = -1;
            m_legacyRxFrequencyRevision = -1;
            m_legacyAllTxtRevisionKey.clear();
            syncLegacyBackendDecodeList();
        }
    });
