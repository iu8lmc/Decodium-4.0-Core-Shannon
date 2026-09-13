# FT2 — Roadmap tecnica verso la perfezione

> Documento di ingegnerizzazione condiviso tra **IU8LMC** e **elisir80**. Solo aspetti tecnici: DSP, protocollo, threading, codice. Nessun aspetto di adoption/marketing/IARU.
>
> *v1 — 2026-05-22 — basato su Decodium 4.0 1.0.272*

---

## Architettura attuale FT2 (baseline 1.0.272)

| Aspetto | Implementazione | Note |
|---|---|---|
| Modulazione | 8-FSK costellazione (eredità WSJT family) | da confermare: stessa di FT8 al rate diverso? |
| Slot length | 3.75 s | 4× FT8, 2× FT4 |
| Symbol rate | ~13.33 baud (da verificare in `FtxFt2Stage7.cpp`) | implica simbolo ~75 ms |
| Bandwidth audio | ~50 Hz @ baud nominale | analogo FT8 |
| LDPC code | (174,91) eredità FT8 a parametri rigenerati | Salvatore in 1.0.271 ha esteso `FtxLdpc.cpp` +151 |
| Stage pipeline | 7 stage (`FtxFt2Stage7.cpp` 89 KB) | iterative refinement |
| Async decoder | sì, no period-lock (`asyncDecodeEnabled`) | trigger ogni 100 ms |
| Dual carrier | opzionale (`dualCarrierEnabled`) | throughput potenziale 2× |
| QuickQSO mode | sì (`quickQsoEnabled`) | accorcia sequence |
| Conservative weak-signal pack | opt-in 1.0.174 (`Ft2Conservative`) | parametri decoder tunati |
| Partner Memory | drag-marker mantiene QSO 1.0.187 | recovery scenario |
| Tx2 Resend on Stall | 1.0.187 | retry automatico |
| Test FTX weak | sì, 1.0.271 (`tests/test_ftx_weak_decode.cpp` 36 KB) | regression continua |

**Quello che manca in chiave INGEGNERISTICA** è raccolto sotto in 7 aree.

---

## Aggiornamento 2026-05-26 (fork iu8lmc, post-analisi 1.0.289→292)

Analisi RX/TX FT2 approfondita (agenti rx+tx, sessioni live 20m). Conclusioni operative:

- **Priorità SNR n.1 = §1.1 (hashed-callsign AP cache band-wide), stima −3 dB.** È l'UNICO guadagno di floor non-dead-end rimasto: syncmin/OSD/subtract/AP-per-QSO sono **già tutti attivi full-depth** nel path async FT2 (`FtxFt2Stage7.cpp:1689,1714-1716,1554,2193`). Punto d'innesto della cache: `build_ap_setup` (`FtxFt2Stage7.cpp:1689`), pre-LDPC, conferma candidate borderline se hash28 visto di recente in banda. Richiede audit collisioni hash (§7.2, 5M call) prima del merge.
- **Dead-end CONFERMATI (non ritentare):** coherentAvg/turbo/neuralSync (nessun guadagno su FT8); aumentare thread/depth (decode serial-bound, cap 8→24 inutile, final-pass deep peggiora); abbassare syncmin async (già 0.70 @ depth≥3 — oltre = falsi positivi).
- **Quick-win async (fork, opt-in):** *decode adattivo* — il re-decode async ridecodifica ogni 100ms finestre sovrapposte al 95%; throttle a ~350ms in solo-ascolto (non in QSO/CQ) libera CPU senza perdere decode. Shippato fork-only 1.0.292 (`DecodiumBridge::onAsyncDecodeTimer`).
- **TX FT2 sano:** sequencer/cap/cadenza ok. Cap signoff SNR-adattivo corretto. La cadenza CQ a slot alterni è **necessaria** (parity periodo = finestra RX), non un bug.

Toggle FT2 fork-only aggiunti (opt-in, default OFF): `ft2FullDecodeInAutoCq`, `ft2QuickGiveUpStrong`, `ft2AdaptiveDecode`.

---

## 1. Decoder DSP — gap critici sul floor SNR

### 1.1 AP (A-Priori) decoding con hashed callsign cache

