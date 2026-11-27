void DecodiumBridge::armCqAutoReplyWindow(const QString& reason)
{
    QTime const nowUtc = QDateTime::currentDateTimeUtc().time();
    m_cqAutoReplyArmSecond = nowUtc.msecsSinceStartOfDay() / 1000;
    m_cqAutoReplyArmWallMs = QDateTime::currentMSecsSinceEpoch();
    bridgeLog(QStringLiteral("CQ auto-reply window armed: utc_s=%1 reason=%2")
                  .arg(m_cqAutoReplyArmSecond)
                  .arg(reason));
}

void DecodiumBridge::clearCqAutoReplyWindow(const QString& reason)
{
    if (m_cqAutoReplyArmSecond < 0 && m_cqAutoReplyArmWallMs <= 0) {
        return;
    }
    bridgeLog(QStringLiteral("CQ auto-reply window cleared: reason=%1").arg(reason));
    m_cqAutoReplyArmSecond = -1;
    m_cqAutoReplyArmWallMs = 0;
}

bool DecodiumBridge::decodeIsFreshForCqAutoReply(const QString& utcToken, QString* reason) const
{
    if (m_cqAutoReplyArmSecond < 0) {
        return true;
    }

    int const decodeSeconds = secondsFromUtcDisplayToken(utcToken);
    if (decodeSeconds < 0) {
        return true;
    }

    int const afterArmSeconds = signedUtcSecondDelta(m_cqAutoReplyArmSecond, decodeSeconds);
    // 1.0.391 - tolleranza predates per FT2 async. L'arm e' fissato all'attivazione AutoCQ
    // (armCqAutoReplyWindow: autocq-enabled ~13168 / tx6-rearm manuale ~13057) e troncato al
    // SECONDO UTC. In FT2 gli slot cadono su confini a passo 3.75s (sub-secondo), quindi lo
    // slot-start della risposta del partner, troncato al secondo, puo' cadere PRIMA del
    // secondo di arm di ~1 periodo -> la risposta LEGITTIMA al nostro CQ veniva scartata come
    // stale (caso reale ZL3DMH<-ZL1BW: predates CQ arm by 4s ~= un periodo FT2 -> bloccato a
    // ripetere CQ, 0 QSO). Accetto i decode che precedono l'arm fino a ~1 periodo + 2s; la
    // staleness REALE resta coperta dal controllo di eta' (ageSeconds > maxAgeSeconds) sotto,
    // riferito a ora. FT8/FT4 (e FT2 sync) invariati: tolleranza 0 -> reject identico.
    int const armPeriodMs = periodMsForMode(m_mode);
    int const armPeriodSeconds = qMax(1, (armPeriodMs + 999) / 1000);
    int const predatesToleranceSeconds =
        (m_mode == QStringLiteral("FT2") && m_asyncTxEnabled) ? (armPeriodSeconds + 2) : 0;
    if (afterArmSeconds < -predatesToleranceSeconds) {
        if (reason) {
            *reason = QStringLiteral("decode predates CQ arm by %1s (toll %2s)")
                          .arg(-afterArmSeconds).arg(predatesToleranceSeconds);
        }
        return false;
    }

    int const nowSeconds =
        QDateTime::currentDateTimeUtc().time().msecsSinceStartOfDay() / 1000;
    int const ageSeconds = signedUtcSecondDelta(decodeSeconds, nowSeconds);
    int const periodMs = periodMsForMode(m_mode);
    int const periodSeconds = qMax(1, (periodMs + 999) / 1000);
    // NOTA asimmetria intenzionale: la tolleranza predates sopra vale per FT2 async
    // anche SENZA m_autoCqRepeat, mentre il maxAge stretto (6s) richiede autoCqRepeat.
    // Nel TX6-rearm manuale async quindi un decode fino a ~20s di eta' che precede
    // l'arm di <=6s viene accettato: e' il flusso "il partner mi ha chiamato pochi
    // secondi fa, ri-armo il CQ e gli rispondo" — voluto, non stringere.
    int const maxAgeSeconds =
        (m_mode == QStringLiteral("FT2") && m_asyncTxEnabled && m_autoCqRepeat)
            ? qMax(12, periodSeconds * 2 + 4)
            : qMax(20, periodSeconds * 2 + 5);
    if (ageSeconds > maxAgeSeconds) {
        if (reason) {
            *reason = QStringLiteral("decode age %1s > %2s").arg(ageSeconds).arg(maxAgeSeconds);
        }
        return false;
    }

    return true;
}
