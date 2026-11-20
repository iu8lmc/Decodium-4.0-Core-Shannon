void DecodiumBridge::armFt2AutoCqOneShotAfterCompletedTx(int txNum, const QString& reason)
{
    if (m_mode != QStringLiteral("FT2")
        || !m_asyncTxEnabled
        || !m_autoCqRepeat
        || m_multiAnswerMode
        || txNum < 2
        || txNum > 4
        || m_dxCall.trimmed().isEmpty()) {
        return;
    }

    // 1.0.371 - opt-in: per TX4 (RR73 al partner) in AutoCQ async consenti il
    // deferred-log se il partner sparisce, ma NON lasciare TX armata. La prima
    // implementazione saltava anche il one-shot wait/disarm e poteva ritrasmettere
    // RR73 fino al cap. Qui armiamo il deferred-log e poi proseguiamo nel normale
    // one-shot: attesa di un fresh partner decode + TX disabilitata.
    if (txNum == 4 && m_ft2LogRr73OnPartnerLeft && m_nTx73 >= 1) {
        if (!m_ft2DeferredLogPending) {
            m_ft2DeferredLogPending = true;
            capturePendingAutoLogSnapshot();
        }
        bridgeLog(QStringLiteral("FT2 AutoCQ TX4 RR73: deferred-log armed without extra RR73 retries (opt-in)"));
    }

    armFt2AutoCqAwaitingPartnerDecode(txNum, reason);
    if (!m_txEnabled) {
        return;
    }

    bridgeLog(QStringLiteral("FT2 AutoCQ one-shot: TX%1 internally disarmed until fresh partner decode")
                  .arg(txNum));
    m_txEnabled = false;
    emit txEnabledChanged();
    clearDeferredManualSyncTx(QStringLiteral("ft2-autocq-one-shot"));
    if (usingLegacyBackendForTx() && legacyBackendAvailable()) {
        m_legacyBackend->setTxEnabled(false);
        scheduleLegacyStateRefreshBurst();
    }
}

