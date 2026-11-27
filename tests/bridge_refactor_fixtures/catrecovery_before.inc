bool DecodiumBridge::resetCatConnection()
{
    if (m_transmitting || m_tuning) {
        const QString message = QStringLiteral("CAT recovery postponed while the radio is transmitting or tuning.");
        emit statusMessage(message);
        return false;
    }

    bridgeLog(QStringLiteral("CAT safe recovery requested: releasing all CAT backends"));
    if (m_cat4OmCat) {
        m_cat4OmCat->disconnectRig();
    }
    if (m_omniRigCat) {
        m_omniRigCat->disconnectRig();
    }
    if (m_hamlibCat) {
        m_hamlibCat->disconnectRig();
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

    // Keep the selected backend and its profile intact.  retryRigConnection()
    // applies the same ownership rules used by the normal Connect button and
    // therefore avoids opening a serial port twice.
    retryRigConnection();
    emit statusMessage(QStringLiteral("CAT recovery started: reconnecting the selected radio profile."));
    return true;
}

void DecodiumBridge::retryRigConnection()
{
    if (legacyOwnsRigControl(m_legacyBackend)) {
        m_legacyBackend->retryRigConnection();
        return;
    }

    if (useLegacyRigControlFallback(m_legacyBackend, m_catBackend)) {
        m_legacyBackend->retryRigConnection();
        return;
    }

    // Prima di tentare connectRig sul backend attivo, assicuriamo che gli
    // altri due backend abbiano rilasciato la porta seriale. Questo evita
    // errori "serial port already open / Access denied" quando l'utente
    // passa da OmniRig a Hamlib (OmniRig.exe tiene la COM via COM object).
    if (m_catBackend != "cat4om"  && m_cat4OmCat)  m_cat4OmCat->disconnectRig();
    if (m_catBackend != "omnirig" && m_omniRigCat) m_omniRigCat->disconnectRig();
    if (!isHamlibFamilyBackend(m_catBackend) && m_hamlibCat)  m_hamlibCat->disconnectRig();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

    if (m_catBackend == "cat4om" && m_cat4OmCat) {
        m_cat4OmCat->connectRig();
    } else if (m_catBackend == "omnirig" && m_omniRigCat) {
        m_omniRigCat->connectRig();
    } else if (m_hamlibCat) {
        m_hamlibCat->connectRig();
    }
}




