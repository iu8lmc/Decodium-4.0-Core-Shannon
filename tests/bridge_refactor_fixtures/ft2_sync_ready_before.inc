void DecodiumBridge::onFt2DecodeReady(quint64 serial, QStringList rows)
{
#if DECODIUM_HAS_SSTV
    if (m_sstvWorkspaceActive) {
        // Reuse the common callback gate so serial bookkeeping is released.
        onFt8DecodeReady(serial, QStringList {});
        return;
    }
#endif
    // Sprint3-A: il pass sync non passava da onFt2AsyncDecodeReady -> le sue
    // righe non seminavano la AP cache. Stesso pattern (e stessa funzione di
    // hash) del seed async; anti-loop: salta i decode cache-confirmed (nap=7).
    if (m_ft2ApHashCache && m_mode == QStringLiteral("FT2")) {
        qint64 const apNowMs = QDateTime::currentMSecsSinceEpoch();
        for (const auto& row : rows) {
            QStringList const f = parseFt8Row(row);
            if (f.size() < 6 || f[5].trimmed() == QLatin1String("7")) continue;
            for (quint32 const h28 : decodium::ft2MessageCallHashes(f[4]))
                m_hashedCallsignCache.add(h28, apNowMs);
        }
    }
    onFt8DecodeReady(serial, rows); // stessa logica di parsing
}

