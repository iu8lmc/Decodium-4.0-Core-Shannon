void DecodiumBridge::requestRigFrequencyFromBridge(double hz, const QString& reason)
{
    if (hz <= 0.0) {
        return;
    }
    // A user QSY ends tracking; do not add an old-band correction to a new dial.
    if (m_q65Doppler) m_q65Doppler->setEnabled(false);

    // Resolve and validate the physical rig/IF dial before publishing the
    // logical QSY.  A large negative transverter offset can otherwise turn a
    // valid on-air frequency into an invalid CAT request with no useful UI
    // explanation.
    double const dialHz = applyFrequencyCalibration(hz);
    if (m_catConnected && dialHz <= 0.0) {
        emit errorMessage(QStringLiteral("Cannot tune %1 Hz: the configured station/transverter offset produces an invalid CAT dial (%2 Hz)")
                              .arg(QString::number(hz, 'f', 0),
                                   QString::number(dialHz, 'f', 0)));
        bridgeLog(QStringLiteral("CAT local QSY rejected by %1: logical=%2 Hz physical=%3 Hz")
                      .arg(reason,
                           QString::number(hz, 'f', 0),
                           QString::number(dialHz, 'f', 0)));
        return;
    }

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    m_localCatFrequencyPreviousHz = m_frequency;
    m_localCatFrequencyTargetHz = hz;
    m_localCatFrequencyGuardUntilMs = nowMs + 8000;
    m_localCatFrequencyGuardMaxMs = nowMs + 20000;  // 1.0.362 tetto re-arm guard
    setFrequency(hz);

    if (!m_catConnected) {
        bridgeLog(QStringLiteral("Local QSY requested by %1: %2 Hz (CAT not connected, bridge state only)")
                      .arg(reason, QString::number(hz, 'f', 0)));
        return;
    }

    if (isHamlibFamilyBackend(m_catBackend)) {
        activeCatSetFreq(this, m_hamlibCat, m_catBackend, dialHz, m_omniRigCat, m_legacyBackend);
        schedulePostQsyCatSettledSync(hz, reason);
        bridgeLog(QStringLiteral("CAT local QSY requested by %1: %2 Hz via %3 (mode/split sync delayed)")
                      .arg(reason,
                           QString::number(hz, 'f', 0),
                           m_catBackend));
        return;
    }

    // In RTTY il modo giusto e' quello dell'RTTY, non quello dei modi digitali.
    bool const inRtty = m_mode.compare(QStringLiteral("RTTY"), Qt::CaseInsensitive) == 0;
    if (inRtty)
        applyRttyRigMode(reason + QStringLiteral("/mode"));
    else
        applyConfiguredCatRigMode(reason + QStringLiteral("/mode"));
    activeCatSetFreq(this, m_hamlibCat, m_catBackend, dialHz, m_omniRigCat, m_legacyBackend);
    syncActiveCatTxSplitFrequency(reason + QStringLiteral("/dial"));

    // E ancora una volta dopo il salto. Molti apparati, la FT-991A fra questi,
    // ricordano un modo diverso per ciascuna banda e ci tornano da soli appena
    // ci si sposta: il modo impostato prima del QSY viene cancellato dal QSY
    // stesso. Mezzo secondo basta perche' la radio abbia finito di cambiare
    // banda; applyRttyRigMode non fa nulla se il modo e' gia' quello giusto o
    // se nel frattempo si e' usciti da RTTY.
    if (inRtty) {
        QTimer::singleShot(500, this, [this, reason]() {
            applyRttyRigMode(reason + QStringLiteral("/dopo-qsy"));
        });
    }
    bridgeLog(QStringLiteral("CAT local QSY requested by %1: %2 Hz via %3")
                  .arg(reason,
                       QString::number(hz, 'f', 0),
                       m_catBackend));
}

void DecodiumBridge::schedulePostQsyCatSettledSync(double hz, const QString& reason, int delayMs,
                                                    int retryAttempt)
{
    if (hz <= 0.0
        || !isHamlibFamilyBackend(m_catBackend)
        || !m_hamlibCat
        || !m_hamlibCat->connected()) {
        return;
    }

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    m_catFrequencySettleUntilMs = std::max(m_catFrequencySettleUntilMs,
                                           nowMs + qMax(250, delayMs));
    quint64 const serial = ++m_catQsySettleSerial;
    QTimer::singleShot(qMax(250, delayMs), this, [this, hz, reason, serial, retryAttempt]() {
        if (serial != m_catQsySettleSerial) {
            return;
        }
        if (!isHamlibFamilyBackend(m_catBackend)
            || !m_hamlibCat
            || !m_hamlibCat->connected()) {
            return;
        }
        if (m_localCatFrequencyTargetHz > 0.0
            && std::abs(m_localCatFrequencyTargetHz - hz) > 1.0) {
            return;
        }

        double const physicalReportedHz = m_hamlibCat->frequency();
        double const reportedHz = logicalFrequencyFromCatDial(physicalReportedHz);
        bool const qsyStillCurrent =
            (m_localCatFrequencyTargetHz > 0.0
             && std::abs(m_localCatFrequencyTargetHz - hz) <= 1.0)
            || std::abs(m_frequency - hz) <= 1.0;
        if (retryAttempt < 2
            && qsyStillCurrent
            && reportedHz > 0.0
            && std::abs(reportedHz - hz) > 1.0) {
            int const nextAttempt = retryAttempt + 1;
            bridgeLog(QStringLiteral("CAT post-QSY retry %1/2: requested=%2 Hz rig=%3 Hz logical=%4 Hz (%5)")
                          .arg(nextAttempt)
                          .arg(QString::number(hz, 'f', 0),
                               QString::number(physicalReportedHz, 'f', 0),
                               QString::number(reportedHz, 'f', 0),
                               reason));
            activeCatSetFreq(this, m_hamlibCat, m_catBackend,
                             applyFrequencyCalibration(hz), m_omniRigCat, m_legacyBackend);
            schedulePostQsyCatSettledSync(hz, reason, 1000, nextAttempt);
            return;
        }

        m_catFrequencySettleUntilMs = 0;
        QString const settledReason = reason + QStringLiteral("/settled");
        bridgeLog(QStringLiteral("CAT post-QSY settled sync (%1): %2 Hz")
                      .arg(settledReason, QString::number(hz, 'f', 0)));
        applyConfiguredCatRigMode(settledReason);
        syncActiveCatTxSplitFrequency(settledReason);
    });
}

bool DecodiumBridge::hamlibCatFrequencySettleActive(const QString& reason) const
{
    if (!isHamlibFamilyBackend(m_catBackend)
        || reason.contains(QStringLiteral("settled"), Qt::CaseInsensitive)) {
        return false;
    }
    return m_catFrequencySettleUntilMs > QDateTime::currentMSecsSinceEpoch();
}


