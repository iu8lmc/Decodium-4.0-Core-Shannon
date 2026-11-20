void DecodiumBridge::maybeDispatchFt8EarlyDecode(qint64 utcSlot, int msInSlot, int periodMs)
{
    if (m_mode != QStringLiteral("FT8")
        || periodMs != 15000
        || !m_monitoring
        || usingLegacyBackendForTx()
        || m_transmitting
        || m_tuning
        || m_rxAudioSuspendedForTx) {
        return;
    }

    if (m_ft8EarlyDecodeSlot != utcSlot) {
        m_ft8EarlyDecodeSlot = utcSlot;
        m_ft8EarlyDecode41Sent = false;
        m_ft8EarlyDecode47Sent = false;
    }

    static constexpr int kFt8Early41Ms = 10900;
    static constexpr int kFt8Early47Ms = 14250;

    int nzhsym = 0;
    bool mark41 = false;
    bool mark47 = false;
    if (!m_ft8EarlyDecode41Sent && msInSlot >= kFt8Early41Ms) {
        nzhsym = 41;
        mark41 = true;
    } else if (m_ft8EarlyDecode41Sent && !m_ft8EarlyDecode47Sent && msInSlot >= kFt8Early47Ms) {
        nzhsym = 47;
        mark47 = true;
    }

    if (nzhsym == 0) {
        return;
    }

    if (mark47) {
        // The second partial FT8 pass was tested on Windows with both nzhsym=47
        // and a short full-symbol pre-final path.  It either returned no rows
        // or stole budget from the final pass, so keep the live path focused:
        // one shallow preview, then the prioritized final decode.
        m_ft8EarlyDecode47Sent = true;
        return;
    }

    // 1.0.178 — Skip solo su severe pressure OR esplicito lowCpuMode.
    // Prima skipparvamo anche su mild pressure (UI stall 600-1099ms) → utenti
    // perdevano le passate FT8 early predecode (nzhsym=41/47, ~50% yield) su
    // qualsiasi PC modesto con UI stall ricorrente. Mild pressure ora passa,
    // sarà il decodeDepth/threadLimit a calare automaticamente.
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    qint64 const monitoringAgeMs =
        (m_monitoring && m_monitoringSince.isValid())
            ? m_monitoringSince.msecsTo(QDateTime::currentDateTimeUtc())
            : std::numeric_limits<qint64>::max();
    bool const startupWindow =
        monitoringAgeMs >= 0
        && monitoringAgeMs < 45000;
    int const earlyFtThreadBudget = effectiveFtThreadLimitForDecode();
    bool const startupQmlGuardActive =
        nowMs < m_ft8StartupEarlyDecodeGuardUntilMs;
    bool const startupCpuPressureGuard =
        startupWindow
        && (cpuPressureActive() || m_cpuPressureEventCount > 0);
    bool const startupPressureGuard =
        startupQmlGuardActive
        || startupCpuPressureGuard
        || (startupWindow && earlyFtThreadBudget <= 2);

    if (m_lowCpuModeEnabled || cpuPressureSevereActive() || startupPressureGuard) {
        if (mark41) m_ft8EarlyDecode41Sent = true;
        if (mark47) m_ft8EarlyDecode47Sent = true;
        if (nowMs - m_lastEarlyDecodeSkipLogMs > 2000) {
            m_lastEarlyDecodeSkipLogMs = nowMs;
            QString reason;
            if (m_lowCpuModeEnabled) {
                reason = QStringLiteral("Low CPU mode");
            } else if (cpuPressureSevereActive()) {
                reason = QStringLiteral("severe CPU pressure");
            } else if (startupQmlGuardActive) {
                reason = QStringLiteral("slow QML startup guard");
            } else if (startupCpuPressureGuard) {
                reason = QStringLiteral("startup CPU pressure");
            } else {
                reason = QStringLiteral("startup FT thread budget");
            }
            bridgeLog(QStringLiteral("FT8 early predecode skipped: %1 nzhsym=%2 ageMs=%3 threads=%4 guardMs=%5")
                          .arg(reason)
                          .arg(nzhsym)
                          .arg(monitoringAgeMs)
                          .arg(earlyFtThreadBudget)
                          .arg(qMax<qint64>(0, m_ft8StartupEarlyDecodeGuardUntilMs - nowMs)));
        }
        return;
    }

    QString ft8BacklogDetail;
    qint64 const backlogNowMs = QDateTime::currentMSecsSinceEpoch();
    if (ft8LiveDecodeBacklogActive(backlogNowMs, 9000, 0, &ft8BacklogDetail)) {
        if (mark41) m_ft8EarlyDecode41Sent = true;
        if (mark47) m_ft8EarlyDecode47Sent = true;
        drainFt8LiveDecodeBacklog(0, QStringLiteral("early-skip %1").arg(ft8BacklogDetail));
        return;
    }

    int const effectiveDepth = effectiveDecodeDepth();
    // Early preview = la passata TIMELY (gira intra-slot, prima della fine slot).
    // Con Deep Search la portiamo a depth 3 (syncmin 6 vs 8, budget candidati 2400
    // vs 1000): cattura piu' segnali deboli ENTRO la finestra timely. Restiamo a
    // depth 3 e non 4 perche' il budget depth-3 (2200ms) entra nei ~2.2s prima che
    // il final pass faccia cancelCurrentDecode; depth 4 (3000ms) verrebbe troncato.
    int const earlyDepthCap = m_deepSearchEnabled ? 3 : 2;
    int const decodeDepth = qMin(effectiveDepth, earlyDepthCap);
    if (decodeDepth <= 1) {
        if (mark41) m_ft8EarlyDecode41Sent = true;
        if (mark47) m_ft8EarlyDecode47Sent = true;
        return;
    }

    QVector<short> audioSnapshot;
    {
        QMutexLocker locker(&m_audioBufferMutex);
        audioSnapshot = m_audioBuffer;
    }

    int const minSamples = (nzhsym == 41) ? 120000 : 132000;
    if (audioSnapshot.size() < minSamples) {
        qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
        if (nowMs - m_lastEarlyDecodeSkipLogMs > 1500) {
            m_lastEarlyDecodeSkipLogMs = nowMs;
            bridgeLog("FT8 early predecode skipped: nzhsym=" + QString::number(nzhsym) +
                      " samples=" + QString::number(audioSnapshot.size()) +
                      " min=" + QString::number(minSamples));
        }
        return;
    }

    quint64 const serial = ++m_decodeSerial;
    int nutc = utcTokenForSlotStart(utcSlot, periodMs);
    QString utcToken = utcDisplayTokenForSlotStart(utcSlot, periodMs);
    if (nutc <= 0) {
        utcToken = QDateTime::currentDateTimeUtc().toString(QStringLiteral("hhmmss"));
        nutc = utcToken.toInt();
    }

    m_decodeModeBySerial.insert(serial, QStringLiteral("FT8"));
    m_decodeUtcTokenBySerial.insert(serial, utcToken);
    m_decodeStartMsBySerial.insert(serial, QDateTime::currentMSecsSinceEpoch());
    m_decodeSessionBySerial.insert(serial, m_decodeSessionId);
    bridgeLog("FT8 early visible decode: serial=" + QString::number(serial) +
              " nzhsym=" + QString::number(nzhsym) +
              " samples=" + QString::number(audioSnapshot.size()) +
              " slot=" + QString::number(utcSlot) +
              " depth=" + QString::number(decodeDepth) +
              (effectiveDepth != decodeDepth
                   ? QStringLiteral(" liveDepthCap=%1").arg(effectiveDepth)
                   : QString()));

    if (m_ft8Worker && mark41) {
        m_ft8Worker->markLatestDecodeSerial(serial);
        m_ft8Worker->cancelCurrentDecode();
        bridgeLog(QStringLiteral("FT8 early visible decode: preempting stale FT8 decode for live slot serial=%1")
                      .arg(serial));
    }
    queueFt8DecodeRequest(audioSnapshot, serial, nutc, utcSlot, decodeDepth,
                           legacyDecodeQsoProgress(), legacyDecodeCqHint(),
                           nzhsym, false, false);
    if (mark41) m_ft8EarlyDecode41Sent = true;
    if (mark47) m_ft8EarlyDecode47Sent = true;
}

