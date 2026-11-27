void DecodiumBridge::refreshAudioDevices()
{
    refreshAudioDevicesInternal(QStringLiteral("manual refreshAudioDevices"),
                                false,
                                true);
}

bool DecodiumBridge::restoreAudioDefaults()
{
    if (m_transmitting || m_tuning) {
        const QString message = QStringLiteral("Audio recovery postponed while the radio is transmitting or tuning.");
        emit statusMessage(message);
        return false;
    }

    // Force a fresh enumeration: after sleep/wake or a USB/Pulse route change
    // the cached QAudioDevice objects can describe the old route.  Selecting
    // the current system defaults also clears stale device identities through
    // the normal setters, without deleting any other audio preferences.
    refreshAudioDeviceCache(QStringLiteral("safe audio recovery"), false, true);
    const QString input = m_cachedDefaultAudioInput.description().trimmed();
    const QString output = m_cachedDefaultAudioOutput.description().trimmed();
    if (input.isEmpty() && output.isEmpty()) {
        const QString message = QStringLiteral("Audio recovery could not find a system input or output device.");
        bridgeLog(message);
        emit errorMessage(message);
        return false;
    }

    if (!input.isEmpty()) {
        setAudioInputDevice(input);
    }
    if (!output.isEmpty()) {
        setAudioOutputDevice(output);
    }
    setAudioInputChannel(0);
    setAudioOutputChannel(0);
    refreshAudioDevicesInternal(QStringLiteral("safe audio recovery rebind"), false, false);

    const QString message = QStringLiteral("Audio restored to the current system defaults.");
    bridgeLog(message + QStringLiteral(" input=[%1] output=[%2]").arg(input, output));
    emit statusMessage(message);
    return true;
}

