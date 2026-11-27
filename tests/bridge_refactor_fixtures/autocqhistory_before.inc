void DecodiumBridge::clearRecentAutoCqAbandoned(const QString& call,
                                                double freqHz,
                                                const QString& mode,
                                                const QString& reason)
{
    QString const dedupeCall = Radio::base_callsign(call).trimmed().toUpper();
    QString const dedupeBand = autoCqBandKeyForFrequency(freqHz).trimmed().toUpper();
    QString const dedupeMode = (mode.isEmpty() ? m_mode : mode).trimmed().toUpper();
    if (dedupeCall.isEmpty() || dedupeBand.isEmpty() || dedupeBand == QStringLiteral("--") || dedupeMode.isEmpty()) {
        return;
    }

    QString const dedupeKey = QStringLiteral("%1|%2|%3").arg(dedupeCall, dedupeBand, dedupeMode);
    if (m_recentAutoCqAbandonedUtcByKey.remove(dedupeKey) <= 0) {
        return;
    }

    bridgeLog(QStringLiteral("AutoCQ clear-abandoned: %1 (%2)")
                  .arg(dedupeKey,
                       reason.trimmed().isEmpty() ? QStringLiteral("live caller") : reason.trimmed()));
}

void DecodiumBridge::rememberRecentAutoCqAbandoned(const QString& call, double freqHz, const QString& mode)
{
    QString const dedupeCall = Radio::base_callsign(call).trimmed().toUpper();
    QString const dedupeBand = autoCqBandKeyForFrequency(freqHz).trimmed().toUpper();
    QString const dedupeMode = mode.trimmed().toUpper();
    if (dedupeCall.isEmpty() || dedupeBand.isEmpty() || dedupeBand == QStringLiteral("--") || dedupeMode.isEmpty()) {
        return;
    }

    QDateTime const nowUtc = QDateTime::currentDateTimeUtc();
    QString const dedupeKey = QStringLiteral("%1|%2|%3").arg(dedupeCall, dedupeBand, dedupeMode);
    m_recentAutoCqAbandonedUtcByKey.insert(dedupeKey, nowUtc);
    pruneRecentDuplicateCache(m_recentAutoCqAbandonedUtcByKey, nowUtc);
    bridgeLog(QStringLiteral("AutoCQ remember-abandoned: %1").arg(dedupeKey));
}

void DecodiumBridge::rememberRecentAutoCqWorked(const QString& call, double freqHz, const QString& mode)
{
    QString const dedupeCall = Radio::base_callsign(call).trimmed().toUpper();
    QString const dedupeBand = autoCqBandKeyForFrequency(freqHz).trimmed().toUpper();
    QString const dedupeMode = mode.trimmed().toUpper();
    if (dedupeCall.isEmpty() || dedupeBand.isEmpty() || dedupeBand == QStringLiteral("--") || dedupeMode.isEmpty()) {
        return;
    }

    QDateTime const nowUtc = QDateTime::currentDateTimeUtc();
    QString const dedupeKey = QStringLiteral("%1|%2|%3").arg(dedupeCall, dedupeBand, dedupeMode);
    m_recentAutoCqWorkedUtcByKey.insert(dedupeKey, nowUtc);
    pruneRecentDuplicateCache(m_recentAutoCqWorkedUtcByKey, nowUtc);
    bridgeLog(QStringLiteral("AutoCQ remember-worked: %1").arg(dedupeKey));
}

void DecodiumBridge::rememberCompletedAutoCqPartner(const QString& call, bool logged, const QString& reason)
{
    QString const completedBase = Radio::base_callsign(call).trimmed().toUpper();
    if (completedBase.isEmpty()) {
        return;
    }

    m_qsoCooldown[completedBase] = QDateTime::currentMSecsSinceEpoch();

    if (m_autoCqRepeat || m_multiAnswerMode) {
        if (logged) {
            rememberRecentAutoCqWorked(completedBase, m_frequency, m_mode);
        } else {
            rememberRecentAutoCqAbandoned(completedBase, m_frequency, m_mode);
        }
        removeCallerFromQueue(completedBase);
    }

    bridgeLog(QStringLiteral("AutoCQ completed partner guard: %1 logged=%2 (%3)")
                  .arg(completedBase)
                  .arg(logged ? 1 : 0)
                  .arg(reason));
}

