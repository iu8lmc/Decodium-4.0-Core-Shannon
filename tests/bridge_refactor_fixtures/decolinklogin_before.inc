static QString decolinkSecretService() { return QStringLiteral("Decodium/Decolink"); }

QVariantMap DecodiumBridge::decolinkSavedLogin() const
{
    auto const get = [](const char* key, const QVariant& def) {
        return decodium::profiledSettingsValue(QString(), QString::fromLatin1(key), def);
    };
    QVariantMap m;
    QString const email = get("DecolinkEmail", QString()).toString();
    m.insert(QStringLiteral("authHost"),
             get("DecolinkAuthHost", QStringLiteral("decolink.ft2.it")).toString());
    m.insert(QStringLiteral("relayHost"), get("DecolinkRelayHost", QString()).toString());
    m.insert(QStringLiteral("relayPort"), get("DecolinkRelayPort", 5555).toInt());
    m.insert(QStringLiteral("email"), email);
    m.insert(QStringLiteral("station"), get("DecolinkStation", QString()).toString());
    m.insert(QStringLiteral("audioProfile"), get("DecolinkAudioProfile", -1).toInt());
    bool saved = false;
    if (!email.isEmpty()) {
        auto const r = secure_settings::default_backend().lookup(decolinkSecretService(), email);
        saved = r.found && !r.value.isEmpty();
    }
    m.insert(QStringLiteral("hasPassword"), saved);
    return m;
}

void DecodiumBridge::decolinkConnect(const QString& authHost, const QString& relayHost,
                                     int relayPort, const QString& email,
                                     const QString& password, const QString& station,
                                     bool remember)
{
    QString const mail = email.trimmed();
    QString pw = password;
    if (pw.isEmpty() && !mail.isEmpty()) {
        auto const r = secure_settings::default_backend().lookup(decolinkSecretService(), mail);
        if (r.found)
            pw = r.value;
    }
    if (mail.isEmpty() || pw.isEmpty()) {
        emit errorMessage(tr("Decolink: enter e-mail and password"));
        return;
    }

    QSettings s(QSettings::IniFormat, QSettings::UserScope,
                QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    bool const inProfile = decodium::beginActiveSettingsProfile(s);
    s.setValue(QStringLiteral("DecolinkAuthHost"), authHost.trimmed());
    s.setValue(QStringLiteral("DecolinkRelayHost"), relayHost.trimmed());
    s.setValue(QStringLiteral("DecolinkRelayPort"), relayPort);
    s.setValue(QStringLiteral("DecolinkEmail"), mail);
    s.setValue(QStringLiteral("DecolinkStation"), station.trimmed());
    if (inProfile) {
        s.endGroup();
        s.endGroup();
    }
    s.sync();

    // La password sta nel deposito sicuro del sistema, mai nel file INI.
    if (remember && !password.isEmpty()) {
        QString err;
        if (!secure_settings::default_backend().store(decolinkSecretService(), mail, password, &err))
            bridgeLog(QStringLiteral("Decolink: password not stored (%1)").arg(err));
    } else if (!remember) {
        secure_settings::default_backend().remove(decolinkSecretService(), mail);
    }

    decolinkLinkObject();
    m_decolinkLink->setAudioProfile(decodium::profiledSettingsValue(
        QString(), QStringLiteral("DecolinkAudioProfile"), -1).toInt());
    m_decolinkLink->connectTo(authHost.trimmed(), relayHost.trimmed(), relayPort, mail, pw,
                              station.trimmed());
    bridgeLog(QStringLiteral("Decolink: connecting as %1 to %2").arg(mail, authHost.trimmed()));
}

void DecodiumBridge::setDecolinkAudioProfile(int profile)
{
    if (profile != -1 && profile != 0 && profile != 3)
        profile = -1;
    QSettings s(QSettings::IniFormat, QSettings::UserScope,
                QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    bool const inProfile = decodium::beginActiveSettingsProfile(s);
    s.setValue(QStringLiteral("DecolinkAudioProfile"), profile);
    if (inProfile) {
        s.endGroup();
        s.endGroup();
    }
    s.sync();
    decolinkLinkObject();
    m_decolinkLink->setAudioProfile(profile);
}

void DecodiumBridge::decolinkForgetPassword()
{
    QString const mail = decodium::profiledSettingsValue(
        QString(), QStringLiteral("DecolinkEmail"), QString()).toString();
    if (!mail.isEmpty())
        secure_settings::default_backend().remove(decolinkSecretService(), mail);
}