**Problema**: in WSJT-X 2.7 FT8 arriva stabile a **−24 dB SNR @ 2.5 kHz BW** grazie ad AP. Quando un callsign del messaggio è già "visto" recentemente sulla banda, i 28 bit corrispondenti diventano "anchor" prima di LDPC, dando ~3-5 dB di guadagno SNR.

**Implementazione proposta**:

```cpp
// lib/persistence/HashedCallsignCache.h (nuovo)
class HashedCallsignCache {
public:
    // Aggiunge una call al ring (idempotente). Triggered da
    // DecodiumBridge::noteDecodeCommitted dopo ogni decode accettato.
    void add(QString const& callsign, qint64 utcMs);

    // Query thread-safe: vero se hash28 e' in cache e ts > now - TTL_MS
    bool containsHash28(quint32 hash28) const noexcept;

    // Iteratore lock-free per il detector thread (snapshot atomico)
    QVector<quint32> snapshot() const noexcept;

private:
    static constexpr int RING_SIZE = 1024;
    static constexpr qint64 TTL_MS = 30LL * 60LL * 1000LL;  // 30 min
    struct Entry { quint32 hash28; qint64 utcMs; };
    std::array<Entry, RING_SIZE> m_ring;
    std::atomic<int> m_writeIdx {0};
    mutable QReadWriteLock m_lock;  // o std::shared_mutex
};

// Hash 28-bit usato da WSJT-X (vedi pack77_1.f90 / unpack77.cpp)
quint32 hash28(QString const& callsign) noexcept;
```

Integrazione in `FtxFt2Stage7`:

```cpp
// Prima della invocazione LDPC, se la candidate ha hash28 noto su uno dei
// due slot callsign, pre-inserire i 28 bit corrispondenti con LLR = +inf
// (o saturazione a +127 in int8 LLR scale).
auto const partial = stage7.recoverCallsignCandidates(rxSamples);
for (auto const& cand : partial) {
    if (m_hashedCache.containsHash28(cand.hash28dx)) {
        ldpcLLR[28_bit_offset_dx + i] = INT8_MAX;  // anchor bits
    }
    if (m_hashedCache.containsHash28(cand.hash28de)) {
        ldpcLLR[28_bit_offset_de + i] = INT8_MAX;
    }
}
```

**Effort**: 1-2 settimane. **Delta SNR atteso**: −3 dB sul floor.

### 1.2 Belief Propagation — count + early-exit

**Verifica necessaria**: nel `FtxLdpc.cpp` post-1.0.271 quante iter max sono fissate?

```cpp
// Da auditare in FtxLdpc.cpp
constexpr int LDPC_MAX_ITER = ???;  // FT8 mainstream usa 250
```

**Proposta**:
- Portare `LDPC_MAX_ITER = 250` (era forse 50)
- Early-exit appena `syndrome == 0` (zero correzione necessaria) → spesso converge in 30-80 iter
- Timeout hard a 80 ms per candidate per evitare worst-case CPU spike
- Misurare con `QElapsedTimer` quanti decode beneficiano dell'estensione

**Effort**: 2-3 giorni. **Delta SNR atteso**: −0.5 / −1 dB. **Rischio**: CPU spike → mitigare con timeout.

### 1.3 OSD (Ordered Statistics Decoding) fallback

Per i candidate che BP non risolve in `LDPC_MAX_ITER`, attivare OSD order-2 come fallback. Reference implementation: `osd174.f90` di WSJT-X (BSD-licensed) o `osd2.c` di Tomas Hood.

**Pseudocodice**:

```cpp
DecodeResult tryDecodeFT2(SoftBits const& llr) {
    auto bp = ldpc_bp_decode(llr, 250);
    if (bp.success) return bp.toResult();
    // BP failed -> OSD order-2 (costoso ma ~10-15% recovery)
    auto osd = osd_order2_decode(llr, /*candidates=*/4);
    return osd.success ? osd.toResult() : DecodeResult::Failed;
}
```

**Effort**: 2 settimane (porting + tuning). **Delta SNR atteso**: −1.5 / −2 dB sul tail dei decode borderline. **Costo CPU**: ~200 ms su candidate non-BP, ma solo su ~20% dei tentativi.

