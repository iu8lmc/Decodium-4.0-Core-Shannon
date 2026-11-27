void DecodiumBridge::onFt8DecodeReady(quint64 serial, QStringList rows)
{
    if (serial == m_deepDecodeVisualSerial) {
        m_deepDecodeVisualSerial = 0;
        m_deepDecodeVisualThrottleUntilMs = 0;
    }
    noteDecodeReadySlotStart();
    auto decodeReadyPhaseGuard = qScopeGuard([] {
        DecodiumBridge::noteDecodeReadySlotEnd();
    });
#if DECODIUM_HAS_SSTV
    if (m_sstvWorkspaceActive) {
        m_decodeStartMsBySerial.remove(serial);
        m_decodeModeBySerial.remove(serial);
        m_decodeUtcTokenBySerial.remove(serial);
        m_decodeSessionBySerial.remove(serial);
        m_ft8PendingDeepFollowups.remove(serial);
        m_ft8EarlyDecodeSerials.remove(serial);
        if (m_decoding) {
            m_decoding = false;
            emit decodingChanged();
        }
        bridgeLog(QStringLiteral(
            "decode callback discarded while SSTV workspace is active: serial=%1 rows=%2")
                      .arg(serial)
                      .arg(rows.size()));
        return;
    }
#endif
    MainThreadTraceScope trace(QStringLiteral("on_ft8_decode_ready"),
                               QStringLiteral("serial=%1 rows=%2 mode=%3 tx=%4")
                                   .arg(serial)
                                   .arg(rows.size())
                                   .arg(m_mode)
                                   .arg(m_transmitting ? 1 : 0));
    QElapsedTimer ft8PhaseTimer;
    ft8PhaseTimer.start();
    qint64 phaseSetupMs = 0;
    qint64 phaseAutoSeqMs = 0;
    qint64 phaseRowsMs = 0;
    qint64 phasePostRowsMs = 0;
    qint64 rowEnrichMs = 0;
    qint64 rowHashResolveMs = 0;
    qint64 rowHashResolveLoadMs = 0;
    qint64 rowAppendMs = 0;
    qint64 rowRxMirrorMs = 0;
    qint64 rowSecondaryQueueMs = 0;
    int rowEnrichReused = 0;
    int rowEnrichFresh = 0;
    int rowHashResolveAttempts = 0;
    int rowHashResolveResolved = 0;
    if (shouldIgnoreDecodeCallbacks()) {
        bridgeLog("onFt8DecodeReady: ignored during shutdown");
        return;
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

    if (m_ft8EarlyDecodeSerials.remove(serial) > 0) {
        bridgeLog("onFt8DecodeReady: hidden FT8 early stage serial=" + QString::number(serial) +
                  " rows=" + QString::number(rows.size()));
        forgetDecodeSerial();
        return;
    }

    QString const serialMode = m_decodeModeBySerial.value(serial);
    quint64 const serialSession = m_decodeSessionBySerial.value(serial, m_decodeSessionId);
    if (serialSession != m_decodeSessionId) {
        bridgeLog("onFt8DecodeReady: ignored stale callback serial=" + QString::number(serial) +
                  " session=" + QString::number(serialSession) +
                  " currentSession=" + QString::number(m_decodeSessionId));
        forgetDecodeSerial();
        return;
    }
    if (!serialMode.isEmpty() && serialMode != m_mode) {
        bridgeLog("onFt8DecodeReady: ignored stale " + serialMode +
                  " callback after mode change to " + m_mode +
                  " serial=" + QString::number(serial));
        forgetDecodeSerial();
        return;
    }
    qint64 const startedAtMs = m_decodeStartMsBySerial.value(serial, 0);
    qint64 const elapsedMs = startedAtMs > 0
        ? QDateTime::currentMSecsSinceEpoch() - startedAtMs
        : -1;
    // A final FT4 pass starts at the slot boundary and has roughly 6.2 s
    // before the next early pass. Treating a healthy 2.5-3.5 s depth-3 pass
    // as pressure needlessly disabled DEEP on capable 8-thread systems.
    // Keep one second of scheduling headroom and match the backlog threshold.
    if (serialMode == QStringLiteral("FT4") && elapsedMs > 5000) {
        activateFt4LatencyGuard(QDateTime::currentMSecsSinceEpoch(),
                                QStringLiteral("slow callback serial=%1 elapsedMs=%2 rows=%3")
                                    .arg(serial)
                                    .arg(elapsedMs)
                                    .arg(rows.size()));
    }
    if (serialMode == QStringLiteral("FT4") && elapsedMs > 9000) {
        ++m_ft4LateCallbackStreak;
        activateFt4AdaptiveCpuLimit(QDateTime::currentMSecsSinceEpoch(),
                                    QStringLiteral("late callback serial=%1 elapsedMs=%2 rows=%3")
                                        .arg(serial)
                                        .arg(elapsedMs)
                                        .arg(rows.size()));
        bridgeLog("onFt8DecodeReady: ignored late FT4 callback serial=" + QString::number(serial) +
                  " rows=" + QString::number(rows.size()) +
                  " elapsedMs=" + QString::number(elapsedMs) +
                  " streak=" + QString::number(m_ft4LateCallbackStreak));
        forgetDecodeSerial();
        return;
    }
    if (serialMode == QStringLiteral("FT4") && elapsedMs >= 0 && elapsedMs <= 7500) {
        m_ft4LateCallbackStreak = 0;
    }

    // I deep follow-up FT8 sono decode-list-only. La decisione TX e' gia'
    // stata presa dal fast pass: da qui in poi non devono attivare resume,
    // MAM, W&P, UDP, alert o auto-seq.
    bool const ft8DeepInTxListOnly = m_ft8DeepInTxSerials.remove(serial) > 0;
    if (ft8DeepInTxListOnly) {
        bridgeLog(QStringLiteral("onFt8DecodeReady: FT8 list-only deep serial=%1 rows=%2")
                      .arg(serial)
                      .arg(rows.size()));
    }

    // 1.0.304 (#9) — resume-on-reply: se in Halt con partner armato e quello ri-risponde,
    // riprende il QSO (re-ingaggio via processDecodeDoubleClick). Se ha ripreso, salta
    // l'auto-seq su QUESTA batch (più sotto) per evitare doppio advance sullo stesso decode.
    bool const resumedQso = ft8DeepInTxListOnly ? false : tryResumeQsoOnReply(rows);
    // DX-watch armato: se il target compare on-air, ARMED -> CALLING. Inerte se non armato.
    if (!resumedQso && !ft8DeepInTxListOnly) tryArmedWatchSeesTarget(rows);
    if (!ft8DeepInTxListOnly) observeTargetCallDecodeRows(rows, serialMode.isEmpty() ? m_mode : serialMode);

    if (isTimeSyncDecodeMode(m_mode)) {
        if (serialMode.isEmpty()) {
            bridgeLog("onFt8DecodeReady: ignored stale time-sync callback serial=" + QString::number(serial) +
                      " currentMode=" + m_mode);
            forgetDecodeSerial();
            return;
        }
    }

    if (rows.isEmpty() && serialMode == QStringLiteral("FT2")) {
        bridgeLog("onFt2DecodeReady: slot senza decode serial=" + QString::number(serial));
    } else {
        bridgeLog("onFt8DecodeReady: serial=" + QString::number(serial) +
                  " rows=" + QString::number(rows.size()));
    }
    for (int dbgI = 0; dbgI < qMin(rows.size(), 3); ++dbgI)
        bridgeLog("  raw[" + QString::number(dbgI) + "]='" + rows[dbgI] + "'");

    if (m_dxccLookup && !m_dxccLookup->isLoaded()) {
        loadDxccLookupAsync();
    }

    QString const decodeMode = serialMode;
    QString const forcedUtcRaw = m_decodeUtcTokenBySerial.value(serial).trimmed();
    QString const forcedUtcToken = forcedUtcRaw.isEmpty()
        ? QString()
        : normalizeUtcDisplayToken(forcedUtcRaw);
    bool const trackTimeSync = isTimeSyncDecodeMode(decodeMode);
    bool const msk144DecodeMode = decodeMode == QStringLiteral("MSK144");
    auto decodeEntryTimeForRow = [&](QString const& rawTime) -> QString {
        if (trackTimeSync && !forcedUtcToken.isEmpty()) {
            return forcedUtcToken;
        }

        // The MSK144 worker can return a zero UTC prefix when the bridge owns
        // the decode slot timing. If we keep "000000", the same RF decode is
        // later mirrored from ALL.TXT with the real UTC and appears twice.
        QString const normalizedRaw = normalizeUtcDisplayToken(rawTime);
        if (msk144DecodeMode
            && !forcedUtcToken.isEmpty()
            && normalizedRaw == QStringLiteral("000000")) {
            return forcedUtcToken;
        }

        return rawTime;
    };
    if (decodeMode == QStringLiteral("FT8") && elapsedMs > 7000) {
        bridgeLog("onFt8DecodeReady: slow FT8 serial=" + QString::number(serial) +
                  " rows=" + QString::number(rows.size()) +
                  " elapsedMs=" + QString::number(elapsedMs) +
                  " (processing results)");
        noteCpuPressure(QStringLiteral("slow FT8 callback %1ms").arg(elapsedMs),
                        elapsedMs > 12000 ? 45000 : 30000,
                        elapsedMs > 15000);
    }
    // The embedded legacy window owns radio/audio/TX, but the QML bridge owns
    // FT8 decoding. Mirroring the hidden legacy FT8 model would require a
    // second decoder on every slot and was the source of long first-slot UI
    // stalls. Other legacy modes retain their existing mirror path.
    bool const legacyUiMirrorActive =
        usingLegacyBackendForTx() && decodeMode != QStringLiteral("FT8");
    QVector<double> dtSamples;
    bool changed = false;
    int parseFailures = 0;
    int guardrailFiltered = 0;
    int semanticFiltered = 0;
    int userFiltered = 0;
    int uiFiltered = 0;
    int cqOnlyFiltered = 0;
    int myCallOnlyFiltered = 0;
    int myCallMatches = 0;
    int duplicatesSkipped = 0;
    int accepted = 0;
    DecodeUserFilterConfig const userFilterConfig = readDecodeUserFilterConfig();
    QString const flowMode = decodeMode.isEmpty() ? m_mode : decodeMode;
    bool const timeSyncFlowMode =
        flowMode == QStringLiteral("FT8")
        || flowMode == QStringLiteral("FT4");
    QString customAllTxtBatch;
    QString legacyAllTxtBatch;
    QSet<QString> skippedDuplicateDecodeKeys;
    if (!legacyUiMirrorActive) {
        ensureNativeDecodeDedupIndex();
    }
    auto isRecentNativeDuplicate = [this, legacyUiMirrorActive](QString const& key) {
        return !legacyUiMirrorActive
            && !key.isEmpty()
            && m_nativeDecodeDedupKeys.contains(key);
    };
    bool const signoffWatch = signoffDecodeWatchActive();
    phaseSetupMs = ft8PhaseTimer.elapsed();
    ft8PhaseTimer.restart();

    // 1.0.179 — Smooth Decode Flow scheduler. Se attivo + auto-fallback OK +
    // batch grande (>5) + non FT2 (gia' streaming via async), spalma il
    // rilascio. FT8/FT4 restano fuori: sono mode a slot stretti e l'utente si
    // aspetta la lista decode al boundary, non una coda che scivola nello slot
    // successivo.
    int adaptivePrefilteredRows = 0;
    if (decodeMode == QStringLiteral("FT8")
        && !ft8DeepInTxListOnly
        && !legacyUiMirrorActive
        && rows.size() > 8
        && (m_lowCpuModeEnabled || cpuPressureActive() || elapsedMs > 9000)) {
        QSet<QString> batchDecodeDedupKeys;
        batchDecodeDedupKeys.reserve(rows.size());
        QStringList filteredRows;
        filteredRows.reserve(rows.size());
        for (const auto& row : std::as_const(rows)) {
            QStringList const f = parseFt8Row(row);
            if (f.size() < 8) {
                filteredRows.append(row);
                continue;
            }
            QString const key = decodeDedupKey(decodeEntryTimeForRow(f[0]),
                                               f[7],
                                               f[4]);
            if (key.isEmpty()) {
                filteredRows.append(row);
                continue;
            }
            if (isRecentNativeDuplicate(key) || batchDecodeDedupKeys.contains(key)) {
                skippedDuplicateDecodeKeys.insert(key);
                ++adaptivePrefilteredRows;
                continue;
            }
            batchDecodeDedupKeys.insert(key);
            filteredRows.append(row);
        }
        if (adaptivePrefilteredRows > 0) {
            bridgeLog(QStringLiteral("FT8 adaptive result prefilter: serial=%1 raw=%2 kept=%3 dropped=%4 elapsedMs=%5 pressure=%6")
                          .arg(serial)
                          .arg(rows.size())
                          .arg(filteredRows.size())
                          .arg(adaptivePrefilteredRows)
                          .arg(elapsedMs)
                          .arg(cpuPressureActive() ? 1 : 0));
            trace.addDetail(QStringLiteral("adaptive_prefilter=%1").arg(adaptivePrefilteredRows));
            rows = filteredRows;
            duplicatesSkipped += adaptivePrefilteredRows;
        }
    }

    // Operational decisions must not wait for UI enrichment, map projection,
    // reporter batching or disk logging. Feed the sequencer from the raw
    // decoder result first, then publish the remaining work incrementally.
    bool const ft2SignoffRescueScan =
        !ft8DeepInTxListOnly
        && !resumedQso
        && m_mode == QStringLiteral("FT2")
        && m_asyncTxEnabled
        && !m_callsign.isEmpty()
        && (m_autoSeq || m_autoCqRepeat);
    bool autoCallStarted = false;
    if (!ft8DeepInTxListOnly && !resumedQso && m_autoCallEnabled) {
        autoCallStarted = tryAutoCallFromRows(rows);
    }
    bool const autoSeqActive =
        !ft8DeepInTxListOnly
        && !resumedQso
        && !m_callsign.isEmpty()
        && ((m_autoSeq && (m_txEnabled || !m_dxCall.isEmpty()))
            || m_autoCqRepeat);
    bool autoSeqGotResponse = false;
    if (!ft8DeepInTxListOnly && mamMultiStreamSequencerActive()) {
        for (QString const& row : std::as_const(rows)) {
            QStringList const fields = parseFt8Row(row);
            if (fields.size() >= 5) {
                mamIngestDecode(fields);
            }
        }
    } else if (!autoCallStarted && (autoSeqActive || ft2SignoffRescueScan)) {
        QStringList const feedRows = scoredCallerRowOrder(rows);
        for (QString const& row : feedRows) {
            QStringList const fields = parseFt8Row(row);
            if (fields.size() < 5) {
                continue;
            }
            QString const msgText = fields.at(4);
            QString localHashPartner;
            bool const directedToLiteralCall =
                messageContainsCallToken(msgText,
                                         m_callsign.trimmed().toUpper(),
                                         normalizedBaseCall(m_callsign.trimmed().toUpper()));
            bool const directedToLocalHash =
                !directedToLiteralCall
                && isDirectedToLocalHashFromActivePartner(msgText, &localHashPartner);
            if (!shouldAcceptDecodedMessage(msgText, nullptr, directedToLocalHash)) {
                continue;
            }
            QString const autoSeqDedupKey = fields.size() >= 8
                ? decodeDedupKey(decodeEntryTimeForRow(fields.at(0)),
                                 fields.at(7),
                                 msgText)
                : QString();
            if (isRecentNativeDuplicate(autoSeqDedupKey)) {
                continue;
            }
            bool const signoffRescue =
                !autoSeqActive
                && ft2SignoffRescueScan
                && isDirectedActivePartnerSignoffDecode(fields);
            if (!autoSeqActive && !signoffRescue) {
                continue;
            }
            QString localTxEchoReason;
            if (shouldSuppressRecentLocalTxEchoDecode(msgText,
                                                      QStringLiteral("ft-sync-autoseq"),
                                                      &localTxEchoReason)) {
                continue;
            }
            if (!directedToLiteralCall && !directedToLocalHash) {
                continue;
            }
            if (directedToLiteralCall
                && shouldSuppressDirectedGhostDecode(fields, QStringLiteral("ft-sync-autoseq"))) {
                continue;
            }
            if (signoffRescue) {
                bridgeLog(QStringLiteral("FT2 signoff rescue: feeding priority auto-seq: %1")
                              .arg(msgText));
            } else if (directedToLocalHash) {
                bridgeLog(QStringLiteral("autoSeq: priority local-hash decode from %1: %2")
                              .arg(localHashPartner, msgText));
            }
            autoSequenceStep(fields);
            autoSeqGotResponse = true;
        }
    }

    const bool useSmoothFlow = m_smoothDecodeFlow
        && rows.size() > 5
        && m_mode != QStringLiteral("FT2")
        && !timeSyncFlowMode
        && !isUiStallActive(500, 5000);
    if (useSmoothFlow) {
        // Flush residuo dello slot precedente (no merge tra serial diversi).
        if (!m_pendingDecodeReleaseQueue.isEmpty()
            && m_lastReleaseSerial != static_cast<qint64>(serial)) {
            for (auto const& e : std::as_const(m_pendingDecodeReleaseQueue)) {
                m_decodeList.append(QVariant(e));
                if (!e.value(QStringLiteral("isTx")).toBool()) {
                    if (m_nativeDecodeDedupSessionId != m_decodeSessionId) {
                        resetNativeDecodeDedupIndex();
                    }
                    rememberNativeDecodeDedupKey(decodeDedupKey(
                        e.value(QStringLiteral("time")).toString(),
                        e.value(QStringLiteral("freq")).toString(),
                        e.value(QStringLiteral("message")).toString()));
                }
                // 1.0.239 (Phase 5.2 fix): hook persistenza + counter sul
                // flush-serial-change smooth-flow path.
                noteDecodeCommitted();
                persistDecodeHistoryEntry(e);
            }
            m_pendingDecodeReleaseQueue.clear();
            trimDecodeListsIfNeeded();  // bounded in-memory decode history
            emitDecodeListChangedThrottled();
        }
        m_lastReleaseSerial = static_cast<qint64>(serial);
    }
    phaseAutoSeqMs = ft8PhaseTimer.elapsed();
    ft8PhaseTimer.restart();

    QHash<QString, QVariantMap> previousFt8EnrichmentBySemanticKey;
    if (decodeMode == QStringLiteral("FT8") && !legacyUiMirrorActive && !m_decodeList.isEmpty()) {
        int const firstPreviousRow = qMax(0, m_decodeList.size() - 500);
        previousFt8EnrichmentBySemanticKey.reserve(m_decodeList.size() - firstPreviousRow);
        for (int i = m_decodeList.size() - 1; i >= firstPreviousRow; --i) {
            QVariantMap const previous = m_decodeList.at(i).toMap();
            if (previous.value(QStringLiteral("isTx")).toBool()) {
                continue;
            }
            QString const previousMode =
                previous.value(QStringLiteral("mode")).toString().trimmed().toUpper();
            if (previousMode != QStringLiteral("FT8")) {
                continue;
            }
            QString const key = decodeMirrorSemanticKey(previous);
            if (!key.isEmpty() && !previousFt8EnrichmentBySemanticKey.contains(key)) {
                previousFt8EnrichmentBySemanticKey.insert(key, previous);
            }
        }
    }
    auto refreshReusedFt8DisplayFields = [this](QVariantMap& entry) {
        QString const timeStr = entry.value(QStringLiteral("time")).toString();
        QString digits;
        digits.reserve(timeStr.size());
        for (QChar const c : timeStr) {
            if (c.isDigit()) {
                digits.append(c);
            }
        }
        entry[QStringLiteral("formattedTime")] = digits.size() >= 6
            ? digits.left(2) + QLatin1Char(':') + digits.mid(2, 2)
                + QLatin1Char(':') + digits.mid(4, 2)
            : (digits.size() == 4
                   ? digits.left(2) + QLatin1Char(':') + digits.mid(2, 2)
                   : timeStr);

        QString snrHex;
        if (entry.value(QStringLiteral("isTx")).toBool()) {
            snrHex = QStringLiteral("#f1c40f");
        } else if (m_themeManager) {
            bool parseOk = false;
            int const dbVal = entry.value(QStringLiteral("db")).toString().toInt(&parseOk);
            QColor pick = parseOk && dbVal > -5
                ? m_themeManager->accentColor()
                : (parseOk && dbVal > -15
                       ? m_themeManager->secondaryColor()
                       : m_themeManager->textSecondary());
            snrHex = pick.name(QColor::HexRgb);
        }
        entry[QStringLiteral("snrColor")] = snrHex;

        QString const dxCallTrimmed = m_dxCall.trimmed().toUpper();
        QString const rightCall = entry.value(QStringLiteral("dxCallsign")).toString().trimmed().toUpper();
        QString const fromCall = entry.value(QStringLiteral("fromCall")).toString().trimmed().toUpper();
        entry[QStringLiteral("matchesDxCall")] =
            !dxCallTrimmed.isEmpty()
            && ((!rightCall.isEmpty() && rightCall == dxCallTrimmed)
                || (!fromCall.isEmpty() && fromCall == dxCallTrimmed));
    };
    auto tryReuseFt8Enrichment = [&](QVariantMap& entry) {
        if (previousFt8EnrichmentBySemanticKey.isEmpty()) {
            return false;
        }
        auto const it = previousFt8EnrichmentBySemanticKey.constFind(decodeMirrorSemanticKey(entry));
        if (it == previousFt8EnrichmentBySemanticKey.constEnd()) {
            return false;
        }

        QVariant const currentTime = entry.value(QStringLiteral("time"));
        QVariant const currentTimestamp = entry.value(QStringLiteral("timestamp"));
        QVariant const currentDb = entry.value(QStringLiteral("db"));
        QVariant const currentDt = entry.value(QStringLiteral("dt"));
        QVariant const currentQuality = entry.value(QStringLiteral("quality"));
        QVariant const currentAptype = entry.value(QStringLiteral("aptype"));
        QVariant const currentDecodeSessionId = entry.value(QStringLiteral("decodeSessionId"));
        QVariant const currentHasUnresolvedPeer = entry.value(QStringLiteral("hasUnresolvedPeer"));

        entry = it.value();
        entry[QStringLiteral("time")] = currentTime;
        entry[QStringLiteral("timestamp")] = currentTimestamp;
        entry[QStringLiteral("db")] = currentDb;
        entry[QStringLiteral("dt")] = currentDt;
        entry[QStringLiteral("quality")] = currentQuality;
        entry[QStringLiteral("aptype")] = currentAptype;
        entry[QStringLiteral("decodeSessionId")] = currentDecodeSessionId;
        entry[QStringLiteral("hasUnresolvedPeer")] = currentHasUnresolvedPeer;
        refreshReusedFt8DisplayFields(entry);
        return true;
    };
    QVector<HashResolveAllTxtCandidate> ft8HashResolveCandidates;
    bool ft8HashResolveCandidatesLoaded = false;
    auto ensureFt8HashResolveCandidatesLoaded = [&]() {
        if (ft8HashResolveCandidatesLoaded) {
            return;
        }
        ft8HashResolveCandidatesLoaded = true;
        QElapsedTimer loadTimer;
        loadTimer.start();
        static constexpr qint64 kHashResolveTailBytes = 8 * 1024 * 1024;
        static constexpr int kHashResolveMaxLinesPerFile = 50000;
        QStringList const paths = hashResolveLogCandidatePaths(legacyAllTxtPath());
        qint64 estimatedReadBytes = 0;
        for (QString const& path : paths) {
            QFileInfo const info(path);
            if (info.exists() && info.isFile()) {
                estimatedReadBytes += qMin(info.size(), kHashResolveTailBytes);
            }
        }
        bool const deferHashResolve =
            rows.size() > 8
            || cpuPressureActive()
            || estimatedReadBytes > 512 * 1024;
        if (deferHashResolve) {
            rowHashResolveLoadMs = 0;
            bridgeLog(QStringLiteral("[FT8-HASH] live resolve deferred rows=%1 pressure=%2 estimated_kb=%3 paths=%4")
                          .arg(rows.size())
                          .arg(cpuPressureActive() ? 1 : 0)
                          .arg(estimatedReadBytes / 1024)
                          .arg(paths.size()));
            return;
        }
        for (QString const& path : paths) {
            QFileInfo const info(path);
            if (!info.exists() || !info.isFile()) {
                continue;
            }

            QStringList const lines = readTextTailLines(info.absoluteFilePath(),
                                                        kHashResolveTailBytes,
                                                        kHashResolveMaxLinesPerFile);
            for (QString const& line : lines) {
                HashResolveAllTxtCandidate candidate;
                if (!parseHashResolveAllTxtLine(line, &candidate)
                    || candidate.message.contains(QStringLiteral("<...>"))) {
                    continue;
                }
                candidate.source = info.fileName();
                ft8HashResolveCandidates.append(candidate);
            }
        }
        rowHashResolveLoadMs = loadTimer.elapsed();
        if (rowHashResolveLoadMs >= 250) {
            bridgeLog(QStringLiteral("[FT8-HASH] batch seed loaded candidates=%1 paths=%2 elapsed_ms=%3")
                          .arg(ft8HashResolveCandidates.size())
                          .arg(paths.size())
                          .arg(rowHashResolveLoadMs));
        }
    };
    auto resolveFt8HashPlaceholdersFromBatch = [&](QString const& timeToken,
                                                   QString const& audioFreq,
                                                   QString const& message,
                                                   QString* sourceOut) {
        if (sourceOut) {
            sourceOut->clear();
        }
        QString const normalizedMode = (decodeMode.isEmpty() ? m_mode : decodeMode).trimmed().toUpper();
        if (normalizedMode != QStringLiteral("FT8")
            || !message.contains(QStringLiteral("<...>"))) {
            return message;
        }

        bool freqOk = false;
        int const targetAudioHz = audioFreq.trimmed().toInt(&freqOk);
        if (!freqOk || targetAudioHz <= 0
            || secondsFromUtcDisplayToken(timeToken) < 0) {
            return message;
        }

        ++rowHashResolveAttempts;
        QElapsedTimer resolveTimer;
        resolveTimer.start();
        ensureFt8HashResolveCandidatesLoaded();
        QString source;
        QString const resolved = resolveHashMessageFromCandidates(normalizeUtcDisplayToken(timeToken),
                                                                  targetAudioHz,
                                                                  message,
                                                                  ft8HashResolveCandidates,
                                                                  &source);
        rowHashResolveMs += resolveTimer.elapsed();
        if (resolved.compare(message, Qt::CaseInsensitive) != 0) {
            ++rowHashResolveResolved;
            if (sourceOut) {
                *sourceOut = source;
            }
            bridgeLog(QStringLiteral("[FT8-HASH] resolved from batch time=%1 hz=%2 '%3' -> '%4' source=%5 candidates=%6")
                          .arg(normalizeUtcDisplayToken(timeToken),
                               QString::number(targetAudioHz),
                               message,
                               resolved,
                               source.isEmpty() ? QStringLiteral("<unknown>") : source)
                          .arg(ft8HashResolveCandidates.size()));
            return resolved;
        }
        return message;
    };

    for (const auto& row : rows) {
        // parseFt8Row returns: [time, snr, dt, df, message, aptype, quality, freq]
        QStringList f = parseFt8Row(row);
        if (f.size() < 8) {
            bridgeLog("  parseFt8Row FAILED for: '" + row + "'");
            ++parseFailures;
            continue;
        }
        bool const signoffWatchCandidate = signoffWatch && isDirectedActivePartnerSignoffDecode(f);
        auto logSignoffWatchDrop = [&](QString const& stage, QString const& detail = QString()) {
            if (!signoffWatchCandidate) {
                return;
            }
            bridgeLog(QStringLiteral("[SIGNOFFRX] %1 mode=%2 serial=%3 msg='%4'%5 row='%6'")
                          .arg(stage,
                               decodeMode.isEmpty() ? m_mode : decodeMode,
                               QString::number(serial),
                               f.value(4),
                               detail.isEmpty() ? QString() : QStringLiteral(" detail=%1").arg(detail),
                               row));
        };
        if (signoffWatchCandidate) {
            bridgeLog(QStringLiteral("[SIGNOFFRX] raw candidate mode=%1 serial=%2 msg='%3' snr=%4 dt=%5 freq=%6")
                          .arg(decodeMode.isEmpty() ? m_mode : decodeMode,
                               QString::number(serial),
                               f.value(4),
                               f.value(1),
                               f.value(2),
                               f.value(7)));
        }

        // FDR: Shannon False Decode Reduction — 4 livelli di filtro
        {
            int snrVal = f[1].trimmed().toInt();
            float qualVal = f[6].trimmed().toFloat();
            if (!isValidDecode(f[4], snrVal, f[5], qualVal)) {
                bridgeLog("FDR: filtered decode: " + f[4]);
                logSignoffWatchDrop(QStringLiteral("filtered FDR"));
                ++guardrailFiltered;
                continue;  // salta questo decode
            }
        }

        QString semanticRejectReason;
        if (!shouldAcceptDecodedMessage(f[4], &semanticRejectReason, true)) {
            bridgeLog(QStringLiteral("semantic decode filter: %1 mode=%2 msg='%3'")
                          .arg(semanticRejectReason, decodeMode.isEmpty() ? m_mode : decodeMode, f[4]));
            logSignoffWatchDrop(QStringLiteral("filtered semantic"), semanticRejectReason);
            ++semanticFiltered;
            continue;
        }
        QString localTxEchoReason;
        if (shouldSuppressRecentLocalTxEchoDecode(f[4],
                                                  QStringLiteral("ft-sync-ui"),
                                                  &localTxEchoReason)) {
            logSignoffWatchDrop(QStringLiteral("filtered local-tx-echo"), localTxEchoReason);
            ++duplicatesSkipped;
            continue;
        }
        QString reportConflictReason;
        if (shouldSuppressConflictingDirectedReportDecode(f,
                                                          QStringLiteral("ft8-ui"),
                                                          &reportConflictReason)) {
            logSignoffWatchDrop(QStringLiteral("filtered report-conflict"), reportConflictReason);
            ++duplicatesSkipped;
            continue;
        }

        if (trackTimeSync) {
            bool snrOk = false;
            bool dtOk = false;
            int const snrVal = f[1].trimmed().toInt(&snrOk);
            double const dtVal = f[2].trimmed().toDouble(&dtOk);
            if (snrOk && dtOk && shouldTrackDtSample(snrVal, dtVal, f[4], decodeMode)) {
                dtSamples.append(dtVal);
            }
        }

        // Calcola proprietà derivate
        QString msg     = f[4];
        QString const entryTime = decodeEntryTimeForRow(f[0]);
        if (msg.contains(QStringLiteral("<...>"))) {
            QString hashResolveSource;
            QString const resolvedMsg =
                resolveFt8HashPlaceholdersFromBatch(entryTime,
                                                     f[7],
                                                     msg,
                                                     &hashResolveSource);
            if (resolvedMsg.compare(msg, Qt::CaseInsensitive) != 0) {
                msg = resolvedMsg;
                f[4] = msg;
            }
        }
        bool isCQ       = msg.startsWith("CQ ", Qt::CaseInsensitive) || msg == "CQ";
        bool const hasUnresolvedPlaceholder = msg.contains(QStringLiteral("<...>"));
        bool isMyCall = messageMentionsLocalCall(msg);
        if (!isMyCall && isDirectedToLocalHashFromActivePartner(msg)) {
            isMyCall = true;
        }
        QString fromCall = extractDecodedCallsign(msg, isCQ);

        QString const dedupKey = decodeDedupKey(entryTime, f[7], msg);
        // 1.0.538 iu8lmc - lo stesso segnale ridecodificato dalla passata
        // profonda torna con qualche hertz di scarto rispetto alla prima
        // stima. Con la sola chiave esatta erano due decodifiche distinte:
        // due righe in lista e due pacchetti UDP per lo stesso messaggio,
        // che i collettori esterni contavano come traffico doppio. Il
        // percorso FT2 async quantizzava gia' la frequenza per lo stesso
        // motivo; qui si confronta con una tolleranza di pochi hertz.
        bool nearDuplicate = false;
        if (!legacyUiMirrorActive && !dedupKey.isEmpty()) {
            bool freqOk = false;
            int const freqHz = f[7].trimmed().toInt(&freqOk);
            if (freqOk) {
                for (int delta = -kNativeDecodeDedupFreqToleranceHz;
                     delta <= kNativeDecodeDedupFreqToleranceHz && !nearDuplicate;
                     ++delta) {
                    if (delta == 0) {
                        continue;
                    }
                    nearDuplicate = isRecentNativeDuplicate(
                        decodeDedupKey(entryTime, QString::number(freqHz + delta), msg));
                }
            }
        }
        if (!legacyUiMirrorActive
            && !dedupKey.isEmpty()
            && (isRecentNativeDuplicate(dedupKey) || nearDuplicate)) {
            skippedDuplicateDecodeKeys.insert(dedupKey);
            logSignoffWatchDrop(QStringLiteral("filtered recent-duplicate"), dedupKey);
            ++duplicatesSkipped;
            continue;
        }

        QVariantMap entry;
        entry["time"]    = entryTime;
        entry["db"]      = f[1];
        entry["dt"]      = f[2];
        entry["freq"]    = f[7];   // frequenza assoluta audio Hz
        entry["message"] = msg;
        entry["aptype"]  = f[5];
        entry["quality"] = f[6];
        entry["mode"]    = decodeMode.isEmpty() ? m_mode : decodeMode;
        entry["isTx"]    = false;
        entry["isCQ"]    = isCQ;
        entry["isMyCall"] = isMyCall;
        entry["fromCall"] = fromCall;
        entry["hasUnresolvedPeer"] = hasUnresolvedPlaceholder;
        entry["decodeSessionId"] = static_cast<qulonglong>(m_decodeSessionId);
        entry["timestamp"] = static_cast<qulonglong>(
            decodeEntryTimestampMsecs(entryTime, QDateTime::currentMSecsSinceEpoch()));
        {
            QElapsedTimer sectionTimer;
            sectionTimer.start();
            if (tryReuseFt8Enrichment(entry)) {
                ++rowEnrichReused;
            } else {
                enrichDecodeEntry(entry);
                ++rowEnrichFresh;
            }
            rowEnrichMs += sectionTimer.elapsed();
        }
        isMyCall = entry.value(QStringLiteral("isMyCall")).toBool();
        if (m_hideGhostDecodes && looksLikeGhostDecode(entry)) {
            bridgeLog(QStringLiteral("ghost decode filter: mode=%1 msg='%2'")
                          .arg(entry.value("mode").toString(), msg));
            logSignoffWatchDrop(QStringLiteral("filtered ghost"));
            ++semanticFiltered;
            continue;
        }
        QString userFilterReason;
        if (!shouldAcceptDecodeEntryByUserFilters(entry, userFilterConfig, false, &userFilterReason)) {
            bridgeLog(QStringLiteral("user decode filter: %1 mode=%2 msg='%3'")
                          .arg(userFilterReason, entry.value("mode").toString(), msg));
            logSignoffWatchDrop(QStringLiteral("filtered user"), userFilterReason);
            ++userFiltered;
            continue;
        }
        if (!ft8DeepInTxListOnly && !hasUnresolvedPlaceholder) {
            maybeEnqueueMamCallerFromDecode(f);
            // Legacy owns its own incremental secondary queue. The native
            // path below queues secondary work independently of presentation;
            // UI filters must not silence calls addressed to the operator.
            if (legacyUiMirrorActive) {
                maybeQueuePskReporterSpot(entry, msg, isCQ, f[7], f[1], entry.value("mode").toString());
            }
        }

        // ALL.TXT must reflect valid decoder output, not the current frontend
        // presentation filter. Keep this before CQ/MyCall UI filters so
        // "CQ Only" hides reports on screen without losing them on disk.
        if (!legacyUiMirrorActive) {
            if (!dedupKey.isEmpty()) {
                rememberNativeDecodeDedupKey(dedupKey);
            }

            QString const utcNow = approxUtcDateTimeForDisplayToken(entryTime).toString("yyMMdd_hhmmss");
            double const freqMhz = m_frequency / 1e6;
            customAllTxtBatch += utcNow
                + QStringLiteral("  %1").arg(freqMhz, 10, 'f', 3)
                + QStringLiteral("  Rx  ")
                + QStringLiteral("%1").arg(m_mode, -6)
                + QStringLiteral("  %1").arg(f[1].trimmed(), 4)
                + QStringLiteral("  %1").arg(f[2].trimmed(), 5)
                + QStringLiteral("  %1").arg(f[7].trimmed(), 5)
                + QStringLiteral("  ")
                + msg
                + QLatin1Char('\n');

            QString const legacyMessage = canonicalDecodeMessage(msg);
            if (!legacyMessage.isEmpty()) {
                QString const legacyMode = m_mode.leftJustified(6, QLatin1Char(' '));
                legacyAllTxtBatch += QStringLiteral("%1 %2  Rx  %3%4%5%6  %7\n")
                    .arg(approxUtcDateTimeForDisplayToken(entryTime).toString(QStringLiteral("yyMMdd_hhmmss")))
                    .arg(freqMhz, 10, 'f', 3)
                    .arg(legacyMode)
                    .arg(f[1].trimmed(), 4)
                    .arg(f[2].trimmed(), 5)
                    .arg(f[7].trimmed(), 5)
                    .arg(legacyMessage);
            }
        }

        bool const filteredByCqOnly = !m_filtersBypassed && m_filterCqOnly && !passesCqFilter(isCQ, msg);
        bool const filteredByMyCallOnly = !m_filtersBypassed && m_filterMyCallOnly && !isMyCall;
        if (isMyCall) {
            ++myCallMatches;
        }
        if (!legacyUiMirrorActive) {
            NativeDecodeSecondaryWork secondaryWork;
            secondaryWork.entry = entry;
            secondaryWork.rawRow = row;
            secondaryWork.serial = serial;
            secondaryWork.key = dedupKey;
            secondaryWork.route(!filteredByCqOnly && !filteredByMyCallOnly, ft8DeepInTxListOnly);
            if (secondaryWork.hasWork()) {
                QElapsedTimer sectionTimer;
                sectionTimer.start();
                queueNativeDecodeSecondaryWork(std::move(secondaryWork));
                rowSecondaryQueueMs += sectionTimer.elapsed();
            }
        }
        if (filteredByCqOnly || filteredByMyCallOnly) {
            // L'archivio SQLite e' un registro come ALL.TXT e PSK Reporter, non
            // una vista: con "solo CQ" acceso perdeva tutte le righe dirette
            // (misurato il 10/9/2026: 14% conservate in un'ora con il filtro
            // acceso, 100% con il filtro spento). Si archivia prima di nascondere.
            persistDecodeHistoryEntry(entry);
            if (!legacyUiMirrorActive && filteredByCqOnly && !filteredByMyCallOnly) {
                appendRxDecodeEntry(entry);
            }
            if (filteredByCqOnly) {
                ++cqOnlyFiltered;
            }
            if (filteredByMyCallOnly) {
                ++myCallOnlyFiltered;
            }
            logSignoffWatchDrop(QStringLiteral("filtered ui"),
                                QStringLiteral("cqOnly=%1 myCallOnly=%2")
                                    .arg(filteredByCqOnly ? 1 : 0)
                                    .arg(filteredByMyCallOnly ? 1 : 0));
            ++uiFiltered;
            continue;
        }

        if (!ft8DeepInTxListOnly && !hasUnresolvedPlaceholder) {
            tryStartWaitPounceFromEntry(entry, m_decodeList, QStringLiteral("ft8"));
        }

        if (!legacyUiMirrorActive) {
            // 1.0.179 — Smooth Decode Flow: se attivo, accoda in release
            // queue invece di append-and-emit. Altrimenti path legacy.
            if (useSmoothFlow) {
                m_pendingDecodeReleaseQueue.append(entry);
            } else {
                // 1.0.239 (Phase 5.2 fix): usa appendDecodeMapToList per il
                // path legacy inline. Garantisce hook noteDecodeCommitted +
                // enqueuePersistDecode senza duplicare.
                QElapsedTimer sectionTimer;
                sectionTimer.start();
                appendDecodeMapToList(entry);
                rowAppendMs += sectionTimer.elapsed();
            }
            {
                QElapsedTimer sectionTimer;
                sectionTimer.start();
            appendRxDecodeEntry(entry);
                rowRxMirrorMs += sectionTimer.elapsed();
            }
            changed = true;
        }
        ++accepted;
        if (signoffWatchCandidate) {
            bridgeLog(QStringLiteral("[SIGNOFFRX] accepted mode=%1 serial=%2 msg='%3' isMyCall=%4")
                          .arg(decodeMode.isEmpty() ? m_mode : decodeMode,
                               QString::number(serial),
                               msg,
                               isMyCall ? QStringLiteral("1") : QStringLiteral("0")));
        }

        // C17 — Fox OTP: verifica codice se SuperFox mode attivo
        // Formato messaggio Fox: "CALL OTP" dove OTP è numerico a 6 cifre
        if (!ft8DeepInTxListOnly && m_foxMode && !isMyCall) {
            QStringList parts = msg.split(' ', Qt::SkipEmptyParts);
            if (parts.size() == 2) {
                const QString& possibleCode = parts[1];
                if (possibleCode.length() == 6 && possibleCode[0].isDigit()) {
                    int audioHz = f.size() > 7 ? f[7].toInt() : 750;
                    verifyOtp(parts[0], possibleCode, audioHz);
                }
            }
        }
    }
    phaseRowsMs = ft8PhaseTimer.elapsed();
    ft8PhaseTimer.restart();
    appendDecodeTextBatchesAsync(logAllTxtPath(),
                                 std::move(customAllTxtBatch),
                                 legacyAllTxtPath(),
                                 std::move(legacyAllTxtBatch));

    // cq_only/mycall_only are rejection counters, not alert matches.
    bridgeLog(QStringLiteral("onFt8DecodeReady summary: raw=%1 accepted=%2 parse_fail=%3 guardrail=%4 semantic=%5 user_filtered=%6 ui_filtered=%7 cq_only=%8 mycall_only=%9 dupes=%10 mycall_matches=%11")
                  .arg(rows.size())
                  .arg(accepted)
                  .arg(parseFailures)
                  .arg(guardrailFiltered)
                  .arg(semanticFiltered)
                  .arg(userFiltered)
                  .arg(uiFiltered)
                  .arg(cqOnlyFiltered)
                  .arg(myCallOnlyFiltered)
                  .arg(duplicatesSkipped)
                  .arg(myCallMatches));
    phasePostRowsMs = ft8PhaseTimer.elapsed();
    trace.addDetail(QStringLiteral("phase_ms=setup:%1 autoseq:%2 rows:%3 post:%4")
                        .arg(phaseSetupMs)
                        .arg(phaseAutoSeqMs)
                        .arg(phaseRowsMs)
                        .arg(phasePostRowsMs));
    trace.addDetail(QStringLiteral("row_phase_ms=hash:%1 hash_load:%2 enrich:%3 append:%4 rx:%5 secondary:%6 reuse:%7 fresh:%8 hash_attempts:%9 hash_resolved:%10")
                        .arg(rowHashResolveMs)
                        .arg(rowHashResolveLoadMs)
                        .arg(rowEnrichMs)
                        .arg(rowAppendMs)
                        .arg(rowRxMirrorMs)
                        .arg(rowSecondaryQueueMs)
                        .arg(rowEnrichReused)
                        .arg(rowEnrichFresh)
                        .arg(rowHashResolveAttempts)
                        .arg(rowHashResolveResolved));
    trace.addDetail(QStringLiteral("accepted=%1 parse_fail=%2 semantic=%3 user_filtered=%4 ui_filtered=%5 dupes=%6 changed=%7")
                        .arg(accepted)
                        .arg(parseFailures)
                        .arg(semanticFiltered)
                        .arg(userFiltered)
                        .arg(uiFiltered)
                        .arg(duplicatesSkipped)
                        .arg(changed ? 1 : 0));
    if (rows.isEmpty()) {
        ++m_zeroRawDecodeStreak;
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const bool recentAudioHealth = m_lastAudioHealthMs > 0 && now - m_lastAudioHealthMs <= 3000;
        const bool overdrivenAudio =
            recentAudioHealth
            && m_lastAudioHealthPeak >= 0.98
            && m_lastAudioHealthRms >= 0.35;
        if (m_zeroRawDecodeStreak >= 4
            && overdrivenAudio
            && now - m_lastZeroDecodeAudioWarningMs >= 15000) {
            bridgeLog(QStringLiteral("Decode warning: zero raw FT rows while RX audio is overdriven streak=%1 rms=%2 peak=%3 clipped=%4/%5")
                          .arg(m_zeroRawDecodeStreak)
                          .arg(m_lastAudioHealthRms, 0, 'g', 4)
                          .arg(m_lastAudioHealthPeak, 0, 'g', 4)
                          .arg(m_lastAudioHealthClippedSamples)
                          .arg(m_lastAudioHealthSamples));
            emit statusMessage(QStringLiteral("Nessuna decodifica: audio RX saturo/clippato"));
            m_lastZeroDecodeAudioWarningMs = now;
        }
    } else {
        m_zeroRawDecodeStreak = 0;
    }
    // 1.0.179 — Smooth Decode Flow: se la queue smooth ha entry, arma il
    // drain timer con interval adattivo. Spread target ~periodMs-2s entro
    // [1500..3000]ms, intervallo per row in [80..200]ms, chunk=2 oltre 25.
    if (useSmoothFlow && !m_pendingDecodeReleaseQueue.isEmpty()) {
        int const n = m_pendingDecodeReleaseQueue.size();
        int const periodMs = periodMsForMode(m_mode);
        int const spreadMs = qBound(1500, periodMs - 2000, 3000);
        int const intervalMs = qBound(80, spreadMs / qMax(1, n), 200);
        m_decodeReleaseChunkSize = (n > 25) ? 2 : 1;
        if (m_decodeReleaseTimer) m_decodeReleaseTimer->start(intervalMs);
        bridgeLog(QStringLiteral("[SmoothFlow] queue=%1 chunk=%2 interval=%3ms")
                      .arg(n).arg(m_decodeReleaseChunkSize).arg(intervalMs));
    } else if (changed) {
        emitDecodeListChangedThrottled();  // 1.0.142: throttle FT8 burst
    }
    if (legacyUiMirrorActive) {
        syncLegacyBackendDecodeList();
    }
    finalizeTimeSyncDecodeCycle(serial, decodeMode, dtSamples);
    m_decoding = false;
    emit decodingChanged();
    bool ft8AutoTxDecodeSettled = false;
    if (decodeMode == QStringLiteral("FT8") && !ft8DeepInTxListOnly
        && serial == m_ft8AutoTxFinalDecodeSerial && usesFt8AutoTxDecodeGate()) {
        using Gate = decodium::Ft8AutoTxDecodeGate;
        auto const context = ft8AutoTxDecodeContext();
        // Only the final pass for the immediately preceding RX slot releases
        // TX, including an empty result (a legitimate retry). An early or
        // list-only deep result must not pre-empt the final partner response.
        if (forcedUtcToken == utcDisplayTokenForSlotStart(context.slot - 1, Gate::periodMs)) {
            int const elapsed = int(correctedUtcEpochMs() % Gate::periodMs);
            m_ft8AutoTxDecodeGate.decoded(context, elapsed);
            ft8AutoTxDecodeSettled = true;
            bridgeLog(QStringLiteral("FT8 auto TX: previous RX result processed serial=%1 elapsed=%2ms response=%3")
                          .arg(serial).arg(elapsed).arg(autoSeqGotResponse ? 1 : 0));
        }
    }
    if ((autoSeqGotResponse || ft8AutoTxDecodeSettled) && !m_transmitting && !m_tuning) {
        QString const autoTxMode = decodeMode.isEmpty() ? m_mode : decodeMode;
        QString const normalizedAutoTxMode = autoTxMode.trimmed().toUpper();
        bool canStartInThisSlot = true;
        if (!usesFt8AutoTxDecodeGate() && isTimeSyncDecodeMode(normalizedAutoTxMode)
            && !(normalizedAutoTxMode == QStringLiteral("FT2") && m_asyncTxEnabled)) {
            int const periodMs = periodMsForMode(normalizedAutoTxMode);
            if (periodMs > 0) {
                int const elapsedMs = static_cast<int>(
                    correctedUtcEpochMs() % static_cast<qint64>(periodMs));
                // P1-A: stesso grace CLAMPATO sotto pressione del fallback timer, altrimenti
                // su macchine lente FT8/FT4 una risposta decodificata tra 700 e 900ms verrebbe
                // differita qui mentre il fallback l'avrebbe accettata -> ciclo perso.
                int const graceMs = effectiveAutoTxDecodeGraceMs(normalizedAutoTxMode);
                // 1.0.317 — opt-in ft8FastSequence: invece di scartare lo slot quando
                // il decode arriva tardivo (>grace), accetta fino a d3CapMs (~11s slot 15s).
                // Il TX risulta shiftato ma resta nello stesso slot → niente +15s di defer.
                // Su PC carichi la pressure-clamp in effectiveAutoTxDecodeGraceMs già
                // forza grace>=900ms quindi il flag non danneggia la stabilità.
                int effectiveCap = graceMs;
                bool const activeQsoLateDecodeCanStillReply =
                    (normalizedAutoTxMode == QStringLiteral("FT8") || normalizedAutoTxMode == QStringLiteral("FT4"))
                    && !m_dxCall.trimmed().isEmpty()
                    && m_currentTx >= 1
                    && m_currentTx <= 5
                    && (m_lastNtx >= 1 || m_qsoProgress > 2 || m_pendingAutoSeqTxAfterActiveTx > 0);
                if ((m_ft8FastSequence && normalizedAutoTxMode == QStringLiteral("FT8"))
                    || activeQsoLateDecodeCanStillReply) {
                    int const periodMs = periodMsForMode(normalizedAutoTxMode);
                    if (periodMs > 0) {
                        int const latestStartMs = latestD3CompatibleSyncTxStartMs(normalizedAutoTxMode, periodMs, true);
                        effectiveCap = qMax(graceMs, latestStartMs);
                    }
                }
                if (elapsedMs > effectiveCap) {
                    canStartInThisSlot = false;
                    bridgeLog(QStringLiteral("onFt8DecodeReady: auto-seq response after TX start grace, deferring to next slot mode=%1 elapsed=%2ms grace=%3ms cap=%4ms")
                                  .arg(autoTxMode)
                                  .arg(elapsedMs)
                                  .arg(graceMs)
                                  .arg(effectiveCap));
                }
            }
        }
        if (canStartInThisSlot) {
            bridgeLog(QStringLiteral("onFt8DecodeReady: auto-seq response processed, checking TX"));
            checkAndStartPeriodicTx();
        }
    }

    // F1 subpass harvest: al RISULTATO del deep, lancia un terzo decode subpass=1
    // list-only nel gap prima del boundary (recupero deboli tardivo, mai auto-seq/TX).
    // Cap auto-dimensionato al gap (F1a conservativo: niente preemption).
    {
        Ft8PendingDeepFollowup pendingHarvest = m_ft8PendingSubpassHarvest.take(serial);
        if (!pendingHarvest.audio.isEmpty() && m_ft8SubpassHarvest
            && m_mode == QStringLiteral("FT8")
            && pendingHarvest.sessionId == m_decodeSessionId) {
            bool const harvestBusyTx = m_transmitting || m_tuning;
            bool const harvestPressure = m_lowCpuModeEnabled || cpuPressureActive();
            int const harvestPeriodMs = periodMsForMode(QStringLiteral("FT8"));
            int const harvestIntoSlotMs = harvestPeriodMs > 0
                ? static_cast<int>(correctedUtcEpochMs() % harvestPeriodMs) : 0;
            static constexpr int kHarvestBoundaryMarginMs = 1500;
            static constexpr int kHarvestMinUsefulMs = 4000;
            static constexpr int kHarvestMaxMs = 6500;  // F1a: cap basso, finisce prima del boundary
            int const harvestGapMs = qMax(0, harvestPeriodMs - harvestIntoSlotMs - kHarvestBoundaryMarginMs);
            int const harvestCapMs = qMin(harvestGapMs, kHarvestMaxMs);
            if (!harvestBusyTx && !harvestPressure && harvestCapMs >= kHarvestMinUsefulMs) {
                quint64 const harvestSerial = ++m_decodeSerial;
                m_decodeStartMsBySerial.insert(harvestSerial, QDateTime::currentMSecsSinceEpoch());
                m_decodeModeBySerial.insert(harvestSerial, QStringLiteral("FT8"));
                m_decodeUtcTokenBySerial.insert(harvestSerial, pendingHarvest.utcToken);
                m_decodeSessionBySerial.insert(harvestSerial, m_decodeSessionId);
                bridgeLog(QStringLiteral("FT8 subpass harvest dispatch: serial=%1 capMs=%2 gapMs=%3 intoSlotMs=%4")
                              .arg(harvestSerial).arg(harvestCapMs).arg(harvestGapMs).arg(harvestIntoSlotMs));
                queueFt8DecodeRequest(pendingHarvest.audio, harvestSerial, pendingHarvest.nutc,
                                      pendingHarvest.slotIndex, pendingHarvest.decodeDepth,
                                      pendingHarvest.decodeQsoProgress, pendingHarvest.cqHint,
                                      50, pendingHarvest.ft8ApEnabled, false, true, harvestCapMs, true);
            } else {
                bridgeLog(QStringLiteral("FT8 subpass harvest skipped: tx=%1 pressure=%2 gapMs=%3 capMs=%4")
                              .arg(harvestBusyTx ? 1 : 0).arg(harvestPressure ? 1 : 0)
                              .arg(harvestGapMs).arg(harvestCapMs));
            }
        }
    }
    Ft8PendingDeepFollowup pendingDeep = m_ft8PendingDeepFollowups.take(serial);
    if (!pendingDeep.audio.isEmpty()) {
        static constexpr int kFt8DeepDispatchSafetyMs = 250;
        static constexpr int kFt8DeepMaxLiveBudgetMs = 10500;
        // La fase profonda di FT8 e' ACCESA: la soglia e' 2500 ms, che il
        // budget reale raggiunge (osservato fra 5500 e 6150 ms sul campo).
        //
        // Storia, perche' non si ripeta. La soglia nasce a 7000 ms, cioe' PIU'
        // del massimo ottenibile: il budget e' latestCompleteMs - adesso - 250,
        // e latestCompleteMs e' la fine dello slot piu' kFt8DeepLatestOverrunMs
        // (6800), quindi il tetto e' 6550 ms con dispatch istantaneo. La
        // condizione non poteva mai essere vera: il follow-up profondo veniva
        // sempre scartato, lo scarto veniva letto come backlog del worker e
        // faceva scattare il cooldown di 6 slot. AP, profondita' 4 e subpass
        // non giravano mai in FT8, col pulsante GAL acceso a vuoto.
        //
        // Abbassarla nella 1.0.595 riaccese lo stadio e con esso un difetto
        // che dormiva da quando la soglia era stata introdotta: l'applicazione
        // moriva entro due minuti con corruzione dello heap (0xc0000374).
        // La 1.0.596 la rimise a 7000 per spegnere tutto.
        //
        // La 1.0.597 ha trovato la causa, che NON era la fase profonda: era il
        // ridimensionamento del pool OpenMP in FT8DecodeWorker.cpp. Cambiare il
        // numero di thread a ogni decodifica distrugge e ricrea il pool, e i
        // thread che muoiono eseguono i distruttori dei propri oggetti
        // thread_local dentro LdrShutdownThread, con heap e loader gia' in
        // smontaggio. La fase profonda raddoppiava soltanto il ricambio di
        // thread, rendendo sistematico un difetto altrimenti raro. Corretto
        // quello, verificato in aria: 23 minuti, 138 lanci, zero crash,
        // memoria stabile.
        //
        // DECODIUM_FT8_DEEP_MIN_BUDGET rialza la soglia per spegnere lo stadio
        // (un valore sopra 6550 lo disattiva del tutto).
        static int const kFt8DeepMinUsefulBudgetMs = [] {
          char const* raw = std::getenv ("DECODIUM_FT8_DEEP_MIN_BUDGET");
          int const v = raw ? std::atoi (raw) : 0;
          return (v > 0 && v <= 20000) ? v : 2500;
        }();
        qint64 const correctedNowMs = correctedUtcEpochMs();
        int const budgetMs =
            qBound(0,
                   static_cast<int>(pendingDeep.latestCompleteMs
                                    - correctedNowMs
                                    - kFt8DeepDispatchSafetyMs),
                   kFt8DeepMaxLiveBudgetMs);
        qInfo().noquote()
            << QStringLiteral("[FT8DISPATCH] triggerSerial=%1 followupBudgetMs=%2 latestMs=%3 nowMs=%4 pendingDepth=%5 ft8ap=%6")
                   .arg(serial)
                   .arg(budgetMs)
                   .arg(pendingDeep.latestCompleteMs)
                   .arg(correctedNowMs)
                   .arg(pendingDeep.decodeDepth)
                   .arg(pendingDeep.ft8ApEnabled ? 1 : 0);
        bool const stalePending = pendingDeep.sessionId != m_decodeSessionId
            || m_mode != QStringLiteral("FT8");
        bool const busyForTx = m_transmitting || m_tuning;
        bool const pressure = m_lowCpuModeEnabled || cpuPressureActive();
        if (stalePending || busyForTx || pressure || budgetMs < kFt8DeepMinUsefulBudgetMs) {
            // P0 (1.0.403): se il deep e' tornato troppo tardi per essere utile
            // (budgetMs < min) e NON per stale/tx/pressure, e' un segnale di backlog
            // sul runtime_mutex single-flight: il worker e' indietro. Raffreddiamo il
            // deep per qualche slot cosi' la pipeline early+fast drena e i fast pass
            // tornano a essere consegnati (la produzione non e' mai toccata).
            if (budgetMs < kFt8DeepMinUsefulBudgetMs && !stalePending && !busyForTx && !pressure) {
                static constexpr int kFt8DeepBacklogCooldownSlots = 6;
                m_ft8DeepFollowupCooldownSlots = kFt8DeepBacklogCooldownSlots;
                bridgeLog(QStringLiteral("FT8 deep follow-up backlog: cooldown %1 slot (worker indietro, budgetMs=%2)")
                              .arg(kFt8DeepBacklogCooldownSlots)
                              .arg(budgetMs));
            }
            bridgeLog(QStringLiteral("FT8 final deep followup skipped after fast: budgetMs=%1 stale=%2 txBusy=%3 pressure=%4 latestMs=%5 nowMs=%6")
                          .arg(budgetMs)
                          .arg(stalePending ? 1 : 0)
                          .arg(busyForTx ? 1 : 0)
                          .arg(pressure ? 1 : 0)
                          .arg(pendingDeep.latestCompleteMs)
                          .arg(correctedNowMs));
        } else {
            quint64 const deepSerial = ++m_decodeSerial;
            qint64 const nowWallMs = QDateTime::currentMSecsSinceEpoch();
            m_decodeStartMsBySerial.insert(deepSerial, nowWallMs);
            m_decodeModeBySerial.insert(deepSerial, QStringLiteral("FT8"));
            m_decodeUtcTokenBySerial.insert(deepSerial, pendingDeep.utcToken);
            m_decodeSessionBySerial.insert(deepSerial, m_decodeSessionId);

            // 1.0.392 - fix regressione upstream 1.0.389: il deep follow-up FUORI TX
            // deve passare per il processing completo (auto-seq, resume, MAM, alert)
            // come in 1.0.299: la reply del partner debole spesso esiste SOLO nel
            // deep (AP/depth-4, il fast gira a depth<=2 senza AP). Le righe gia'
            // decodificate dal fast sono dedupate dall'indice incrementale ->
            // skippedDuplicateDecodeKeys, quindi niente doppio autoSequenceStep.
            // List-only resta SOLO per il follow-up in TX (niente avanzamenti di
            // stato mentre trasmettiamo).
            int const deepThreadCount = adaptiveInteractiveFtThreadCount(effectiveFtThreadLimit());
            bridgeLog(QStringLiteral("FT8 final deep followup dispatch: triggerSerial=%1 serial=%2 depth=%3 ft8ap=%4 maxMs=%5 threads=%6 listOnly=%7%8")
                          .arg(serial)
                          .arg(deepSerial)
                          .arg(pendingDeep.decodeDepth)
                          .arg(pendingDeep.ft8ApEnabled ? 1 : 0)
                          .arg(budgetMs)
                          .arg(deepThreadCount)
                          .arg(pendingDeep.inTx ? 1 : 0)
                          .arg(pendingDeep.inTx ? QStringLiteral(" inTx=1") : QString()));
            queueFt8DecodeRequest(pendingDeep.audio, deepSerial, pendingDeep.nutc,
                                  pendingDeep.slotIndex, pendingDeep.decodeDepth,
                                  pendingDeep.decodeQsoProgress, pendingDeep.cqHint,
                                  50, pendingDeep.ft8ApEnabled, false, pendingDeep.inTx,
                                  budgetMs, false, deepThreadCount);
            if (m_ft8SubpassHarvest) {
                // F1: prepara l'harvest subpass (terzo decode nel gap dopo il deep).
                Ft8PendingDeepFollowup harvest = pendingDeep;
                harvest.sessionId = m_decodeSessionId;
                m_ft8PendingSubpassHarvest.clear();  // al piu' 1 pending (single-flight)
                m_ft8PendingSubpassHarvest.insert(deepSerial, std::move(harvest));
            }
        }
    }
}

