void DecodiumBridge::onAsyncDecodeTimer()
{
    if (m_mode != QStringLiteral("FT2")) {
        if (m_asyncDecodeTimer && m_asyncDecodeTimer->isActive()) {
            m_asyncDecodeTimer->stop();
        }
        m_asyncDecodePending = false;
        m_asyncAudioPos.store(0, std::memory_order_release);
        if (m_ft2Worker) {
            m_ft2Worker->markLatestDecodeSerial(std::numeric_limits<quint64>::max());
            m_ft2Worker->cancelCurrentDecode();
        }
        static qint64 s_lastFt2AsyncStopLogMs = 0;
        qint64 const stopNowMs = QDateTime::currentMSecsSinceEpoch();
        if (stopNowMs - s_lastFt2AsyncStopLogMs >= 10000) {
            s_lastFt2AsyncStopLogMs = stopNowMs;
            bridgeLog(QStringLiteral("FT2 async decode timer stopped outside FT2 mode=%1")
                          .arg(m_mode));
        }
        return;
    }
    if (!m_monitoring || m_asyncDecodePending) return;
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    int minAsyncDecodeIntervalMs = 0;
    if (m_lowCpuModeEnabled) {
        // Low CPU esplicito: throttle aggressivo voluto dall'utente
        minAsyncDecodeIntervalMs = cpuPressureSevereActive() ? 2000
                                 : (cpuPressureActive() ? 1500 : 1000);
    } else if (cpuPressureSevereActive()) {
        // 1.0.178 — Solo severe pressure throttla in modo significativo;
        // mild pressure ora NON applica throttle (era 350ms, troppo
        // aggressivo, FT2 perdeva 90% dispatch su PC modesti).
        minAsyncDecodeIntervalMs = 500;
    }
    // 1.0.292 — #1+#3 decode adattivo (opt-in, default OFF): in SOLO-ASCOLTO (no CQ,
    // no QSO, nessun partner) throttla il re-decode async a ~350ms — non ha senso
    // ridecodificare ogni 100ms una finestra sovrapposta al 95% (risparmio CPU, meno
    // picchi transitori). Quando ASPETTI una risposta (AutoCQ/QSO) resta a piena cadenza
    // (100ms) per non aggiungere latenza. Non perde decode: la finestra scorrente copre
    // comunque tutto l'audio.
    if (m_ft2AdaptiveDecode && minAsyncDecodeIntervalMs < 350) {
        bool const expectingReply = m_txEnabled || m_autoCqRepeat || !m_dxCall.isEmpty();
        if (!expectingReply) {
            minAsyncDecodeIntervalMs = 350;
        }
    }
    if (minAsyncDecodeIntervalMs > 0
        && m_lastFt2AsyncDecodeDispatchMs > 0
        && nowMs - m_lastFt2AsyncDecodeDispatchMs < minAsyncDecodeIntervalMs) {
        return;
    }
    if ((m_transmitting || m_tuning) && (m_lowCpuModeEnabled || cpuPressureActive())) {
        static qint64 s_lastTxSkipLogMs = 0;
        if (nowMs - s_lastTxSkipLogMs >= 5000) {
            s_lastTxSkipLogMs = nowMs;
            bridgeLog(QStringLiteral("FT2 async decode skipped during TX: slow CPU assist lowCpu=%1 cpuPressure=%2")
                          .arg(m_lowCpuModeEnabled ? 1 : 0)
                          .arg(cpuPressureActive() ? 1 : 0));
        }
        return;
    }
    // Snapshot atomico unico: il callback audio (writer) può incrementare
    // m_asyncAudioPos in qualsiasi momento. Leggiamolo una sola volta per
    // garantire coerenza tra il check di soglia, il calcolo di start, e
    // la copia dei 45000 campioni.
    // uint64_t: wrap-around well-defined; aritmetica unsigned per evitare
    // indici negativi nel modulo (che produrrebbero OOB).
    uint64_t const pos = m_asyncAudioPos.load(std::memory_order_acquire);
    if (pos < 45000) return;  // non abbastanza audio ancora
    // Sprint1-3 (1.0.393): se l'audio non e' avanzato dall'ultimo dispatch (RX
    // fermo durante il nostro TX, o device in stallo) la finestra sarebbe
    // IDENTICA: ridecodificarla a piena profondita' costa 300-700ms di CPU per
    // zero righe nuove. Skip finche' il writer non avanza.
    if (m_ft2AsyncLastDispatchAudioPos == pos) return;

    decodium::ft2::AsyncDecodeRequest req;
    req.audio.resize(45000);
    uint64_t const start = (pos - 45000) % ASYNC_BUF_SIZE;
    for (int i = 0; i < 45000; ++i)
        req.audio[i] = m_asyncAudio[(start + i) % ASYNC_BUF_SIZE];
    // FT2 asincrono F4: la posizione assoluta serve al worker per
    // ritrovare nella finestra i segnali decodificati nei giri precedenti.
    req.audioEnd = static_cast<qint64>(pos);

    int nfqsoTarget = qBound(m_nfa, m_rxFrequency, m_nfb);
    if (m_qsoPartnerFreqHz > 0 && !m_dxCall.trimmed().isEmpty()
        && QDateTime::currentMSecsSinceEpoch() - m_qsoPartnerFreqMs < 120000) {
        nfqsoTarget = qBound(m_nfa, m_qsoPartnerFreqHz, m_nfb);
    }
    int const nfqso = nfqsoTarget;
    int const decodePeriodMs = periodMsForMode(QStringLiteral("FT2"));
    if (decodePeriodMs > 0) {
        qint64 const windowStartMs = nowMs - static_cast<qint64>(decodePeriodMs);
        qint64 const slotIndex =
            (windowStartMs + static_cast<qint64>(decodePeriodMs / 2))
            / static_cast<qint64>(decodePeriodMs);
        req.nutc = utcTokenForSlotStart(slotIndex, decodePeriodMs);
        // FIX A v2 (1.0.356): memorizza lo slot-start del frame partner in scala
        // corrected-UTC, calcolato ORA al dispatch dalla finestra audio (stabile),
        // non al callback ready (latenza-dipendente). onFt2AsyncDecodeReady lo usa
        // come ancora. Coerente con refNowMs=nowCorrectedMs in scheduleSmartFt2AsyncTx.
        qint64 const correctedNowMs = correctedUtcEpochMs();
        qint64 const correctedWindowStart = correctedNowMs - static_cast<qint64>(decodePeriodMs);
        m_ft2AsyncDispatchSlotStartMs =
            ((correctedWindowStart + static_cast<qint64>(decodePeriodMs / 2))
             / static_cast<qint64>(decodePeriodMs))
            * static_cast<qint64>(decodePeriodMs);
    }
    req.nqsoprogress = legacyDecodeQsoProgress();
    req.nfqso = nfqso;
    // Sprint2-1 (opt-in): quando ATTENDI una reply la decisione TX serve solo
    // vicino a nfqso, ma ogni tentativo async scandagliava 200-4000Hz a piena
    // profondita' (decode_ms mediana 447ms, p99 2.7s -> 43.7% delle reply
    // azionabili solo allo slot successivo). Pass stretto nfqso+/-150Hz con
    // full-band 1 ogni 4 cicli per non perdere la band activity.
    int reqNfa = m_nfa;
    int reqNfb = m_nfb;
    if (m_ft2NarrowAsyncDecode) {
        bool const narrowExpectingReply = m_txEnabled || m_autoCqRepeat || !m_dxCall.isEmpty();
        if (narrowExpectingReply && (++m_ft2NarrowPassCounter % 4) != 0) {
            int const narrowLo = qMax(m_nfa, nfqso - 150);
            int const narrowHi = qMin(m_nfb, nfqso + 150);
            if (narrowHi - narrowLo >= 100) {
                reqNfa = narrowLo;
                reqNfb = narrowHi;
                if (nowMs - m_ft2NarrowLogMs >= 30000) {
                    m_ft2NarrowLogMs = nowMs;
                    bridgeLog(QStringLiteral("FT2 narrow async pass attivo: %1-%2Hz (full-band 1/4 cicli)")
                                  .arg(narrowLo).arg(narrowHi));
                }
            }
        }
    }
    req.nfa   = reqNfa;
    req.nfb   = reqNfb;
    int const asyncDepth = effectiveDecodeDepth();
    req.ndepth   = (m_transmitting || m_tuning) ? qMin(asyncDepth, 2) : asyncDepth;
    req.threadCount = effectiveFtThreadLimit();
    req.ncontest = m_ncontest;
    req.mycall   = m_callsign.toLocal8Bit();
    req.hiscall  = m_dxCall.toLocal8Bit();
    // 1.0.294 — AP cache Fase 1: passa lo snapshot (by-value) degli hash28 validi al
    // worker. Single-thread GUI → nessun lock. Solo se il toggle è ON.
    if (m_ft2ApHashCache)
        req.apHashCache = m_hashedCallsignCache.snapshotValid(nowMs);

    // FT2 asincrono F3 (DECODIUM_FT2_ASYNC_INCREMENTALE=1): si cercano
    // solo gli inizi dei frame diventati completi dall'ultimo giro, come in
    // JTTY. Il tempo nuovo si misura in campioni, non con l'orologio.
    // FT2 asincrono F5 (acceso; DECODIUM_FT2_ASYNC_ATTESO=0 lo spegne): in QSO la risposta
    // del corrispondente comincia fra 0,2 e 1,0 s dopo la fine della nostra
    // trasmissione, sulla sua frequenza. La' si prova un candidato esente dai
    // cancelli del sincronismo, con l'AP di mycall+hiscall.
    static bool const ft2AsyncAtteso =
        qEnvironmentVariable("DECODIUM_FT2_ASYNC_ATTESO").trimmed() != QStringLiteral("0");
    if (ft2AsyncAtteso && !m_dxCall.trimmed().isEmpty() && m_lastTxEndMs > 0) {
        qint64 const wallMs = QDateTime::currentMSecsSinceEpoch();
        if (wallMs - m_lastTxEndMs >= 0 && wallMs - m_lastTxEndMs < 6000) {
            double const windowStart = wallMs / 1000.0 - 3.75;
            double const txEnd = m_lastTxEndMs / 1000.0;
            int const lo = qMax(-688, static_cast<int>(std::floor((txEnd + 0.2 - windowStart) * 1333.33)));
            int const hi = qMin(2024, static_cast<int>(std::ceil((txEnd + 1.0 - windowStart) * 1333.33)));
            if (lo <= hi) {
                req.expectF = static_cast<float>(nfqso);
                req.expectLo = lo;
                req.expectHi = hi;
                static qint64 s_lastF5LoggedTxEndMs = 0;
                if (s_lastF5LoggedTxEndMs != m_lastTxEndMs) {
                    s_lastF5LoggedTxEndMs = m_lastTxEndMs;
                    bridgeLog(QStringLiteral("[FT2-ATTESO] attesa dopo TX: dx=%1 f=%2")
                                  .arg(m_dxCall.trimmed()).arg(nfqso));
                }
            }
        }
    }
    static bool const ft2AsyncIncremental =
        qEnvironmentVariableIntValue("DECODIUM_FT2_ASYNC_INCREMENTALE") == 1;
    if (ft2AsyncIncremental) {
        double const tauMax = 45000.0 / 12000.0 - 103.0 * 288.0 / 12000.0;   // 1,278 s
        double delta = 1.8;
        if (m_ft2AsyncLastDispatchAudioPos > 0 && pos > m_ft2AsyncLastDispatchAudioPos)
            delta = qMin(1.8, static_cast<double>(pos - m_ft2AsyncLastDispatchAudioPos) / 12000.0);
        req.ibLo = qMax(-688, static_cast<int>(std::floor((tauMax - delta - 0.02) * 1333.33)));
        req.ibHi = qMin(2024, static_cast<int>(std::ceil((tauMax + 0.02) * 1333.33)));
    }
    m_asyncDecodePending = true;
    m_lastFt2AsyncDecodeDispatchMs = nowMs;
    // Istante (orologio di sistema) in cui la finestra di 45 000 campioni
    // finiva: l'inizio assoluto di ogni riga e' questo meno 3,75 s piu' il DT.
    m_ft2AsyncRegistryDispatchMs = QDateTime::currentMSecsSinceEpoch();
    m_ft2AsyncLastDispatchAudioPos = pos;
    // Sprint3-C: decode async oltre la deadline (p99 osservato 2.7s, max 9.7s)
    // blocca il single-flight su audio ormai vecchio di uno slot. Cancel
    // COOPERATIVO (stage7_should_cancel per-pass): i pass completati emettono
    // comunque. Solo path async; il sync weak-recovery resta integro.
    if (m_ft2AsyncDeadlineMs > 0) {
        quint64 const dispatchSeq = ++m_ft2AsyncDispatchSeq;
        QTimer::singleShot(m_ft2AsyncDeadlineMs, this, [this, dispatchSeq]() {
            if (m_asyncDecodePending && dispatchSeq == m_ft2AsyncDispatchSeq && m_ft2Worker) {
                m_ft2Worker->cancelCurrentDecode();
                bridgeLog(QStringLiteral("FT2 async decode deadline: cancel cooperativo a %1ms (dispatch #%2)")
                              .arg(m_ft2AsyncDeadlineMs).arg(dispatchSeq));
            }
        });
    }
    auto* worker = m_ft2Worker;
    QMetaObject::invokeMethod(worker, [worker, req]() {
        worker->decodeAsync(req);
    }, Qt::QueuedConnection);
}



