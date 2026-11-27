void DecodiumBridge::removeCallerFromQueue(const QString& call)
{
    QString const targetBase = Radio::base_callsign(call).trimmed().toUpper();
    if (targetBase.isEmpty() || m_callerQueue.isEmpty()) {
        return;
    }

    bool removed = false;
    QStringList remaining;
    remaining.reserve(m_callerQueue.size());
    for (QString const& entry : m_callerQueue) {
        QStringList const parts = entry.split(' ', Qt::SkipEmptyParts);
        QString const entryBase = parts.isEmpty()
            ? QString {}
            : Radio::base_callsign(parts.first()).trimmed().toUpper();
        if (!entryBase.isEmpty() && entryBase == targetBase) {
            removed = true;
            continue;
        }
        remaining.push_back(entry);
    }

    if (removed) {
        m_callerQueue = remaining;
        emit callerQueueChanged();
    }
}

QString DecodiumBridge::activeMamCallerBase() const
{
    QString activePartner = m_dxCall.trimmed();
    if (activePartner.isEmpty()) {
        activePartner = m_autoCqLockedCall.trimmed();
    }
    if (activePartner.isEmpty()) {
        activePartner = inferredPartnerForAutolog().trimmed();
    }
    return normalizedBaseCall(activePartner);
}

void DecodiumBridge::removeActiveMamCallerFromQueue(const QString& reason)
{
    if ((!m_multiAnswerMode && !m_autoCqRepeat) || m_callerQueue.isEmpty()) {
        return;
    }

    QString const activeBase = activeMamCallerBase();
    if (activeBase.isEmpty()) {
        return;
    }

    int const before = m_callerQueue.size();
    removeCallerFromQueue(activeBase);
    if (m_callerQueue.size() != before) {
        bridgeLog(QStringLiteral("MAM queue: removed active caller %1 (%2)")
                      .arg(activeBase, reason));
    }
}

void DecodiumBridge::maybeEnqueueMamCallerFromDecode(const QStringList& fields,
                                                     bool markCompletedSignoff)
{
    if ((!m_multiAnswerMode && !m_autoCqRepeat) || m_manualTxHold || fields.size() < 5) {
        return;
    }
    if (shouldSuppressDirectedGhostDecode(fields, QStringLiteral("mam-queue"))) {
        return;
    }

    QString const msg = fields.value(4).trimmed();
    bool freqOk = false;
    int const freq = fields.size() >= 8 ? fields.value(7).toInt(&freqOk) : 0;
    int const snr = decodeSnrOrDefault(fields.value(1), -99);
    maybeEnqueueMamCallerFromMessage(msg, freqOk ? freq : -1, snr, markCompletedSignoff);
}

void DecodiumBridge::maybeEnqueueMamCallerFromMessage(const QString& message,
                                                      int freq,
                                                      int snr,
                                                      bool markCompletedSignoff)
{
    if ((!m_multiAnswerMode && !m_autoCqRepeat) || m_manualTxHold) {
        return;
    }

    QString const msg = message.trimmed();
    QStringList const parts = msg.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() < 2) {
        return;
    }

    QString const myCallUpper = m_callsign.trimmed().toUpper();
    QString const myBaseUpper = normalizedBaseCall(myCallUpper);
    if (myCallUpper.isEmpty()) {
        return;
    }

    if (!tokenMatchesCall(parts.first(), myCallUpper, myBaseUpper)) {
        return;
    }

    QString activePartner = m_dxCall.trimmed();
    if (activePartner.isEmpty()) {
        activePartner = m_autoCqLockedCall.trimmed();
    }
    if (activePartner.isEmpty()) {
        activePartner = inferredPartnerForAutolog().trimmed();
    }
    QString const activePartnerBase = normalizedBaseCall(activePartner);

    QString const partner = inferPartnerFromDirectedMessage(msg, myCallUpper, myBaseUpper);
    QString const partnerBase = normalizedBaseCall(partner);
    if (partnerBase.isEmpty() || partnerBase == myBaseUpper) {
        return;
    }
    QString const activeMamBase = activeMamCallerBase();

    QString const lastToken = normalizeCallToken(parts.last()).trimmed().toUpper();
    bool const directedFinalSignoff =
        lastToken == QStringLiteral("73")
        || lastToken == QStringLiteral("RR73")
        || lastToken == QStringLiteral("RRR");
    if (directedFinalSignoff) {
        removeCallerFromQueue(partnerBase);
        if ((!activePartnerBase.isEmpty() && partnerBase == activePartnerBase)
            || (!activeMamBase.isEmpty() && partnerBase == activeMamBase)) {
            bridgeLog(QStringLiteral("MAM queue: active signoff left to auto-seq %1 msg=%2")
                          .arg(partnerBase, msg));
            return;
        }
        if (markCompletedSignoff) {
            m_qsoCooldown[partnerBase] = QDateTime::currentMSecsSinceEpoch();
            rememberRecentAutoCqWorked(partnerBase, m_frequency, m_mode);
            bridgeLog(QStringLiteral("MAM queue: mark completed %1 from directed signoff msg=%2")
                          .arg(partnerBase, msg));
        } else {
            bridgeLog(QStringLiteral("MAM queue: skip directed signoff %1 msg=%2")
                          .arg(partnerBase, msg));
        }
        return;
    }

    if (isRecentAutoCqDuplicate(partnerBase, m_frequency, m_mode)) {
        removeCallerFromQueue(partnerBase);
        bridgeLog(QStringLiteral("MAM queue: skip recently completed or abandoned partner %1 msg=%2")
                      .arg(partnerBase, msg));
        return;
    }

    if ((!activePartnerBase.isEmpty() && partnerBase == activePartnerBase)
        || (!activeMamBase.isEmpty() && partnerBase == activeMamBase)) {
        removeCallerFromQueue(partnerBase);
        return;
    }

    if (enqueueCallerInternal(partnerBase, freq, snr, true)) {
        bridgeLog(QStringLiteral("MAM queue: enqueued %1 from directed decode while active=%2 msg=%3")
                      .arg(partnerBase, activePartnerBase.isEmpty() ? QStringLiteral("IDLE") : activePartnerBase, msg));
        // 1.0.364+ MAM multi-stream nativo (FASE 2): in MAM la promozione dei
        // caller la fa mamPromoteFromQueue() dentro mamDispatchPeriod; qui NON
        // chiamiamo processNextInQueue (avvierebbe il path single-QSO).
        if (activePartnerBase.isEmpty() && (m_autoCqRepeat || m_txEnabled || m_currentTx == 6)
            && !mamMultiStreamSequencerActive()) {
            processNextInQueue();
        }
    }
}


