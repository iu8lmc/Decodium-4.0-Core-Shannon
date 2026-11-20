bool DecodiumBridge::updateSstvRxControls(const QVariantMap& controls)
{
#if DECODIUM_HAS_SSTV
    initialiseSstvRuntime();
    if (!m_sstvRxRuntime || m_sstvRedecodeActive) {
        return false;
    }
    SstvRxControlSettings parsed;
    QString error;
    if (!sstvRxSettingsFromMap(
            controls,
            m_sstvRxRuntime->rxControlSnapshot().settings,
            &parsed,
            &error)
        || !m_sstvRxRuntime->replaceRxControlSettings(parsed)) {
        emit errorMessage(error.isEmpty()
            ? tr("The SSTV RX controls were rejected") : error);
        return false;
    }

    persistSstvRxControls(parsed);
    return true;
#else
    Q_UNUSED(controls)
    return false;
#endif
}

