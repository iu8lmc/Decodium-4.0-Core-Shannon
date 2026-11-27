void DecodiumBridge::setVhfUhfFeatures(bool v)
{
    if (m_vhfUhfFeatures == v) {
        return;
    }

    m_vhfUhfFeatures = v;

    QSettings s(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(s);
    s.setValue(QStringLiteral("vhfUhfFeatures"), m_vhfUhfFeatures);
    s.setValue(QStringLiteral("VHFUHF"), m_vhfUhfFeatures);
    s.sync();
    syncSettingToLegacyIni(QStringLiteral("vhfUhfFeatures"), m_vhfUhfFeatures);
    syncSettingToLegacyIni(QStringLiteral("VHFUHF"), m_vhfUhfFeatures);

    emit vhfUhfFeaturesChanged();
    emit settingValueChanged(QStringLiteral("vhfUhfFeatures"), m_vhfUhfFeatures);
    emit settingValueChanged(QStringLiteral("VHFUHF"), m_vhfUhfFeatures);
}