bool DecodiumBridge::applyPendingAutoSeqTxAfterCompletedTx(int finishedTx)
{
    if (m_pendingAutoSeqTxAfterActiveTx <= 0) {
        return false;
    }

    int const deferredTx = m_pendingAutoSeqTxAfterActiveTx;
    QString const pendingPartnerBase = m_pendingAutoSeqPartnerBase;
    QString const pendingMessage = m_pendingAutoSeqMessage;
    QString const pendingMode = m_pendingAutoSeqMode;
    m_pendingAutoSeqTxAfterActiveTx = 0;
    m_pendingAutoSeqPartnerBase.clear();
    m_pendingAutoSeqMessage.clear();
    m_pendingAutoSeqMode.clear();

    QString const activePartnerBase = Radio::base_callsign(m_dxCall).trimmed().toUpper();
    QString staleReason;
    if (!pendingMode.isEmpty()
        && pendingMode != m_mode.trimmed().toUpper()) {
        staleReason = QStringLiteral("mode changed %1->%2")
            .arg(pendingMode, m_mode.trimmed().toUpper());
    } else if (!pendingPartnerBase.isEmpty()
               && (activePartnerBase.isEmpty() || pendingPartnerBase != activePartnerBase)) {
        staleReason = QStringLiteral("partner changed %1->%2")
            .arg(pendingPartnerBase,
                 activePartnerBase.isEmpty() ? QStringLiteral("<none>") : activePartnerBase);
    } else if (!pendingMessage.isEmpty()
               && !activePartnerBase.isEmpty()
               && !messageContainsCallToken(pendingMessage, m_dxCall, activePartnerBase)) {
        staleReason = QStringLiteral("pending message does not mention active partner");
    }

    if (!staleReason.isEmpty()) {
        bridgeLog(QStringLiteral("auto-seq: discard stale deferred TX%1 after active TX%2 completed: %3 pendingMsg=[%4]")
                      .arg(deferredTx)
                      .arg(finishedTx)
                      .arg(staleReason, pendingMessage));
        return false;
    }

    bool const holdFt2ReplyAfterActiveCq =
        m_mode == QStringLiteral("FT2")
        && m_asyncTxEnabled
        && finishedTx == 6
        && deferredTx == 2
        && !pendingPartnerBase.isEmpty();
    if (holdFt2ReplyAfterActiveCq) {
        if (deferredTx != m_currentTx) {
            bridgeLog(QStringLiteral("auto-seq: FT2 caller %1 decoded during CQ TX; arm TX%2 but wait safe RX period")
                          .arg(pendingPartnerBase)
                          .arg(deferredTx));
            if (!advanceQsoState(deferredTx)) {
                return false;
            }
        } else {
            bridgeLog(QStringLiteral("auto-seq: FT2 caller %1 decoded during CQ TX; TX%2 already armed, wait safe RX period")
                          .arg(pendingPartnerBase)
                          .arg(deferredTx));
        }

        m_txRetryCount = 0;
        m_txWatchdogTicks = 0;
        m_autoCQPeriodsMissed = 0;
        updateAutoCqPartnerLock();
        if (m_autoCqRepeat) {
            armFt2AutoCqOneShotAfterCompletedTx(deferredTx,
                                                QStringLiteral("caller decoded during CQ TX"));
        } else {
            m_asyncLastTxEndMs = QDateTime::currentMSecsSinceEpoch();
            m_lastNtx = deferredTx;
            if (!m_txEnabled) {
                setTxEnabled(true);
            }
        }
        return true;
    }

    if (deferredTx != m_currentTx) {
        bridgeLog(QStringLiteral("auto-seq: applying deferred TX%1 after active TX%2 completed")
                      .arg(deferredTx)
                      .arg(finishedTx));
        if (!advanceQsoState(deferredTx)) {
            return false;
        }
    } else {
        bridgeLog(QStringLiteral("auto-seq: deferred TX%1 already current after active TX")
                      .arg(deferredTx));
    }

    m_txRetryCount = 0;
    m_txWatchdogTicks = 0;
    m_autoCQPeriodsMissed = 0;
    if (deferredTx == 5) {
        m_logAfterOwn73 = true;
        m_ft2DeferredLogPending = false;
    }
    // FIX 1.0.359 (9H1SR, log a 2 lati Bruce/ZL3DMH): un reply differito TX3/4/5 dopo
    // un CQ FT2 ancora attivo NON deve partire nello slot del partner — che ha appena
    // risposto nello slot opposto al nostro CQ. Senza questo, advanceQsoState+setTxEnabled
    // fa partire il TX col solo debounce 200ms -> stesso slot del partner -> collisione
    // (entrambi in TX, nessuno ascolta). Replico il meccanismo GIA' attivo per TX2
    // (holdFt2ReplyAfterActiveCq ramo deferredTx==2): marco m_lastNtx=deferredTx +
    // m_asyncLastTxEndMs=now cosi' il gate di periodo (sameExchangeStepRetry in
    // checkAndStartPeriodicTx) attende un periodo e il reply parte nel NOSTRO slot
    // successivo, saltando lo slot del partner. deferredTx==2 non arriva qui (gestito
    // sopra). Solo FT2 async; tutti gli altri modi/percorsi invariati.
    if (m_mode == QStringLiteral("FT2") && m_asyncTxEnabled
        && finishedTx == 6 && deferredTx >= 3 && deferredTx <= 5) {
        m_asyncLastTxEndMs = QDateTime::currentMSecsSinceEpoch();
        m_lastNtx = deferredTx;
        bridgeLog(QStringLiteral("auto-seq: FT2 reply TX%1 differito dopo CQ -> attende un periodo e salta lo slot del partner (anti-collisione)")
                      .arg(deferredTx));
    }
    if (!m_txEnabled) {
        setTxEnabled(true);
    }
    updateAutoCqPartnerLock();
    return true;
}

