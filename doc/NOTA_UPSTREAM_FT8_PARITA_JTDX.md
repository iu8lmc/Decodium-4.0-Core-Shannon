# Nota tecnica upstream — Parità decode FT8 Decodium vs JTDX

**Autore:** IU8LMC (fork iu8lmc) · **Data:** 2026-05-24 · **Destinatario:** Salvatore / upstream elisir80

## Obiettivo dell'indagine

Misura sistematica del divario di decode FT8 tra Decodium 4.0 (1.0.280) e JTDX,
sugli **stessi slot, stessa banda (20m, 14.074), stessa propagazione** (confronto
slot-per-slot sui due `ALL.TXT`). Metodo: conteggio decode unici per slot,
deduplicati, su finestre da 15-23 slot per annullare il rumore di banda.

## Baseline misurato

- Decodium stock 1.0.280: **~80-85% dei decode di JTDX** (band-dependent).
- JTDX gira con: `NDepth=3` (Deep), `FT8Sensitivity=2`, `nPreampass=4`, `Hint=true`,
  `FT8WideDXCallSearch=true`. Media ~33-37 decode/slot su 20m affollato.
- Inoltre: **~15% dei decode FT8 di Decodium mostravano `<...>`** (callsign hashato
  non risolto) contro **~0% di JTDX**.

## Cause-radice identificate

### 1. Soglia di sync troppo alta sulle passate di sottrazione (sensibilità)
La soglia di sync per-passata (`ftx_ft8_prepare_pass_c`, `FtxDecodeBookkeeping.cpp`)
partiva da 1.0 e scendeva a 0.88 (pass 3) / 0.67 (pass 4-5). I segnali deboli
isolati (SNR -16..-23) che JTDX (Sensitivity=2) aggancia restavano sotto soglia.
> **Nota:** `ft8_candidate_sync_threshold` (`FtxFt8Stage4.cpp:2870`) è un path
> secondario (`cq_only_decode`), NON la soglia principale — fuorviante in diagnosi.

### 2. Hash table dei callsign non-standard azzerata per ogni candidato (BUG)
`FtxFt8Stage4.cpp` (funzione di decode del singolo candidato) chiamava
`legacy_pack77_reset_context_c()` **per ogni candidato**, azzerando la hash table
`call→hash` (`g_legacy_pack77_context`, thread_local) prima di ogni decode.
L'unpack auto-salva i call completi (`saveHashCall`, `FtxMessageEncoder.cpp:1367+`),
ma il reset li cancellava subito → nessun accumulo → ~15% di `<...>`.
JTDX mantiene la hash table persistente per ore → risolve tutto.

### 3. (NON risolto) Separazione dei segnali sovrapposti — gap architetturale
Il grosso del divario residuo su banda affollata sono **cluster di segnali
sovrapposti** (es. 1500/1506/1517 Hz). Decodium fa la sottrazione (`lsubtract=1`,
`ftx_subtract_ft8_c`) su 5 passate, ma la qualità/profondità della
subtract-and-research è inferiore a JTDX (`nPreampass=4` + decoder ottimizzato).

## Modifiche applicate sul fork (proposte per review/assorbimento)

