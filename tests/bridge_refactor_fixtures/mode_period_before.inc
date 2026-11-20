void DecodiumBridge::updatePeriodTicksMax()
{
    // Keep the period table centralized in periodMsForMode(), including
    // configured variants such as Q65-30A/Q65-60B and FST4/FST4W periods.
    int const periodMs = effectivePeriodMsForMode(m_mode);
    m_periodTicksMax = qMax(1, qRound(static_cast<double>(periodMs) / TIMER_MS));
}

QStringList DecodiumBridge::availableModes() const
{
    // RTTY in fondo: e' l'unico che non passa dai decodificatori a slot, e
    // sceglierlo ferma la decodifica dei modi digitali invece di affiancarsi.
    return {"FT8", "FT2", "FT2-Link", "FT4", "Q65", "MSK144", "JT65", "JT9", "JT4", "FST4", "FST4W", "WSPR", "RTTY", "JTTY", "CW", "SSB"};
}

