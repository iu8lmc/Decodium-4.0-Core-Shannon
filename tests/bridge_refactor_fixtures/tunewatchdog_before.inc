void DecodiumBridge::armTuneWatchdog()
{
    if (!getSetting(QStringLiteral("TuneWatchdog"), true).toBool()) {
        return;
    }
    int const configured = getSetting(QStringLiteral("TuneWatchdogTime"), 90).toInt();
    if (configured <= 0) {
        return;   // lo spin box arriva a 0: zero secondi vale "nessun limite"
    }
    int const seconds = qBound(5, configured, 3600);
    if (!m_tuneWatchdogTimer) {
        m_tuneWatchdogTimer = new QTimer(this);
        m_tuneWatchdogTimer->setSingleShot(true);
        connect(m_tuneWatchdogTimer, &QTimer::timeout, this, [this]() {
            if (!m_tuning && !m_bridgeAudioTuneActive) {
                return;   // gia' fermo: niente da fare
            }
            bridgeLog(QStringLiteral("Tune watchdog: timeout -> Tune fermato"));
            emit statusMessage(QStringLiteral("Tune watchdog: tempo scaduto, Tune fermato"));
            stopTune();
        });
    }
    m_tuneWatchdogTimer->start(seconds * 1000);
    bridgeLog(QStringLiteral("Tune watchdog armato: %1 s").arg(seconds));
}

void DecodiumBridge::disarmTuneWatchdog(const QString& reason)
{
    if (!m_tuneWatchdogTimer || !m_tuneWatchdogTimer->isActive()) {
        return;
    }
    m_tuneWatchdogTimer->stop();
    bridgeLog(QStringLiteral("Tune watchdog disarmato (%1)").arg(reason));
}

