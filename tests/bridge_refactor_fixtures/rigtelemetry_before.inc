bool DecodiumBridge::catConnected() const { return m_catConnected; }
QString DecodiumBridge::catRigName() const { return m_catRigName; }
QString DecodiumBridge::catMode() const { return m_catMode; }

void DecodiumBridge::updateRigTelemetry(double powerWatts, double swr, double alc, bool alcValid)
{
    if (m_rigPowerWatts == powerWatts
        && m_rigSwr == swr
        && m_rigAlc == alc
        && m_rigAlcValid == alcValid) {
        return;
    }
    m_rigPowerWatts = powerWatts;
    m_rigSwr = swr;
    m_rigAlc = alc;  // 1.0.323 — ALC meter
    m_rigAlcValid = alcValid;
    emit rigTelemetryChanged();
    enforceSwrTransmissionLimit(QStringLiteral("telemetry"));
    enforceWsprPowerTransmissionLimit(QStringLiteral("telemetry"));
}

