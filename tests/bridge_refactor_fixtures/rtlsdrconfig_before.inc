bool DecodiumBridge::rtlSdrDirectSamplingAvailable(int deviceIndex) const
{
    if (deviceIndex < 0 || deviceIndex >= m_rtlSdrDevices.size()) {
        return false;
    }
    return !decodium::rtl_sdr::isRtlSdrBlogV4Identity(
        m_rtlSdrDevices.at(deviceIndex));
}

bool DecodiumBridge::rtlSdrEnabled() const
{
    return m_rtlSdrEnabledCache.value(decodium::activeSettingsProfileName(), [this]() {
        return getSetting(QStringLiteral("RtlSdrEnabled"), false).toBool();
    });
}

bool DecodiumBridge::rtlSdrGeneralReceiverConfigured() const
{
    if (!rtlSdrEnabled()) {
        return false;
    }
    return getSetting(QStringLiteral("RtlSdrDemodulator"), QStringLiteral("weak"))
               .toString().trimmed().compare(QStringLiteral("weak"), Qt::CaseInsensitive) != 0;
}

bool DecodiumBridge::rtlSdrGeneralReceiverActive() const
{
    return rtlSdrGeneralReceiverConfigured()
        && m_rtlSdrInput
        && m_rtlSdrInput->isRunning()
        && m_rtlSdrInput->activeConfig().demodulator
               != RtlSdrDsp::Demodulator::WeakSignal;
}

