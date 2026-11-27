bool DecodiumBridge::ft2LinkSatelliteHalfDuplexRequested() const
{
    return getSetting(QStringLiteral("Ft2LinkSatelliteHalfDuplexEnabled"), false).toBool();
}

bool DecodiumBridge::ft2LinkSatelliteHalfDuplexOperationActive() const
{
    return m_ft2LinkSatelliteHalfDuplexState != 0;
}

void DecodiumBridge::setFt2LinkSatelliteHalfDuplexState(int state, const QString& detail)
{
    if (m_ft2LinkSatelliteHalfDuplexState == state
        && m_ft2LinkSatelliteHalfDuplexDetail == detail) {
        return;
    }
    m_ft2LinkSatelliteHalfDuplexState = state;
    m_ft2LinkSatelliteHalfDuplexDetail = detail;
    emit ft2LinkSatelliteHalfDuplexStatusChanged();
}

QVariantMap DecodiumBridge::ft2LinkSatelliteHalfDuplexStatus() const
{
    using decodium::ft2link_satellite::HalfDuplexConfiguration;
    HalfDuplexConfiguration configuration;
    configuration.enabled = ft2LinkSatelliteHalfDuplexRequested();
    configuration.rxDialHz = getSetting(QStringLiteral("Ft2LinkSatelliteRxDialHz"), 0).toLongLong();
    configuration.txDialHz = getSetting(QStringLiteral("Ft2LinkSatelliteTxDialHz"), 0).toLongLong();
    configuration.settleMs = decodium::ft2link_satellite::normalizedSettleMs(
        getSetting(QStringLiteral("Ft2LinkSatelliteCatSettleMs"), 900).toInt());

    QString state = QStringLiteral("Idle");
    switch (m_ft2LinkSatelliteHalfDuplexState) {
    case 1: state = QStringLiteral("Preparing TX VFO"); break;
    case 2: state = QStringLiteral("Transmitting"); break;
    case 3: state = QStringLiteral("Returning to RX VFO"); break;
    default: break;
    }

    QVariantMap result;
    result.insert(QStringLiteral("enabled"), configuration.enabled);
    result.insert(QStringLiteral("busy"), ft2LinkSatelliteHalfDuplexOperationActive());
    result.insert(QStringLiteral("state"), state);
    result.insert(QStringLiteral("detail"), m_ft2LinkSatelliteHalfDuplexDetail);
    result.insert(QStringLiteral("rxDialHz"), ft2LinkSatelliteHalfDuplexOperationActive()
                      ? m_ft2LinkSatelliteHalfDuplexRxDialHz : configuration.rxDialHz);
    result.insert(QStringLiteral("txDialHz"), ft2LinkSatelliteHalfDuplexOperationActive()
                      ? m_ft2LinkSatelliteHalfDuplexTxDialHz : configuration.txDialHz);
    result.insert(QStringLiteral("settleMs"), ft2LinkSatelliteHalfDuplexOperationActive()
                      ? m_ft2LinkSatelliteHalfDuplexSettleMs : configuration.settleMs);
    result.insert(QStringLiteral("configurationError"),
                  decodium::ft2link_satellite::validationError(configuration));
    result.insert(QStringLiteral("supportedBackend"),
                  m_catBackend == QStringLiteral("hamlib"));
    result.insert(QStringLiteral("catConnected"),
                  m_hamlibCat && m_hamlibCat->connected());
    result.insert(QStringLiteral("rigSplitMode"),
                  m_hamlibCat ? m_hamlibCat->splitMode() : QString());
    return result;
}

