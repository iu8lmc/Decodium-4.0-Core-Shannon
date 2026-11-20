    // 1.0.159 — DecoSyncTime fase 1: wrapper centrale time-sync. Possiede
    // un NtpClient interno (per ora unica source). Default ENABLED; il
    // caricamento delle settings successivo puo' sovrascrivere.
    m_decoSyncTime    = new DecoSyncTime(this);
    m_ntpClient       = m_decoSyncTime->legacyNtpClient();
    connect(m_ntpClient, &NtpClient::offsetUpdated, this, [this](double offsetMs) {
        if (qFuzzyCompare(m_ntpOffsetMs + 1.0, offsetMs + 1.0)) {
            return;
        }
        double const previousOffsetMs = m_ntpOffsetMs;
        m_ntpOffsetMs = offsetMs;
        setGlobalNtpOffsetMs(offsetMs);
        emit ntpOffsetMsChanged();
        if (m_ntpEnabled
            && m_ntpSynced
            && m_monitoring
            && isTimeSyncDecodeMode(m_mode)
            && std::abs(offsetMs - previousOffsetMs) >= 50.0) {
            // 1.0.166 — hold-off post-TX: il PTT release causa jitter NTP
            // transitorio. Senza guard, ogni rearm chiama
            // resetRxPeriodAccumulation() che svuota m_audioBuffer e setta
            // m_lastPeriodSlot=-1 → decoder muto fino a stabilizzazione
            // (a volte mai). DecoSyncTime fasi 2-4 hanno aumentato la
            // frequenza degli offsetUpdated emit, peggiorando il problema.
            qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
            bool const recentlyTx = (m_lastTxEndMs > 0)
                                    && (nowMs - m_lastTxEndMs < 5000);
            bool const recentlyRearmed = (m_lastNtpRearmMs > 0)
                                         && (nowMs - m_lastNtpRearmMs < 30000);
            if (recentlyTx || recentlyRearmed) {
                bridgeLog(QStringLiteral("NTP offset update suppressed: %1ms (recentTx=%2 recentRearm=%3)")
                              .arg(QString::number(offsetMs - previousOffsetMs, 'f', 1),
                                   recentlyTx ? "1" : "0",
                                   recentlyRearmed ? "1" : "0"));
            } else {
                m_lastNtpRearmMs = nowMs;
                quint64 const sessionId = ++m_periodTimerSessionId;
                armPeriodTimerForCurrentMode(sessionId, QStringLiteral("NTP offset update"));
            }
        }
    });
    connect(m_ntpClient, &NtpClient::syncStatusChanged, this, [this](bool synced, const QString& statusText) {
        Q_UNUSED(statusText)
        bool const effectiveSynced = m_ntpEnabled && synced;
        bool const becameSynced = !m_ntpSynced && effectiveSynced;
        if (m_ntpSynced != effectiveSynced) {
            m_ntpSynced = effectiveSynced;
            emit ntpSyncedChanged();
        }
        if (becameSynced
            && m_monitoring
            && isTimeSyncDecodeMode(m_mode)
            && std::abs(m_ntpOffsetMs) >= 50.0) {
            quint64 const sessionId = ++m_periodTimerSessionId;
            armPeriodTimerForCurrentMode(sessionId, QStringLiteral("NTP sync"));
        }
        bridgeLog(QStringLiteral("NTP status: enabled=%1 synced=%2 offset=%3 ms")
                      .arg(m_ntpEnabled ? QStringLiteral("1") : QStringLiteral("0"),
                           effectiveSynced ? QStringLiteral("1") : QStringLiteral("0"),
                           QString::number(m_ntpOffsetMs, 'f', 1)));
    });
    connect(m_ntpClient, &NtpClient::errorOccurred, this, [](const QString& msg) {
        bridgeLog(QStringLiteral("NTP error: ") + msg);
    });

