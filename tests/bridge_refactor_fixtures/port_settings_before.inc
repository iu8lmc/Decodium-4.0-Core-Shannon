bool DecodiumBridge::decoPortAutoStart() const
{
    return decodium::profiledSettingsValue(QString(),
                                           QStringLiteral("DecoPortAutoStart"),
                                           false).toBool();
}

bool DecodiumBridge::ft2DriftSearchEnabled() const
{
    return decodium::profiledSettingsValue(QString(),
                                           QStringLiteral("Ft2DriftSearch"),
                                           true).toBool();
}

void DecodiumBridge::setFt2DriftSearchEnabled(bool on)
{
    QSettings s(QSettings::IniFormat, QSettings::UserScope,
                QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    if (decodium::beginActiveSettingsProfile(s)) {
        s.setValue(QStringLiteral("Ft2DriftSearch"), on);
        s.endGroup();
        s.endGroup();
    } else {
        s.setValue(QStringLiteral("Ft2DriftSearch"), on);
    }
    s.sync();
    bridgeLog(QStringLiteral("FT2 drift search set: %1 (applies at next start)").arg(on ? 1 : 0));
}

void DecodiumBridge::setDecoPortAutoStart(bool on)
{
    // Si scrive DOVE si legge. profiledSettingsValue guarda prima nel profilo
    // attivo e solo dopo nella radice: un setter che scrivesse in radice
    // funzionerebbe finche' il profilo non ha la chiave, e smetterebbe di
    // funzionare il giorno in cui qualcuno ce la mette.
    QSettings s(QSettings::IniFormat, QSettings::UserScope,
                QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    if (decodium::beginActiveSettingsProfile(s)) {
        s.setValue(QStringLiteral("DecoPortAutoStart"), on);
        s.endGroup();
        s.endGroup();
    } else {
        s.setValue(QStringLiteral("DecoPortAutoStart"), on);
    }
    s.sync();
    bridgeLog(QStringLiteral("DecoPort auto-start set: %1").arg(on ? 1 : 0));
}

int DecodiumBridge::decoPortConfiguredPort() const
{
    int const port = decodium::profiledSettingsValue(QString(),
                                                     QStringLiteral("DecoPortPort"),
                                                     5559).toInt();
    return (port >= 1024 && port <= 65535) ? port : 5559;
}
