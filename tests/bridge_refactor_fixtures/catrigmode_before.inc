void DecodiumBridge::applyConfiguredCatRigMode(const QString& reason,
                                               const QString& explicitMode)
{
    // CW is a radio mode, not a digital audio mode. DATA-U/DIGU remains the
    // configured path for digital application modes; the CW workspace asks
    // the local rig or remote gateway for CW so its carrier stays on the dial.
    QString rigMode = explicitMode.trimmed();
    if (rigMode.isEmpty()) {
        rigMode = m_mode == QStringLiteral("SSB") ? m_ssbRadioMode
            : (m_mode == QStringLiteral("CW") ? QStringLiteral("CW") : configuredCatRigMode());
    }
    const QString upperRigMode = rigMode.toUpper();
    if (upperRigMode == QStringLiteral("DATA/PACKET")
        || upperRigMode == QStringLiteral("DATA/PKT")
        || upperRigMode == QStringLiteral("DATA-USB")
        || upperRigMode == QStringLiteral("USB-D")
        || upperRigMode == QStringLiteral("DIGU")
        || upperRigMode == QStringLiteral("PKTUSB")
        || upperRigMode == QStringLiteral("PKT-U")) {
        rigMode = QStringLiteral("DATA-U");
    } else if (upperRigMode == QStringLiteral("DATA-LSB")
               || upperRigMode == QStringLiteral("DIGL")
               || upperRigMode == QStringLiteral("PKTLSB")
               || upperRigMode == QStringLiteral("PKT-L")) {
        rigMode = QStringLiteral("DATA-L");
    }
    if (rigMode.isEmpty() && m_mode != QStringLiteral("RTTY")) {
        rigMode = m_rttyRigModeState.restoreTarget(
            rttyCatContext(), rigMode, QDateTime::currentMSecsSinceEpoch());
        if (!rigMode.isEmpty() && (m_transmitting || m_tuning || m_decoPortRemoteKeyed)) {
            bridgeLog(QStringLiteral("RTTY exit CAT restore deferred: TX/Tune active"));
            return;
        }
    }
    if (rigMode.isEmpty()) {
        return;
    }

    if (m_networkRigctlStartupReadOnly
        && isNetworkRigctlCat(m_catBackend, m_hamlibCat)) {
        bridgeLog(QStringLiteral(
            "CAT NET rigctl startup: deferred mode write until state is known (%1)").arg(reason));
        return;
    }

    if (hamlibCatFrequencySettleActive(reason)) {
        bridgeLog(QStringLiteral("CAT rig mode sync delayed during QSY settle (%1): %2")
                      .arg(reason, rigMode));
        return;
    }

    if (!m_catMode.trimmed().isEmpty()
        && m_catMode.compare(rigMode, Qt::CaseInsensitive) == 0) {
        return;
    }

    if (decoPortIsCat(m_catBackend)) {
        if (!g_decoPortCatLink || !g_decoPortCatLink->isLinked())
            return;
        // Il vocabolario di DecoPort e' funzionale: DIGU vuol dire "metti il
        // codec USB dentro il modulatore". La voce "modo radio CAT" descrive
        // invece come quel modo si scrive su UNA radio locale — "USB",
        // "DATA-U" — e su DecoPort quella traduzione spetta al gateway.
        //
        // Mandare USB metterebbe la radio remota sul microfono. I modi digitali
        // usano DIGU/DIGL e il codec USB; il CW usa il keying remoto nativo.
        QString const wanted = m_mode == QStringLiteral("SSB") ? m_ssbRadioMode
            : (m_mode == QStringLiteral("CW") ? QStringLiteral("CW")
               : (rigMode.contains(QStringLiteral("-L"), Qt::CaseInsensitive)
                  || rigMode.compare(QStringLiteral("LSB"), Qt::CaseInsensitive) == 0
                      ? QStringLiteral("DIGL") : QStringLiteral("DIGU")));
        if (m_catMode.compare(wanted, Qt::CaseInsensitive) == 0)
            return;
        bridgeLog(QStringLiteral("CAT rig mode sync (%1): decoport -> %2 (configurato: %3)")
                      .arg(reason, wanted, rigMode));
        g_decoPortCatLink->setModeName(wanted);
        return;
    }

    if (m_catBackend == QStringLiteral("cat4om")) {
        if (m_cat4OmCat && m_cat4OmCat->connected()) {
            bridgeLog(QStringLiteral("CAT rig mode sync (%1): cat4om -> %2").arg(reason, rigMode));
            m_cat4OmCat->setRigMode(rigMode);
            // CAT4OM can acknowledge PTT before the radio has applied the
            // digital-mode command.  Reassert DATA-U after the PTT edge so a
            // stale USB snapshot cannot win during TX.
            if (configuredCatRigModeRequestsDataPacket()
                && reason.compare(QStringLiteral("startTx"), Qt::CaseInsensitive) == 0) {
                QTimer::singleShot(350, this, [this]() {
                    if (m_transmitting && m_mode != QStringLiteral("RTTY")
                        && m_catBackend == QStringLiteral("cat4om")
                        && m_cat4OmCat && m_cat4OmCat->connected()) {
                        bridgeLog(QStringLiteral("CAT rig mode verify (post-PTT): cat4om -> DATA-U"));
                        m_cat4OmCat->setRigMode(QStringLiteral("DATA-U"));
                    }
                });
            }
        }
        return;
    }

    if (isHamlibFamilyBackend(m_catBackend)) {
        if (m_hamlibCat && m_hamlibCat->connected()) {
            bridgeLog(QStringLiteral("CAT rig mode sync (%1): %2 -> %3").arg(reason, m_catBackend, rigMode));
            m_hamlibCat->setRigMode(rigMode);
        }
        return;
    }

    if (m_catBackend == QStringLiteral("omnirig") && m_omniRigCat && m_omniRigCat->connected()) {
        bridgeLog(QStringLiteral("CAT rig mode sync (%1): omnirig -> %2").arg(reason, rigMode));
        m_omniRigCat->setRigMode(rigMode);
        if (configuredCatRigModeRequestsDataPacket()
            && reason.compare(QStringLiteral("startTx"), Qt::CaseInsensitive) == 0) {
            QTimer::singleShot(350, this, [this]() {
                if (m_transmitting && m_mode != QStringLiteral("RTTY")
                    && m_catBackend == QStringLiteral("omnirig")
                    && m_omniRigCat && m_omniRigCat->connected()) {
                    bridgeLog(QStringLiteral("CAT rig mode verify (post-PTT): omnirig -> DATA-U"));
                    m_omniRigCat->setRigMode(QStringLiteral("DATA-U"));
                }
            });
        }
    }
}


