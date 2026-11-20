# Decodium 4 FT2 v1.0.599

Version 1.0.599 builds on the upstream v1.0.598 baseline with safer accelerated
LDPC decoding, improved CAT/transverter operation, satellite logging, a compact
working-frequency selector, and a more robust Decometer window.

## English (UK)

### Fast LDPC: safe dispatch on every supported CPU

- The portable dispatcher and the original generic decoder are now compiled
  for the minimum target CPU. AVX2/FMA code is isolated in a separate x86
  translation unit and is entered only after checking AVX, AVX2, FMA,
  OSXSAVE, and the operating system's XMM/YMM state support.
- Apple Silicon and other ARM64 builds now use a native NEON min-sum backend.
  The `LDPC` switch therefore selects `fastldpc-neon` on supported ARM64
  systems and `fastldpc-avx2-fma` on supported x86 systems.
- If the required SIMD facilities are unavailable, Decodium automatically
  uses the original generic decoder instead of executing unsupported
  instructions. This keeps Windows systems without AVX2/FMA usable.
- `DECODIUM_FT2_DISABLE_FASTLDPC=1` is now an unconditional emergency
  override: a saved enabled state in the user interface can no longer turn the
  accelerated backend back on.
- Startup diagnostics identify the CPU model, architecture, AVX/AVX2/FMA,
  OSXSAVE/XCR0 state, NEON capability, compiled backend, selected decoder, and
  fallback reason.
- Added deterministic tests for the emergency fallback and for bit-for-bit
  equivalence between the ARM64 NEON min-sum implementation and the scalar
  reference.

### Working frequencies, QO-100, and transverters

- Added a compact, responsive frequency selector beside the operating-mode
  control. It reuses Decodium's existing Working Frequencies table, filters
  entries by mode, marks preferred entries, and performs QSY through the normal
  bridge/CAT path.
- Preferred `ALL`-mode entries can now serve as the quick frequency for a
  selected digital mode. This makes satellite and cross-band presets such as
  QO-100 available without duplicating them for every mode.
- Signed station/transverter offsets are validated over their full supported
  range. Adding a second offset for the same band updates the existing entry
  instead of leaving ambiguous duplicates.
- CAT polling now maps the physical rig/IF dial back to Decodium's logical
  on-air frequency. Local QSY, post-QSY verification, split-TX calculation,
  startup synchronisation, and subsequent CAT reports use the same reversible
  calibration and per-band offset chain.
- Invalid physical CAT targets produced by an offset are rejected with a clear
  message instead of being sent silently to the rig.
- The QSO confirmation window now includes the RX/downlink frequency. QO-100
  proposes its configured downlink frequency (or 10489.540000 MHz as the safe
  default), while still allowing manual editing.
- Satellite QSOs now propagate `PROP_MODE=SAT`, `SAT_NAME`, `SAT_MODE`,
  `FREQ_RX`, and the derived `BAND_RX` into ADIF. The compatible values are
  also carried through the existing logger/UDP forwarding path.

### CAT and DecoPort stability

- DecoPort/Hamlib rig opening now runs wholly on its dedicated worker thread,
  so a slow or unresponsive serial device cannot hold the graphical thread.
- Repeated open requests are coalesced while a connection is pending, and
  open/opening state is protected independently of the Hamlib handle.
- Shutdown closes Hamlib and its polling timer on the owning worker, moves the
  object back safely, then joins the thread. This removes cross-thread QObject
  lifetime hazards.
- Added a regression test confirming dedicated-thread ownership and immediate
  rejection of an incomplete open request.

### Decometer window

- The Decometer face is now normal content inside its native top-level window,
  rather than a nested Dialog/Overlay. This avoids a Qt 6.11 threaded-renderer
  race seen when opening the instrument on macOS.
- The content is prepared while the native window is hidden and presented only
  after the asynchronous loader is ready.
- Desktop coordinates are no longer bound back to a centred position. Dragging
  uses explicit global coordinates, allowing the frameless instrument to reach
  screen edges and corners without springing back.
- Size normalisation, saved geometry, multi-screen movement, closing, and
  telemetry polling remain owned by the native window.

### Validation and downloads

- The native Qt build and focused regression tests cover the Fast LDPC
  dispatcher, ARM64 NEON equivalence, FT2 QSO simulation, and DecoPort worker
  behaviour.
- Release artefacts are built by GitHub Actions for Windows x64, macOS Apple
  Silicon (Tahoe and Sequoia), macOS Intel (Ventura, Sonoma, and Sequoia), and
  Linux x86_64/aarch64 AppImage targets. SHA-256 companion files are supplied
  for every DMG and AppImage.
- GitHub's generated `.zip` and `.tar.gz` archives for tag `v1.0.599` are the
  source-code downloads for this release.

---

## Italiano

La versione 1.0.599 parte dalla base upstream v1.0.598 e aggiunge una selezione
LDPC accelerata più sicura, una gestione CAT/transverter più completa, il log
satellitare, un selettore compatto delle frequenze operative e una finestra
Decometer più robusta.

### Fast LDPC: selezione sicura su ogni CPU supportata

