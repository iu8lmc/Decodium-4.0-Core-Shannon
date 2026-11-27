# FT4 UI freeze durante RX — dossier tecnico (2026-07-13)

**Reporter:** IU8LMC · **Build:** 1.0.480 (e66f2f37) · **Per:** Salvatore / elisir80

## Sintomo
- In **FT4**, **durante la ricezione** (mentre decodifica), la UI si congela ~1 s e riprende, ripetutamente (~ad ogni slot 7.5 s). A volte percepito come blocco dell'**intero PC** per qualche secondo.
- In **FT8 e FT2 NON succede** (confermato dall'operatore su più sessioni): girano fluidi.
- Persiste dopo aggiornamento driver, cambio backend, e i workaround sotto.

## Ambiente
- GPU **NVIDIA RTX 3060 Ti**, driver aggiornato **610.74** (32.0.16.1074) — clean install.
- Windows 11 23H2. **Dual monitor**: LG Ultrawide 3440×1440@60Hz (primario) + un 4K@30Hz (poi portato a 60Hz).
- Profilo attivo **WEBSDR**, ready-profile **"contest"**, ftThreads=8, ProcessPriority era HIGH (poi Normal).

## Firma dal debugger (gdb, catturata su freeze REALE durante RX FT4, backend OpenGL)
Cattura con watcher automatico che scatta solo su freeze sostenuto ≥~0.9 s durante ricezione reale:
- **Main thread (Thread 1):** `ntdll!ZwWaitForSingleObject` ← `Qt6Core` ← **`Qt6Quick`** ← `Qt6Widgets` ← `Qt6Gui`
  = **sync del threaded render loop di Qt Quick** (main thread aspetta il render thread).
- **QSGRenderThread:** `win32u!NtGdiDdDDIWaitForVerticalBlankEvent` ← `nvoglv64!DrvPresentBuffers` ←
  `wglSwapBuffers` ← `gdi32full!SwapBuffers` ← Qt6Gui/Qt6Quick = **present appeso al vblank**.
- **Thread `FT4DecodeWorker`:** ATTIVO, in esecuzione dentro decodium.exe (unico thread non in wait)
  = **stava decodificando FT4** al momento del freeze.

**Interpretazione:** la firma "present/render-sync" è **SINTOMO**: il main thread è ritardato/occupato,
non raggiunge `polishAndSync`, il render thread parcheggia nel present. La causa è a monte (RX/main-thread),
NON nel render tech (vedi cosa è stato escluso).

## COSA È STATO ESCLUSO (con metodo — NON ripartire da qui)
| Ipotesi | Azione | Esito |
|---|---|---|
| Driver NVIDIA 591.86 buggato | clean install → 610.74 | ha risolto un **freeze TOTALE** separato (device hang), **NON** questo |
| Backend grafico | testati WARP / D3D12 / **OpenGL** | OpenGL migliore; tutti mostrano la stessa firma present-stall |
| V-Sync | Off in Pannello NVIDIA | nessun effetto |
| Refresh monitor | 30→60 Hz (era 4K@30) | nessun effetto |
| Offload GPU | `DECODIUM_DISABLE_GPU_PANADAPTER_FFT` + `_WATERFALL_SHADER` + `_SPECTRUM_GPU_GRAPH` | nessun effetto |
| Priorità processo | HIGH → Normal (profilo contest la metteva HIGH) | nessun effetto sul freeze |
| **Gamba A** (fallback OSD FT4 gated su core grezzi) | `DECODIUM_FT4_HARDWARE_THREADS_OVERRIDE=6` (spegne gate ≥8/≥16) | **nessun effetto** |
| **Gamba B** (doppia consegna decode/slot) | patch: early-return in `maybeDispatchFt4EarlyDecode` (early-decode OFF) | **nessun effetto** |

## Analisi di codice raccolta (agente UI) — punti FT4-specifici (ma i test sopra li smentiscono come causa)
- `Detector/FtxFt4Decoder.cpp:379` `ft4_hardware_threads()` = `hardware_concurrency()` grezzo → abilita
  fallback OSD (norder3 :449, norder4 :467, grid-edge :685, budget "fast" più LUNGHI su ≥16 :485-519) e
  depth-4 su molti-core, **ignorando** ftThreads/riserva-UI/pressure. Nessun `pragma omp` nel path FT4
  (solo `FtxFt8Stage4.cpp`) → decode FT4 **single-thread**. Latency-guard a 2500ms / late-drop a 9000ms
  (`DecodiumBridge.cpp:36775-36795`) = il decode FT4 sfora spesso.
- Doppia dispatch/slot: `maybeDispatchFt4EarlyDecode` (`DecodiumBridge.cpp:41089`, early a 6.25s) + finale
  (`:38744`); ogni consegna → `onFt8DecodeReady` (`:36720`) esegue **sincrono sul main thread** enrich +
  `normalizeDecodeEntriesForDisplay` O(N≤1500) (`:36690-36716`). ~2×/7.5s in FT4 vs ~1×/15s FT8.
- **NB:** disattivare la doppia dispatch (Gamba B) NON ha tolto il freeze → o il costo main-thread è
  altrove (spectrum feed? DecodeListModel reset? label/DX-cluster overlay?), o la causa non è il rate di
  consegna ma un **singolo evento bloccante** per-slot in FT4.

## Piste NON ancora esplorate (per chi può riprodurre)
1. **Cosa fa esattamente il main thread nei ~1 s** — serve una cattura con simboli o un tracer main-thread
   (l'app rilascio è stripped). Instrumentazione già presente: `PanadapterItem.cpp:1624-1629`
   (`syncmax_model_emit_start_ago_ms` / `syncmax_decode_ready_start_ago_ms`), `[PANMETRIC] qsg_phase`
   con `sync_max_ms`. In 1.0.480 il logging runtime è però quasi muto: **il rilevatore `[UIDBG] main-thread
   stall` NON logga più** (ultimo evento nel diagnostic.log è di giorni prima) → riattivarlo/abbassare soglia
   sarebbe il singolo strumento più utile.
2. **DecodeListModel** reset/insert su banda FT4 affollata (molti decode/slot) — profilare beginResetModel
   vs dataChanged.
3. **Path audio del profilo WEBSDR** (feed di rete/TCI?) — verificare se un'operazione RX/audio periodica
   blocca il main thread proprio in FT4.

## Richiesta a Salvatore
Riprodurre in FT4 con RX reale + **riattivare `[UIDBG] main-thread stall`** (o un tracer) per vedere DOVE il
main thread spende gli ~1 s per-slot in FT4 (e non in FT8). La firma present/vblank è sintomo; la causa è nel
lavoro main-thread FT4-specifico, non ancora localizzata perché i due candidati (Gamba A/B) sono stati
confutati da test env/patch.

## SISTEMA 100% SANO — la causa è SOFTWARE (aggiornamento 2026-07-13, sessione 2)
Escluso l'INTERO sistema con strumenti dedicati:
- **CPU** max 20% durante RX FT4 (mai satura). **RAM** 128 GB, 86 liberi (mai pressione). **Paging** trascurabile.
- **GPU:** util a raffiche fino 94% MA con **clock a 210-420 MHz** (P8/P5) → sembrava strozzatura power-mgmt;
  clock **FORZATI a 1800 MHz (P0)** via `nvidia-smi --lock-gpu-clocks=1500,2145` (admin) → **freeze INVARIATO**
  = clock GPU NON è la causa (era sintomo del carico bursty).
- **LatencyMon (Home 7.31, 2:30 run):** scheda Main **VERDE** ("sistema adatto"); scheda Drivers: highest
  execution MAX **0.23 ms** (dxgkrnl), nvlddmkm 0.15 ms, HDAudBus 0.046 ms → **NESSUN driver con latenza
  problematica** (un colpevole starebbe su decine-centinaia di ms). USB Audio CODEC / audio: scagionati.
- **Test isolamento render:** nascosto il waterfall (`uiWaterfallPanelVisible=0`) + overlay callsign/DX-cluster
  OFF → **freeze INVARIATO** = NON è il rendering del waterfall né gli overlay.

**⇒ Conclusione forte: NON è hardware, driver, Windows, né il contenuto renderizzato. È il MECCANISMO di
rendering/present di Qt Quick sotto il carico del flusso-decode FT4, in Decodium.** Il "mouse si impunta" è il
cursore SOPRA la finestra Decodium il cui present è fermo (il resto del desktop resta ok → coerente con
LatencyMon verde + monitor CPU che NON rilevano mai stallo di sistema, gap max 173 ms).

**⚠️ LIMITE della diagnosi remota (onestà):** (1) impossibile riprodurre RX FT4 reale su questa macchina →
niente profiling/iterazione fix; (2) le catture gdb mostrano SEMPRE la stessa firma (main in render-sync +
render in present) che potrebbe essere lo stato NORMALE del threaded render loop, NON isolabile dal freeze vero
col probe SendMessageTimeout. **Serve: riprodurre FT4 reale + qmlprofiler / build con simboli** per vedere il
per-frame cost e QUALE update QML/binding esplode all'arrivo del decode FT4. Piste non ancora escluse:
DecodeListModel (beginResetModel vs dataChanged su banda affollata), Signal RX list, un binding/animazione QML
che riparte ad ogni consegna decode, il feed spettro sul main thread.

## ⭐⭐ SVOLTA (2026-07-14, sessione 3): È UN BLOCCO DEL MAIN THREAD DI ~2,2 SECONDI
Aggiunto un **tracer** in `installMainThreadWatchdog` (main_qml.cpp:208): ogni stallo del main thread ≥200ms
scritto SUBITO su `%LOCALAPPDATA%\decodium_ft4stall.log` (force-flush). Durante RX FT4 reale ha catturato
**41 stalli**, pattern regolare:
```
2026-07-13T23:57:01 STALL delta_ms=2052 monitoring=1 spectrum=1
2026-07-13T23:57:15 STALL delta_ms=2150 ...
2026-07-13T23:58:01 STALL delta_ms=3233 ...
... (delta 2000-3200ms, ~ogni 10-12s, sempre monitoring=1)
```
**Il main thread di Decodium si BLOCCA per ~2,2 secondi (fino a 3,2s), ripetutamente, durante la ricezione FT4.**
Non è un micro-scatto di render: è un blocco pieno del main thread. Il mouse "si impunta" = cursore sopra la
finestra Decodium ferma (resto del desktop ok → coerente con LatencyMon VERDE + i monitor CPU che non vedono
stalli di sistema). `[MAINWATCH]` conferma (max_ms cresce coi burst).

**Il main thread CALCOLA, non aspetta (gdb doppio-scatto durante il blocco):** un secondo scatto lo becca in
`win32u!NtUserKillTimer` ← `Qt6Gui`/`Qt6Core` (event dispatch) ← **codice di decodium.exe** (frame `?? ()` senza
simboli). Quindi macina **elaborazione UI/QML/eventi** per ~2,2s all'arrivo del decode FT4 (killtimer = gestione
timer di animazioni/QML). NON è il render thread, NON la GPU.

**Sospetto forte (da confermare col nome funzione):** il path di consegna decode FT4 sul main thread —
`DecodiumBridge::onFt8DecodeReady` → enrich per-riga + `normalizeDecodeEntriesForDisplay` O(N≤1500)
(DecodiumBridge.cpp:36690-36720) e/o l'aggiornamento QML della lista Full Spectrum (delegate + animazioni
`decodeRowSlideAnim`) su banda FT4 affollata (molti decode/slot). Disattivare la DOPPIA dispatch (early-decode,
Gamba B) NON ha aiutato → è il costo della SINGOLA consegna, non il rate.