### 1.4 Sub-symbol time/frequency tracking

WSJT-X usa una stima fine di tempo e frequenza per symbol (interpolazione cubic o spline) che recupera multipath modesti.

**Proposta**: stage di refinement prima di LDPC che fa fit polinomiale di rank 2 su tempo/frequenza per ogni Costas array fragment, e applica de-rotation pre-demod.

**Effort**: 1 settimana. **Delta SNR atteso**: −0.3 / −0.5 dB ma più importante: miglioramento decoding su segnali con QSB/multipath.

### 1.5 Iterative interference cancellation (IIC)

Quando 2 portanti FT2 si sovrappongono (es. dual-carrier su 2 stazioni diverse), dopo aver decodato la più forte, **sottrarre il signal ricostruito dal buffer audio** e ridecodare. Recovery del 5-10% sui pile-up.

**Effort**: 2 settimane (richiede synth FT2 di ricostruzione + alignment fase). **Delta**: nessuno su segnali isolati, **+10% decode rate** su pile-up.

---

## 2. Sequencer FT2 — robustezza state machine

### 2.1 Audit reentrancy

Da log diagnostic precedente ho visto path come `advanceQsoState` (skill memoria `agente-tx`). Test fuzzing necessari:

- Decode partner arriva DURANTE `startTx()` (transitorio): cosa succede?
- `transmittingChanged` emesso 2 volte rapide (race): doppio TX o stuck?
- Partner manda RR73 prima del nostro 73: chi vince?
- `qsoLogged` triggera mentre TX corrente è ancora in flight

**Proposta**: test harness in `tests/test_ft2_sequencer_fuzz.cpp` che simula sequenze randomized di event timing e verifica invariants (es. "transmitting=true implica m_currentTx != 0", "qsoLogged=true blocca prossimo dxCall change in <500ms", etc).

**Effort**: 2 settimane. **Impatto**: zero crash post-merge upstream, regression detection automatica.

### 2.2 State machine esplicita

Oggi (memoria `project_ft2_sequencer_1_0_101`) la sequencer FT2 è un set di if-else accumulati. Una riscrittura come **state machine esplicita** (Qt State Machine framework o std::variant tagged union) renderebbe:

- States: `IDLE`, `CALLING_CQ`, `ANSWERING_PARTNER`, `IN_QSO_REPORT`, `IN_QSO_RR73`, `LOGGING`, `SIGNOFF`, `RESET`
- Transitions: tabella esplicita event → state
- Invariants documentati
- Diagnostic log produce trace `[FT2-SM] state=X -> Y trigger=Z`

**Effort**: 3-4 settimane (refactor sensibile). **Impatto**: manutenibilità + assenza di edge case latenti.

### 2.3 Deferred signoff window

Quando il partner manda RR73 ma noi non siamo ancora pronti (es. TX in playback), serve un buffer "deferred signoff" con TTL. Già esiste come `late-signoff-arm` (memoria), ma serve audit:

- TTL configurabile (oggi hardcoded?)
- Cleanup deterministico
- Log struttura: `arm/defer/dispatch/abort`

---

## 3. Audio TX path — precisione + recovery

### 3.1 PTT timing precision

PTT deve aprire prima dell'audio (lead-in 100-300 ms) e chiudere DOPO l'ultimo sample. Misura attuale con `QElapsedTimer` dovrebbe loggare:

- `t_ptt_on - t_audio_first_sample` (atteso > 100 ms, < 300 ms)
- `t_audio_last_sample - t_ptt_off` (atteso > 50 ms, < 200 ms)

**Proposta**: aggiungere `[TX-TIMING]` log line nella telemetry summary (già esiste `[TX-TL]` dal 1.0.218) per quantificare jitter PTT.

### 3.2 Audio sink underrun detection robusta

Vedi watchdog 1.0.225 + audio sink park 1.0.216-225. Da rafforzare:

- Se underrun durante TX → marcare TX come "compromised" e schedulare retry su slot successivo
- Soglia underrun (bytes mancanti) configurabile
- Telemetry: `underruns_per_tx` count + `bytes_underrun_total`

### 3.3 Clock drift compensation