- Il dispatcher portabile e il decoder generico originale vengono ora
  compilati per la CPU minima. Il codice AVX2/FMA è isolato in una unità x86
  separata e viene richiamato soltanto dopo avere verificato AVX, AVX2, FMA,
  OSXSAVE e il supporto del sistema operativo allo stato XMM/YMM.
- Le build Apple Silicon e ARM64 dispongono adesso di un backend min-sum NEON
  nativo. Il pulsante `LDPC` seleziona quindi `fastldpc-neon` sui sistemi ARM64
  compatibili e `fastldpc-avx2-fma` sui sistemi x86 compatibili.
- Se le estensioni SIMD richieste non sono disponibili, Decodium usa
  automaticamente il decoder generico originale senza eseguire istruzioni non
  supportate. In questo modo anche i PC Windows privi di AVX2/FMA possono
  continuare a decodificare.
- `DECODIUM_FT2_DISABLE_FASTLDPC=1` ha ora precedenza assoluta: un'impostazione
  salvata con il toggle acceso non può più riattivare il backend accelerato.
- Il log iniziale indica modello CPU, architettura, AVX/AVX2/FMA,
  OSXSAVE/XCR0, capacità NEON, backend compilato, decoder scelto e motivo
  dell'eventuale fallback.
- Sono stati aggiunti test deterministici per l'override di emergenza e per
  l'equivalenza bit-per-bit fra il min-sum NEON ARM64 e il riferimento scalare.

### Frequenze operative, QO-100 e transverter

- A fianco del selettore della modalità è disponibile un menu frequenza
  compatto e responsive. Riutilizza la tabella Working Frequencies già
  presente, filtra per modalità, evidenzia i valori preferiti ed esegue il QSY
  attraverso il normale percorso bridge/CAT.
- Le voci preferite con modalità `ALL` possono essere usate come frequenza
  rapida della modalità digitale selezionata. I preset satellitari e cross-band
  come QO-100 non devono quindi essere duplicati per ogni modalità.
- Gli offset di stazione/transverter con segno vengono validati su tutto
  l'intervallo supportato. Inserire un secondo offset per la stessa banda
  aggiorna la voce esistente ed evita duplicati ambigui.
- Il polling CAT converte ora la frequenza fisica della radio/IF nella frequenza
  logica on-air mostrata da Decodium. QSY locale, verifica post-QSY, split TX,
  sincronizzazione iniziale e aggiornamenti CAT successivi usano la stessa
  catena reversibile di calibrazione e offset per banda.
- Un target CAT fisico non valido prodotto da un offset viene rifiutato con un
  messaggio chiaro, senza essere inviato silenziosamente alla radio.
- La conferma del QSO include ora la frequenza RX/downlink. Per QO-100 viene
  proposta la frequenza downlink configurata, oppure 10489.540000 MHz come
  valore predefinito sicuro, sempre modificabile dall'operatore.
- I QSO satellitari includono ora in ADIF `PROP_MODE=SAT`, `SAT_NAME`,
  `SAT_MODE`, `FREQ_RX` e il `BAND_RX` derivato. I valori compatibili vengono
  inoltrati anche tramite il percorso esistente verso logger e UDP.

### Stabilità CAT e DecoPort

- L'apertura della radio DecoPort/Hamlib avviene interamente nel worker
  dedicato: una porta seriale lenta o non raggiungibile non blocca più il
  thread grafico.
- Le richieste di apertura ripetute vengono accorpate mentre la connessione è
  in corso; gli stati open/opening sono protetti indipendentemente
  dall'handle Hamlib.
- In chiusura Hamlib e il timer di polling vengono fermati nel thread
  proprietario, l'oggetto viene riportato in sicurezza al chiamante e il worker
  viene atteso. Sono così eliminati rischi di durata QObject fra thread diversi.
- Un nuovo test di regressione verifica il worker dedicato e il rifiuto
  immediato delle richieste di apertura incomplete.

### Finestra Decometer

- Il quadrante Decometer è ora contenuto direttamente nella propria finestra
  nativa, senza un Dialog/Overlay annidato. Questo evita una race del renderer
  threaded di Qt 6.11 osservata all'apertura su macOS.
- Il contenuto viene preparato mentre la finestra è nascosta e mostrato soltanto
  quando il loader asincrono è pronto.
- Le coordinate desktop non sono più vincolate alla posizione centrata. Il
  trascinamento usa coordinate globali esplicite e permette di raggiungere
  bordi e angoli dello schermo senza che la finestra venga respinta.
- Normalizzazione delle dimensioni, geometria salvata, spostamento fra schermi,
  chiusura e polling della telemetria restano gestiti dalla finestra nativa.

### Verifica e download

- La build Qt nativa e i test mirati coprono dispatcher Fast LDPC, equivalenza
  NEON ARM64, simulazione QSO FT2 e comportamento del worker DecoPort.
- Gli artefatti sono prodotti da GitHub Actions per Windows x64, macOS Apple
  Silicon (Tahoe e Sequoia), macOS Intel (Ventura, Sonoma e Sequoia) e Linux
  AppImage x86_64/aarch64. Ogni DMG e AppImage è accompagnato dal relativo
  file SHA-256.
- Gli archivi `.zip` e `.tar.gz` generati da GitHub per il tag `v1.0.599`
  costituiscono il download del codice sorgente di questa release.
