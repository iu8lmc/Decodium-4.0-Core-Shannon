void DecodiumBridge::armPeriodTimerForCurrentMode(quint64 sessionId, const QString& reason)
{
    if (!m_periodTimer) {
        return;
    }

    m_periodTimer->stop();

    QString const modeSnapshot = m_mode;
    int const periodMs = effectivePeriodMsForMode(modeSnapshot);
    qint64 const msNow = correctedUtcMsecsSinceStartOfDay();
    int const msInPeriod = (periodMs > 0) ? static_cast<int>(msNow % static_cast<qint64>(periodMs)) : 0;
    int msToNext = (periodMs > 0) ? (periodMs - msInPeriod) : TIMER_MS;
    if (msToNext < TIMER_MS) {
        msToNext += periodMs;
    }

    bridgeLog(reason + ": UTC sync wait=" + QString::number(msToNext) +
              "ms (period=" + QString::number(periodMs) +
              "ms mode=" + modeSnapshot +
              " ntp=" + QString::number((m_ntpEnabled && m_ntpSynced) ? m_ntpOffsetMs : 0.0, 'f', 1) +
              " session=" + QString::number(sessionId) + ")");

    if (modeSnapshot == QStringLiteral("WSPR")
        || bridgeOwnsLegacyJtRxWhenCatSuppressed(modeSnapshot)
        || bridgeOwnsFst4Audio(modeSnapshot)) {
        resetRxPeriodAccumulation(true);
        m_periodTicks = 0;
        m_lastPeriodSlot = -1;
        m_bridgeOwnedJtFallbackLastSlot = -1;
        m_bridgeOwnedJtFallbackLastDispatchMs = 0;
        m_bridgeOwnedJtFallbackLastMode.clear();
        m_periodTimer->start();
        bridgeLog(QStringLiteral("%1: %2 period timer started immediately; next UTC boundary in %3ms (session=%4)")
                      .arg(reason)
                      .arg(modeSnapshot)
                      .arg(msToNext)
                      .arg(sessionId));
        return;
    }

    QTimer::singleShot(msToNext, this, [this, sessionId, modeSnapshot]() {
        if (!m_monitoring) {
            return;
        }
        if (sessionId != m_periodTimerSessionId || m_mode != modeSnapshot) {
            bridgeLog("period timer start skipped: stale session/mode (wanted mode=" +
                      modeSnapshot + " session=" + QString::number(sessionId) +
                      ", current mode=" + m_mode +
                      " session=" + QString::number(m_periodTimerSessionId) + ")");
            return;
        }

        resetRxPeriodAccumulation(true);
        m_periodTimer->start();
        bridgeLog("period timer started at UTC boundary (mode=" + modeSnapshot +
                  " session=" + QString::number(sessionId) + ")");
    });
}

