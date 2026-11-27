QObject* DecodiumBridge::q65Doppler() const { return m_q65Doppler; }

QStringList DecodiumBridge::audioInputDevices() const { return m_audioInputDevices; }

QStringList DecodiumBridge::audioOutputDevices() const { return m_audioOutputDevices; }

QString DecodiumBridge::audioInputDevice() const { return m_audioInputDevice; }

void DecodiumBridge::setUiPaletteIndex(int v)
{
    v = qBound(0, v, 10);
    if (m_uiPaletteIndex == v) {
        return;
    }

    m_uiPaletteIndex = v;
    if (usingLegacyBackendForTx() && v <= 5) {
        m_legacyBackend->setWaterfallPalette(decodiumBridgeLegacyPaletteNameForUiIndex(v));
    }
    emit uiPaletteIndexChanged();
}

QObject* DecodiumBridge::activeStations() const
{
    return m_activeStations;
}