| # | File | Modifica | Effetto misurato |
|---|------|----------|------------------|
| 1 | `Detector/FtxDecodeBookkeeping.cpp` (`ftx_ft8_prepare_pass_c`) | per `ndepth>=4`: `local_syncmin *= 0.80` (e `*0.85` su pass≥4) → soglia sync deep più bassa sulle passate di sottrazione | conteggio **80%→~90%**, stabile, 0 stall; **opt-in** (solo con Deep Search) |
| 2 | `DecodiumBridge.cpp` (`maybeDispatchFt8EarlyDecode`) | early preview cap depth 2→3 con Deep Search (resta entro i ~2.2s pre-fine-slot) | **96% dei decode entro +3s** (timely); il fast pass finale resta depth 2 (+1.1s) |
| 3 | `Detector/FtxFt8Stage4.cpp` | rimosso `legacy_pack77_reset_context_c()` per-candidato; il context thread_local accumula come JTDX | **`<...>` 15%→~5%** (cala ancora con l'uptime); universale |

### Vincoli/lezioni dai test (cosa NON fare)
- **Multi-threading non scala**: alzare `ftThreads`/cap OpenMP 4→8→24 non ha dato
  guadagno (CPU <1 core anche con 24 thread richiesti su CPU 32-core). Il decode è
  serial-bound; la velocità di JTDX non è replicabile col solo parallelismo.
- **Final pass "deep"**: rendere il fast pass finale depth 4 lo rende **lento
  (+10.7s) e tardivo** e fa **scendere** il conteggio (82%) — il decode profondo non
  completa entro la finestra timely. Da NON fare senza decode parallelo reale.
- **Over-tuning syncmin** (0.72/0.80) regredisce a 88%: 0.80/0.85 è l'ottimo.
- Misurare sempre su **finestre lunghe** (15+ slot): finestre da 4 slot sono
  dominate dal rumore di propagazione e portano a conclusioni sbagliate.

## Raccomandazioni per il 100% (lavoro core, upstream)

1. **Sottrazione iterativa stile JTDX** (`nPreampass`): più cicli di
   subtract-strongest-and-research sul residuo, per scoprire i segnali sovrapposti.
   È il gap architetturale dominante su banda affollata.
   > **Aggiornamento (vedi Parte 4):** analizzando il codice si è scoperto che la
   > sottrazione è GIÀ iterativa (subtract-in-place per-candidato, fino a 5 passate di
   > research sul residuo). Il gap NON era "poche passate" ma la **qualità** della
   > sottrazione: si sottraeva senza raffinare il DT del forte. Risolto col Quick-win A
   > (`lrefinedt=1`) — 93.5%→101.4%. Il `nPreampass` esterno aggiuntivo resta possibile
   > ma con ROI ormai basso.
2. **Parallelismo reale del decode** (across-candidate / across-band-segment) per
   permettere un decode profondo *entro la finestra timely* invece del follow-up
   tardivo (+13s). Oggi `run_main_passes` itera candidati in serie.
3. **Hash table persistente condivisa** (oltre al fix #3): un
   `sharedDecode77Context` (già esistente, `FtxMessageEncoder.cpp:3201`) popolato
   da tutti i decode e consultato come fallback, con clear solo su cambio banda —
   per portare i `<...>` verso 0 come JTDX anche subito dopo l'avvio.

## Stato finale del fork (1.0.280 + 3 fix)

~85-93% di JTDX (band-dependent) · 96% timely (≤+3s) · `<...>` ~5% · 0 stall · 0 underrun.
Da ~80% e 15% di call illeggibili. Le modifiche #1/#2 sono opt-in dietro "Deep Search".

---

# Parte 2 — Bug FT8 "QSO non si chiude" (2026-05-25, fork 1.0.284)

**Sintomo di campo:** molti utenti non chiudono QSO **FT8** con D4 (la stazione
remota risponde ma il QSO non si completa: D4 ripete / il partner non riceve
risposta in tempo), mentre **D3 e JTDX chiudono gli stessi QSO sulle stesse
macchine**. Sintomi riportati: "TX parte ma non avanza" + "il partner non risponde".

## Diagnosi (provata da cattura live, QSO con EA3IXP debole -20)

Il timing TX/PTT è SANO (`[TX-TL] sink_create dt=0ms`, payload +500ms, 0 underrun) —
**non è un problema di lead-in/PTT/trimming di per sé**. Il difetto è di
**ordinamento decode↔TX**:

1. L'auto-sequence FT8 **committa il TX successivo (CQ) al confine slot (+137ms)
   PRIMA di decodificare lo slot appena finito**. Catena: period tick
   `DecodiumBridge.cpp:26217` → `autoTxWaitsForDecode` era falso perché
   `pendingAutoDecodeBeforeBoundary` (`26159`) richiede un decode **già in volo** al
   boundary, ma il decode dello slot finito viene dispatchato solo a **+650ms**
   (`26212`).
2. La risposta del partner — **debole, mancata dalla passata early-visible**
   (depth 3 su buffer parziale `nzhsym=41`) — viene presa solo dal decode FINALE a
   **~+0.8-1.0s**, quando D4 sta già trasmettendo CQ.
3. D4 vuole rispondere ma **rinvia di un ciclo** (`auto-seq: defer TX3 while active
   TX2 is still playing`) → il partner ripete → D4 ridecodifica tardi → **loop, QSO
   mai chiuso**.

**Perché D3/JTDX sì:** decodificano-poi-decidono (commit TX dopo il decode) e
agganciano i deboli in tempo. **Perché "del computer":** su PC lenti l'early-visible
viene **saltato** (CPU pressure) → *ogni* risposta arriva tardi → *ogni* QSO fallisce
così; su PC veloci scatta solo coi partner deboli.

## Fix (commit `deaab40`, 6 modifiche accoppiate in `DecodiumBridge.cpp`)

1. **Decode-then-decide** (`~26217`): l'auto-TX in CQ-repeat/auto-seq aspetta il
   decode dello slot appena finito anche senza decode già pending —
   `autoTxWaitsForDecode = shouldDeferAutoTxUntilTimeSyncDecode && (pending || timeSyncMode)`.
2. **Grace** (`autoTxDecodeGraceMs`, `~3911`): FT8 900→**1200ms**, FT4 600→700ms (il
   decode + `advanceQsoState` completa ~+1.0s; il fallback deve scattare dopo).
3. **No clamp aggressivo sotto pressione** (`~19406`): FT2=80ms ma FT8/FT4
   `qMax(900, min(grace,1200))` — le macchine lente devono aspettare il decode **di
   più**, non di meno (il clamp a 150ms vanificava il fix proprio sulle affette).
4. **Shift invece di trim** (`syncTxPcmStartOffsetBytes`, `~12677`): la partenza
   tardiva (TX risposta a ~+1.0-1.5s) invia l'**intera forma d'onda da ora** (DT
   decodificabile) invece di troncare il fronte fino a **+2000ms**. Il trim
   distruggeva la prima Costas → il partner non decodificava.
5. **Cap latest-start FT8/FT4 a 2000ms** (`latestD3CompatibleSyncTxStartMs`, `~3887`,
   era ~10640): oltre 2000ms → **DEFER al boundary successivo** invece di trimmare
   (i CQ-resume tardivi non escono più troncati). Allineato a `maxShiftDtMs`.

> FT2 async invariato. Le modifiche cambiano il timing TX FT8 (parte ~+1.3s shiftato
> vs +0.5s): decodificabile, ma è un cambio reale da monitorare.

## Validazione on-air (macchina veloce)

- QSO **FT8 (DL3EBJ)** e **FT2 (EA2AA)** completati e **loggati** (TX2 → report →
  TX4 RR73 → 73 → QSO completo). Nel run pre-fix EA3IXP loopava all'infinito.
- CQ/risposte sempre **interi** (`slot_elapsed ~1.3-1.4s`, `pcm_pos=65536`, nessun
  `sync PCM offset`); TX tardivi (+10.9s) **deferiti**, **zero trim**.
- Rilasciato come **1.0.284** (field-test) su iu8lmc per test sulle **macchine lente
  affette** — il loopback NON riproduce il trigger (segnali forti → early-visible li
  prende).

## Proposta per upstream

Il fix è contenuto in `DecodiumBridge.cpp` (sequencer + audio start), zero dipendenze
nuove, FT2 async intatto. Suggerirei di assorbirlo come default FT8/FT4. Punto aperto
ortogonale: rendere la **early-visible** più sensibile ai deboli (o non saltarla sotto
pressione) ridurrebbe il numero di risposte prese tardi, abbassando la dipendenza dal
grace — ma il decode-then-decide è comunque la rete di sicurezza corretta.

---

# Parte 3 — FT2: stesso bug nel path sync, risolto rendendo FT2 sempre-async (1.0.285)

Estesa l'analisi a **FT2** (commit `4a24539`). FT2 ha **due path TX**:

- **FT2 async** (`m_asyncTxEnabled`, default del fork): **IMMUNE** al bug. È
  decode-then-decide per costruzione — `onFt2AsyncDecodeReady`
  (`DecodiumBridge.cpp:25654`) → quando un decode contiene il mio call →
  `autoSequenceStep` + `scheduleSmartFt2AsyncTx("decode-response")` (`25720`). Il TX
  è sempre conseguenza di un decode, mai committato al boundary.
- **FT2 sync** (async OFF): **AFFETTO dallo stesso bug FT8** (Parte 2). Usa
  `autoTxWaitsForDecode` ma con grace FT2 = **250ms** (`autoTxDecodeGraceMs("FT2")`,
  `~3928`) **< settle decode FT2 = 1000ms** (`~26249`) → committa il TX prima del
  decode della risposta → stesso loop. Inoltre, lo slot FT2 (3.75s) è così stretto
  che la finestra di shift utile è solo ~980ms, oltre la quale si trimmerebbe.

**Risoluzione scelta (più pulita del patch):** rendere **FT2 sempre-async,
permanente e non disattivabile**, eliminando del tutto il path sync. Upstream già
considera l'async FT2 "mandatory" (commento a `~9830`: *"Async L2 is mandatory and
always ON"*), ma il flag poteva essere off all'avvio. Reso airtight in
`DecodiumBridge.h`:
- `m_asyncTxEnabled` default `false`→`true` (chiude il buco di startup quando il modo
  FT2 è ripristinato prima della forzatura di mode-entry).
- `setAsyncTxEnabled` ignora qualsiasi richiesta di disattivazione (sempre true).
- `m_asyncTxEnabled` è gated da `m_mode=="FT2"` ovunque → inerte per FT8/FT4. Nessun
  toggle UI scrivibile esisteva (solo un indicatore di stato).

**Validato on-air:** QSO FT2 (OZ5BD) chiuso e loggato via path async
(`smartFt2Tx [decode-response] strategy=S1-slot-est` → report → RR73 → 73 →
`QSO completo` `logged=1`), con **zero** `brief FT2 decode grace` (il path sync non
viene mai entrato). Rilasciato in **1.0.285** (FT2-async + fix FT8) insieme a
**1.0.284** (solo fix FT8).

**Per upstream:** se preferisci mantenere il path sync FT2 disponibile, l'alternativa
è allineare il grace FT2 sync al settle (250→~1100ms) e cappare il latest-start FT2
alla finestra shift (~980ms) come fatto per FT8/FT4 — ma rendere FT2 solo-async è più
semplice e coerente con la regola "Async L2 mandatory" già presente.

---

# Parte 4 — Quick-win A: raffinamento DT pre-sottrazione (2026-05-26, fork 1.0.298)

Ripresa dell'indagine di parità (Parte 1) per attaccare il gap residuo su banda
affollata (segnali sovrapposti). **Ribalta l'assunzione iniziale della raccomandazione #1.**

## Scoperta: la sottrazione è GIÀ iterativa
Contrariamente all'ipotesi "facciamo poca subtract-and-research", il codice fa già
fino a **5 passate** di sottrazione-e-ricerca sul residuo (`ndepth>=3`):
- `run_main_passes` (`Detector/FtxFt8Stage4.cpp:3447`) itera le passate; `npass=5` per
  `ndepth>=3`, `3` per `ndepth<=2`, `2` per `ndepth==1` (`:3455-3463`).
- Ogni candidato decodificato con CRC-14 OK viene **sottratto in-place** dal buffer
  condiviso `dd0` (`ftx_subtract_ft8_c`, `:3258-3261`), con `lsubtract=1` su OGNI
  passata (`FtxDecodeBookkeeping.cpp`, `local_lsubtract=1` costante).
- La passata successiva ri-cerca i candidati (`ftx_sync8_search_stage4_c`) **sul
  residuo già decurtato dai forti**. Più l'early→subtract→full
  (`ftx_ft8_apply_saved_subtractions_c`) = ~1.x preampass effettivi.

Quindi il `nPreampass` di JTDX e le nostre passate sync sono concetti vicini: NON è la
quantità di giri il problema dominante.

## La vera leva: QUALITÀ della sottrazione (DT refinement)
Nel main pass sottraevamo i forti con `lrefinedt=0` (`FtxFt8Stage4.cpp:3260`, ultimo
arg): **nessun raffinamento del DT** del segnale prima di toglierlo. Una sottrazione
con DT non allineato al sub-campione lascia un **residuo "sporco"** (buche/code
spettrali) che maschera proprio i segnali sovrapposti deboli. JTDX raffina il DT del
forte prima di sottrarlo → residuo pulito → i deboli sotto emergono.

`ftx_subtract_ft8_c` SUPPORTA già il refinement (`FtxSubtract.cpp:717`, path
`lrefinedt!=0` a `:745-768`): valuta la metrica di sottrazione a idt −90/0/+90,
fit parabolico per l'offset ottimale `dx`, poi sottrae al DT raffinato `round(90·dx)`.
Era semplicemente **mai attivato** nel main pass.

## Fix applicato

| File | Modifica | Effetto misurato |
|------|----------|------------------|
| `Detector/FtxFt8Stage4.cpp:3260` | sottrazione main-pass: `lrefinedt` da `0` a `(request.ndepth>=4)?1:0` → raffina il DT del forte prima di sottrarlo, solo su decode profondo | parità JTDX **93.5%→101.4%** (30 slot, densità ~18 dec/slot); ora pareggia/supera JTDX, slot peggiore 75%→82% |

**Gate `request.ndepth>=4`**: stesso opt-in "Deep Search" dei fix #1/#2 (Parte 1).
Per FT8 `request.ndepth` è il depth base 1-4 (NON i bit legacy con flag 0x10/0x20);
`>=4` = decode più profondo, coerente con `live_full_ap` (`:3149`). Sotto soglia il
refinement non scatta (zero costo per i preset leggeri).

> ⚠️ **Trappola da evitare:** il subtract è dentro la funzione di decode per-candidato,
> dove l'unica variabile "pass" in scope è `pass_index` (`:3166`), che è l'**AP-pass del
> candidato**, NON la passata di subtract-research esterna (quella è in `run_main_passes`
> `:3447`). Gateare su `pass_index>=4` sarebbe stato sbagliato (gaterebbe sul tipo AP).
> Il gate corretto in scope è `request.ndepth`.

## Costo e sicurezza
- Costo: il path `lrefinedt=1` fa **3× subtract-eval** (idt −90/0/+90) per il fit, ma
  solo sui candidati forti delle passate deep → trascurabile (il subtract è 1 FFT-pair).
- Protetto dal **deadline hard** per-candidato e per-passata (`stage4_should_cancel`,
  `FtxFt8Stage4.cpp:101`, `FT8DecodeWorker.cpp:57`): nessun rischio stall/sforo slot.
- **Validazione no-regression (on-air, 30 slot 20m):** zero stall/underrun/cancel/
  deadline-truncation; decode timing invariato (slow pass ~12s, timely dall'early
  preview); Decodium decodifica **di più** (19.1 vs 18.8 dec/slot), nessun calo.

## Dead-end ri-confermati (NON rifare)
- Aggiungere passate "deep" al final/fast pass → lento e regredisce (Parte 1).
- `nPreampass` esterno aggiuntivo (loop attorno a `run_main_passes`): possibile ma ROI
  ormai basso dopo Quick-win A (già a parità+); il guadagno marginale dei preampass
  cala in fretta.

## Per upstream
Modifica di **1 riga** (più gate), zero dipendenze nuove, opt-in deep, FT4/FT2 intatti
(il path è FT8). Candidata all'assorbimento come default sotto Deep Search, coerente
coi 3 fix di Parte 1 già assorbiti. Eventuale estensione futura: applicare il
refinement anche a `ndepth==3` (Deep "leggero") se si vuole il guadagno anche lì.
