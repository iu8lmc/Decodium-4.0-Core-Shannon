void DecodiumBridge::mamPromoteFromQueue()
{
    // Signoffs proven by a late R+report have priority over new callers and
    // retain their original exchange state; the compact normal queue does not.
    mamPromoteLateReportRecoveries();
    rankCallerQueue(QStringLiteral("mam-promote"));
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    int const slotsBefore = m_mamSlots.size();
    while (m_mamSlots.size() < m_mamMaxStreams && !m_callerQueue.isEmpty()) {
        QString const entry = m_callerQueue.constFirst();
        QStringList const parts = entry.split(' ', Qt::SkipEmptyParts);
        if (parts.isEmpty()) {
            m_callerQueue.takeFirst();
            emit callerQueueChanged();
            continue;
        }
        QString const base = normalizedBaseCall(parts.first());
        if (base.isEmpty() || base == normalizedBaseCall(m_callsign)) {
            m_callerQueue.takeFirst();
            emit callerQueueChanged();
            continue;
        }
        if (mamSlotIndexForCall(base) >= 0
            || isRecentAutoCqDuplicate(base, m_frequency, m_mode)
            || m_qsoCooldown.contains(base)) {
            m_callerQueue.takeFirst();
            emit callerQueueChanged();
            continue;
        }
        int const qFreq = parts.size() >= 2 ? parts.at(1).toInt() : m_rxFrequency;
        int const qSnr  = parts.size() >= 3 ? qBound(-50, parts.at(2).toInt(), 49) : -10;
        MamQsoSlot s;
        s.call = base;
        s.callFull = base; // dalla coda abbiamo solo la base
        int const fHz = qBound(200, qFreq > 0 ? qFreq : m_rxFrequency, 4000);
        bool frequencyBusy = false;
        for (MamQsoSlot const& other : m_mamSlots) {
            if (qAbs(other.audioFreqHz - fHz) < mamMinStreamGapHz()) {
                frequencyBusy = true;
                break;
            }
        }
        if (frequencyBusy) {
            bridgeLog(QStringLiteral("MAM queue waiting for caller frequency: %1 @ %2Hz")
                          .arg(base)
                          .arg(fHz));
            break;
        }
        m_callerQueue.takeFirst();
        emit callerQueueChanged();
        s.audioFreqHz = fHz;
        s.progress = 2;
        s.currentTx = 2;
        // -10 is only the placeholder given to a station picked by a click: no
        // real figure for its link, so the power budget must treat it as unknown.
        s.partnerSnrDb = (qSnr == -10) ? 127 : qSnr;
        s.reportSent = formatSignalReport(qSnr);
        s.reportReceived = QStringLiteral("-10");
        s.startedOnMs = nowMs;
        s.lastHeardMs = nowMs;
        m_mamSlots.append(s);
        bridgeLog(QStringLiteral("MAM slot promoted from queue: %1 freq=%2Hz (slots=%3)")
                      .arg(s.callFull).arg(s.audioFreqHz).arg(m_mamSlots.size()));
    }
    if (m_mamSlots.size() != slotsBefore) {
        emit mamActiveSlotsChanged();
    }
}