bool DecodiumBridge::beginFt2LinkSatelliteHalfDuplexTx()
{
    using decodium::ft2link_satellite::HalfDuplexConfiguration;
    HalfDuplexConfiguration configuration;
    configuration.enabled = true;
    configuration.rxDialHz = getSetting(QStringLiteral("Ft2LinkSatelliteRxDialHz"), 0).toLongLong();
    configuration.txDialHz = getSetting(QStringLiteral("Ft2LinkSatelliteTxDialHz"), 0).toLongLong();
    configuration.settleMs = decodium::ft2link_satellite::normalizedSettleMs(
        getSetting(QStringLiteral("Ft2LinkSatelliteCatSettleMs"), 900).toInt());

    QString const configurationError = decodium::ft2link_satellite::validationError(configuration);
    if (!configurationError.isEmpty()) {
        emit errorMessage(QStringLiteral("FT2-Link satellite half-duplex: %1").arg(configurationError));
        return false;
    }
    if (ft2LinkSatelliteHalfDuplexOperationActive()) {
        emit errorMessage(QStringLiteral("FT2-Link satellite half-duplex: a previous RX/TX transition is still active"));
        return false;
    }
    if (m_catBackend != QStringLiteral("hamlib") || !m_hamlibCat || !m_hamlibCat->connected()) {
        emit errorMessage(QStringLiteral("FT2-Link satellite half-duplex requires a connected Hamlib CAT rig"));
        return false;
    }
    if (m_hamlibCat->splitMode().trimmed().compare(QStringLiteral("rig"), Qt::CaseInsensitive) != 0) {
        emit errorMessage(QStringLiteral("FT2-Link satellite half-duplex requires Hamlib Split mode 'rig'"));
        return false;
    }
    if (!m_hamlibCat->canPtt() || m_hamlibCat->pttActive()) {
        emit errorMessage(QStringLiteral("FT2-Link satellite half-duplex requires idle CAT PTT (VOX is not supported)"));
        return false;
    }

    m_ft2LinkSatelliteHalfDuplexRxDialHz = configuration.rxDialHz;
    m_ft2LinkSatelliteHalfDuplexTxDialHz = configuration.txDialHz;
    m_ft2LinkSatelliteHalfDuplexSettleMs = configuration.settleMs;
    quint64 const serial = ++m_ft2LinkSatelliteHalfDuplexSerial;
    setFt2LinkSatelliteHalfDuplexState(1,
        QStringLiteral("CAT RX %1 Hz -> TX %2 Hz; waiting %3 ms before PTT")
            .arg(configuration.rxDialHz)
            .arg(configuration.txDialHz)
            .arg(configuration.settleMs));

    if (!m_hamlibCat->prepareSatelliteHalfDuplex(static_cast<double>(configuration.rxDialHz),
                                                  static_cast<double>(configuration.txDialHz))) {
        m_ft2LinkSatelliteHalfDuplexRxDialHz = 0;
        m_ft2LinkSatelliteHalfDuplexTxDialHz = 0;
        setFt2LinkSatelliteHalfDuplexState(0, QStringLiteral("CAT refused the satellite VFO preparation"));
        emit errorMessage(QStringLiteral("FT2-Link satellite half-duplex: CAT refused the RX/TX VFO preparation"));
        return false;
    }

    bridgeLog(QStringLiteral("[FT2SAT] TX prepare serial=%1 rx_hz=%2 tx_hz=%3 settle_ms=%4")
                  .arg(serial)
                  .arg(configuration.rxDialHz)
                  .arg(configuration.txDialHz)
                  .arg(configuration.settleMs));
    emit statusMessage(QStringLiteral("FT2-Link satellite: preparing TX VFO before PTT"));

    QTimer::singleShot(configuration.settleMs, this, [this, serial]() {
        if (serial != m_ft2LinkSatelliteHalfDuplexSerial
            || m_ft2LinkSatelliteHalfDuplexState != 1) {
            return;
        }
        if (!m_hamlibCat || !m_hamlibCat->connected() || !m_hamlibCat->canPtt()) {
            restoreFt2LinkSatelliteHalfDuplexRx(QStringLiteral("CAT became unavailable before PTT"), true);
            return;
        }
        bool const rxReady = std::abs(m_hamlibCat->frequency()
                                      - static_cast<double>(m_ft2LinkSatelliteHalfDuplexRxDialHz)) <= 1.0;
        bool const txReady = std::abs(m_hamlibCat->txFrequency()
                                      - static_cast<double>(m_ft2LinkSatelliteHalfDuplexTxDialHz)) <= 1.0;
        if (!rxReady || !txReady || !m_hamlibCat->split()) {
            restoreFt2LinkSatelliteHalfDuplexRx(QStringLiteral("CAT VFO state could not be verified before PTT"), true);
            return;
        }

        startTx();
        if (!m_transmitting) {
            restoreFt2LinkSatelliteHalfDuplexRx(QStringLiteral("TX did not start after CAT preparation"), true);
        } else {
            setFt2LinkSatelliteHalfDuplexState(2, QStringLiteral("TX active; RX audio is suspended"));
        }
    });
    return true;
}