Già visto in memoria `project_clock_drift_w32time_audio_underrun`. Decodium ha logica `time-sync decode aligned buffered audio`, ma TX side?

**Proposta**: 
- All'avvio TX, snapshot offset NTP vs clock locale
- Se drift > 100 ms, log warning + opzionalmente refuse TX
- Adattare sample timing su slot boundary

### 3.4 Pre-TX SWR/PWR gate

Già discusso ma re-impostato come ingegneria:

```cpp
bool DecodiumBridge::shouldAllowStartTx() {
    if (!m_catConnected) return true;  // no telemetria = fidiamo
    if (m_rigSwr > 2.5) {
        bridgeLog("[TX-GATE] block: SWR=%1 > 2.5").arg(m_rigSwr);
        return false;
    }
    if (m_rigPowerWatts > 0 && m_rigPowerWatts < 0.1) {
        bridgeLog("[TX-GATE] warn: PWR=%1 W rig in standby?").arg(m_rigPowerWatts);
    }
    return true;
}
```

---

## 4. Threading + concurrency

### 4.1 Decoder thread pool sizing

Oggi `bridge.ftThreads` configurabile 1/2/4 (vedi StatusBar FT Threads indicator). Audit:

- Costo memoria per thread (Stage 7 candidate buffer): quanto?
- Scaling efficiency: 2 thread danno 1.8× speedup? 4 thread danno 3.2×?
- Sweet spot per CPU 4/8/16 core

**Proposta**: benchmark `tools/bench_ft2_decode.cpp` con worker scaling + memory profile.

### 4.2 SIMD optimization audit

FT8 mainstream usa SIMD intrinsics (SSE2/AVX2) per FFT + correlation. Verifica:

- `FtxFt2Stage7.cpp` usa SIMD? Quali parti?
- FFT impl: usa FFTW3 o kissfft o custom?
- Su CPU moderne (AVX-512), c'è guadagno potenziale?

**Effort**: 1 settimana audit + 1-2 settimane porting. **Delta**: −30 / −50% CPU su decoder hot path → libera budget per BP iterations più aggressive (vedi 1.2).

### 4.3 Lock-free queues per decoder → bridge

Audit di `enqueuePersistDecode` (Phase 5.2) — già usa QueuedConnection (Qt event loop). Ma il path decoder thread → bridge main thread può essere ottimizzato con lock-free SPSC ring buffer per ridurre latency di update UI.

---

## 5. Test infrastructure — coverage SNR floor

### 5.1 Synthetic channel models

Salvatore in 1.0.271 ha `test_ftx_weak_decode.cpp` (36 KB) con AWGN. Espandere a:

- **Rayleigh fading** (multipath HF tipico ionosfera)
- **Doppler spread** (auroral/polar path)
- **Phase noise** (rig low-cost LO)
- **Mixed AWGN + carrier interference**

**Effort**: 1 settimana. **Impatto**: copertura real-world.

### 5.2 SNR floor regression

```cpp
// tests/test_ft2_snr_floor.cpp
TEST(Ft2SnrFloor, AwgnBaseline) {
    auto const result = simulateFt2DecodeSnrFloor(
        /*messages=*/1000,
        /*snrRange=*/{-32.0, -10.0},
        /*snrStepDb=*/0.5);
    ASSERT_LE(result.snrFloor50pct, -22.0);  // 50% decode rate @ SNR
    ASSERT_LE(result.snrFloor80pct, -18.0);  // 80% decode rate @ SNR
}
```

Tracciare in CI: ogni commit produce un punto in serie temporale `snr_floor_50pct` per banda di confidence. Alert se regressione > 0.5 dB.

### 5.3 End-to-end sequencer test (loopback)

Memoria `reference_loopback_qso_test` cita ALPHA + BRAVO + 2 virtual cables. Automatizzare in CI:

- Container Docker con 2 Decodium istanze + virtual audio bridge
- Script Python esegue 10 QSO automatici
- Verifica DB persistence + ADIF export contiene esattamente 10 QSO

**Effort**: 2 settimane setup. **Impatto**: regression detection end-to-end.

---

## 6. Performance / memory profiling

### 6.1 Memory leak audit long-session

