void DecodiumBridge::mamPromoteLateReportRecoveries()
{
    if (m_mamSlots.size() >= m_mamMaxStreams || m_mamLateReportRecoveries.isEmpty()) {
        return;
    }
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    int promoted = 0;
    bool queueChanged = false;
    for (auto it = m_mamLateReportRecoveries.begin();
         it != m_mamLateReportRecoveries.end() && m_mamSlots.size() < m_mamMaxStreams;) {
        if (it->expiresMs <= nowMs) {
            it = m_mamLateReportRecoveries.erase(it);
            continue;
        }
        if (!it->readyForSlot) {
            ++it;
            continue;
        }
        if (isRecentMamWorkedOrLoggedDuplicate(it->slot.call, m_frequency, m_mode)) {
            // 1.0.724: already logged recently: do not promote a copy of the exchange (R4AGO, 28/10)
            it = m_mamLateReportRecoveries.erase(it);
            continue;
        }
        MamQsoSlot restored = it->slot;
        bool frequencyBusy = false;
        for (MamQsoSlot const& other : m_mamSlots) {
            if (qAbs(other.audioFreqHz - restored.audioFreqHz) < mamMinStreamGapHz()) {
                frequencyBusy = true;
                break;
            }
        }
        if (frequencyBusy) {
            ++it;
            continue;
        }
        QString const call = restored.callFull;
        queueChanged = queueChanged || it->readyForSlot;
        it = m_mamLateReportRecoveries.erase(it);
        m_mamSlots.append(restored);
        ++promoted;
        bridgeLog(QStringLiteral("MAM deferred reply promoted: %1 -> TX%2 (slots=%3)")
                      .arg(call)
                      .arg(restored.currentTx)
                      .arg(m_mamSlots.size()));
    }
    if (promoted > 0) {
        emit mamActiveSlotsChanged();
    }
    if (queueChanged) {
        emit callerQueueChanged();
    }
}

