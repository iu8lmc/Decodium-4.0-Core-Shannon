void DecodiumBridge::onAudioLevelForFt2Gate()
{
    if (m_mode != QStringLiteral("FT2") || !m_asyncTxEnabled) {
        return;
    }
    // RMS threshold per "quiet" — empirico, copre noise floor tipico USB CODEC
    // a guadagno medio. Gating: 3 sample consecutivi sotto soglia ≈ ~300ms.
    constexpr double kQuietThreshold = 0.005;
    if (m_audioLevel < kQuietThreshold) {
        if (m_ft2AsyncAudioQuietRuns < 100) {
            ++m_ft2AsyncAudioQuietRuns;
        }
    } else {
        m_ft2AsyncAudioQuietRuns = 0;
    }
}

// 1.0.447 - Leva#6-A: requisiti dei gate smart-TX (S2 RMS / S3 decode-quiet / S4 jitter) adattivi
// all'occupazione del canale FT2. Toggle OFF (default) o non-FT2/non-async -> ritorna i literal
// storici (3/400/50/200): path byte-identico, lo score NON viene nemmeno consultato. Canale CHIARO
// (audio silenzioso da un po' + nessun decode recente) -> piu' reattivo; AFFOLLATO (audio raramente
// silenzioso o decode molto recenti) -> piu' conservativo. NORMAL = identico alla baseline.
void DecodiumBridge::ft2AsyncGateRequirements(qint64 nowMs, int& rmsRunsReq, int& decodeQuietReq,
                                              int& jitterBase, int& jitterSpan) const
{
    rmsRunsReq = 3; decodeQuietReq = 400; jitterBase = 50; jitterSpan = 200;
    if (!m_ft2AdaptiveTxGates || m_mode != QStringLiteral("FT2") || !m_asyncTxEnabled) return;
    qint64 const decodeGapMs = m_ft2AsyncLastDecodeMs > 0 ? (nowMs - m_ft2AsyncLastDecodeMs) : 999999;
    int const quiet = m_ft2AsyncAudioQuietRuns;
    if (quiet >= 6 && decodeGapMs > 1500) {        // canale CHIARO -> anticipa entro la finestra safe
        rmsRunsReq = 2; decodeQuietReq = 250; jitterBase = 20; jitterSpan = 80;
    } else if (quiet <= 1 || decodeGapMs < 400) {  // canale AFFOLLATO -> piu' margine anti-collisione
        rmsRunsReq = 5; decodeQuietReq = 600; jitterBase = 80; jitterSpan = 300;
    }
}

