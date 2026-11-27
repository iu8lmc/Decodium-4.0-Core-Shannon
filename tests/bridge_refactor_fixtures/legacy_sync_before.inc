void DecodiumBridge::syncLegacyBackendDecodeList()
{
    bool const rttyApplicationMode = isStreamingKeyboardMode(m_mode);
#if DECODIUM_HAS_SSTV
    if (m_sstvWorkspaceActive || rttyApplicationMode) {
        // Discard any final queued legacy result. The ALL.TXT cursor is
        // advanced once at workspace exit, so nothing from the suspended
        // interval is mirrored after normal monitoring resumes.
        if (m_legacyBackend) {
            if (!m_legacyBackend->bandActivityLines().isEmpty()) {
                m_legacyBackend->clearBandActivity();
            }
            if (!m_legacyBackend->rxFrequencyLines().isEmpty()) {
                m_legacyBackend->clearRxFrequency();
            }
            m_legacyBandActivityRevision =
                m_legacyBackend->bandActivityRevision();
            m_legacyRxFrequencyRevision =
                m_legacyBackend->rxFrequencyRevision();
        }
        return;
    }
#else
    if (rttyApplicationMode) {
        if (m_legacyBackend) {
            if (!m_legacyBackend->bandActivityLines().isEmpty()) {
                m_legacyBackend->clearBandActivity();
            }
            if (!m_legacyBackend->rxFrequencyLines().isEmpty()) {
                m_legacyBackend->clearRxFrequency();
            }
            m_legacyBandActivityRevision = m_legacyBackend->bandActivityRevision();
            m_legacyRxFrequencyRevision = m_legacyBackend->rxFrequencyRevision();
        }
        return;
    }
#endif
    if (!usingLegacyBackendForTx()) {
        return;
    }
    if (m_syncingLegacyBackendDecodeList) {
        return;
    }
    QScopedValueRollback<bool> legacyDecodeGuard(m_syncingLegacyBackendDecodeList, true);

    auto dropModeChangeClearedEntries = [this](QVariantList const& entries) {
        if (m_legacyModeChangeClearedDecodeKeys.isEmpty()) {
            return entries;
        }
        QVariantList filtered;
        filtered.reserve(entries.size());
        for (QVariant const& value : entries) {
            QVariantMap const entry = value.toMap();
            if (m_legacyModeChangeClearedDecodeKeys.contains(decodeMirrorEntryKey(entry))) {
                continue;
            }
            filtered.append(entry);
        }
        return filtered;
    };
    auto dropPrunedBandEntries = [this](QVariantList const& entries) {
        if (m_legacyPrunedBandMirrorKeys.isEmpty()) {
            return entries;
        }
        QVariantList filtered;
        filtered.reserve(entries.size());
        for (QVariant const& value : entries) {
            QVariantMap const entry = value.toMap();
            QString const key = decodeMirrorEntryKey(entry);
            if (!key.isEmpty() && m_legacyPrunedBandMirrorKeys.contains(key)) {
                continue;
            }
            filtered.append(entry);
        }
        return filtered;
    };
    auto dropRxClearedEntries = [this](QVariantList const& entries) {
        if (m_clearedRxDecodeKeys.isEmpty() && m_legacyClearedRxMirrorKeys.isEmpty()) {
            return entries;
        }
        QVariantList filtered;
        filtered.reserve(entries.size());
        for (QVariant const& value : entries) {
            QVariantMap const entry = value.toMap();
            QString const clearKey = decodeRxClearEntryKey(entry);
            QString const mirrorKey = decodeMirrorEntryKey(entry);
            if ((!clearKey.isEmpty() && m_clearedRxDecodeKeys.contains(clearKey))
                || (!mirrorKey.isEmpty() && m_legacyClearedRxMirrorKeys.contains(mirrorKey))) {
                continue;
            }
            filtered.append(entry);
        }
        return filtered;
    };
    auto dropConflictingDirectedReportEntries = [this](QVariantList const& entries, const QString& source) {
        QVariantList filtered;
        filtered.reserve(entries.size());
        for (QVariant const& value : entries) {
            QVariantMap const entry = value.toMap();
            QStringList fields;
            fields << entry.value(QStringLiteral("time")).toString()
                   << entry.value(QStringLiteral("db")).toString()
                   << entry.value(QStringLiteral("dt")).toString()
                   << entry.value(QStringLiteral("freq")).toString()
                   << entry.value(QStringLiteral("message")).toString()
                   << entry.value(QStringLiteral("aptype")).toString()
                   << entry.value(QStringLiteral("quality")).toString()
                   << entry.value(QStringLiteral("freq")).toString();
            QString conflictReason;
            if (shouldSuppressConflictingDirectedReportDecode(fields, source, &conflictReason)) {
                continue;
            }
            filtered.append(entry);
        }
        return filtered;
    };

    int const bandRevision = m_legacyBackend->bandActivityRevision();
    int const rxRevision = m_legacyBackend->rxFrequencyRevision();
    bool const bandChanged = bandRevision != m_legacyBandActivityRevision;
    bool const rxChanged = rxRevision != m_legacyRxFrequencyRevision;
    QString allTxtRevisionKey;
    if (isTimeSyncDecodeMode(m_mode)) {
        QString const path = legacyAllTxtPath().trimmed();
        if (!path.isEmpty()) {
            QFileInfo const info(path);
            if (info.exists()) {
                allTxtRevisionKey =
                    info.absoluteFilePath()
                    + QStringLiteral("|")
                    + QString::number(info.size())
                    + QStringLiteral("|")
                    + QString::number(info.lastModified().toMSecsSinceEpoch());
            } else {
                allTxtRevisionKey = path;
            }
        }
    }
    bool const allTxtChanged = allTxtRevisionKey != m_legacyAllTxtRevisionKey;

    bool bandDeltaReset = false;
    bool rxDeltaReset = false;
    QStringList const bandDeltaLines = bandChanged
        ? m_legacyBackend->takeBandActivityDelta(&bandDeltaReset)
        : QStringList {};
    QStringList const rxDeltaLines = rxChanged
        ? m_legacyBackend->takeRxFrequencyDelta(&rxDeltaReset)
        : QStringList {};
    bool const useBandDelta = m_legacyBandActivityRevision >= 0 && !bandDeltaReset;
    bool const useRxDelta = m_legacyRxFrequencyRevision >= 0 && !rxDeltaReset;

    if (!bandChanged && !rxChanged && !allTxtChanged) {
        return;
    }

    DecodeUserFilterConfig const userFilterConfig = readDecodeUserFilterConfig();
    auto applyLegacyUiFilters = [this, &userFilterConfig](QVariantList const& entries, bool applyCqOnly) {
        QVariantList filtered;
        filtered.reserve(entries.size());
        for (QVariant const& value : entries) {
            QVariantMap const entry = value.toMap();
            if (!entry.value(QStringLiteral("isTx")).toBool()) {
                if (!shouldAcceptDecodeEntryByUserFilters(entry, userFilterConfig, false, nullptr)) {
                    continue;
                }
                if (applyCqOnly
                    && !m_filtersBypassed
                    && m_filterCqOnly
                    && !passesCqFilter(entry.value(QStringLiteral("isCQ")).toBool(),
                                       entry.value(QStringLiteral("message")).toString())) {
                    continue;
                }
                if (!m_filtersBypassed
                    && m_filterMyCallOnly
                    && !entry.value(QStringLiteral("isMyCall")).toBool()) {
                    continue;
                }
            }
            filtered.append(entry);
        }
        return filtered;
    };
    MainThreadTraceScope trace(QStringLiteral("sync_legacy_backend_decode_list"),
                               QStringLiteral("band_changed=%1 rx_changed=%2 alltxt_changed=%3 band_rev=%4 rx_rev=%5 decode_rows=%6 rx_rows=%7")
                                   .arg(bandChanged ? 1 : 0)
                                   .arg(rxChanged ? 1 : 0)
                                   .arg(allTxtChanged ? 1 : 0)
                                   .arg(bandRevision)
                                   .arg(rxRevision)
                                   .arg(m_decodeList.size())
                                   .arg(m_rxDecodeList.size()));

    if (m_dxccLookup && !m_dxccLookup->isLoaded()) {
        loadDxccLookupAsync();
    }

    auto feedMamQueueFromEntries = [this](QVariantList const& entries) {
        if ((!m_multiAnswerMode && !m_autoCqRepeat) || m_manualTxHold) {
            return;
        }
        // With native MAM multi-stream enabled, mamIngestDecode() is the
        // authoritative per-call state machine.  Feeding this legacy queue
        // first used to create a second, serial QSO alongside the slots; the
        // mixer then sent the slots while the UI/sequencer still followed the
        // unrelated serial partner.
        if (mamMultiStreamSequencerActive()) {
            return;
        }
        for (QVariant const& value : entries) {
            QVariantMap const entry = value.toMap();
            if (entry.value(QStringLiteral("isTx")).toBool()) {
                continue;
            }
            QString const message = entry.value(QStringLiteral("message")).toString().trimmed();
            if (message.isEmpty()) {
                continue;
            }
            QString staleReason;
            if (!decodeIsFreshForCqAutoReply(entry.value(QStringLiteral("time")).toString(),
                                             &staleReason)) {
                bridgeLog(QStringLiteral("MAM queue: skip stale directed decode (%1): %2")
                              .arg(staleReason, message));
                continue;
            }
            bool freqOk = false;
            int const freq = entry.value(QStringLiteral("freq")).toString().trimmed().toInt(&freqOk);
            int const snr = decodeSnrOrDefault(entry.value(QStringLiteral("db")).toString(), -99);
            QStringList fields;
            fields << entry.value(QStringLiteral("time")).toString()
                   << entry.value(QStringLiteral("db")).toString()
                   << entry.value(QStringLiteral("dt")).toString()
                   << entry.value(QStringLiteral("freq")).toString()
                   << message
                   << entry.value(QStringLiteral("aptype")).toString()
                   << entry.value(QStringLiteral("quality")).toString()
                   << entry.value(QStringLiteral("freq")).toString();
            if (shouldSuppressDirectedGhostDecode(fields, QStringLiteral("legacy-mirror-mam"))) {
                continue;
            }
            maybeEnqueueMamCallerFromMessage(message, freqOk ? freq : -1, snr, true);
        }
    };
    auto feedWaitPounceFromEntries = [this](QVariantList const& entries,
                                            QVariantList const& contextEntries,
                                            QString const& source) {
        if (!m_waitPounceActive) {
            return;
        }

        for (QVariant const& value : entries) {
            QVariantMap const entry = value.toMap();
            if (entry.value(QStringLiteral("isTx")).toBool()) {
                continue;
            }
            if (tryStartWaitPounceFromEntry(entry, contextEntries, source)) {
                bridgeLog(QStringLiteral("legacy-mirror Wait & Pounce feed accepted source=%1 msg=%2")
                              .arg(source,
                                   entry.value(QStringLiteral("message")).toString().trimmed()));
                return;
            }
        }
    };
    auto feedAutoSequenceFromEntries = [this](QVariantList const& entries) {
        bool const autoSeqActive = !m_callsign.isEmpty()
            && ((m_autoSeq && (m_txEnabled || !m_dxCall.isEmpty()))
                || m_autoCqRepeat);
        // A QSO already owned by MAM must still receive its final confirmation
        // when the operator stops AutoCQ/MAM between our RR73 and the peer's
        // 73.  In that situation no new MAM QSO may be started, but dropping
        // the decode here leaves the existing slot unlogged forever.
        bool const mamCompletionPending = !m_mamSlots.isEmpty()
            || !m_mamLateReportRecoveries.isEmpty();
        if ((!autoSeqActive && !mamCompletionPending) || m_manualTxHold) {
            return;
        }
        QString const myCallUpper = m_callsign.trimmed().toUpper();
        QString const myBaseUpper = normalizedBaseCall(myCallUpper);
        int const nowSeconds =
            QDateTime::currentDateTimeUtc().time().msecsSinceStartOfDay() / 1000;
        int const periodMs = periodMsForMode(m_mode);
        int const freshnessWindowSeconds = qMax(20, (periodMs > 0 ? periodMs / 1000 : 15) * 2 + 5);
        auto const isFreshLegacyDecode = [&](QString const& token) {
            int const decodeSeconds = secondsFromUtcDisplayToken(token);
            if (decodeSeconds < 0) {
                return true;
            }
            int diff = std::abs(nowSeconds - decodeSeconds);
            diff = qMin(diff, 86400 - diff);
            return diff <= freshnessWindowSeconds;
        };

        for (QVariant const& value : entries) {
            QVariantMap const entry = value.toMap();
            if (entry.value(QStringLiteral("isTx")).toBool()) {
                continue;
            }
            QString const message = entry.value(QStringLiteral("message")).toString().trimmed();
            if (message.isEmpty()) {
                continue;
            }
            bool const directedToMe = entry.value(QStringLiteral("isMyCall")).toBool()
                || messageContainsCallToken(message, myCallUpper, myBaseUpper)
                || messageMentionsLocalCall(message);
            if (!directedToMe) {
                continue;
            }
            if (!isFreshLegacyDecode(entry.value(QStringLiteral("time")).toString())) {
                continue;
            }
            QString staleReason;
            if (!decodeIsFreshForCqAutoReply(entry.value(QStringLiteral("time")).toString(),
                                             &staleReason)) {
                bridgeLog(QStringLiteral("legacy-mirror autoSeq skip stale directed decode (%1): %2")
                              .arg(staleReason, message));
                continue;
            }

            QStringList fields;
            fields << entry.value(QStringLiteral("time")).toString()
                   << entry.value(QStringLiteral("db")).toString()
                   << entry.value(QStringLiteral("dt")).toString()
                   << entry.value(QStringLiteral("freq")).toString()
                   << message
                   << entry.value(QStringLiteral("aptype")).toString()
                   << entry.value(QStringLiteral("quality")).toString()
                   << entry.value(QStringLiteral("freq")).toString();
            QString const mamPartner = inferPartnerFromDirectedMessage(message,
                                                                         myCallUpper,
                                                                         myBaseUpper);
            QString const mamPartnerBase = normalizedBaseCall(mamPartner);
            bool const matchesExistingMam = !mamPartnerBase.isEmpty()
                && (mamSlotIndexForCall(mamPartnerBase) >= 0
                    || m_mamLateReportRecoveries.contains(mamPartnerBase));

            // Once MAM is disabled, accept only decodes that belong to an
            // existing MAM QSO. This preserves final-73 logging without
            // allowing a new caller to enter MAM while it is off.
            if (mamMultiStreamSequencerActive() || matchesExistingMam) {
                bridgeLog(QStringLiteral("legacy-mirror MAM %1ingest: %2")
                              .arg(mamMultiStreamSequencerActive()
                                       ? QString()
                                       : QStringLiteral("completion "))
                              .arg(message));
                mamIngestDecode(fields);
                continue;
            }

            if (!autoSeqActive) {
                continue;
            }
            if (shouldSuppressDirectedGhostDecode(fields, QStringLiteral("legacy-mirror-autoseq"))) {
                continue;
            }
            bridgeLog(QStringLiteral("legacy-mirror autoSeq feed: %1").arg(message));
            autoSequenceStep(fields);
        }
    };

    // The embedded legacy backend publishes decoded CQ rows through the
    // mirror path rather than through onFt8DecodeReady().  Auto Call must see
    // the same rows on macOS/legacy TX as it does on the native decoder path;
    // otherwise the CQ is visible in the UI but can never become TX1.
    auto feedAutoCallFromEntries = [this](QVariantList const& entries) {
        if (!m_autoCallEnabled || entries.isEmpty()) {
            return false;
        }

        QStringList rows;
        rows.reserve(entries.size());
        for (QVariant const& value : entries) {
            QVariantMap const entry = value.toMap();
            if (entry.value(QStringLiteral("isTx")).toBool()) {
                continue;
            }
            QString const message = entry.value(QStringLiteral("message"))
                                        .toString().trimmed();
            if (message.isEmpty()
                || !entry.value(QStringLiteral("isCQ")).toBool()) {
                continue;
            }

            QString time = normalizeUtcDisplayToken(
                entry.value(QStringLiteral("time")).toString());
            if (time.isEmpty()) {
                time = QDateTime::currentDateTimeUtc().toString(QStringLiteral("hhmmss"));
            }
            bool freqOk = false;
            int audioOffset = entry.value(QStringLiteral("freq"))
                                  .toString().trimmed().toInt(&freqOk);
            if (!freqOk) {
                audioOffset = 0;
            }
            // parseFt8Row expects the raw decoder frequency, then subtracts
            // m_nfa to return the audio offset used by setRxFrequency().
            int const rawFrequency = qBound(0, m_nfa + audioOffset, 5000);
            rows.append(QStringLiteral("%1 %2 %3 %4 ~  %5")
                            .arg(time,
                                 entry.value(QStringLiteral("db")).toString().trimmed(),
                                 entry.value(QStringLiteral("dt")).toString().trimmed(),
                                 QString::number(rawFrequency),
                                 message));
        }
        return tryAutoCallFromRows(rows);
    };

    QVariantList currentBandFallbackSnapshot;
    bool currentBandFallbackSnapshotReady = false;
    auto appendUniqueEntries = [](QVariantList& target, QVariantList const& entries) {
        QSet<QString> seen;
        seen.reserve(target.size() + entries.size());
        for (QVariant const& value : std::as_const(target)) {
            QString const key = decodeMirrorEntryKey(value.toMap());
            if (!key.isEmpty()) seen.insert(key);
        }
        QVariantList appended;
        appended.reserve(entries.size());
        for (QVariant const& value : entries) {
            QVariantMap const entry = value.toMap();
            QString const key = decodeMirrorEntryKey(entry);
            if (key.isEmpty() || seen.contains(key)) {
                continue;
            }
            seen.insert(key);
            target.append(entry);
            appended.append(entry);
        }
        return appended;
    };
    auto updateActiveStationsFromEntries = [this](QVariantList const& entries, bool reset) {
        if (!m_activeStations) {
            return;
        }
        if (reset) {
            m_activeStations->clear();
        }
        for (QVariant const& value : entries) {
            QVariantMap const entry = value.toMap();
            if (entry.value(QStringLiteral("isTx")).toBool()
                || !entry.value(QStringLiteral("isCQ")).toBool()) {
                continue;
            }
            QString const fromCall = entry.value(QStringLiteral("fromCall")).toString().trimmed();
            bool ok = false;
            int const freqHz = entry.value(QStringLiteral("freq")).toString().trimmed().toInt(&ok);
            if (fromCall.isEmpty() || !ok) {
                continue;
            }
            m_activeStations->addStation(fromCall,
                                         freqHz,
                                         entry.value(QStringLiteral("db")).toString().trimmed().toInt(),
                                         entry.value(QStringLiteral("dxGrid")).toString(),
                                         entry.value(QStringLiteral("time")).toString(),
                                         true);
        }
    };

    if (bandChanged || allTxtChanged) {
        if (useBandDelta) {
            if (bandChanged) {
                m_legacyBandActivityRevision = bandRevision;
            }
            QVariantList mirroredDecodes = mirrorLegacyDecodeLines(
                bandDeltaLines, false, QVariantList {}, userFilterConfig);
            mirroredDecodes = augmentLegacyMirrorWithAllTxt(mirroredDecodes, false);
            mirroredDecodes = dropModeChangeClearedEntries(mirroredDecodes);
            mirroredDecodes = dropPrunedBandEntries(mirroredDecodes);
            mirroredDecodes = dropConflictingDirectedReportEntries(
                mirroredDecodes, QStringLiteral("legacy-band-delta"));
            currentBandFallbackSnapshot = mirroredDecodes;
            currentBandFallbackSnapshotReady = true;

            QVariantList const newBandDecodes =
                decodeEntriesNotInPrevious(mirroredDecodes, m_decodeList);
            feedMamQueueFromEntries(newBandDecodes);
            feedWaitPounceFromEntries(newBandDecodes, m_decodeList,
                                      QStringLiteral("legacy-band-delta"));
            feedAutoCallFromEntries(newBandDecodes);
            feedAutoSequenceFromEntries(newBandDecodes);
            queueLegacyDecodeSecondaryWork(newBandDecodes,
                                           QStringLiteral("legacy-band-delta"));
            updateActiveStationsFromEntries(newBandDecodes, false);

            QVariantList bandUiDecodes;
            bandUiDecodes.reserve(newBandDecodes.size());
            for (QVariant const& value : newBandDecodes) {
                QVariantMap const entry = value.toMap();
                if (!entry.value(QStringLiteral("isTx")).toBool()) {
                    bandUiDecodes.append(entry);
                }
            }
            bandUiDecodes = applyLegacyUiFilters(bandUiDecodes, true);
            int const previousBandRows = m_decodeList.size();
            QVariantList const appendedBandEntries =
                appendUniqueEntries(m_decodeList, bandUiDecodes);
            if (!appendedBandEntries.isEmpty()) {
                trimDecodeListsIfNeeded();
                bool const appendOnly =
                    m_decodeList.size()
                    == previousBandRows + appendedBandEntries.size();
                if (!appendOnly
                    || !applyLegacyBandModelDelta(appendedBandEntries)) {
                    invalidateLegacyDecodeModelDeltaFastPath();
                }
                emitDecodeListChangedThrottled();
            }
            bridgeLog(QStringLiteral("legacy-mirror band delta: raw=%1 accepted=%2 total=%3")
                          .arg(bandDeltaLines.size())
                          .arg(bandUiDecodes.size())
                          .arg(m_decodeList.size()));
        } else {
        invalidateLegacyDecodeModelDeltaFastPath();
        m_legacyBandActivityRevision = bandRevision;
        QStringList const bandLines = m_legacyBackend->bandActivityLines();
        QVariantList mirroredDecodes = augmentLegacyMirrorWithAllTxt(QVariantList {}, false);
        QVariantList const widgetMirrored =
            mirrorLegacyDecodeLines(bandLines, false, m_decodeList, userFilterConfig);
        if (mirroredDecodes.isEmpty()) {
            mirroredDecodes = widgetMirrored;
        } else {
            QSet<QString> seen;
            for (QVariant const& value : std::as_const(mirroredDecodes)) {
                seen.insert(decodeMirrorEntryKey(value.toMap()));
            }
            for (QVariant const& value : std::as_const(widgetMirrored)) {
                QVariantMap const entry = value.toMap();
                QString const key = decodeMirrorEntryKey(entry);
                if (seen.contains(key)) {
                    continue;
                }
                seen.insert(key);
                mirroredDecodes.append(entry);
            }
        }
        mirroredDecodes = dropModeChangeClearedEntries(mirroredDecodes);
        mirroredDecodes = dropPrunedBandEntries(mirroredDecodes);
        mirroredDecodes = dropConflictingDirectedReportEntries(mirroredDecodes, QStringLiteral("legacy-band"));
        currentBandFallbackSnapshot = mirroredDecodes;
        currentBandFallbackSnapshotReady = true;
        QVariantList const newBandDecodes =
            decodeEntriesNotInPrevious(mirroredDecodes, m_decodeList);
        feedMamQueueFromEntries(newBandDecodes);
        feedWaitPounceFromEntries(newBandDecodes, m_decodeList, QStringLiteral("legacy-band"));
        feedAutoCallFromEntries(newBandDecodes);
        feedAutoSequenceFromEntries(newBandDecodes);
        queueLegacyDecodeSecondaryWork(newBandDecodes, QStringLiteral("legacy-band"));
        if (m_activeStations) {
            m_activeStations->clear();
            for (QVariant const& value : std::as_const(mirroredDecodes)) {
                QVariantMap const entry = value.toMap();
                if (entry.value(QStringLiteral("isTx")).toBool() || !entry.value(QStringLiteral("isCQ")).toBool()) {
                    continue;
                }
                QString const fromCall = entry.value(QStringLiteral("fromCall")).toString().trimmed();
                if (fromCall.isEmpty()) {
                    continue;
                }
                bool ok = false;
                int const freqHz = entry.value(QStringLiteral("freq")).toString().trimmed().toInt(&ok);
                int const snr = entry.value(QStringLiteral("db")).toString().trimmed().toInt();
                if (!ok) {
                    continue;
                }
                m_activeStations->addStation(fromCall,
                                             freqHz,
                                             snr,
                                             entry.value(QStringLiteral("dxGrid")).toString(),
                                             entry.value(QStringLiteral("time")).toString(),
                                             true);
            }
        }
        QVariantList bandUiDecodes;
        bandUiDecodes.reserve(mirroredDecodes.size());
        for (QVariant const& value : std::as_const(mirroredDecodes)) {
            QVariantMap const entry = value.toMap();
            if (entry.value(QStringLiteral("isTx")).toBool()) {
                continue;
            }
            bandUiDecodes.append(entry);
        }
        mirroredDecodes = applyLegacyUiFilters(bandUiDecodes, true);
        bridgeLog(QStringLiteral("legacy-mirror band: raw=%1 mirrored=%2")
                      .arg(bandLines.size())
                      .arg(mirroredDecodes.size()));
        if (mirroredDecodes.isEmpty() && !m_decodeList.isEmpty()) {
            bridgeLog(QStringLiteral("legacy-mirror band: skipped transient empty snapshot, keeping %1 rows")
                          .arg(m_decodeList.size()));
        } else if (m_decodeList != mirroredDecodes) {
            m_decodeList = mirroredDecodes;
            trimDecodeListsIfNeeded();
            emitDecodeListChangedThrottled();  // 1.0.142: throttle mirror replace-all
        }
        }
    }

    if (rxChanged || allTxtChanged || bandChanged) {
        if (useRxDelta && (rxChanged || currentBandFallbackSnapshotReady)) {
            if (rxChanged) {
                m_legacyRxFrequencyRevision = rxRevision;
            }
            QVariantList mergedRxDecodes = mirrorLegacyDecodeLines(
                rxDeltaLines, true, QVariantList {}, userFilterConfig);
            // If the band branch did not consume a changed ALL.TXT tail, let
            // the RX branch upgrade timestamps without rebuilding history.
            if (!currentBandFallbackSnapshotReady && allTxtChanged) {
                mergedRxDecodes = augmentLegacyMirrorWithAllTxt(mergedRxDecodes, true);
            }
            mergedRxDecodes = dropModeChangeClearedEntries(mergedRxDecodes);
            mergedRxDecodes = dropRxClearedEntries(mergedRxDecodes);

            QSet<QString> seen;
            for (QVariant const& value : std::as_const(mergedRxDecodes)) {
                seen.insert(decodeMirrorEntryKey(value.toMap()));
            }
            if (currentBandFallbackSnapshotReady) {
                for (QVariant const& value : std::as_const(currentBandFallbackSnapshot)) {
                    QVariantMap entry = value.toMap();
                    if (!shouldMirrorToRxPane(entry)) {
                        continue;
                    }
                    QString const key = decodeMirrorEntryKey(entry);
                    if (key.isEmpty() || seen.contains(key)
                        || m_legacyClearedRxMirrorKeys.contains(key)) {
                        continue;
                    }
                    seen.insert(key);
                    entry[QStringLiteral("forceRxPane")] = true;
                    mergedRxDecodes.append(entry);
                }
            }
            mergedRxDecodes = dropConflictingDirectedReportEntries(
                mergedRxDecodes, QStringLiteral("legacy-rx-delta"));
            QVariantList const newRxDecodes =
                decodeEntriesNotInPrevious(mergedRxDecodes, m_rxDecodeList);
            feedMamQueueFromEntries(newRxDecodes);
            feedWaitPounceFromEntries(newRxDecodes, m_rxDecodeList,
                                      QStringLiteral("legacy-rx-delta"));
            feedAutoCallFromEntries(newRxDecodes);
            feedAutoSequenceFromEntries(newRxDecodes);
            queueLegacyDecodeSecondaryWork(newRxDecodes,
                                           QStringLiteral("legacy-rx-delta"));

            QVariantList rxUiDecodes = applyLegacyUiFilters(newRxDecodes, false);
            coalesceRxPaneTxRows(rxUiDecodes, m_mode);
            int const previousRxRows = m_rxDecodeList.size();
            QVariantList const appendedRxEntries =
                appendUniqueEntries(m_rxDecodeList, rxUiDecodes);
            if (!appendedRxEntries.isEmpty()) {
                trimDecodeListsIfNeeded();
                bool const appendOnly =
                    m_rxDecodeList.size()
                    == previousRxRows + appendedRxEntries.size();
                if (!appendOnly
                    || !applyLegacyRxModelDelta(appendedRxEntries)) {
                    m_skipNextLegacyRxModelSnapshot = false;
                }
                emitRxDecodeListChangedThrottled(false);
            }
            bridgeLog(QStringLiteral("legacy-mirror rx delta: raw=%1 accepted=%2 total=%3")
                          .arg(rxDeltaLines.size())
                          .arg(rxUiDecodes.size())
                          .arg(m_rxDecodeList.size()));
        } else {
        invalidateLegacyDecodeModelDeltaFastPath();
        if (rxChanged) {
            m_legacyRxFrequencyRevision = rxRevision;
        }

        QStringList const rxLines = m_legacyBackend->rxFrequencyLines();
        QVariantList mergedRxDecodes =
            mirrorLegacyDecodeLines(rxLines, true, m_rxDecodeList, userFilterConfig);
        mergedRxDecodes = augmentLegacyMirrorWithAllTxt(mergedRxDecodes, true);
        mergedRxDecodes = dropModeChangeClearedEntries(mergedRxDecodes);
        mergedRxDecodes = dropRxClearedEntries(mergedRxDecodes);
        bridgeLog(QStringLiteral("legacy-mirror rx: raw=%1 mirrored=%2")
                      .arg(rxLines.size())
                      .arg(mergedRxDecodes.size()));
        QSet<QString> seen;
        for (const QVariant& value : std::as_const(mergedRxDecodes)) {
            QString const key = decodeMirrorEntryKey(value.toMap());
            seen.insert(key);
        }

        // CQ Only is a Band Activity filter only: keep using the raw legacy band
        // mirror here so direct callers and QSO replies still reach Signal RX.
        QVariantList bandFallbackDecodes;
        if (currentBandFallbackSnapshotReady) {
            bandFallbackDecodes = currentBandFallbackSnapshot;
        } else {
            bandFallbackDecodes = mirrorLegacyDecodeLines(
                m_legacyBackend->bandActivityLines(), false, m_decodeList, userFilterConfig);
            bandFallbackDecodes = augmentLegacyMirrorWithAllTxt(bandFallbackDecodes, false);
        }
        bandFallbackDecodes = dropModeChangeClearedEntries(bandFallbackDecodes);
        bandFallbackDecodes = dropPrunedBandEntries(bandFallbackDecodes);
        bandFallbackDecodes = dropRxClearedEntries(bandFallbackDecodes);

        // Legacy widget routing can occasionally miss FT8/FT4/FT2 messages directed to us
        // or belonging to the current QSO. Mirror them from Band Activity as a fallback.
        for (const QVariant& value : std::as_const(bandFallbackDecodes)) {
            QVariantMap entry = value.toMap();
            if (!shouldMirrorToRxPane(entry)) {
                continue;
            }
            QString const key = decodeMirrorEntryKey(entry);
            if (m_legacyClearedRxMirrorKeys.contains(key)) {
                continue;
            }
            if (seen.contains(key)) {
                continue;
            }
            seen.insert(key);
            entry[QStringLiteral("forceRxPane")] = true;
            mergedRxDecodes.append(entry);
        }

        mergedRxDecodes = dropConflictingDirectedReportEntries(mergedRxDecodes, QStringLiteral("legacy-rx"));
        QVariantList const newRxDecodes =
            decodeEntriesNotInPrevious(mergedRxDecodes, m_rxDecodeList);
        feedMamQueueFromEntries(newRxDecodes);
        feedWaitPounceFromEntries(newRxDecodes, m_rxDecodeList, QStringLiteral("legacy-rx"));
        feedAutoCallFromEntries(newRxDecodes);
        feedAutoSequenceFromEntries(newRxDecodes);
        queueLegacyDecodeSecondaryWork(newRxDecodes, QStringLiteral("legacy-rx"));

        // The legacy backend exposes RX Frequency as its current widget rows.
        // Keep Signal RX as a running history, otherwise switching to the next
        // AutoCQ partner drops the previous CQ/report/signoff lines from QML.
        for (const QVariant& value : std::as_const(m_rxDecodeList)) {
            QVariantMap entry = value.toMap();
            QString const key = decodeMirrorEntryKey(entry);
            if (key.isEmpty()
                || seen.contains(key)
                || m_legacyClearedRxMirrorKeys.contains(key)) {
                continue;
            }
            seen.insert(key);
            entry[QStringLiteral("forceRxPane")] = true;
            mergedRxDecodes.append(entry);
        }

        mergedRxDecodes = applyLegacyUiFilters(mergedRxDecodes, false);

        if (mergedRxDecodes.isEmpty() && !m_rxDecodeList.isEmpty()) {
            bridgeLog(QStringLiteral("legacy-mirror rx: skipped transient empty snapshot, keeping %1 rows")
                          .arg(m_rxDecodeList.size()));
        } else if (m_rxDecodeList != mergedRxDecodes) {
            m_rxDecodeList = mergedRxDecodes;
            rebuildRxDecodeMirrorIndex();
            trimDecodeListsIfNeeded();
            emitRxDecodeListChangedThrottled(false);
        }
        }
    }

    trace.addDetail(QStringLiteral("new_decode_rows=%1 new_rx_rows=%2")
                        .arg(m_decodeList.size())
                        .arg(m_rxDecodeList.size()));
    m_legacyAllTxtRevisionKey = allTxtRevisionKey;
}

