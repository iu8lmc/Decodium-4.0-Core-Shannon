    // Audio custom: salta le guardie FT (validazione payload, fallback CQ,
    // Fox/Hound). CW e FT2-Link hanno testo/waveform arbitrari.
    if (!customAudioTxActive) {
        if (!ensureCurrentTxCanTransmit(QStringLiteral("startTx"))) return;
        if (!checkSwrAllowsTransmission(QStringLiteral("startTx"))) return;
        if (legacyTxBackendRequested() && !legacyBackendAvailable() && !ensureLegacyBackendAvailable()) {
            emit errorMessage(QStringLiteral("Backend legacy non disponibile per Fox/Hound"));
            return;
        }
        if (!prepareHoundTxSelectionForStart(QStringLiteral("startTx"))) {
            return;
        }
        if (!ensureCurrentTxCanTransmit(QStringLiteral("startTx after hound prepare"))) return;

        bool const fallbackToCq = (m_currentTx >= 1 && m_currentTx <= 5) &&
                                  m_dxCall.trimmed().isEmpty() &&
                                  selectedSlotMessage().trimmed().isEmpty() &&
                                  !m_tx6.trimmed().isEmpty() &&
                                  m_specialOperationActivity != kSpecialOpHound;
        if (fallbackToCq) {
            bridgeLog("startTx: no DX payload available, selecting TX6/CQ");
            if (!selectCurrentTxIfAllowed(6, QStringLiteral("startTx fallback CQ"))) {
                return;
            }
        }
    }

