void DecodiumBridge::syncNtpNow()
{
    if (!m_ntpClient) {
        return;
    }

    if (!m_ntpEnabled) {
        emit statusMessage(QStringLiteral("NTP disabilitato: abilitalo in Impostazioni > Avanzate"));
        bridgeLog(QStringLiteral("NTP sync requested while disabled"));
        return;
    }

    configureNtpClientForMode(m_mode);
    emit statusMessage(m_ntpCustomServer.isEmpty()
                       ? QStringLiteral("NTP sync in corso sui server pubblici...")
                       : QStringLiteral("NTP sync in corso su %1...").arg(m_ntpCustomServer));
    bridgeLog(QStringLiteral("NTP sync requested manually: server=%1")
                  .arg(m_ntpCustomServer.isEmpty() ? QStringLiteral("<auto>") : m_ntpCustomServer));
    m_ntpClient->syncNow();
}

