void DecodiumBridge::rememberAudioInputDeviceIdentity(QAudioDevice const& device,
                                                      const QString& reason,
                                                      bool updateDisplayName)
{
    if (!updateDisplayName && m_audioInputDevice.trimmed().isEmpty()) {
        if (!m_audioInputDeviceId.isEmpty()) {
            m_audioInputDeviceId.clear();
            persistSingleAudioIdentity(true, m_audioInputDevice, m_audioInputDeviceId);
        }
        return;
    }

    QString const deviceName = device.description().trimmed();
    QString const deviceId = audioDeviceIdForSettings(device);
    bool changed = false;
    bool nameChanged = false;

    if (updateDisplayName
        && !deviceName.isEmpty()
        && !audioDeviceNameExactMatches(m_audioInputDevice, deviceName)) {
        m_audioInputDevice = deviceName;
        nameChanged = true;
        changed = true;
    }
    if (!deviceId.isEmpty()
        && m_audioInputDeviceId.compare(deviceId, Qt::CaseInsensitive) != 0) {
        m_audioInputDeviceId = deviceId;
        changed = true;
    }

    if (!changed) {
        return;
    }

    bridgeLog(QStringLiteral("audio input identity remembered (%1): %2")
                  .arg(reason.trimmed().isEmpty() ? QStringLiteral("resolved") : reason.trimmed(),
                       audioDeviceSelectionForLog(m_audioInputDevice, m_audioInputDeviceId)));
    persistSingleAudioIdentity(true, m_audioInputDevice, m_audioInputDeviceId);
    if (nameChanged) {
        syncAudioDeviceSettingsToLegacyIni();
        if (legacyBackendAvailable()) {
            m_legacyBackend->setAudioInputDeviceName(m_audioInputDevice);
        }
        emit audioInputDeviceChanged();
        emit sstvRxSourceDeviceChanged();
    }
}

void DecodiumBridge::rememberAudioOutputDeviceIdentity(QAudioDevice const& device,
                                                       const QString& reason,
                                                       bool updateDisplayName)
{
    if (!updateDisplayName && m_audioOutputDevice.trimmed().isEmpty()) {
        if (!m_audioOutputDeviceId.isEmpty()) {
            m_audioOutputDeviceId.clear();
            persistSingleAudioIdentity(false, m_audioOutputDevice, m_audioOutputDeviceId);
        }
        return;
    }

    QString const deviceName = device.description().trimmed();
    QString const deviceId = audioDeviceIdForSettings(device);
    bool changed = false;
    bool nameChanged = false;

    if (updateDisplayName
        && !deviceName.isEmpty()
        && !audioDeviceNameExactMatches(m_audioOutputDevice, deviceName)) {
        m_audioOutputDevice = deviceName;
        nameChanged = true;
        changed = true;
    }
    if (!deviceId.isEmpty()
        && m_audioOutputDeviceId.compare(deviceId, Qt::CaseInsensitive) != 0) {
        m_audioOutputDeviceId = deviceId;
        changed = true;
    }

    if (!changed) {
        return;
    }

    bridgeLog(QStringLiteral("audio output identity remembered (%1): %2")
                  .arg(reason.trimmed().isEmpty() ? QStringLiteral("resolved") : reason.trimmed(),
                       audioDeviceSelectionForLog(m_audioOutputDevice, m_audioOutputDeviceId)));
    persistSingleAudioIdentity(false, m_audioOutputDevice, m_audioOutputDeviceId);
    m_cachedTxOutputDeviceValid = false;
    invalidateTxAudioCache();
    if (nameChanged) {
        syncAudioDeviceSettingsToLegacyIni();
        if (legacyBackendAvailable()) {
            m_legacyBackend->setAudioOutputDeviceName(m_audioOutputDevice);
        }
        emit audioOutputDeviceChanged();
    }
}

void DecodiumBridge::syncAudioDeviceSettingsToLegacyIni()
{
    syncSettingToLegacyIni(QStringLiteral("SoundInName"), m_audioInputDevice);
    syncSettingToLegacyIni(QStringLiteral("SoundOutName"), m_audioOutputDevice);
    syncSettingToLegacyIni(QStringLiteral("AudioInputChannel"), audioChannelSettingValue(m_audioInputChannel));
    syncSettingToLegacyIni(QStringLiteral("AudioOutputChannel"), audioChannelSettingValue(m_audioOutputChannel));
}

