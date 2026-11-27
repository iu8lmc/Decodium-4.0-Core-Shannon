void DecodiumBridge::setTxRequestedState(bool requested)
{
    if (m_txRequested == requested) return;
    m_txRequested = requested;
    emit txRequestedChanged();
}

void DecodiumBridge::setPttPendingState(bool pending)
{
    if (m_pttPending == pending) return;
    m_pttPending = pending;
    emit pttPendingChanged();
}

void DecodiumBridge::setPttConfirmedState(bool confirmed)
{
    if (m_pttConfirmed == confirmed) return;
    m_pttConfirmed = confirmed;
    emit pttConfirmedChanged();
}

void DecodiumBridge::beginLegacyPttTransition(
    const QString& reason,
    decodium::tx::PttConfirmationMode confirmationMode,
    std::function<bool()> confirmedAction)
{
    quint64 const serial = ++m_pttTransitionSerial;
    m_pttTransitionReason = reason;
    m_pttConfirmedAction = std::move(confirmedAction);
    m_pttOnDispatched = false;
    m_pttOffSentForTransition = false;
    setTxRequestedState(true);
    setPttConfirmedState(false);
    setPttPendingState(true);

    QString const modeName = confirmationMode == decodium::tx::PttConfirmationMode::RigFeedback
        ? QStringLiteral("rig-feedback")
        : (confirmationMode == decodium::tx::PttConfirmationMode::CommandDispatch
               ? QStringLiteral("command-dispatch")
               : QStringLiteral("audio-activity"));
    txTimelineLog(QStringLiteral("[TX-TL] ptt_pending serial=%1 reason=%2 mode=%3 backend=%4 method=%5")
                      .arg(serial)
                      .arg(reason, modeName, m_catBackend, activePttMethod()));
    emit statusMessage(QStringLiteral("PTT in attesa (%1)").arg(m_catBackend));

    if (confirmationMode != decodium::tx::PttConfirmationMode::RigFeedback) {
        QTimer::singleShot(0, this, [this, serial, modeName]() {
            if (serial != m_pttTransitionSerial || !m_pttPending) return;
            confirmLegacyPttTransition(modeName);
        });
        return;
    }

    if (activeCatReportsPttActive()) {
        QTimer::singleShot(0, this, [this, serial]() {
            if (serial != m_pttTransitionSerial || !m_pttPending) return;
            confirmLegacyPttTransition(QStringLiteral("already-active"));
        });
    }
    QTimer::singleShot(decodium::tx::pttFeedbackTimeoutMs(), this, [this, serial]() {
        if (serial != m_pttTransitionSerial || !m_pttPending) return;
        failLegacyPttTransition(QStringLiteral("PTT non confermato entro %1 ms")
                                    .arg(decodium::tx::pttFeedbackTimeoutMs()),
                                // A failed PTT must not start a TX retry loop
                                // on macOS.  The retry re-enters the CAT/TX
                                // path while the panadapter is running and
                                // can repeatedly reconnect the radio.
#if defined(Q_OS_MAC)
                                false);
#else
                                true);
#endif
    });
}

void DecodiumBridge::confirmLegacyPttTransition(const QString& source)
{
    if (!m_txRequested || !m_pttPending) return;
    quint64 const serial = m_pttTransitionSerial;
    setPttPendingState(false);
    setPttConfirmedState(true);
    txTimelineLog(QStringLiteral("[TX-TL] ptt_confirmed serial=%1 reason=%2 source=%3 backend=%4")
                      .arg(serial)
                      .arg(m_pttTransitionReason, source, m_catBackend));

    std::function<bool()> action = std::move(m_pttConfirmedAction);
    m_pttConfirmedAction = {};
    QTimer::singleShot(0, this, [this, serial, action]() mutable {
        if (serial != m_pttTransitionSerial || !m_txRequested || !m_pttConfirmed) return;
        if (action && !action()) {
            failLegacyPttTransition(QStringLiteral("Audio TX non avviato dopo la conferma PTT"), true);
        }
    });
}

