void DecodiumBridge::mamPruneSlots()
{
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    // MAM does not enter the legacy auto-sequencer's periodic cleanup.  Keep
    // its shared signoff cooldown bounded here as well, otherwise an entry
    // made hours earlier silently rejects a perfectly valid new caller.
    for (auto it = m_qsoCooldown.begin(); it != m_qsoCooldown.end();) {
        if (nowMs - it.value() > kQsoCooldownWindowMs) {
            m_postLogReengageCount.remove(it.key());
            it = m_qsoCooldown.erase(it);
        } else {
            ++it;
        }
    }
    bool lateQueueChanged = false;
    for (auto it = m_mamLateReportRecoveries.begin();
         it != m_mamLateReportRecoveries.end();) {
        if (it->expiresMs <= nowMs) {
            bridgeLog(QStringLiteral("MAM late report recovery expired: %1")
                          .arg(it->slot.callFull));
            lateQueueChanged = lateQueueChanged || it->readyForSlot;
            it = m_mamLateReportRecoveries.erase(it);
        } else {
            ++it;
        }
    }
    if (m_mamSlots.isEmpty()) {
        if (lateQueueChanged) {
            emit callerQueueChanged();
        }
        return;
    }
    qint64 const periodMs = qMax<qint64>(1, periodMsForMode(QStringLiteral("FT8")));
    int const maxRetry = maxCallerRetries();
    int removed = 0;
    for (int i = m_mamSlots.size() - 1; i >= 0; --i) {
        MamQsoSlot& s = m_mamSlots[i];
        bool drop = false;
        QString dropReason;
        if (s.logged || s.state == MamQsoSlot::State::Done) {
            drop = true;
        } else if (s.weakFinalAck) {
            drop = true;
            dropReason = QStringLiteral("low-confidence 73 after our RR73");
        } else if (s.retryCount > maxRetry) {
            drop = true;
            dropReason = QStringLiteral("retry limit %1").arg(maxRetry);
        } else if (s.lastHeardMs > 0 && s.lastTxSlotMs == 0 && (nowMs - s.lastHeardMs) > 12 * periodMs) {
            // Never called: the power budget kept it waiting. It gets longer than a
            // station we are already working before it is given up.
            drop = true;
            dropReason = QStringLiteral("never called, waited > 12 periods");
        } else if (s.lastHeardMs > 0 && s.lastTxSlotMs > 0
                   && (nowMs - qMax(s.lastHeardMs, s.lastTxSlotMs)) > 6 * periodMs) {
            // The silence is counted from the last time we heard it OR called it:
            // a station promoted from the queue and called late (24/10, 25/10 on the
            // air) used to be dropped as silent right after its first call.
            drop = true;
            dropReason = QStringLiteral("silent > 6 periods");
        }
        if (drop && !dropReason.isEmpty() && !s.logged) {
            // 1.0.724: a dropped exchange is remembered as abandoned for the duplicate window, so the
            // next decode from that station does not reopen it at once (I5IIL, LA1K, SP9SIL, 28/10).
            rememberRecentAutoCqAbandoned(s.call, m_frequency, m_mode);
            MamResumeCarry carry;
            carry.lateResumes = s.lateResumes;
            carry.expiresMs = nowMs + 10 * 60 * 1000;
            m_mamResumeCarry.insert(s.call, carry);
        }
        if (drop && !dropReason.isEmpty()) {
            // A sent RR73 is not evidence that the partner received it. Do
            // not log on retry/silence: retain the exchange briefly so only a
            // subsequent decoded final 73 from this same station can close
            // and log it.
            // The partner acknowledged our report (progress >= 4), we sent RR73 and
            // then it went silent or kept repeating: with "log RR73 when the
            // partner leaves" on, that is a complete QSO from the Fox's side.
            // On the air a station got 12 RR73 and the contact was never logged.
            if (!s.logged && m_ft2LogRr73OnPartnerLeft && s.currentTx == 4
                && s.progress >= 4 && s.lastNtx == 4) {
                mamLogSlot(s);
                bridgeLog(QStringLiteral("MAM slot %1 logged on RR73 (partner left)").arg(s.callFull));
            }
            bool retainedForLateReport = false;
            if (!s.logged && s.progress >= 2 && s.progress <= 4) {
                MamLateReportRecovery recovery;
                recovery.slot = s;
                recovery.slot.lastNtx = -1;
                recovery.slot.retryCount = 0;
                // With callers queued the exchange waits for its final 73 for half the time:
                // a recovered station is promoted ahead of the queue (R6AC, F4IIX waited 2-3 min).
                int const retainSecs = m_callerQueue.isEmpty() ? 120 : 60;
                recovery.expiresMs = nowMs + qint64(retainSecs) * 1000;
                m_mamLateReportRecoveries.insert(s.call, recovery);
                retainedForLateReport = true;
                bridgeLog(QStringLiteral("MAM slot %1 retained pending final 73 (%2s)")
                              .arg(s.callFull).arg(retainSecs));
            }
            // The message used to say "dropped without log" even when the line just above had
            // logged the QSO (LZ1DD, EA1FJD, EB3JT...: 21 "dropped", 8 really not logged).
            if (s.logged) {
                bridgeLog(QStringLiteral("MAM slot %1 closed, QSO logged (%2)")
                              .arg(s.callFull, dropReason));
            } else if (retainedForLateReport) {
                // 1.0.725: not a failure: the exchange is kept for a late report or final 73
                bridgeLog(QStringLiteral("MAM slot %1 held for a late report, not logged yet: %2")
                              .arg(s.callFull, dropReason));
            } else {
                bridgeLog(QStringLiteral("MAM slot %1 dropped without log: %2")
                              .arg(s.callFull, dropReason));
            }
        }
        if (drop) {
            m_mamSlots.removeAt(i);
            ++removed;
        }
    }
    if (removed > 0) {
        bridgeLog(QStringLiteral("MAM prune: removed %1 slot(s), %2 active").arg(removed).arg(m_mamSlots.size()));
        emit mamActiveSlotsChanged();
    }
    if (lateQueueChanged) {
        emit callerQueueChanged();
    }
}

