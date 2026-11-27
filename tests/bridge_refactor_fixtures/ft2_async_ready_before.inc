void DecodiumBridge::onFt2AsyncDecodeReady(QStringList rows)
{
    MainThreadTraceScope trace(QStringLiteral("on_ft2_async_decode_ready"),
                               QStringLiteral("rows=%1 mode=%2 tx=%3")
                                   .arg(rows.size())
                                   .arg(m_mode)
                                   .arg(m_transmitting ? 1 : 0));
    // Path turbo async: stessa logica di onFt8DecodeReady ma senza serial.
    // Deduplica per messaggio: non aggiunge righe già presenti nella lista.
    m_asyncDecodePending = false;
#if DECODIUM_HAS_SSTV
    if (m_sstvWorkspaceActive) {
        if (m_asyncDecodeTimer) {
            m_asyncDecodeTimer->stop();
        }
        if (m_decoding) {
            m_decoding = false;
            emit decodingChanged();
        }
        bridgeLog(QStringLiteral(
            "FT2 async callback discarded while SSTV workspace is active: rows=%1")
                      .arg(rows.size()));
        return;
    }
#endif
    if (shouldIgnoreDecodeCallbacks()) {
        bridgeLog("onFt2AsyncDecodeReady: ignored during shutdown");
        return;
    }
    // Se l'utente ha cambiato modo mentre il decoder FT2 era ancora in volo,
    // il worker emette comunque il risultato via Qt::QueuedConnection.
    // Senza questo guard finiremmo per iniettare spot marker '~' (FT2) nella
    // decodeList e broadcastarli su WebSocket anche dopo lo switch a FT8/FT4.
    if (m_mode != QStringLiteral("FT2")) {
        if (m_asyncDecodeTimer) {
            m_asyncDecodeTimer->stop();
        }
        bridgeLog(QStringLiteral("onFt2AsyncDecodeReady: ignored/stopped outside FT2 mode=%1")
                      .arg(m_mode));
        return;
    }

    if (rows.isEmpty()) {
        // Sprint1-4 (1.0.393): 22k righe in 46h (~13% del log) bruciavano
        // l'orizzonte di rotazione 5MB e il logger e' sincrono. Aggregato 60s.
        ++m_ft2EmptyAsyncAttempts;
        qint64 const emptyNowMs = QDateTime::currentMSecsSinceEpoch();
        if (m_ft2EmptyAsyncAttemptLogMs == 0
            || emptyNowMs - m_ft2EmptyAsyncAttemptLogMs >= 60000) {
            bridgeLog(QStringLiteral("FT2 async decode: %1 attempt vuoti aggregati (finestra %2s)")
                          .arg(m_ft2EmptyAsyncAttempts)
                          .arg(m_ft2EmptyAsyncAttemptLogMs == 0
                                   ? 0
                                   : (emptyNowMs - m_ft2EmptyAsyncAttemptLogMs) / 1000));
            m_ft2EmptyAsyncAttemptLogMs = emptyNowMs;
            m_ft2EmptyAsyncAttempts = 0;
        }
        return;
    }

    bridgeLog("onFt2AsyncDecodeReady: rows=" + QString::number(rows.size()));
    bridgeLog("  async_raw[0]='" + rows[0] + "'");

    // 1.0.365+ MAM multi-stream nativo (FASE 2, FT2 async): se il sequencer
    // multi-QSO e' attivo i decode diretti a me alimentano mamIngestDecode
    // (slot per-call) invece dell'auto-seq single-QSO (il blocco early-autoseq
    // sotto e' gated dallo stesso predicato). NON facciamo return: lasciamo che
    // il lavoro UI/lista/DXCC sotto popoli la decode-list come in onFt8DecodeReady.
    // Il TX FT2 parte dal boundary di onPeriodTimer -> checkAndStartPeriodicTx ->
    // mamDispatchPeriod (gia' gated). Con MAM OFF nulla di questo scatta e il
    // path FT2 async normale resta byte-identico.
    if (mamMultiStreamSequencerActive()) {
        for (const auto& row : rows) {
            QStringList f = parseFt8Row(row);
            if (f.size() >= 5) mamIngestDecode(f);
        }
    }

    if (m_dxccLookup && !m_dxccLookup->isLoaded()) {
        loadDxccLookupAsync();
    }

    // 1.0.304 (#9) — resume-on-reply (FT2-async): l'Halt qui wipa m_dxCall, ma l'arm in
    // engageManualTxHold ha salvato il partner; se ri-risponde, processDecodeDoubleClick lo
    // re-ingaggia. Se ha ripreso, salta l'auto-seq su questa batch (evita doppio advance).
    bool const resumedQso = tryResumeQsoOnReply(rows);
    // DX-watch armato: se il target compare on-air, ARMED -> CALLING. Inerte se non armato.
    if (!resumedQso) tryArmedWatchSeesTarget(rows);
    observeTargetCallDecodeRows(rows, QStringLiteral("FT2"));

    // FT2 async is latency sensitive: drive the QSO sequencer before the
    // heavier UI/list/DXCC work below, so a directed reply can arm the next
    // TX without waiting for repaint and band-map bookkeeping.
    bool const earlyAutoSeqActive =
        !resumedQso &&
        !m_callsign.isEmpty() &&
        ((m_autoSeq && (m_txEnabled || !m_dxCall.isEmpty()))
         || m_autoCqRepeat);
    bool const earlyFt2SignoffRescueScan =
        !resumedQso
        && !m_callsign.isEmpty()
        && m_asyncTxEnabled
        && (m_autoSeq || m_autoCqRepeat);
    bool autoCallStarted = false;
    if (!resumedQso && m_autoCallEnabled) {
        autoCallStarted = tryAutoCallFromRows(rows);
    }
    if (!autoCallStarted
        && (earlyAutoSeqActive || earlyFt2SignoffRescueScan)
        && !mamMultiStreamSequencerActive()) {
        bool gotResponse = false;
        // FIX A: DT (secondi) del decode partner diretto, per ancorare lo slot.
        double partnerDtSec = 0.0;
        bool partnerDtValid = false;
        QString const myCallUpper = m_callsign.trimmed().toUpper();
        QString const myBaseUpper = normalizedBaseCall(myCallUpper);
        QStringList const feedRows = scoredCallerRowOrder(rows);
        for (const auto& row : feedRows) {
            QStringList f = parseFt8Row(row);
            if (f.size() < 5) continue;
            bool const apCacheRescuedDecode =
                f.size() > 5 && f[5].trimmed() == QLatin1String("7");
            if (apCacheRescuedDecode) {
                // Sprint3-B: il rescue resta display-only (Fase 1) TRANNE il caso
                // stretto mid-QSO: messaggio diretto a NOI dal partner ATTIVO
                // (m_dxCall). Un falso accept costa 1 slot; il beneficio e' il
                // partner in QSB che non blocca piu' il sequencer (-3dB utile).
                // Mai per nuovi caller.
                QString const rescuedPeer = directedPeerTokenFromMessage(
                    f[4], myCallUpper, myBaseUpper);
                QString const activeDx = m_dxCall.trimmed().toUpper();
                bool const rescuedFromActivePartner =
                    !activeDx.isEmpty()
                    && !rescuedPeer.isEmpty()
                    && normalizedBaseCall(rescuedPeer) == normalizedBaseCall(activeDx);
                if (!rescuedFromActivePartner) {
                    bridgeLog(QStringLiteral("FT2 autoSeq: AP-cache-rescued decode is display-only, not feeding AutoSeq/TX: %1")
                                  .arg(f[4]));
                    continue;
                }
                if (!decodium::seq::mayFeedRescuedDecodeToSequencer(f[4])) {
                    // A low-confidence rescue may keep the exchange moving but never
                    // close it: a phantom 73/RR73/RRR/TU would log a QSO that did not
                    // happen (report from T77RN, 26/10/2026).
                    bridgeLog(QStringLiteral("FT2 autoSeq: AP-rescued signoff from active partner %1 ignored, a rescue cannot close the QSO: %2")
                                  .arg(activeDx, f[4]));
                    continue;
                }
                bridgeLog(QStringLiteral("FT2 autoSeq: AP-rescued decode dal partner attivo %1 -> feed al sequencer: %2")
                              .arg(activeDx, f[4]));
            }
            QString localHashPartner;
            bool const directedToLiteralCall = messageContainsCallToken(f[4], myCallUpper, myBaseUpper);
            bool const directedToLocalHash =
                !directedToLiteralCall
                && isDirectedToLocalHashFromActivePartner(f[4], &localHashPartner);
            if (!shouldAcceptDecodedMessage(f[4], nullptr, directedToLocalHash)) continue;
            bool const signoffRescue =
                !earlyAutoSeqActive
                && earlyFt2SignoffRescueScan
                && isDirectedActivePartnerSignoffDecode(f);
            if (!earlyAutoSeqActive && !signoffRescue) {
                continue;
            }
            QString localTxEchoReason;
            if (shouldSuppressRecentLocalTxEchoDecode(f[4],
                                                      QStringLiteral("ft2-autoseq"),
                                                      &localTxEchoReason)) {
                continue;
            }
            if (directedToLiteralCall || directedToLocalHash) {
                if (directedToLiteralCall
                    && shouldSuppressDirectedGhostDecode(f, QStringLiteral("ft2-autoseq"))) {
                    continue;
                }
                if (signoffRescue) {
                    bridgeLog(QStringLiteral("FT2 signoff rescue: feed async decode to auto-seq: %1")
                                  .arg(f[4]));
                } else if (directedToLocalHash) {
                    bridgeLog(QStringLiteral("FT2 autoSeq: feed local-hash directed decode from %1: %2")
                                  .arg(localHashPartner, f[4]));
                }
                autoSequenceStep(f);
                gotResponse = true;
                // FIX A: cattura il DT del partner diretto (f[2] = secondi).
                if (f.size() >= 3) {
                    bool dtOk = false;
                    double const dt = f[2].trimmed().toDouble(&dtOk);
                    if (dtOk) { partnerDtSec = dt; partnerDtValid = true; }
                }
            }
        }
        // Track timing per smart scheduler FT2 async
        if (m_mode == QStringLiteral("FT2") && m_asyncTxEnabled && !rows.isEmpty()) {
            qint64 const decodeNowMs = QDateTime::currentMSecsSinceEpoch();
            // Burst detection: gap > 1500ms = nuovo burst (nuovo frame partner)
            bool const newBurst = m_ft2AsyncLastDecodeMs == 0
                || (decodeNowMs - m_ft2AsyncLastDecodeMs) > 1500;
            if (newBurst) {
                m_ft2AsyncFirstDecodeMs = decodeNowMs;
            }
            // FIX A: ancora la stima al TEMPO-SEGNALE. Lo slot del frame del
            // partner = boundary del periodo che ha contenuto il frame,
            // ricavato dal clock UTC corretto + DT. Il decode async arriva
            // verso/poco dopo la fine del frame, quindi lo slot di
            // appartenenza e' quello iniziato ~meta'-payload fa.
            // FIX A v2 (1.0.356): base = slot calcolato al DISPATCH
            // (m_ft2AsyncDispatchSlotStartMs, dalla finestra audio), NON al
            // correctedUtcEpochMs() di QUI: il callback ready arriva 0.2-2.6s dopo
            // il dispatch e ricalcolare lo slot ora faceva oscillare la stima di
            // ~mezzo periodo. Lo slot al dispatch e' stabile e corretto.
            // Sprint1-1 (1.0.393): prima l'ancora si aggiornava SOLO a inizio
            // burst; su banda affollata i decode di terzi tengono il gap <1500ms
            // e l'ancora restava congelata per minuti (signalEnd -1.6..-10.8s nei
            // log reali), rendendo incalcolabili la strategia S1 e il phase-lock
            // breaker. Ora si rinfresca anche ad ogni decode DIRETTO a noi
            // (partnerDtValid), stessa base stabile: la parita' e' quella vera
            // dell'ultimo frame, non quella di minuti fa.
            if (newBurst || partnerDtValid) {
                int const ft2PeriodMs = periodMsForMode(QStringLiteral("FT2"));
                if (ft2PeriodMs > 0 && m_ft2AsyncDispatchSlotStartMs > 0) {
                    qint64 slotStartMs = m_ft2AsyncDispatchSlotStartMs;
                    if (partnerDtValid) {
                        // CLAMP DT: il DT del decode puo' essere spurio (>|0.8s| =
                        // sync sbagliato o falso decode) e spostare la stima dello slot
                        // su un boundary errato. Limita a +/-0.8s (range fisico FT2).
                        slotStartMs += static_cast<qint64>(qBound(-0.8, partnerDtSec, 0.8) * 1000.0);
                    }
                    qint64 const prevAnchorMs = m_ft2AsyncPartnerSlotMs;
                    m_ft2AsyncPartnerSlotMs = slotStartMs;
                    m_ft2AsyncPartnerSlotSetMs = decodeNowMs;
                    if (partnerDtValid && !newBurst && prevAnchorMs != slotStartMs) {
                        bridgeLog(QStringLiteral("FT2 async anchor refresh: partnerSlot %1 -> %2 (dt=%3)")
                                      .arg(prevAnchorMs).arg(slotStartMs)
                                      .arg(partnerDtSec, 0, 'f', 2));
                    }
                } else if (newBurst) {
                    // Fallback: slot del dispatch non disponibile -> wall-clock estimate.
                    m_ft2AsyncPartnerSlotMs = 0;
                    m_ft2AsyncPartnerSlotSetMs = 0;
                }
            }
            m_ft2AsyncLastDecodeMs = decodeNowMs;
            // 1.0.355 — STEP 2: registra l'INDICE di slot corrected-UTC che l'async
            // ha appena coperto con righe accettate. feedAudioToDecoder confronta il
            // suo slotIndexForUtc (= completedUtcSlot, anch'esso correctedUtcEpochMs()/
            // periodMs) con questo per decidere se il decode sync di fine-slot e'
            // ridondante. NOTA: indice di slot (non ms), per matchare completedUtcSlot.
            {
                int const ft2PeriodMs = periodMsForMode(QStringLiteral("FT2"));
                if (ft2PeriodMs > 0) {
                    m_ft2AsyncDecodeProducedSlotMs =
                        correctedUtcEpochMs() / static_cast<qint64>(ft2PeriodMs);
                }
            }
        }
        if (gotResponse && !m_transmitting) {
            if (m_mode == QStringLiteral("FT2") && m_asyncTxEnabled) {
                // Sistema integrato anti-collisione: combina slot estimation,
                // RMS gate, decode-quiet gate, jitter — vedi scheduleSmartFt2AsyncTx
                scheduleSmartFt2AsyncTx(QStringLiteral("decode-response"));
            } else {
                checkAndStartPeriodicTx();
            }
        }
    }

    // Stage7 emits overlapping batches every 100 ms. The persistent bounded
    // index makes duplicate lookup O(1); this batch set also suppresses repeats
    // before accepted rows are committed to the live model.
    if (m_ft2AsyncDecodeDedupSessionId != m_decodeSessionId) {
        resetFt2AsyncDecodeDedupIndex();
    }
    QSet<QString> batchDedupKeys;

    bool changed = false;
    int accepted = 0;
    int duplicatesSkipped = 0;
    int uiFiltered = 0;
    int userFiltered = 0;
    int semanticFiltered = 0;
    int parseFailures = 0;
    int bestSnr = -99;
    DecodeUserFilterConfig const userFilterConfig = readDecodeUserFilterConfig();
    for (const auto& row : rows) {
        QStringList f = parseFt8Row(row);
        if (f.size() < 8) {
            bridgeLog("  async parseFt8Row FAILED: '" + row + "'");
            ++parseFailures;
            continue;
        }
        QString msg = f[4];
        QString semanticRejectReason;
        bool const directedToLocalHashForUi = isDirectedToLocalHashFromActivePartner(msg);
        if (!shouldAcceptDecodedMessage(msg, &semanticRejectReason, directedToLocalHashForUi)) {
            bridgeLog(QStringLiteral("semantic decode filter FT2 async: %1 msg='%2'")
                          .arg(semanticRejectReason, msg));
            ++semanticFiltered;
            continue;
        }
        QString localTxEchoReason;
        if (shouldSuppressRecentLocalTxEchoDecode(msg,
                                                  QStringLiteral("ft2-ui"),
                                                  &localTxEchoReason)) {
            ++duplicatesSkipped;
            continue;
        }
        QString reportConflictReason;
        if (shouldSuppressConflictingDirectedReportDecode(f,
                                                          QStringLiteral("ft2-ui"),
                                                          &reportConflictReason)) {
            ++duplicatesSkipped;
            continue;
        }
        // Aggiorna il miglior SNR (per S-meter del AsyncModeWidget)
        bool snrOk = false;
        int snrVal = f[1].toInt(&snrOk);
        if (snrOk && snrVal > bestSnr) bestSnr = snrVal;

        bool isCQ     = msg.startsWith("CQ ", Qt::CaseInsensitive) || msg == "CQ";
        bool isMyCall = messageMentionsLocalCall(msg);
        if (!isMyCall && directedToLocalHashForUi) {
            isMyCall = true;
        }

        QVariantMap entry;
        entry["time"]    = f[0];
        entry["db"]      = f[1];
        entry["dt"]      = f[2];
        entry["freq"]    = f[7];
        entry["message"] = msg;
        entry["aptype"]  = f[5];
        entry["quality"] = f[6];
        entry["mode"]    = QStringLiteral("FT2");
        entry["isTx"]    = false;
        entry["isCQ"]    = isCQ;
        entry["isMyCall"] = isMyCall;
        entry["fromCall"] = extractDecodedCallsign(msg, isCQ);
        entry["decodeSessionId"] = static_cast<qulonglong>(m_decodeSessionId);
        // Keep the model timestamp tied to the decoded FT2 window, not to the
        // delayed callback delivery time. Otherwise late async rows can show an
        // old UTC label after newer TX/RX rows in Signal RX.
        entry["timestamp"] = static_cast<qulonglong>(
            decodeEntryTimestampMsecs(f[0], QDateTime::currentMSecsSinceEpoch()));
        enrichDecodeEntry(entry);
        isMyCall = entry.value(QStringLiteral("isMyCall")).toBool();
        if (m_hideGhostDecodes && looksLikeGhostDecode(entry)) {
            bridgeLog(QStringLiteral("ghost decode filter FT2 async: msg='%1'")
                          .arg(msg));
            ++semanticFiltered;
            continue;
        }
        QString userFilterReason;
        if (!shouldAcceptDecodeEntryByUserFilters(entry, userFilterConfig, false, &userFilterReason)) {
            bridgeLog(QStringLiteral("user decode filter FT2 async: %1 msg='%2'")
                          .arg(userFilterReason, msg));
            ++userFiltered;
            continue;
        }

        QString const dedupKey = decodeDedupKeyFt2Async(entry.value("time").toString(),
                                                        entry.value("freq").toString(),
                                                        msg,
                                                        entry.value("timestamp").toLongLong());
        // Acceso di default (banco: doppioni 26 -> 0 su 240
        // trasmissioni, nessuna perdita); DECODIUM_FT2_ASYNC_REGISTRO=0 torna
        // alla chiave per slot.
        static bool const ft2AsyncRegistro =
            qEnvironmentVariable("DECODIUM_FT2_ASYNC_REGISTRO").trimmed() != QStringLiteral("0");
        bool duplicate = false;
        if (ft2AsyncRegistro && m_ft2AsyncRegistryDispatchMs > 0) {
            bool dtOk = false, fOk = false;
            double const dt = f[2].trimmed().toDouble(&dtOk);
            double const fHz = f[7].trimmed().toDouble(&fOk);
            double const start = m_ft2AsyncRegistryDispatchMs / 1000.0 - 3.75 + (dtOk ? dt : 0.0);
            duplicate = !m_ft2AsyncRegistry.admit(canonicalDecodeMessage(msg), fOk ? fHz : 0.0, start,
                                                  QDateTime::currentMSecsSinceEpoch() / 1000.0);
        } else {
            duplicate = m_ft2AsyncDecodeDedupKeys.contains(dedupKey)
                || batchDedupKeys.contains(dedupKey);
        }
        if (duplicate) {
            ++duplicatesSkipped;
            continue;
        }
        batchDedupKeys.insert(dedupKey);

        maybeEnqueueMamCallerFromDecode(f);
        maybeQueuePskReporterSpot(entry, msg, isCQ, f[7], f[1], QStringLiteral("FT2"));
        appendLegacyAllTxtDecodeLine(entry);
        rememberFt2AsyncDecodeDedupKey(dedupKey);

        bool const filteredByCqOnly = !m_filtersBypassed && m_filterCqOnly && !passesCqFilter(isCQ, msg);
        bool const filteredByMyCallOnly = !m_filtersBypassed && m_filterMyCallOnly && !isMyCall;
        if (filteredByCqOnly || filteredByMyCallOnly) {
            persistDecodeHistoryEntry(entry);   // archivio = registro, non vista (vedi il ramo FT8)
            if (filteredByCqOnly && !filteredByMyCallOnly) {
                appendRxDecodeEntry(entry);
            }
            ++uiFiltered;
            continue;
        }

        tryStartWaitPounceFromEntry(entry, m_decodeList, QStringLiteral("ft2-async"));

        appendDecodeMapToList(entry);

        // 1.0.293/294 — AP hashed-callsign cache: registra in cache le call viste in
        // banda + misura hit-rate. USA decodium::ft2MessageCallHashes(msg) — la STESSA
        // funzione del confirm in Stage7, così seed e confirm hashano in modo IDENTICO
        // (altrimenti la cache è inerte). Anti-loop: NON seminare i decode cache-confirmed
        // (nap=7), per non auto-rinforzare un eventuale falso (Fase 1).
        if (m_ft2ApHashCache && entry.value(QStringLiteral("aptype")).toString() != QLatin1String("7")) {
            qint64 const apNowMs = entry.value(QStringLiteral("timestamp")).toLongLong();
            QVector<quint32> const apHashes = decodium::ft2MessageCallHashes(msg);
            int apSeeded = 0, apHits = 0;
            for (quint32 const h28 : apHashes) {
                if (m_hashedCallsignCache.containsHash28(h28, apNowMs)) ++apHits;
                m_hashedCallsignCache.add(h28, apNowMs);
                ++apSeeded;
            }
            if (apSeeded > 0 && apNowMs - m_lastApHashLogMs >= 15000) {
                m_lastApHashLogMs = apNowMs;
                bridgeLog(QStringLiteral("[FT2WS-AP] cache=%1 (TTL30m) seeded=%2 hits=%3")
                              .arg(m_hashedCallsignCache.validCount(apNowMs))
                              .arg(apSeeded).arg(apHits));
            }
        }

        appendRxDecodeEntry(entry);
        // 1.0.212 — Live Map feed incrementale per FT2 async (vedi nota FT8)
        queueWorldMapEntryForReplay(entry, false, 250);
        // 1.0.162 — DecoSyncTime fase 4: feed dt al self-calibrator
        if (m_decoSyncTime && !entry.value("isTx").toBool()) {
            bool dtOk = false, snrOk = false;
            double const dt = entry.value("dt").toString().toDouble(&dtOk);
            int const db = entry.value("db").toString().toInt(&snrOk);
            if (dtOk && snrOk) m_decoSyncTime->reportDecodeDt(dt, db);
        }
        changed = true;
        ++accepted;
    }
    if (changed) {
        // The decode UI snapshot worker owns ordering, separator injection and
        // display normalization. Keeping the source list append-only avoids a
        // full main-thread pass for every FT2 async callback.
        emitDecodeListChangedThrottled();  // 1.0.142: throttle FT2 async (5-20Hz → 4Hz)
    }
    if (bestSnr > -99) setAsyncSnrDb(bestSnr);
    bridgeLog(QStringLiteral("onFt2AsyncDecodeReady summary: raw=%1 accepted=%2 parse_fail=%3 semantic=%4 user_filtered=%5 ui_filtered=%6 dupes=%7")
                  .arg(rows.size())
                  .arg(accepted)
                  .arg(parseFailures)
                  .arg(semanticFiltered)
                  .arg(userFiltered)
                  .arg(uiFiltered)
                  .arg(duplicatesSkipped));
    trace.addDetail(QStringLiteral("accepted=%1 parse_fail=%2 semantic=%3 user_filtered=%4 ui_filtered=%5 dupes=%6 changed=%7")
                        .arg(accepted)
                        .arg(parseFailures)
                        .arg(semanticFiltered)
                        .arg(userFiltered)
                        .arg(uiFiltered)
                        .arg(duplicatesSkipped)
                        .arg(changed ? 1 : 0));

    // Shannon cbAsyncDecode legacy: quando arriva un decode con il nostro
    // callsign, elabora auto-seq e avvia TX IMMEDIATAMENTE senza aspettare
    // il boundary del periodo. IMPORTANTE: checkAndStartPeriodicTx() solo
    // se gotResponse=true (CQ periodico è gestito dal boundary).
    // Auto-seq FT2 async is handled before UI work at the top of this method.
}

