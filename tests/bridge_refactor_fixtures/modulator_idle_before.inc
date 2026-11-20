void DecodiumBridge::finishModulatorIdlePlayback(const QString& reason)
{
    if (sstvTxActive()) {
        bridgeLog(QStringLiteral(
            "finishModulatorIdlePlayback ignored while native SSTV owns TX: %1")
                      .arg(reason));
        return;
    }
    QElapsedTimer totalTimer;
    totalTimer.start();
    qint64 pttOffMs = 0;
    qint64 soundFinishMs = 0;
    qint64 cleanupMs = 0;
    qint64 resumeMs = 0;
    bridgeLog("finishModulatorIdlePlayback: reason=" + reason +
              " transmitting=" + QString::number(m_transmitting) +
              " tuning=" + QString::number(m_tuning));

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_transmitting && m_txPlaybackHoldUntilMs > nowMs) {
        qint64 const delayMs = qMax<qint64>(1, m_txPlaybackHoldUntilMs - nowMs);
        if (!m_txPlaybackReleasePending) {
            m_txPlaybackReleasePending = true;
            bridgeLog(QStringLiteral("finishModulatorIdlePlayback delayed: reason=%1 hold_ms=%2")
                          .arg(reason).arg(delayMs));
            QTimer::singleShot(delayMs, this, [this, reason]() {
                m_txPlaybackReleasePending = false;
                finishModulatorIdlePlayback(reason + QStringLiteral("-held"));
            });
        }
        return;
    }

    if (delayTxFinishUntilPcmConsumed(QStringLiteral("finishModulatorIdlePlayback"),
                                      reason,
                                      false)) {
        return;
    }

    m_txPlaybackReleasePending = false;
    m_txPlaybackHoldUntilMs = 0;
    m_txPlaybackHardDeadlineMs = 0;
    m_txAudioRestartPending = false;
    bool const wasBridgeLegacyTx = m_bridgeAudioLegacyTxActive;
    m_bridgeAudioLegacyTxActive = false;

    QElapsedTimer phaseTimer;
    if (wasBridgeLegacyTx) {
        phaseTimer.start();
        requestLegacyPttOffOnce(QStringLiteral("finish:%1").arg(reason));
        pttOffMs = phaseTimer.elapsed();
    } else if (activeCatCanPtt(this, m_hamlibCat, m_catBackend, m_omniRigCat, m_legacyBackend)) {
        phaseTimer.start();
        activeCatSetPtt(this, m_hamlibCat, m_catBackend, false, m_omniRigCat, m_legacyBackend);
        pttOffMs = phaseTimer.elapsed();
    }

    if (m_soundOutput) {
        phaseTimer.start();
        m_soundOutput->finishPlayback();
        soundFinishMs = phaseTimer.elapsed();
    }
    phaseTimer.start();
    if (m_txPcmBuffer) {
        m_txPcmBuffer->close();
        delete m_txPcmBuffer;
        m_txPcmBuffer = nullptr;
        m_txPcmData.clear();
    }

    bool const wasTransmitting = m_transmitting || wasBridgeLegacyTx;
    bool const wasTuning = m_tuning;
    int const finishedTx = m_activeTxNumber;
    if (m_ft2LinkTxActive) {
        qint64 const guardNowMs = QDateTime::currentMSecsSinceEpoch();
        m_ft2LinkPostTxAckGuardUntilMs = guardNowMs + 15000;
        m_ft2LinkTxActive = false;
        m_pendingFt2LinkText.clear();
        m_pendingFt2LinkWave.clear();
        m_pendingFt2LinkPlan.clear();
        emit ft2LinkTxFinished();

        if (m_tuning) {
            m_tuning = false;
            emit tuningChanged();
        } else if (wasTransmitting) {
            m_transmitting = false;
            emit transmittingChanged();
            emit statusMessage(QStringLiteral("FT2-Link TX completato"));
        }

        cleanupMs = phaseTimer.elapsed();
        phaseTimer.start();
        restoreTxAudioSchedulingBoost(reason);
        resumeRxAudioAfterTx(reason);
        resumeNonAudioTxWork(reason);
        scheduleIdleAudioBufferRelease(180000);
        resumeMs = phaseTimer.elapsed();
        txTimelineLog(QStringLiteral("[TX-TL] tx_finish_ft2link reason=%1 total_ms=%2 ptt_off_ms=%3 sound_finish_ms=%4 cleanup_ms=%5 resume_ms=%6 was_transmitting=%7")
                      .arg(reason)
                      .arg(totalTimer.elapsed())
                      .arg(pttOffMs)
                      .arg(soundFinishMs)
                      .arg(cleanupMs)
                      .arg(resumeMs)
                      .arg(wasTransmitting ? 1 : 0));
        if (wasBridgeLegacyTx) clearLegacyPttTransition(QStringLiteral("finish-ft2link"));
        return;
    }
    noteTxPlaybackFinished(reason, false);

    if (m_tuning) {
        m_tuning = false;
        emit tuningChanged();
        emit statusMessage(tr("Tune finished"));
    } else if (wasTransmitting) {
        m_transmitting = false;
        emit transmittingChanged();
        emit statusMessage(tr("Transmission finished"));
    }
    cleanupMs = phaseTimer.elapsed();

    phaseTimer.start();
    resumeRxAudioAfterTx(reason);
    resumeNonAudioTxWork(reason);
    if (wasTransmitting && !wasTuning) {
        foxElasticOnTxFinished();
    }
    scheduleIdleAudioBufferRelease(180000);
    resumeMs = phaseTimer.elapsed();
    txTimelineLog(QStringLiteral("[TX-TL] tx_finish reason=%1 total_ms=%2 ptt_off_ms=%3 sound_finish_ms=%4 cleanup_ms=%5 resume_ms=%6 bridge_legacy=%7 was_transmitting=%8")
                  .arg(reason)
                  .arg(totalTimer.elapsed())
                  .arg(pttOffMs)
                  .arg(soundFinishMs)
                  .arg(cleanupMs)
                  .arg(resumeMs)
                  .arg(wasBridgeLegacyTx ? 1 : 0)
                  .arg(wasTransmitting ? 1 : 0));
    bool const appliedDeferredAutoSeqAfterActiveTx =
        wasTransmitting && applyPendingAutoSeqTxAfterCompletedTx(finishedTx);
    if (wasTransmitting && !appliedDeferredAutoSeqAfterActiveTx) {
        armFt2AutoCqOneShotAfterCompletedTx(finishedTx, reason);
    }
    if (wasBridgeLegacyTx) clearLegacyPttTransition(QStringLiteral("finish:%1").arg(reason));
}
