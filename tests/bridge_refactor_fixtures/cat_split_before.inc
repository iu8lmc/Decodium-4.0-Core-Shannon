bool DecodiumBridge::catSplitOperationActiveForMode() const
{
    if (!isHamlibFamilyBackend(m_catBackend)
        && m_catBackend != QStringLiteral("cat4om")) {
        return false;
    }

    QObject const* activeCat = catManagerObj();
    if (!activeCat) return false;
    QString const splitMode = normalizedCatSplitMode(activeCat->property("splitMode").toString());
    if (splitMode != QStringLiteral("rig") && splitMode != QStringLiteral("emulate")) {
        return false;
    }

    QString mode = canonicalApplicationDecodeMode(m_mode).trimmed().toUpper();
    if (mode.isEmpty()) {
        QString const rawMode = m_mode.trimmed().toUpper();
        if (rawMode.contains(QStringLiteral("FT8"))) {
            mode = QStringLiteral("FT8");
        } else if (rawMode.contains(QStringLiteral("FT4"))) {
            mode = QStringLiteral("FT4");
        } else if (rawMode.contains(QStringLiteral("FT2"))) {
            mode = QStringLiteral("FT2");
        } else if (rawMode.startsWith(QStringLiteral("FST4"))) {
            mode = rawMode;
        }
    }
    bool const wsjtSplitMode =
        mode == QStringLiteral("FT8")
        || mode == QStringLiteral("FT4")
        || mode == QStringLiteral("FT2")
        || mode == QStringLiteral("FST4")
        || mode.startsWith(QStringLiteral("FST4-"));

    return !m_vhfUhfFeatures || wsjtSplitMode;
}

int DecodiumBridge::catSplitXitHzForTxFrequency(int txFrequencyHz) const
{
    // EME owns absolute RF offsets; do not also apply the FT audio/XIT shift.
    if (m_q65Doppler && m_q65Doppler->applied()) return 0;
    if (!catSplitOperationActiveForMode() || txFrequencyHz <= 0) {
        return 0;
    }

    return (txFrequencyHz / 500) * 500 - 1500;
}

int DecodiumBridge::effectiveTxAudioFrequencyHz() const
{
    int const xitHz = catSplitXitHzForTxFrequency(m_txFrequency);
    return qBound(0, m_txFrequency - xitHz, 5000);
}

double DecodiumBridge::catSplitTxDialFrequencyHz() const
{
    if (m_q65Doppler && m_q65Doppler->applied())
        return m_q65Doppler->txDialHz();
    if (!catSplitOperationActiveForMode() || m_txFrequency <= 0) {
        return 0.0;
    }

    // m_frequency is always the logical/on-air dial.  CAT manager frequency
    // properties contain the physical rig/IF dial and must not be calibrated
    // a second time when the split TX VFO is derived.
    double const dialHz = m_frequency;
    if (dialHz <= 0.0) {
        return 0.0;
    }

    return dialHz + static_cast<double>(catSplitXitHzForTxFrequency(m_txFrequency));
}