**⚠️ NOME FUNZIONE NON OTTENUTO — la build STRIPPA i simboli.** `CMakeLists.txt:148` (blocco `if(NOT
is_debug_build)`) linka con `-Wl,-s -Wl,--strip-all`; anche compilando con `-g -gdwarf-4` (gli .obj e `objects.a`
HANNO il DWARF, 5504 sezioni), l'exe finale è strippato e gdb mostra `?? ()` sui frame decodium.exe (per giunta
ASLR non relocazionato in attach). Rimuovere lo strip da CMakeLists non basta: `decodium_qml.dir/link.txt` tiene
`-Wl,-s` cachato e il reconfigure non lo rigenera. **PER SALVATORE:** con un build NON-strippato (togliere
`-Wl,-s --strip-all`, o build RelWithDebInfo) + `-no-pie` (evita l'ASLR in attach), gdb sul blocco da 2,2s darà
il nome esatto della funzione → fix mirata. Il tracer + il watcher gdb-on-freeze (poll SendMessageTimeout →
gdb thread 1 bt) sono lo strumento pronto.

## Stato lasciato sulla macchina IU8LMC
- Driver 610.74 (freeze totale risolto). Backend **OpenGL** (`DECODIUM_GRAPHICS_BACKEND=opengl`, migliore).
  ProcessPriority **Normal**. Rimosse tutte le var di test. Sorgente e exe **puliti** a 1.0.480.
