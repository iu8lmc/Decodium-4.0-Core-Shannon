void DecodiumBridge::configureCatShare(bool enabled, int port,
                                       bool allowControl, bool allowPtt)
{
    if (port < 1024 || port > 65535)
        port = 4533;
    setSetting(QStringLiteral("CatShareEnabled"), enabled);
    setSetting(QStringLiteral("CatSharePort"), port);
    setSetting(QStringLiteral("CatShareAllowControl"), allowControl);
    setSetting(QStringLiteral("CatShareAllowPtt"), allowPtt);
    if (m_catShare)
        m_catShare->configure(enabled, port, allowControl, allowPtt);
}

QObject* DecodiumBridge::nativeCatShareObject() const
{
    return m_nativeCatShare;
}

// Condivisione nel protocollo della radio. Stesso archivio della CAT condivisa.
// Il percorso del CAT cambia solo riaprendolo: se Hamlib e' collegato si
// riapre, cosi' la seriale passa al multiplexer, o torna a Decodium.
void DecodiumBridge::configureNativeCatShare(bool enabled, bool allowPtt, int rawPort)
{
    if (rawPort != 0 && (rawPort < 1024 || rawPort > 65535))
        rawPort = 0;
    setSetting(QStringLiteral("NativeCatShareEnabled"), enabled);
    setSetting(QStringLiteral("NativeCatShareAllowPtt"), allowPtt);
    setSetting(QStringLiteral("NativeCatShareRawPort"), rawPort);
    if (!m_nativeCatShare)
        return;
    bool const routingChanged = m_nativeCatShare->enabled() != enabled
                                || m_nativeCatShare->rawPort() != rawPort;
    m_nativeCatShare->configure(enabled, allowPtt, rawPort, QString());
    if (routingChanged && m_hamlibCat && m_catBackend == QLatin1String("hamlib")
        && (m_hamlibCat->connected() || m_hamlibCat->connecting())) {
        bridgeLog(QStringLiteral("Radio-protocol sharing %1: reopening the CAT")
                      .arg(enabled ? QStringLiteral("on") : QStringLiteral("off")));
        // Con il CAT aperto, connectRig lo chiude e lo riapre.
        m_hamlibCat->connectRig();
    }
}

void DecodiumBridge::createNativeCatShareVirtualPort()
{
    if (m_nativeCatShare)
        m_nativeCatShare->createVirtualPort();
}

// Spot condivisi. Stesso archivio e stessa forma della CAT condivisa, per la
// stessa ragione: due percorsi di lettura diversi hanno gia' prodotto in
// passato interruttori accesi senza alcun effetto.
void DecodiumBridge::configureSpotShare(bool enabled, int port)
{
    if (port < 1024 || port > 65535)
        port = 4534;
    setSetting(QStringLiteral("SpotShareEnabled"), enabled);
    setSetting(QStringLiteral("SpotSharePort"), port);
    if (m_spotShare)
        m_spotShare->configure(enabled, port);
}

