decodium::seq::CallerScoreInput DecodiumBridge::callerScoreInputFor(const QString& base,
                                                                    int snrDb,
                                                                    bool snrKnown,
                                                                    qint64 waitMs) const
{
    decodium::seq::CallerScoreInput in;
    in.snrDb = snrDb;
    in.snrKnown = snrKnown;
    in.waitMs = waitMs;
    if (base.isEmpty()) {
        return in;
    }
    QString const curBand = freqHzToBandToken(m_frequency);
    in.workedEver = m_worked.callEver.contains(base);
    QString const bandKey = workedCallBandKey(curBand, base);
    in.workedBand = !bandKey.isEmpty()
        && (m_worked.callByBand.contains(bandKey) || m_worked.callTodayByBand.contains(bandKey));
    if (m_dxccLookup && m_dxccLookup->isLoaded()) {
        DxccEntity const ent = m_dxccLookup->lookup(base);
        if (ent.isValid()) {
            in.newDxccEver = !m_worked.dxccEver.contains(ent.name);
            in.newDxccBand = !curBand.isEmpty()
                && !m_worked.dxccByBand.contains(curBand + QLatin1Char('|') + ent.name);
        }
    }
    return in;
}

void DecodiumBridge::rankCallerQueue(const QString& why)
{
    // I "pin" valgono solo per chi e' ancora in coda.
    if (!m_callerQueuePinned.isEmpty()) {
        QSet<QString> queued;
        for (QString const& entry : m_callerQueue) {
            QStringList const parts = entry.split(' ', Qt::SkipEmptyParts);
            if (!parts.isEmpty()) queued.insert(normalizedBaseCall(parts.first()));
        }
        m_callerQueuePinned.intersect(queued);
    }
    if (!m_callerScoring || m_callerQueueManualOrder || m_callerQueue.size() < 2) {
        return;
    }
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();

    QVector<decodium::seq::CallerScoreInput> inputs;
    inputs.reserve(m_callerQueue.size());
    for (QString const& entry : m_callerQueue) {
        QStringList const parts = entry.split(' ', Qt::SkipEmptyParts);
        QString const base = parts.isEmpty() ? QString {} : normalizedBaseCall(parts.first());
        auto const arrival = m_callerQueueArrivalMs.constFind(base);
        qint64 const waitMs = arrival != m_callerQueueArrivalMs.constEnd()
            ? qMax<qint64>(0, nowMs - arrival.value()) : 0;
        bool const snrKnown = parts.size() >= 3;
        decodium::seq::CallerScoreInput in = callerScoreInputFor(base,
                                          snrKnown ? qBound(-50, parts.at(2).toInt(), 49) : -10,
                                          snrKnown,
                                          waitMs);
        in.pinned = m_callerQueuePinned.contains(base);
        inputs.append(in);
    }

    QVector<int> const order = decodium::seq::rankCallers(inputs);
    bool changed = false;
    for (int i = 0; i < order.size(); ++i) {
        if (order.at(i) != i) { changed = true; break; }
    }
    if (!changed) {
        return;
    }

    QStringList ranked;
    ranked.reserve(order.size());
    QStringList summary;
    for (int idx : order) {
        ranked.append(m_callerQueue.at(idx));
        QStringList const parts = m_callerQueue.at(idx).split(' ', Qt::SkipEmptyParts);
        summary.append(QStringLiteral("%1=%2")
                           .arg(parts.isEmpty() ? QString {} : parts.first())
                           .arg(decodium::seq::callerScore(inputs.at(idx)), 0, 'f', 1));
    }
    m_callerQueue = ranked;
    bridgeLog(QStringLiteral("CallerScoring (%1): %2").arg(why, summary.join(QStringLiteral(", "))));
    emit callerQueueChanged();
}

// Con AutoCQ a riposo (nessun dxCall) e piu' stazioni che rispondono nello stesso periodo, la prima
// riga elaborata avvia il QSO e le altre vanno in coda. Ritorna le righe con le sole chiamate dirette
// a noi riordinate per punteggio (la migliore per prima); le altre righe restano dove sono.
// Serve solo a decidere l'ordine di alimentazione del sequencer: la lista decodifiche non cambia.
QStringList DecodiumBridge::scoredCallerRowOrder(const QStringList& rows)
{
    if (!m_callerScoring || !m_autoCqRepeat || rows.size() < 2 || !m_dxCall.trimmed().isEmpty()) {
        return rows;
    }
    QString const myCallUpper = m_callsign.trimmed().toUpper();
    QString const myBaseUpper = normalizedBaseCall(myCallUpper);
    if (myCallUpper.isEmpty()) {
        return rows;
    }

    QVector<int> rowIdx;
    QVector<decodium::seq::CallerScoreInput> inputs;
    QStringList names;
    for (int i = 0; i < rows.size(); ++i) {
        QStringList const f = parseFt8Row(rows.at(i));
        if (f.size() < 5) {
            continue;
        }
        QStringList const parts = f.at(4).simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.size() < 2 || !tokenMatchesCall(parts.first(), myCallUpper, myBaseUpper)) {
            continue;
        }
        QString const lastToken = normalizeCallToken(parts.last()).trimmed().toUpper();
        if (lastToken == QStringLiteral("73") || lastToken == QStringLiteral("RR73")
            || lastToken == QStringLiteral("RRR")) {
            continue;
        }
        QString const partnerBase = normalizedBaseCall(
            inferPartnerFromDirectedMessage(f.at(4), myCallUpper, myBaseUpper));
        if (partnerBase.isEmpty() || partnerBase == myBaseUpper
            || isRecentAutoCqDuplicate(partnerBase, m_frequency, m_mode)) {
            continue;   // il sequencer lo scarterebbe comunque: resta dov'e'
        }
        int const snr = decodeSnrOrDefault(f.value(1), -99);
        inputs.append(callerScoreInputFor(partnerBase, snr, snr > -99, 0));
        rowIdx.append(i);
        names.append(partnerBase);
    }
    if (rowIdx.size() < 2) {
        return rows;
    }

    QVector<int> const order = decodium::seq::rankCallers(inputs);
    bool changed = false;
    for (int k = 0; k < order.size(); ++k) {
        if (order.at(k) != k) { changed = true; break; }
    }
    if (!changed) {
        return rows;
    }

    QStringList out = rows;
    QStringList summary;
    for (int k = 0; k < order.size(); ++k) {
        out[rowIdx.at(k)] = rows.at(rowIdx.at(order.at(k)));
        summary.append(QStringLiteral("%1=%2")
                           .arg(names.at(order.at(k)))
                           .arg(decodium::seq::callerScore(inputs.at(order.at(k))), 0, 'f', 1));
    }
    bridgeLog(QStringLiteral("CallerScoring (slot): %1").arg(summary.join(QStringLiteral(", "))));
    return out;
}
