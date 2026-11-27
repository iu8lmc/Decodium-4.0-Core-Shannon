void DecodiumBridge::onFt4DecodeReady(quint64 serial, QStringList rows)
{
    onFt8DecodeReady(serial, rows);
}

void DecodiumBridge::onQ65DecodeReady(quint64 serial, QStringList rows)
{
    onFt8DecodeReady(serial, rows);   // stesso formato output
}

void DecodiumBridge::onMsk144DecodeReady(quint64 serial, QStringList rows)
{
    onFt8DecodeReady(serial, rows);   // stesso formato output
}

void DecodiumBridge::onLegacyJtDecodeReady(quint64 serial, QStringList rows)
{
    if (shouldIgnoreDecodeCallbacks()) {
        bridgeLog("onLegacyJtDecodeReady: ignored during shutdown");
        return;
    }
#if DECODIUM_HAS_SSTV
    if (m_sstvWorkspaceActive) {
        if (m_decoding) {
            m_decoding = false;
            emit decodingChanged();
        }
        bridgeLog(QStringLiteral(
            "legacy JT callback discarded while SSTV workspace is active: serial=%1 rows=%2")
                      .arg(serial)
                      .arg(rows.size()));
        return;
    }
#endif

    bridgeLog("onLegacyJtDecodeReady: serial=" + QString::number(serial) +
              " rows=" + QString::number(rows.size()) + " mode=" + m_mode);
    if (m_dxccLookup && !m_dxccLookup->isLoaded()) {
        loadDxccLookupAsync();
    }
    Q_UNUSED(serial)
    bool changed = false;
    DecodeUserFilterConfig const userFilterConfig = readDecodeUserFilterConfig();
    ensureNativeDecodeDedupIndex();
    for (const auto& row : rows) {
        QStringList f = parseJt65Row(row);
        if (f.size() < 8) {
            bridgeLog("  parseJt65Row FAILED for: '" + row + "'");
            continue;
        }

        QString msg   = f[4];
        QString semanticRejectReason;
        if (!shouldAcceptDecodedMessage(msg, &semanticRejectReason)) {
            bridgeLog(QStringLiteral("semantic decode filter legacy-jt: %1 msg='%2'")
                          .arg(semanticRejectReason, msg));
            continue;
        }
        bool isCQ     = msg.startsWith("CQ ", Qt::CaseInsensitive) || msg == "CQ";
        bool isMyCall = messageMentionsLocalCall(msg);
        QString fromCall = extractDecodedCallsign(msg, isCQ);

        QVariantMap entry;
        entry["time"]       = f[0];
        entry["db"]         = f[1];
        entry["dt"]         = f[2];
        entry["freq"]       = f[7];
        entry["message"]    = msg;
        entry["aptype"]     = f[5];
        entry["quality"]    = f[6];
        entry["mode"]       = m_mode;
        entry["isTx"]       = false;
        entry["isCQ"]       = isCQ;
        entry["isMyCall"]   = isMyCall;
        entry["fromCall"]   = fromCall;
        entry["decodeSessionId"] = static_cast<qulonglong>(m_decodeSessionId);
        enrichDecodeEntry(entry);
        isMyCall = entry.value(QStringLiteral("isMyCall")).toBool();
        if (m_hideGhostDecodes && looksLikeGhostDecode(entry)) {
            bridgeLog(QStringLiteral("ghost decode filter legacy-jt: msg='%1'")
                          .arg(msg));
            continue;
        }
        QString userFilterReason;
        if (!shouldAcceptDecodeEntryByUserFilters(entry, userFilterConfig, false, &userFilterReason)) {
            bridgeLog(QStringLiteral("user decode filter legacy-jt: %1 msg='%2'")
                          .arg(userFilterReason, msg));
            continue;
        }
        maybeQueuePskReporterSpot(entry, msg, isCQ, f[7], f[1], entry.value("mode").toString());

        bool const filteredByCqOnly = !m_filtersBypassed && m_filterCqOnly && !passesCqFilter(isCQ, msg);
        bool const filteredByMyCallOnly = !m_filtersBypassed && m_filterMyCallOnly && !isMyCall;
        if (filteredByCqOnly || filteredByMyCallOnly) {
            persistDecodeHistoryEntry(entry);   // archivio = registro, non vista (vedi il ramo FT8)
            if (filteredByCqOnly && !filteredByMyCallOnly) {
                appendRxDecodeEntry(entry);
            }
            continue;
        }

        // L'indice incrementale evita una scansione lineare del modello per
        // ogni riga JT ricevuta. Viene resettato a ogni nuova sessione decode.
        QString const dedupKey = decodeDedupKey(entry.value(QStringLiteral("time")).toString(),
                                                 entry.value(QStringLiteral("freq")).toString(),
                                                 msg);
        if (!dedupKey.isEmpty() && m_nativeDecodeDedupKeys.contains(dedupKey)) {
            continue;
        }
        tryStartWaitPounceFromEntry(entry, m_decodeList, QStringLiteral("legacy-jt"));
        appendDecodeMapToList(entry);
        appendRxDecodeEntry(entry);
        // 1.0.162 — DecoSyncTime fase 4: feed dt al self-calibrator
        if (m_decoSyncTime && !entry.value("isTx").toBool()) {
            bool dtOk2 = false, snrOk2 = false;
            double const dt2 = entry.value("dt").toString().toDouble(&dtOk2);
            int const db2 = entry.value("db").toString().toInt(&snrOk2);
            if (dtOk2 && snrOk2) m_decoSyncTime->reportDecodeDt(dt2, db2);
        }
        changed = true;

        maybePlayDecodeAlert(isCQ, isMyCall, msg);
    }
    if (changed) {
        // JT9/JT65/Q65 source rows are normalized by the worker snapshot just
        // like the FTx modes; a throttled notify keeps the GUI responsive on
        // older CPUs receiving a dense decode period.
        emitDecodeListChangedThrottled();
    }
}

void DecodiumBridge::onFst4DecodeReady(quint64 serial, QStringList rows)
{
    if (shouldIgnoreDecodeCallbacks()) {
        bridgeLog("onFst4DecodeReady: ignored during shutdown");
        return;
    }

    bridgeLog("onFst4DecodeReady: serial=" + QString::number(serial) +
              " rows=" + QString::number(rows.size()) + " mode=" + m_mode);
    // FST4 rows have the same format as FT8 (backtick marker), parseFt8Row works
    onFt8DecodeReady(serial, rows);
}
