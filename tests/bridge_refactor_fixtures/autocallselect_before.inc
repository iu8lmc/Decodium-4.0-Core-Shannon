bool DecodiumBridge::tryAutoCallFromRows(const QStringList& rows)
{
    if (!m_autoCallEnabled
        || !autoCallModeSupported()
        || m_targetCallActive
        || m_transmitting
        || m_tuning
        || !m_dxCall.trimmed().isEmpty()
        || (m_qsoProgress > 1 && m_qsoProgress < 6)
        || (m_autoCallMaxQsos > 0 && m_autoCallQsoCount >= m_autoCallMaxQsos)) {
        return false;
    }

    struct Candidate {
        QString message;
        QString time;
        QString snr;
        QString call;
        double distanceKm {-1.0};
        int audioFreq {0};
        int rowIndex {-1};
    };

    QList<Candidate> candidates;
    candidates.reserve(qMin(rows.size(), 256));
    const QString myBase = normalizedBaseCall(m_callsign.trimmed().toUpper());
    const QString band = freqHzToBandToken(m_frequency);
    const DecodeUserFilterConfig userFilters = readDecodeUserFilterConfig();
    const int rowLimit = qMin(rows.size(), 256);

    // Keep this scan bounded and synchronous like the existing auto-sequence
    // prefilter. It only runs while Auto Call is armed and does not publish UI
    // rows, touch the waterfall, or perform network I/O.
    for (int i = 0; i < rowLimit; ++i) {
        const QStringList fields = parseFt8Row(rows.at(i));
        if (fields.size() < 5) {
            continue;
        }
        const QString message = fields.at(4).trimmed();
        const bool isCq = message.startsWith(QStringLiteral("CQ "), Qt::CaseInsensitive)
            || message.compare(QStringLiteral("CQ"), Qt::CaseInsensitive) == 0
            || message.startsWith(QStringLiteral("QRZ "), Qt::CaseInsensitive);
        if (!isCq) {
            continue;
        }

        QString semanticRejectReason;
        if (!shouldAcceptDecodedMessage(message, &semanticRejectReason)) {
            continue;
        }

        const QString call = normalizedUsableCallToken(extractDecodedCallsign(message, true));
        const QString callBase = normalizedBaseCall(call);
        if (call.isEmpty() || callBase.isEmpty() || callBase == myBase) {
            continue;
        }

        const QString callBandKey = workedCallBandKey(band, call);
        if (!callBandKey.isEmpty() && m_worked.callByBand.contains(callBandKey)) {
            // WSJT-Z Auto Call forces "new call on band".
            continue;
        }
        if (isRecentAutoCqWorkedOrLoggedDuplicate(call, m_frequency, m_mode)) {
            continue;
        }

        QVariantMap entry;
        entry[QStringLiteral("time")] = fields.value(0);
        entry[QStringLiteral("db")] = fields.value(1);
        entry[QStringLiteral("dt")] = fields.value(2);
        entry[QStringLiteral("freq")] = fields.value(3);
        entry[QStringLiteral("message")] = message;
        entry[QStringLiteral("mode")] = m_mode;
        entry[QStringLiteral("isTx")] = false;
        entry[QStringLiteral("isCQ")] = true;
        entry[QStringLiteral("fromCall")] = call;
        entry[QStringLiteral("dxCallsign")] = call;
        // Reuse the same in-memory DXCC/grid metadata used by the normal
        // AutoCQ path, so the existing blacklist/whitelist/territory filters
        // behave consistently with manual and AutoCQ operation.
        enrichDecodeEntry(entry);
        QString userFilterRejectReason;
        if (!shouldAcceptDecodeEntryByUserFilters(entry, userFilters, true,
                                                   &userFilterRejectReason)) {
            bridgeLog(QStringLiteral("[AUTOCALL] filtered %1: %2")
                          .arg(userFilterRejectReason, message));
            continue;
        }

        const QString grid = extractDecodedGrid(message);
        const double distanceKm = (!m_grid.trimmed().isEmpty() && !grid.isEmpty())
            ? calcDistance(m_grid, grid)
            : -1.0;
        candidates.append(Candidate {
            message,
            fields.value(0),
            fields.value(1),
            call,
            distanceKm,
            fields.value(3).toInt(),
            i
        });
    }

    if (candidates.isEmpty()) {
        return false;
    }

    Candidate selected = candidates.constLast();
    if (m_autoCallPriority == 0) {
        // A native decode batch normally contains one UTC slot, while the
        // legacy mirror can contain entries from more than one timestamp.
        // Prefer the newest decoded timestamp and use the source row order as
        // a deterministic tie-breaker for same-slot/native rows.
        for (const Candidate& candidate : candidates) {
            const int candidateSeconds = secondsFromUtcDisplayToken(candidate.time);
            const int selectedSeconds = secondsFromUtcDisplayToken(selected.time);
            if (candidateSeconds < 0 || selectedSeconds < 0) {
                if (candidate.rowIndex >= selected.rowIndex) {
                    selected = candidate;
                }
                continue;
            }
            const int delta = signedUtcSecondDelta(selectedSeconds, candidateSeconds);
            if (delta > 0 || (delta == 0 && candidate.rowIndex >= selected.rowIndex)) {
                selected = candidate;
            }
        }
    } else if (m_autoCallPriority == 1) {
        for (const Candidate& candidate : candidates) {
            bool candidateOk = false;
            bool selectedOk = false;
            const int candidateSnr = candidate.snr.toInt(&candidateOk);
            const int selectedSnr = selected.snr.toInt(&selectedOk);
            if ((candidateOk && !selectedOk)
                || (candidateOk && selectedOk && candidateSnr >= selectedSnr)) {
                selected = candidate;
            }
        }
    } else if (m_autoCallPriority == 2) {
        bool haveDistance = false;
        for (const Candidate& candidate : candidates) {
            if (!std::isfinite(candidate.distanceKm) || candidate.distanceKm < 0.0) {
                continue;
            }
            if (!haveDistance || candidate.distanceKm >= selected.distanceKm) {
                selected = candidate;
                haveDistance = true;
            }
        }
        if (!haveDistance) {
            bridgeLog(QStringLiteral("[AUTOCALL] furthest-grid fallback: no valid station/DX locator; using newest candidate"));
        }
    }

    m_autoCallStartingCandidate = true;
    m_autoCallCurrentCandidate = selected.call;
    bridgeLog(QStringLiteral("[AUTOCALL] selecting %1 priority=%2 time=%3 snr=%4 distanceKm=%5 message=%6")
                  .arg(selected.call)
                  .arg(m_autoCallPriority)
                  .arg(selected.time)
                  .arg(selected.snr)
                  .arg(selected.distanceKm, 0, 'f', 1)
                  .arg(selected.message));
    processDecodeDoubleClick(selected.message, selected.time, selected.snr, selected.audioFreq);
    m_autoCallStartingCandidate = false;

    const bool started = normalizedBaseCall(m_dxCall) == normalizedBaseCall(selected.call)
        && m_txEnabled;
    if (!started) {
        m_autoCallCurrentCandidate.clear();
        bridgeLog(QStringLiteral("[AUTOCALL] candidate %1 was not armed").arg(selected.call));
    } else {
        // processDecodeDoubleClick() arms TX and performs an immediate guarded
        // check. Keep one queued retry as well: decode callbacks can arrive at
        // the slot boundary while the period/timing state is still being
        // updated. The normal periodic guards remain authoritative, so this
        // cannot start an out-of-slot transmission or touch the waterfall.
        const QString candidateBase = normalizedBaseCall(selected.call);
        quint64 const requestGeneration = m_txRequestGeneration;
        QTimer::singleShot(0, this, [this, candidateBase, requestGeneration]() {
            if (m_shuttingDown || QCoreApplication::closingDown()
                || requestGeneration != m_txRequestGeneration
                || !m_autoCallEnabled
                || normalizedBaseCall(m_dxCall) != candidateBase
                || !m_txEnabled
                || m_transmitting
                || m_tuning) {
                return;
            }
            bridgeLog(QStringLiteral("[AUTOCALL] TX pending for %1; retrying guarded slot check")
                          .arg(candidateBase));
            checkAndStartPeriodicTx();
        });
    }
    return started;
}