void DecodiumBridge::finishFt2LinkSatelliteHalfDuplexTx(const QString& reason, bool error)
{
    bool const notifyAdapter = m_ft2LinkTxActive || ft2LinkSatelliteHalfDuplexOperationActive();
    m_ft2LinkTxActive = false;
    m_pendingFt2LinkText.clear();
    m_pendingFt2LinkWave.clear();
    m_pendingFt2LinkPlan.clear();
    m_ft2LinkSatelliteHalfDuplexRxDialHz = 0;
    m_ft2LinkSatelliteHalfDuplexTxDialHz = 0;
    setFt2LinkSatelliteHalfDuplexState(0, reason);
    restoreTxAudioSchedulingBoost(QStringLiteral("ft2sat-%1").arg(reason));
    resumeRxAudioAfterTx(QStringLiteral("ft2sat-%1").arg(reason));
    resumeNonAudioTxWork(QStringLiteral("ft2sat-%1").arg(reason));
    bridgeLog(QStringLiteral("[FT2SAT] complete reason=%1 error=%2")
                  .arg(reason).arg(error ? 1 : 0));
    if (error) {
        emit errorMessage(QStringLiteral("FT2-Link satellite half-duplex: %1").arg(reason));
    } else {
        emit statusMessage(QStringLiteral("FT2-Link satellite: RX restored"));
    }
    if (notifyAdapter) {
        emit ft2LinkTxFinished();
    }
}

void DecodiumBridge::restoreFt2LinkSatelliteHalfDuplexRx(const QString& reason, bool error)
{
    if (!ft2LinkSatelliteHalfDuplexOperationActive()) {
        finishFt2LinkSatelliteHalfDuplexTx(reason, error);
        return;
    }

    quint64 const serial = ++m_ft2LinkSatelliteHalfDuplexSerial;
    setFt2LinkSatelliteHalfDuplexState(3, QStringLiteral("PTT off; restoring RX VFO"));
    if (!m_hamlibCat || !m_hamlibCat->connected()
        || !m_hamlibCat->restoreSatelliteHalfDuplexRx(
               static_cast<double>(m_ft2LinkSatelliteHalfDuplexRxDialHz))) {
        finishFt2LinkSatelliteHalfDuplexTx(reason + QStringLiteral("; CAT RX restore failed"), true);
        return;
    }

    int const settleMs = qBound(250, m_ft2LinkSatelliteHalfDuplexSettleMs, 5000);
    QTimer::singleShot(settleMs, this, [this, serial, reason, error]() {
        if (serial != m_ft2LinkSatelliteHalfDuplexSerial
            || m_ft2LinkSatelliteHalfDuplexState != 3) {
            return;
        }
        bool const rxReady = m_hamlibCat && m_hamlibCat->connected()
            && std::abs(m_hamlibCat->frequency()
                        - static_cast<double>(m_ft2LinkSatelliteHalfDuplexRxDialHz)) <= 1.0
            && !m_hamlibCat->split()
            && m_hamlibCat->txFrequency() <= 1.0;
        finishFt2LinkSatelliteHalfDuplexTx(
            rxReady ? reason : reason + QStringLiteral("; CAT RX restore could not be verified"),
            error || !rxReady);
    });
}

void DecodiumBridge::cancelFt2LinkSatelliteHalfDuplexTx(const QString& reason)
{
    if (!ft2LinkSatelliteHalfDuplexOperationActive()) {
        return;
    }
    ++m_ft2LinkSatelliteHalfDuplexSerial;
    restoreFt2LinkSatelliteHalfDuplexRx(reason, false);
}

