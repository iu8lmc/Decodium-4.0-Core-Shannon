void DecodiumBridge::scheduleSyncTxBoundaryStop(const QString& reason, quint64 txSerial)
{
    if (!shouldAlignTxAudioToCurrentSyncSlot()) {
        return;
    }
    int const periodMs = effectivePeriodMsForMode(m_mode);
    if (periodMs <= 0) {
        return;
    }

    qint64 const nowMs = correctedUtcEpochMs();
    int const elapsedMs = static_cast<int>(nowMs % static_cast<qint64>(periodMs));
    int delayMs = periodMs - elapsedMs + 40;
    if (delayMs <= 40 || delayMs > periodMs + 40) {
        delayMs = periodMs + 40;
    }

    QString const modeSnapshot = m_mode;
    bridgeLog(QStringLiteral("sync TX boundary stop armed: reason=%1 mode=%2 elapsed=%3ms delay=%4ms serial=%5")
                  .arg(reason, modeSnapshot)
                  .arg(elapsedMs)
                  .arg(delayMs)
                  .arg(txSerial));

    QTimer::singleShot(delayMs, this, [this, txSerial, modeSnapshot, reason]() {
        if (!m_transmitting
            || m_tuning
            || m_txPlaybackSerial != txSerial
            || m_mode != modeSnapshot
            || !shouldAlignTxAudioToCurrentSyncSlot()) {
            return;
        }

        bridgeLog(QStringLiteral("sync TX boundary stop: reason=%1 mode=%2 serial=%3")
                      .arg(reason, modeSnapshot)
                      .arg(txSerial));
        m_txPlaybackHoldUntilMs = 0;
        m_txPlaybackHardDeadlineMs = 0;
        if (m_txPcmBuffer && m_txPcmBuffer->isOpen()) {
            m_txPcmBuffer->seek(m_txPcmBuffer->size());
        }
        completeTxPlayback(QStringLiteral("slot-boundary"));
    });
}

bool DecodiumBridge::delayTxFinishUntilPcmConsumed(const QString& context,
                                                   const QString& reason,
                                                   bool useCompletePlayback)
{
    if (!m_transmitting || !m_txPcmBuffer || m_txPcmBuffer->atEnd()) {
        return false;
    }

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    qint64 const hardDeadline = m_txPlaybackHardDeadlineMs;
    if (hardDeadline <= 0 || nowMs < hardDeadline) {
        qint64 const delayMs = hardDeadline > 0
            ? qBound<qint64>(qint64(50), hardDeadline - nowMs, qint64(250))
            : qint64(250);
        if (!m_txPlaybackReleasePending) {
            m_txPlaybackReleasePending = true;
            bridgeLog(QStringLiteral("%1 delayed: TX PCM not drained reason=%2 pos=%3/%4 delay_ms=%5 ft2Link=%6 bridgeLegacy=%7")
                          .arg(context, reason)
                          .arg(m_txPcmBuffer->pos())
                          .arg(m_txPcmBuffer->size())
                          .arg(delayMs)
                          .arg(m_ft2LinkTxActive ? 1 : 0)
                          .arg(m_bridgeAudioLegacyTxActive ? 1 : 0));
            QTimer::singleShot(delayMs, this, [this, reason, useCompletePlayback]() {
                m_txPlaybackReleasePending = false;
                if (useCompletePlayback) {
                    completeTxPlayback(reason + QStringLiteral("-buffer"));
                } else {
                    finishModulatorIdlePlayback(reason + QStringLiteral("-buffer"));
                }
            });
        }
        return true;
    }

    bridgeLog(QStringLiteral("%1 forcing incomplete TX PCM after hard deadline: reason=%2 pos=%3/%4 ft2Link=%5 bridgeLegacy=%6")
                  .arg(context, reason)
                  .arg(m_txPcmBuffer->pos())
                  .arg(m_txPcmBuffer->size())
                  .arg(m_ft2LinkTxActive ? 1 : 0)
                  .arg(m_bridgeAudioLegacyTxActive ? 1 : 0));
    return false;
}

