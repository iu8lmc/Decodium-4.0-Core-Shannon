void DecodiumBridge::setTxEnabled(bool v)
{
    if (v && !txDispatchGuardAllows(QStringLiteral("setTxEnabled"))) {
        return;
    }
    bool const rearmingFromOff = v && !m_txEnabled;

    if (!v) {
        m_mapRosterCallPending = false;
        ++m_mapRosterCallSerial;
    }

    if (!v && m_autoCqRepeat) {
        bridgeLog(QStringLiteral("setTxEnabled(false): disabling AutoCQ with TX"));
        setAutoCqRepeat(false);
    }
    if (!v) {
        // A MAM payload belongs to the current automatic dispatch only. It
        // must never survive a stop and be selected by a later manual TX.
        clearMamPendingTxPayload(QStringLiteral("tx-disabled"));
    }

    if (v && !prepareHoundTxSelectionForStart(QStringLiteral("tx-enable"))) {
        if (m_txEnabled) {
            m_txEnabled = false;
            emit txEnabledChanged();
        }
        if (usingLegacyBackendForTx() && legacyBackendAvailable()) {
            m_legacyBackend->setTxEnabled(false);
            scheduleLegacyStateRefreshBurst();
        }
        clearDeferredManualSyncTx(QStringLiteral("hound-tx-blocked"));
        return;
    }

    if (v) {
        clearWorldMapClosedQso(m_dxCall);
        clearManualTxHold(QStringLiteral("tx-enabled"));
        if (rearmingFromOff && !m_transmitting && !m_tuning && !m_autoCqRepeat) {
            bool const manualCqRearmAfterCompletedQso =
                m_currentTx == 6
                && m_pendingAutoSeqTxAfterActiveTx == 0
                && !m_dxCall.trimmed().isEmpty()
                && (m_qsoProgress == 0
                    || m_qsoProgress == 6
                    || m_nTx73 > 0
                    || m_lastNtx == 5
                    || messageCarries73Payload(m_lastTransmittedMessage));
            if (manualCqRearmAfterCompletedQso) {
                clearCompletedQsoTxFields(m_dxCall, QStringLiteral("manual-cq-rearm"));
            }
            if (m_currentTx == 6 && m_dxCall.trimmed().isEmpty()) {
                armCqAutoReplyWindow(QStringLiteral("tx6-rearm"));
            }
            resetManualTxRearmState(QStringLiteral("tx-enabled"));
        }
    } else if (!m_autoCqRepeat) {
        clearCqAutoReplyWindow(QStringLiteral("tx-disabled"));
    }

    bool const changed = (m_txEnabled != v);
    if (changed) {
        m_txEnabled = v;
        emit txEnabledChanged();
    }

#if defined(Q_OS_MAC)
    if (!v) {
        cancelPendingLegacyBridgeAudioStart(QStringLiteral("tx-disabled"));
    }
#endif

    // The legacy backend can clear its own TX-enable state after completing a
    // QSO while the QML bridge still shows TX enabled. A subsequent double-click
    // must therefore re-assert the command even when the bridge boolean did not
    // change, otherwise the new message is prepared but never transmitted.
    // The embedded backend remains the RX decoder on macOS, but must not arm
    // its one-message sequencer while the native MAM mixer owns TX.
    bool const bridgeOwnsMamTx = m_mamMultiStream
        && isMamMultiStreamMode()
        && (m_autoCqRepeat || m_multiAnswerMode)
        && shouldUseBridgeAudioForLegacyDigitalTx();
    if (usingLegacyBackendForTx() && !m_suppressImmediateTxEnableDispatch) {
        syncLegacyBackendTxState();
        m_legacyBackend->setTxEnabled(bridgeOwnsMamTx ? false : v);
        scheduleLegacyStateRefreshBurst();
    }

    if (v) {
        scheduleTxAudioPrecompute(25);
        ensureSyncTxSchedulerActive(QStringLiteral("tx-enable"));
        if (m_suppressImmediateTxEnableDispatch) {
            return;
        }
        if (!usingLegacyBackendForTx()
            && usesDeferredManualSyncTx()
            && m_monitoring
            && !m_transmitting
            && !m_tuning
            && shouldDeferManualSyncTxStart()) {
            m_deferredManualSyncTx = true;
            scheduleSyncTxAtNextValidSlot(QStringLiteral("tx-enable"), -1, 0);
        }
        if (!usingLegacyBackendForTx()) {
            quint64 const requestGeneration = m_txRequestGeneration;
            QTimer::singleShot(0, this, [this, requestGeneration]() {
                if (m_shuttingDown || QCoreApplication::closingDown()
                    || usingLegacyBackendForTx()
                    || requestGeneration != m_txRequestGeneration) {
                    return;
                }
                if (tryStartDeferredManualSyncTx()) {
                    return;
                }
                checkAndStartPeriodicTx();
            });
        }
    } else {
        clearDeferredManualSyncTx(QStringLiteral("tx-disabled"));
    }
}

void DecodiumBridge::requestManualTxEnable()
{
    armExplicitManualTxRequest(m_currentTx, QStringLiteral("TX panel enable"));
    setTxEnabled(true);
}

void DecodiumBridge::setTxDisabled(int n, bool disabled)
{
    if (n < 1 || n > 6) return;
    int const bit = (1 << (n - 1));
    bool const isDisabled = (m_txDisabledMask & bit) != 0;
    if (isDisabled == disabled) return;
    if (disabled) m_txDisabledMask |=  bit;
    else          m_txDisabledMask &= ~bit;
    bridgeLog(QStringLiteral("setTxDisabled: TX%1 -> %2 (mask=0x%3)")
                  .arg(n)
                  .arg(disabled ? QStringLiteral("disabled") : QStringLiteral("enabled"))
                  .arg(m_txDisabledMask, 0, 16));
    emit txDisabledMaskChanged();
    saveSettingsAsync();
}

