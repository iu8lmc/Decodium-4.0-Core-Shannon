void DecodiumBridge::onWsprDecodeReady(quint64 serial, QStringList rows,
                                        QStringList diagnostics, int exitCode)
{
    noteDecodeReadySlotStart();
    auto decodeReadyPhaseGuard = qScopeGuard([] {
        DecodiumBridge::noteDecodeReadySlotEnd();
    });

    QString const tempWav = m_wsprDecodeTempFiles.take(serial);
    if (!tempWav.isEmpty()) {
        QFile::remove(tempWav);
    }

    auto forgetDecodeSerial = [this, serial]() {
        m_decodeStartMsBySerial.remove(serial);
        m_decodeModeBySerial.remove(serial);
        m_decodeUtcTokenBySerial.remove(serial);
        m_decodeSessionBySerial.remove(serial);
        m_ft8PendingDeepFollowups.remove(serial);
    };
    auto forgetDecodeSerialGuard = qScopeGuard([&forgetDecodeSerial] {
        forgetDecodeSerial();
    });

    if (shouldIgnoreDecodeCallbacks()) {
        bridgeLog("onWsprDecodeReady: ignored during shutdown");
        return;
    }
#if DECODIUM_HAS_SSTV
    if (m_sstvWorkspaceActive) {
        if (m_decoding) {
            m_decoding = false;
            emit decodingChanged();
        }
        bridgeLog(QStringLiteral(
            "WSPR callback discarded while SSTV workspace is active: serial=%1 rows=%2")
                      .arg(serial)
                      .arg(rows.size()));
        return;
    }
#endif

    QString const serialMode = m_decodeModeBySerial.value(serial);
    quint64 const serialSession = m_decodeSessionBySerial.value(serial, m_decodeSessionId);
    if (serialSession != m_decodeSessionId
        || (!serialMode.isEmpty() && serialMode != m_mode)) {
        bridgeLog(QStringLiteral("onWsprDecodeReady: ignored stale callback serial=%1 serialMode=%2 mode=%3 session=%4 currentSession=%5")
                      .arg(serial)
                      .arg(serialMode)
                      .arg(m_mode)
                      .arg(serialSession)
                      .arg(m_decodeSessionId));
        m_decoding = false;
        emit decodingChanged();
        return;
    }

    QStringList decodeRows;
    decodeRows.reserve(rows.size());
    for (QString const& row : std::as_const(rows)) {
        QString const trimmed = row.trimmed();
        if (!trimmed.isEmpty() && trimmed != QStringLiteral("<DecodeFinished>")) {
            decodeRows.append(trimmed);
        }
    }

    bridgeLog(QStringLiteral("onWsprDecodeReady: serial=%1 rows=%2 decoded=%3 exit=%4 diagnostics=%5")
                  .arg(serial)
                  .arg(rows.size())
                  .arg(decodeRows.size())
                  .arg(exitCode)
                  .arg(diagnostics.size()));
    for (int i = 0; i < qMin(decodeRows.size(), 3); ++i) {
        bridgeLog(QStringLiteral("  wspr[%1]='%2'").arg(i).arg(decodeRows.at(i)));
    }
    for (int i = 0; i < qMin(diagnostics.size(), 3); ++i) {
        QString const diag = diagnostics.at(i).trimmed();
        if (!diag.isEmpty()) {
            bridgeLog(QStringLiteral("  wsprdiag[%1]='%2'").arg(i).arg(diag));
        }
    }

    if (m_wsprUploadEnabled && m_wsprUploader && !m_callsign.isEmpty()) {
        m_wsprUploader->setEnabled(m_wsprUploadEnabled);
        m_wsprUploader->setCallsign(m_callsign);
        m_wsprUploader->setGrid(m_grid);
        if (decodeRows.isEmpty()) {
            m_wsprUploader->uploadSpot(QString(), m_frequency,
                                       QStringLiteral("0"),
                                       QString::number(m_wsprPowerDbm));
        } else {
            for (QString const& row : std::as_const(decodeRows)) {
                m_wsprUploader->uploadSpot(row, m_frequency,
                                           QStringLiteral("0"),
                                           QString::number(m_wsprPowerDbm));
            }
        }
    }

    bool changed = false;
    int accepted = 0;
    int parseFailures = 0;
    int filtered = 0;
    QString const forcedUtcToken = m_decodeUtcTokenBySerial.value(serial).trimmed();
    QVector<double> dtSamples;
    DecodeUserFilterConfig const userFilterConfig = readDecodeUserFilterConfig();
    bool const nativeWsprDedup = !usingLegacyBackendForTx();
    if (nativeWsprDedup) {
        ensureNativeDecodeDedupIndex();
    }
    QSet<QString> legacyRecentDecodeDedupKeys;
    if (!nativeWsprDedup) {
        int const listSize = m_decodeList.size();
        int const firstRecent = qMax(0, listSize - 300);
        legacyRecentDecodeDedupKeys.reserve(listSize - firstRecent + decodeRows.size());
        for (int di = firstRecent; di < listSize; ++di) {
            QVariantMap const previous = m_decodeList.at(di).toMap();
            if (previous.value(QStringLiteral("isTx")).toBool()) {
                continue;
            }
            QString const key = decodeDedupKey(previous.value(QStringLiteral("time")).toString(),
                                               previous.value(QStringLiteral("freq")).toString(),
                                               previous.value(QStringLiteral("message")).toString());
            if (!key.isEmpty()) {
                legacyRecentDecodeDedupKeys.insert(key);
            }
        }
    }
    QSet<QString> batchDecodeDedupKeys;
    batchDecodeDedupKeys.reserve(decodeRows.size());

    for (QString const& row : std::as_const(decodeRows)) {
        QStringList f = parseWsprRow(row);
        if (f.size() < 8) {
            ++parseFailures;
            bridgeLog(QStringLiteral("  parseWsprRow FAILED for: '%1'").arg(row));
            continue;
        }

        QString const msg = f[4];
        QString const entryTime = forcedUtcToken.isEmpty() ? f[0] : normalizeUtcDisplayToken(forcedUtcToken);
        bool const isMyCall = messageMentionsLocalCall(msg);
        QString const fromCall = extractDecodedCallsign(msg, false);
        QString const dedupKey = decodeDedupKey(entryTime, f[7], msg);
        if (!dedupKey.isEmpty()
            && ((nativeWsprDedup && m_nativeDecodeDedupKeys.contains(dedupKey))
                || (!nativeWsprDedup && legacyRecentDecodeDedupKeys.contains(dedupKey))
                || batchDecodeDedupKeys.contains(dedupKey))) {
            ++filtered;
            continue;
        }
        if (!dedupKey.isEmpty()) {
            batchDecodeDedupKeys.insert(dedupKey);
        }

        QVariantMap entry;
        entry["time"] = entryTime;
        entry["db"] = f[1];
        entry["dt"] = f[2];
        entry["freq"] = f[7];
        entry["message"] = msg;
        entry["aptype"] = QString();
        entry["drift"] = f[5];
        entry["quality"] = f[6];
        entry["mode"] = QStringLiteral("WSPR");
        entry["isTx"] = false;
        entry["isCQ"] = false;
        entry["isMyCall"] = isMyCall;
        entry["fromCall"] = fromCall;
        entry["decodeSessionId"] = static_cast<qulonglong>(m_decodeSessionId);
        entry["timestamp"] = static_cast<qulonglong>(
            decodeEntryTimestampMsecs(entryTime, QDateTime::currentMSecsSinceEpoch()));
        enrichDecodeEntry(entry);

        QString userFilterReason;
        if (!shouldAcceptDecodeEntryByUserFilters(entry, userFilterConfig, false, &userFilterReason)) {
            ++filtered;
            bridgeLog(QStringLiteral("user decode filter WSPR: %1 msg='%2'")
                          .arg(userFilterReason, msg));
            continue;
        }

        bool snrOk = false;
        bool dtOk = false;
        int const snrVal = f[1].toInt(&snrOk);
        double const dtVal = f[2].toDouble(&dtOk);
        if (snrOk && dtOk) {
            dtSamples.append(dtVal);
            if (m_decoSyncTime) {
                m_decoSyncTime->reportDecodeDt(dtVal, snrVal);
            }
        }

        maybeQueuePskReporterSpot(entry, msg, false, f[7], f[1], QStringLiteral("WSPR"));
        appendDecodeMapToList(entry);
        appendRxDecodeEntry(entry);
        appendLegacyAllTxtDecodeLine(entry);
        queueWorldMapEntryForReplay(entry, false, 250);
        udpSendWsprDecode(true, row);
        maybePlayDecodeAlert(false, entry.value(QStringLiteral("isMyCall")).toBool(), msg);
        changed = true;
        ++accepted;
    }

    if (changed) {
        // WSPR shares the asynchronous decode UI snapshot pipeline with FT8
        // and FT4. Do not sort/normalize the complete source histories here.
        emitDecodeListChangedThrottled();
    }
    finalizeTimeSyncDecodeCycle(serial, QStringLiteral("WSPR"), dtSamples);
    m_decoding = false;
    emit decodingChanged();
    bridgeLog(QStringLiteral("onWsprDecodeReady summary: serial=%1 accepted=%2 parseFail=%3 filtered=%4")
                  .arg(serial)
                  .arg(accepted)
                  .arg(parseFailures)
                  .arg(filtered));
}
