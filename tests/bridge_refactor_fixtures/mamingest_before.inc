void DecodiumBridge::mamIngestDecode(const QStringList& f)
{
    if (f.size() < 5) {
        return;
    }
    QString const myCallUpper = m_callsign.trimmed().toUpper();
    QString const myBaseUpper = normalizedBaseCall(myCallUpper);
    if (myBaseUpper.isEmpty()) {
        return;
    }
    QString const message = f.at(4);
    // Solo i decode diretti al mio call alimentano il sequencer.
    if (!messageContainsCallToken(message, myCallUpper, myBaseUpper)) {
        return;
    }
    // L'eco della nostra stessa trasmissione (la radio la risente a +20/+28 dB e
    // il decoder la legge) contiene il nostro nominativo ma e' DA noi, non per
    // noi. Passando di qui faceva avanzare lo slot come se il corrispondente
    // avesse risposto: il report dell'eco diventava il suo, e il QSO si
    // chiudeva e si loggava con un corrispondente che aveva mandato solo il grid.
    if (messageIsSentByCall(message, myCallUpper, myBaseUpper)) {
        return;
    }
    QString const partner = inferPartnerFromDirectedMessage(message, myCallUpper, myBaseUpper);
    QString const partnerBase = normalizedBaseCall(partner);
    if (partnerBase.isEmpty() || partnerBase == myBaseUpper) {
        return;
    }
    // Only a LOW-CONFIDENCE signoff ('?') is refused. Since 1.0.715 this test ran on every decode,
    // so in MAM every clean 73/RR73 from a partner was thrown away too: no slot could ever be closed
    // by its partner's 73 and it stayed until the RR73 retry limit (T77RN, 28/10/2026).
    if (message.trimmed().endsWith(QLatin1Char('?'))
        && !decodium::seq::mayFeedRescuedDecodeToSequencer(message)) {
        bridgeLog(QStringLiteral("MAM: low-confidence signoff from %1 ignored, it cannot close a QSO: %2")
                      .arg(partnerBase, message));
        // It cannot close the QSO, but a 73 after our RR73 from a station that acknowledged our
        // report means it has stopped waiting: free the slot instead of repeating RR73 up to the
        // retry limit while callers queue (LZ1DD, 28/10/2026: RR73 x4, 11 of 20 QSOs).
        int const weakIdx = mamSlotIndexForCall(partnerBase);
        if (weakIdx >= 0) {
            MamQsoSlot& weak = m_mamSlots[weakIdx];
            if (!weak.logged && !weak.weakFinalAck
                && decodium::seq::weakSignoffEndsRr73Repeats(message, weak.progress, weak.currentTx,
                                                              weak.lastNtx, weak.retryCount,
                                                              !weak.reportReceived.trimmed().isEmpty())) {
                weak.weakFinalAck = true;
                bridgeLog(QStringLiteral("MAM slot %1: low-confidence 73 after our RR73 x%2, no more RR73")
                              .arg(weak.callFull).arg(weak.retryCount));
            }
        }
        return;
    }
    if (m_ft2ElasticFox) {
        m_foxElastic.decodeHeard(partnerBase.toStdString(), QDateTime::currentMSecsSinceEpoch(),
                                 !message.trimmed().endsWith(QLatin1Char('?')));
    }

    // Anti-riapertura: stazioni gia' lavorate di recente o in cooldown non
    // devono riaprire uno slot (eviterebbe loop su 73 ripetuti).
    // 1.0.731: per-call limit: no new slot for a call that already reached it today
    if (qsoLimitReached(partnerBase, m_frequency, m_mode)) {
        bridgeLog(QStringLiteral("MAM: %1 already has %2 QSO(s) today on this band and mode, not answered")
                      .arg(partnerBase).arg(m_maxQsoPerCall));
        return;
    }
    // 1.0.724: a station whose exchange was just dropped may still send its final 73 or a late
    // report: that is recovery traffic, not a new caller, and is let through.
    QStringList const earlyTokens = normalizedMessageTokens(message);
    QString const earlyLast = earlyTokens.isEmpty() ? QString() : earlyTokens.last().trimmed().toUpper();
    bool const recoveryTraffic = m_mamLateReportRecoveries.contains(partnerBase)
        && (earlyLast == QStringLiteral("73") || earlyLast == QStringLiteral("RR73")
            || earlyLast == QStringLiteral("RRR")
            || QRegularExpression(QStringLiteral("^R?[+-][0-9]+$")).match(earlyLast).hasMatch());
    // 1.0.727: a station worked or logged within the 5-minute window is not reopened by a
    // directed decode, whatever the queue (PA3GHO reopened 3.5 min after its log, 29/10).
    if ((isRecentAutoCqDuplicate(partnerBase, m_frequency, m_mode)
         || isRecentMamWorkedOrLoggedDuplicate(partnerBase, m_frequency, m_mode)
         || m_qsoCooldown.contains(partnerBase))
        && !recoveryTraffic) {
        return;
    }

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();

    // Classifica il token terminale del messaggio (oltre ai due call).
    QStringList const tokens = normalizedMessageTokens(message);
    bool hasGrid = false;
    bool hasPlainReport = false;
    bool hasRogerReport = false;
    bool hasSignoff = false;
    bool isFinal73 = false;
    int  receivedReportDb = 0;
    bool receivedReportValid = false;
    for (QString const& tk : tokens) {
        if (tokenMatchesCall(tk, myCallUpper, myBaseUpper)
            || tokenMatchesCall(tk, partner, partnerBase)) {
            continue;
        }
        if (isGridTokenStrict(tk)) {
            hasGrid = true;
        } else if (isPlainSignalReportToken(tk)) {
            hasPlainReport = true;
            receivedReportDb = decodeSnrOrDefault(tk);
            receivedReportValid = true;
        } else if (isRogerSignalReportToken(tk)) {
            hasRogerReport = true;
            QString num = tk;
            num.remove(0, 1); // strip leading 'R'
            receivedReportDb = decodeSnrOrDefault(num);
            receivedReportValid = true;
        } else if (isQsoSignoffToken(tk)) {
            hasSignoff = true;
            if (tk == QStringLiteral("73")) {
                isFinal73 = true;
            }
        }
    }

    int const decodedSnr = (f.size() >= 2) ? decodeSnrOrDefault(f.at(1)) : 127;
    int const partnerFreqHz = (f.size() >= 8)
        ? f.at(7).trimmed().toInt()
        : 0;

    int idx = mamSlotIndexForCall(partnerBase);
    if (idx >= 0) {
        // A new live slot always takes precedence over an older recovery
        // snapshot for the same station.
        m_mamLateReportRecoveries.remove(partnerBase);
    }
    if (idx < 0) {
        // A delayed R+report or final 73 is valid only if it proves that this
        // exact station belongs to an MAM exchange that just expired. A
        // recovery snapshot is the evidence; without it we must never invent
        // a QSO from orphan traffic.
        if ((hasRogerReport && receivedReportValid) || isFinal73 || hasSignoff) {
            for (auto it = m_mamLateReportRecoveries.begin();
                 it != m_mamLateReportRecoveries.end();) {
                if (it->expiresMs <= nowMs) {
                    it = m_mamLateReportRecoveries.erase(it);
                } else {
                    ++it;
                }
            }
            auto recoveryIt = m_mamLateReportRecoveries.find(partnerBase);
            if (recoveryIt != m_mamLateReportRecoveries.end()) {
                MamLateReportRecovery& recovery = recoveryIt.value();
                if (isRecentMamWorkedOrLoggedDuplicate(partnerBase, m_frequency, m_mode)) {
                    // 1.0.724: already in the log: resuming would log it again (R4AGO, 28/10)
                    m_mamLateReportRecoveries.erase(recoveryIt);
                    return;
                }
                if (isFinal73) {
                    MamQsoSlot completed = recovery.slot;
                    completed.lastHeardMs = nowMs;
                    completed.partnerSnrDb = (decodedSnr != 127)
                        ? decodedSnr : completed.partnerSnrDb;
                    m_mamLateReportRecoveries.erase(recoveryIt);
                    mamLogSlot(completed);
                    bridgeLog(QStringLiteral("MAM late final 73: %1 -> logged")
                                  .arg(completed.callFull));
                    emit callerQueueChanged();
                    return;
                }
                if (hasSignoff) {
                    // The partner's RR73/RRR after our R+report closes the exchange from
                    // its side, even though our slot ran out of retries while it was
                    // slow to answer (a person at the key can take 20-40 s). That is as
                    // good as a final 73: log it. Without a report of ours on record
                    // (progress < 3) the signoff cannot be tied to this exchange.
                    if (recovery.slot.progress >= 3) {
                        MamQsoSlot completed = recovery.slot;
                        completed.lastHeardMs = nowMs;
                        completed.partnerSnrDb = (decodedSnr != 127)
                            ? decodedSnr : completed.partnerSnrDb;
                        m_mamLateReportRecoveries.erase(recoveryIt);
                        mamLogSlot(completed);
                        bridgeLog(QStringLiteral("MAM late RR73: %1 -> logged")
                                      .arg(completed.callFull));
                        emit callerQueueChanged();
                    }
                    return;
                }
                // A station that keeps repeating its R+report without ever copying
                // our RR73 must not be answered forever (12 RR73 on the air).
                if (recovery.slot.lateResumes >= 2) {
                    return;
                }
                recovery.slot.lateResumes += 1;
                recovery.slot.lastHeardMs = nowMs;
                recovery.slot.partnerSnrDb = (decodedSnr != 127) ? decodedSnr : recovery.slot.partnerSnrDb;
                recovery.slot.reportReceived = formatSignalReport(receivedReportDb);
                recovery.slot.currentTx = 4; // partner has acknowledged our report: RR73 next
                recovery.slot.progress = 4;
                recovery.slot.lastNtx = -1;
                recovery.slot.retryCount = 0;
                recovery.slot.nTx73 = 0;
                recovery.slot.state = MamQsoSlot::State::Active;
                if (m_mamSlots.size() >= m_mamMaxStreams) {
                    recovery.readyForSlot = true;
                    bridgeLog(QStringLiteral("MAM late report held for free slot: %1 rcvd=%2")
                                  .arg(recovery.slot.callFull, recovery.slot.reportReceived));
                    emit callerQueueChanged();
                } else {
                    MamQsoSlot restored = recovery.slot;
                    m_mamLateReportRecoveries.erase(recoveryIt);
                    m_mamSlots.append(restored);
                    bridgeLog(QStringLiteral("MAM late report resumed: %1 -> TX4 RR73 (slots=%2)")
                                  .arg(restored.callFull)
                                  .arg(m_mamSlots.size()));
                    emit mamActiveSlotsChanged();
                }
                return;
            }
        }
        // A normal first reply contains the caller's grid.  Some FT4 clients
        // use a plain report instead; it remains a valid direct answer even
        // when another MAM stream has been sent in the meantime.  R+reports
        // and signoffs without an existing/recoverable slot stay orphan
        // traffic and must never manufacture a QSO.
        bool const plainReportReply =
            !hasGrid
            && hasPlainReport
            && !hasRogerReport
            && !hasSignoff;
        // 1.0.724: a low-confidence grid ('?') does not open a slot: wait for a clean decode
        // (I5IIL, 28/10: a '?' grid reopened a dropped slot at once).
        if (message.trimmed().endsWith(QLatin1Char('?')) && !plainReportReply) {
            bridgeLog(QStringLiteral("MAM: low-confidence grid from %1 does not open a slot: %2")
                          .arg(partnerBase, message));
            return;
        }
        if (!hasGrid && !plainReportReply) {
            return;
        }
        MamQsoSlot s;
        s.call = partnerBase;
        s.callFull = normalizeCallToken(partner).toUpper();
        for (QString const& tk : tokens) {
            if (isGridTokenStrict(tk)) { s.grid = tk.toUpper(); break; }
        }
        // A reply must always stay on the caller's original audio frequency.
        // It is better to wait for that channel than silently move a station
        // elsewhere, which IU1TRT cannot reasonably be expected to follow.
        s.audioFreqHz = qBound(200, partnerFreqHz > 0 ? partnerFreqHz : m_rxFrequency, 4000);
        s.progress = plainReportReply ? 3 : 2;
        s.currentTx = plainReportReply ? 3 : 2;
        s.partnerSnrDb = (decodedSnr != 127) ? decodedSnr : 127;
        s.reportSent = formatSignalReport((decodedSnr != 127) ? decodedSnr : -10);
        s.reportReceived = plainReportReply && receivedReportValid
            ? formatSignalReport(receivedReportDb)
            : QStringLiteral("-10");
        s.startedOnMs = nowMs;
        s.lastHeardMs = nowMs;

        auto deferPlainReportReply = [&]() {
            MamLateReportRecovery deferred;
            deferred.slot = s;
            deferred.expiresMs = nowMs + 2 * 60 * 1000;
            deferred.readyForSlot = true;
            m_mamLateReportRecoveries.insert(partnerBase, deferred);
            bridgeLog(QStringLiteral("MAM plain report queued: %1 freq=%2Hz tx=3 rcvd=%3")
                          .arg(s.callFull)
                          .arg(s.audioFreqHz)
                          .arg(s.reportReceived));
            emit callerQueueChanged();
        };
        if (m_mamSlots.size() >= m_mamMaxStreams) {
            if (plainReportReply) {
                deferPlainReportReply();
                return;
            }
            if (enqueueCallerInternal(partnerBase, s.audioFreqHz, decodedSnr, true)) {
                bridgeLog(QStringLiteral("MAM slot capacity full: queued initial caller %1 @ %2Hz")
                              .arg(partnerBase)
                              .arg(s.audioFreqHz));
            }
            return;
        }
        bool frequencyBusy = false;
        for (MamQsoSlot const& other : m_mamSlots) {
            if (qAbs(other.audioFreqHz - s.audioFreqHz) < mamMinStreamGapHz()) {
                frequencyBusy = true;
                break;
            }
        }
        if (frequencyBusy) {
            if (plainReportReply) {
                deferPlainReportReply();
            } else if (enqueueCallerInternal(partnerBase, s.audioFreqHz, decodedSnr, true)) {
                bridgeLog(QStringLiteral("MAM initial caller queued: occupied frequency %1 @ %2Hz")
                              .arg(partnerBase)
                              .arg(s.audioFreqHz));
            }
            return;
        }
        {
            auto const carryIt = m_mamResumeCarry.constFind(s.call);
            if (carryIt != m_mamResumeCarry.constEnd() && carryIt->expiresMs > nowMs) {
                s.lateResumes = qMax(s.lateResumes, carryIt->lateResumes);
            }
            m_mamResumeCarry.remove(s.call);
        }
        m_mamSlots.append(s);
        bridgeLog(QStringLiteral("MAM slot opened: %1 grid=%2 freq=%3Hz tx=%4 sent=%5 rcvd=%6 (slots=%7)")
                      .arg(s.callFull, s.grid.isEmpty() ? QStringLiteral("-") : s.grid)
                      .arg(s.audioFreqHz)
                      .arg(s.currentTx)
                      .arg(s.reportSent)
                      .arg(s.reportReceived)
                      .arg(m_mamSlots.size()));
        emit mamActiveSlotsChanged();
        return;
    }

    // Slot esistente: aggiorna avanzamento.
    MamQsoSlot& s = m_mamSlots[idx];
    s.lastHeardMs = nowMs;
    // A low-confidence decode ('?') made by the a-priori memory right after our
    // own transmission is often the station's previous message rebuilt from the
    // tail of OUR signal on its frequency (the Fox answers on the caller's
    // frequency), with a made-up SNR: on the air a station at -22 dB was sent a
    // +09 report. Such a decode may keep the slot alive but must not rewrite the
    // SNR the report is built from, unless nothing else is known yet.
    bool const lowConfidenceDecode = message.trimmed().endsWith(QLatin1Char('?'));
    if (decodedSnr != 127 && (!lowConfidenceDecode || s.partnerSnrDb == 127)) {
        s.partnerSnrDb = decodedSnr;
        if (s.currentTx >= 1 && s.currentTx <= 3) {
            s.reportSent = formatSignalReport(decodedSnr);
        }
    }
    if (receivedReportValid) {
        s.reportReceived = formatSignalReport(receivedReportDb);
        // The R+report is how well the station copied our last frame; undo the
        // amplitude that frame carried to get its SNR on a full-scale stream.
        if (hasRogerReport && s.lastTxGain > 0.0f) {
            s.linkSnrDb = qRound(receivedReportDb - 20.0 * std::log10(static_cast<double>(s.lastTxGain)));
        }
    }

    if (isFinal73) {
        // 73 finale del partner: QSO concluso, logga e marca Done.
        s.currentTx = 5;
        if (!s.logged) {
            mamLogSlot(s);
        } else {
            s.state = MamQsoSlot::State::Done;
        }
        bridgeLog(QStringLiteral("MAM slot %1: final 73 -> done").arg(s.callFull));
        emit mamActiveSlotsChanged();
        return;
    }
    if (hasSignoff) {
        // RR73/RRR del partner: io chiudo con 73.
        s.currentTx = 5;
        s.progress = 5;
    } else if (hasRogerReport) {
        // R+report: io invio RR73.
        s.currentTx = 4;
        s.progress = 4;
    } else if (hasPlainReport) {
        // report semplice: io invio R+report.
        s.currentTx = 3;
        s.progress = 3;
    } else if (hasGrid) {
        // re-invio della grid: resto su report (TX2).
        s.currentTx = 2;
        s.progress = 2;
    }
    emit mamActiveSlotsChanged();
}

