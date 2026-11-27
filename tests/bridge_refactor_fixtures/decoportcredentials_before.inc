QByteArray DecodiumBridge::decoPortAuthKey() const
{
    // La password lasciata dall'installazione viene PRIMA della chiave gia'
    // salvata: se l'utente l'ha appena digitata, e' quella che vuole. Altrimenti
    // reinstallare con una password nuova non cambierebbe niente e sarebbe un
    // modo silenzioso di non funzionare.
    QString const pending = decodium::profiledSettingsValue(
                                QString(), QStringLiteral("DecoPortPasswordPending"),
                                QString()).toString();
    if (!pending.trimmed().isEmpty()) {
        const_cast<DecodiumBridge*>(this)->setDecoPortPassword(pending);
        // setDecoPortPassword ha scritto la chiave e cancellato il testo in
        // chiaro: da qui in avanti si rilegge la chiave come sempre.
    }

    QString const hex = decodium::profiledSettingsValue(QString(),
                                                        QStringLiteral("DecoPortKey"),
                                                        QString()).toString();
    return hex.isEmpty() ? QByteArray() : QByteArray::fromHex(hex.toUtf8());
}

bool DecodiumBridge::hasDecoPortPassword() const
{
    return !decoPortAuthKey().isEmpty();
}

bool DecodiumBridge::setDecoPortPassword(const QString& password)
{
    QByteArray const key = decoport::deriveKeyFromPassword(password);
    if (key.isEmpty()) {
        bridgeLog(QStringLiteral("DecoPort password rejected: empty"));
        return false;
    }
    QSettings s(QSettings::IniFormat, QSettings::UserScope,
                QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    if (decodium::beginActiveSettingsProfile(s)) {
        s.setValue(QStringLiteral("DecoPortKey"), QString::fromUtf8(key.toHex()));
        s.remove(QStringLiteral("DecoPortPasswordPending"));
        s.endGroup();
        s.endGroup();
    } else {
        s.setValue(QStringLiteral("DecoPortKey"), QString::fromUtf8(key.toHex()));
        s.remove(QStringLiteral("DecoPortPasswordPending"));
    }
    // La parola in chiaro lasciata dall'installazione va tolta anche dalla
    // radice, non solo dal profilo attivo.
    s.remove(QStringLiteral("DecoPortPasswordPending"));
    s.sync();

    if (m_decoPortGateway)
        m_decoPortGateway->setAuthKey(key);
    if (m_decoPortLink)
        m_decoPortLink->setAuthKey(key);
    if (m_decoPortDiscovery)
        m_decoPortDiscovery->setAuthKey(key);
    bridgeLog(QStringLiteral("DecoPort password set (key stored, password discarded)"));
    return true;
}

void DecodiumBridge::clearDecoPortPassword()
{
    QSettings s(QSettings::IniFormat, QSettings::UserScope,
                QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    if (decodium::beginActiveSettingsProfile(s)) {
        s.remove(QStringLiteral("DecoPortKey"));
        s.endGroup();
        s.endGroup();
    }
    s.remove(QStringLiteral("DecoPortKey"));
    s.sync();
    if (m_decoPortGateway) {
        m_decoPortGateway->stop();
        m_decoPortGateway->setAuthKey(QByteArray());
    }
    if (m_decoPortLink)
        m_decoPortLink->setAuthKey(QByteArray());
    if (m_decoPortDiscovery)
        m_decoPortDiscovery->setAuthKey(QByteArray());
    bridgeLog(QStringLiteral("DecoPort password cleared; gateway stopped"));
}