Phase 5 DB worker + DevOverlay (1.0.233) hanno migliorato observability. Audit aggiuntivo:

- Valgrind / heaptrack su sessione 24h
- Tracking m_decodeList size + qmlcache + waterfall texture pool
- GPU memory (LiveMap shader textures, dopo 1.0.213-215)

### 6.2 CPU profile decoder hot path

Profiling con Visual Studio Profiler / perf / Intel VTune del decoder Stage 7:

- Top 10 funzioni per CPU%
- Cache miss rate (FFT correlation è cache-sensitive)
- Branch mispredict (BP iteration loop)

### 6.3 GPU offload feasibility

OpenCL / CUDA per LDPC BP? Su GPU consumer (RTX 3060) si potrebbe arrivare a 4× decode throughput, abilitando AP + OSD + IIC senza budget CPU.

**Effort**: 4-6 settimane PoC. **Status**: ricerca, non prodotto. Da considerare solo dopo Gap 1-2-3 chiusi.

---

## 7. Protocollo FT2 — robustezza wire format

### 7.1 Forward error correction extension

FT8 usa LDPC (174,91). FT2 ha lo stesso? Se sì, c'è margine per:

- Concatenated coding: outer Reed-Solomon (5%-10% overhead) per recovery di burst errors da QSB
- Più long codes (348, 174) opt-in per stazioni che vogliono extra robustezza a costo di slot tempo doppio

### 7.2 Hash collision audit

28-bit hash è sufficiente per ~268M callsign space, ma con 5M callsign mondo radio amatore le collision rate è significativa. Verifica:

- Probabilità collision per call hammered (es. K1JT comune)
- Strategia disambiguation: cache stores plain text per top-N hash più visti

### 7.3 Multi-language callsign support

`Ø`, `ö`, special chars in callsign Polish/Czech: già supportati nel pack77? Audit + test.

---

## Priorità immediate (Q3 2026)

Ordinato per **delta SNR / effort ratio**:

1. **Gap 1.1 — Hashed callsign cache + AP decoding** — 2 settimane → **−3 dB**
2. **Gap 1.2 — BP iterations bump 50→250 + early-exit** — 3 giorni → **−0.5 dB**
3. **Gap 1.3 — OSD order-2 fallback** — 2 settimane → **−1.5 dB sul tail**
4. **Gap 5.2 — SNR floor regression in CI** — 1 settimana → previene regressioni future

Con queste 4 cose FT2 dovrebbe stare a **SNR floor 50% @ −26/−27 dB**, **2-3 dB sotto FT8 WSJT-X 2.7**. Quello è il punto in cui FT2 diventa la prima scelta razionale per DX deboli.

---

## Cosa NON considerare ora

Per disciplina di ingegneria:

- ❌ **Cambio modulazione 8FSK → altro** (rifare ogni cosa, ROI negativo)
- ❌ **AI/ML neural decoder** (LDPC con AP + OSD sta vicino al limite di Shannon — un NN non recupera molto)
- ❌ **App mobile dedicata** (out of scope per DSP)
- ❌ **Compression custom messaggi** (77 bit è già denso, no margine)
- ❌ **Coding rate variable** (interop break, opt-in non vale lo stress)

---

## Riferimenti tecnici

- WSJT-X source: `lib/ft8/ldpc174_91_c_decode.f90`, `lib/77bit/pack77*.f90`, `lib/77bit/unpack77.f90`
- AP decoding paper: Joe Taylor K1JT, "FT4 and FT8 Communication Protocols", QEX Jul/Aug 2020
- LDPC reference: Gallager 1962 + modern soft-decision (`Sarah Johnson, "Iterative Error Correction"` 2009)
- OSD reference: Fossorier 1995, e implementazione WSJT-X `lib/ft8/osd174.f90`
- Costas arrays: Costas 1984, used as sync sequence in FT8/FT4/FT2

## Apertura

Salvatore, posso preparare PR PoC per il **Gap 1.1 (hashed callsign cache + AP integration in Stage 7)** sul fork iu8lmc. Tempo stimato: 2 settimane. Se ti torna utile, mandiamo la PR a `elisir80/main` e ne discutiamo i numeri del delta SNR misurato.

73 de IU8LMC
