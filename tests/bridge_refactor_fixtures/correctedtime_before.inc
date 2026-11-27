qint64 DecodiumBridge::correctedUtcEpochMs() const
{
    qint64 nowMs = preciseCurrentMSecsSinceEpoch();
    if (m_ntpEnabled && m_ntpSynced && std::isfinite(m_ntpOffsetMs)) {
        nowMs += qRound64(m_ntpOffsetMs);
    }
    return nowMs;
}

qint64 DecodiumBridge::correctedUtcMsecsSinceStartOfDay() const
{
    static constexpr qint64 kDayMs = 24LL * 60LL * 60LL * 1000LL;
    qint64 ms = correctedUtcEpochMs() % kDayMs;
    if (ms < 0) {
        ms += kDayMs;
    }
    return ms;
}