void DecodiumBridge::failLegacyPttTransition(const QString& reason, bool reschedule)
{
    if (!m_txRequested && !m_pttPending && !m_pttConfirmed) return;
    txTimelineLog(QStringLiteral("[TX-TL] ptt_failed serial=%1 reason=%2 backend=%3 dispatched=%4")
                      .arg(m_pttTransitionSerial)
                      .arg(reason, m_catBackend)
                      .arg(m_pttOnDispatched ? 1 : 0));

    ++m_pttTransitionSerial;
    m_pttConfirmedAction = {};
    setPttPendingState(false);
    setPttConfirmedState(false);
    setTxRequestedState(false);
    abortLegacyBridgeTxRequest(QStringLiteral("ptt-failed"));

    if (reschedule && usesDeferredManualSyncTx() && m_monitoring && !m_manualTxHold) {
        if (!m_autoCqRepeat) m_deferredManualSyncTx = true;
        scheduleSyncTxAtNextValidSlot(QStringLiteral("ptt-confirmation-timeout"), -1, 0);
        emit errorMessage(reason + QStringLiteral(". TX riprogrammato nel prossimo slot valido."));
    } else {
        emit errorMessage(reason);
    }
}

void DecodiumBridge::clearLegacyPttTransition(const QString& reason)
{
    bool const hadState = m_txRequested || m_pttPending || m_pttConfirmed
        || m_pttOnDispatched || static_cast<bool>(m_pttConfirmedAction);
    ++m_pttTransitionSerial;
    m_pttConfirmedAction = {};
    setPttPendingState(false);
    setPttConfirmedState(false);
    setTxRequestedState(false);
    m_pttOnDispatched = false;
    // Keep the OFF latch set until the next ON transition begins.  Hamlib and
    // the legacy backend may report their respective TX-end events in either
    // order; clearing it here would allow the later event to enqueue a second
    // setRigPtt:off command.
    m_pttTransitionReason.clear();
    if (hadState) {
        txTimelineLog(QStringLiteral("[TX-TL] ptt_state_cleared reason=%1").arg(reason));
    }
}

void DecodiumBridge::requestLegacyPttOffOnce(const QString& reason)
{
    bool const rigStillReportsPtt = activeCatReportsPttActive();
    // Un comando di sgancio in piu' non costa nulla; una radio che resta in
    // aria costa moltissimo. Il doppione si scarta solo finche' non c'e'
    // dubbio: se la radio dice ancora di trasmettere, si sgancia e basta.
    if (m_pttOffSentForTransition && !rigStillReportsPtt) {
        bridgeLog(QStringLiteral("PTT OFF deduplicated (%1)").arg(reason));
        return;
    }
    if (!m_pttOnDispatched && !rigStillReportsPtt) {
        bridgeLog(QStringLiteral("PTT OFF skipped (%1): no PTT ON was dispatched").arg(reason));
        return;
    }
    m_pttOffSentForTransition = true;
    if (activeCatCanPtt(this, m_hamlibCat, m_catBackend,
                        m_omniRigCat, m_legacyBackend)) {
        activeCatSetPtt(this, m_hamlibCat, m_catBackend, false,
                        m_omniRigCat, m_legacyBackend);
        txTimelineLog(QStringLiteral("[TX-TL] ptt_off_once reason=%1 backend=%2")
                          .arg(reason, m_catBackend));
        // CI-V/Hamlib can acknowledge the local request before the Icom has
        // physically left TX.  Recheck after the serial polling interval and
        // issue one unconditional RX retry if the radio still reports TX.
        QTimer::singleShot(2300, this, [this, reason]() {
            if (!activeCatCanPtt(this, m_hamlibCat, m_catBackend,
                                 m_omniRigCat, m_legacyBackend)) return;
            const bool stillActive = activeCatReportsPttActive();
            // Always send the follow-up RX command.  The cached state can be
            // stale/false on Hamlib even while the physical rig is in TX.
            activeCatSetPtt(this, m_hamlibCat, m_catBackend, false,
                            m_omniRigCat, m_legacyBackend);
            if (stillActive) {
                txTimelineLog(QStringLiteral("[TX-TL] ptt_off_retry reason=%1 backend=%2")
                                  .arg(reason, m_catBackend));
            } else {
                txTimelineLog(QStringLiteral("[TX-TL] ptt_off_recheck_rx backend=%1")
                                  .arg(m_catBackend));
            }
        });
    }
}

void DecodiumBridge::handleActivePttFeedback(bool active, const QString& backend)
{
    if (!catSignalMatchesBackend(m_catBackend, backend)) return;
    txTimelineLog(QStringLiteral("[TX-TL] ptt_feedback active=%1 backend=%2 pending=%3 confirmed=%4")
                      .arg(active ? 1 : 0)
                      .arg(backend)
                      .arg(m_pttPending ? 1 : 0)
                      .arg(m_pttConfirmed ? 1 : 0));
    if (active && m_pttPending) {
        confirmLegacyPttTransition(QStringLiteral("%1-signal").arg(backend));
    }
}

