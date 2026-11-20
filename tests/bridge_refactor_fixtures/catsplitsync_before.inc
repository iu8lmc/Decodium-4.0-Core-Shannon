void DecodiumBridge::syncActiveCatTxSplitFrequency(const QString& reason)
{
    if (m_q65Doppler && (m_q65Doppler->enabled() || m_q65Doppler->applied())) {
        refreshQ65Doppler();
        if (m_q65Doppler->applied()) return;
    }
    // A satellite half-duplex operation owns both VFOs until the RX restore is
    // complete.  The ordinary FT split code derives a few-kHz XIT from the
    // audio offset and would otherwise overwrite the cross-band CAT state.
    if (ft2LinkSatelliteHalfDuplexOperationActive()) {
        return;
    }

    QObject* const activeCat = catManagerObj();
    if (legacyOwnsRigControl(m_legacyBackend)
        || useLegacyRigControlFallback(m_legacyBackend, m_catBackend)
        || (!isHamlibFamilyBackend(m_catBackend)
            && m_catBackend != QStringLiteral("cat4om"))
        || !activeCat
        || !activeCat->property("connected").toBool()) {
        return;
    }

    if (m_networkRigctlStartupReadOnly
        && isNetworkRigctlCat(m_catBackend, activeCat)) {
        bridgeLog(QStringLiteral(
            "CAT NET rigctl startup: skipped idle TX-frequency/split write (%1)").arg(reason));
        return;
    }

    if (hamlibCatFrequencySettleActive(reason)) {
        bridgeLog(QStringLiteral("CAT split sync delayed during QSY settle (%1)").arg(reason));
        return;
    }

    QString const splitMode = normalizedCatSplitMode(activeCat->property("splitMode").toString());
    if (splitMode == QStringLiteral("none")) {
        return;
    }

    double const txDialHz = catSplitTxDialFrequencyHz();
    if (txDialHz <= 0.0
        && !activeCat->property("split").toBool()
        && activeCat->property("txFrequency").toDouble() <= 1.0) {
        return;
    }

    // txDialHz <= 0 is the split-disable / no-valid-TX-dial sentinel. Preserve it:
    // calibration intercepts must never turn that sentinel into a positive TX VFO.
    double const txDialCalibrated = txDialHz > 0.0
        ? applyFrequencyCalibration(txDialHz)
        : 0.0;
    activeCatSetTxFreq(this, m_hamlibCat, m_catBackend, txDialCalibrated, m_omniRigCat);

    if (txDialHz > 0.0) {
        bridgeLog(QStringLiteral("CAT split sync (%1): mode=%2 dial=%3 tx_ui=%4 xit=%5 tx_dial=%6 tx_audio=%7 via %8")
                      .arg(reason,
                           splitMode,
                           QString::number(m_frequency, 'f', 0),
                           QString::number(m_txFrequency),
                           QString::number(catSplitXitHzForTxFrequency(m_txFrequency)),
                           QString::number(txDialHz, 'f', 0),
                           QString::number(effectiveTxAudioFrequencyHz()),
                           m_catBackend));
    } else {
        bridgeLog(QStringLiteral("CAT split sync (%1): split off mode=%2 tx_ui=%3 via %4")
                      .arg(reason,
                           splitMode.isEmpty() ? QStringLiteral("none") : splitMode,
                           QString::number(m_txFrequency),
                           m_catBackend));
    }
}

