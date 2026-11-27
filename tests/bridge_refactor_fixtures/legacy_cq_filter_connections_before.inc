    connect(this, &DecodiumBridge::filterCqOnlyChanged, this, [this]() {
        if (legacyBackendAvailable()) {
            m_legacyBackend->setCqOnly(false);
        }
        if (usingLegacyBackendForTx()) {
            m_legacyBandActivityRevision = -1;
            m_legacyRxFrequencyRevision = -1;
            m_legacyAllTxtRevisionKey.clear();
            syncLegacyBackendDecodeList();
        }
    });
    // CQ level changes only the Decodium4 presentation filter. The hidden
    // legacy backend must keep emitting all valid decodes so ALL.TXT remains
    // complete, matching WSJT-X/Decodium3 behavior.
    connect(this, &DecodiumBridge::cqFilterLevelChanged, this, [this]() {
        if (legacyBackendAvailable()) {
            m_legacyBackend->setCqOnly(false);
        }
        if (usingLegacyBackendForTx()) {
            m_legacyBandActivityRevision = -1;
            m_legacyRxFrequencyRevision = -1;
            m_legacyAllTxtRevisionKey.clear();
            syncLegacyBackendDecodeList();
        }
    });
