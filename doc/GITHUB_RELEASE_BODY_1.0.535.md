# Decodium 4.0 v1.0.535

Version 1.0.535 consolidates the work delivered after v1.0.533: the complete
1.0.534 translation catalogue update, a fully serialised Hamlib CAT
disconnect/reconnect lifecycle, safer settings persistence, lighter Live Map
updates, and improved Windows hybrid-GPU selection.

## English (British)

### v1.0.534 — Translation catalogue completion

- Added the 100 missing interface entries to all 14 translation catalogues.
- Completed the remaining English, Italian and multilingual strings covering
  Auto Call, RTL-SDR SSB controls, LoTW/eQSL/QRZ confirmation handling, map
  intelligence and related settings.
- Kept the catalogue contexts aligned so that translated text is selected
  consistently at runtime instead of falling back to Italian source strings.

### v1.0.535 — CAT serialisation and responsiveness

- Fully serialised Hamlib disconnect and reconnect operations. A new CAT
  connection is now queued until the previous transceiver has completed
  `rig_close`, emitted its worker shutdown signal and its `QThread` has emitted
  `finished()`.
- Prevented the previous Windows failure mode where a second `rig_open` could
  reach the same COM port while the old handle was still closing, producing
  `serial port \\.\\COMx is already open` and `Access denied`.
- Applied the same lifecycle guard to PWR/SWR polling changes, CI-V changes,
  split-mode changes, TCI audio changes, automatic CAT retries and connection
  watchdog recovery.
- Kept the GUI thread non-blocking. A graceful CAT shutdown is allowed to
  complete first; a bounded safety timeout remains available if a backend is
  stuck in I/O.
- Reworked queued reconnect ownership so a direct disconnect cancels pending
  reconnects while an intentional parameter-change reconnect is performed
  exactly once after cleanup.

### Responsiveness and graphics safety

- Moved several settings writes to the asynchronous persistence path, reducing
  synchronous disk work during active decoding and settings interaction.
- Coalesced Live Map snapshot refreshes over a longer, pressure-aware window and
  requested chart repaint only when the chart is visible and has valid geometry.
- Added Windows NVIDIA Optimus and AMD PowerXpress high-performance hints before
  Qt creates the graphics device, while preserving the user's explicit Windows
  per-application GPU choice.
- No waterfall, panadapter or digital audio processing path was changed by the
  CAT fix. The graphics and map safeguards only reduce avoidable UI work.

### Validation

- Local `decodium_qml` build completed successfully.
- Targeted CAT-adjacent and regression tests completed successfully:
  `test_cat4om_manager`, `test_callsign_intelligence`,
  `test_map_layer_service` and `test_rtlsdr_dsp`.
- `git diff --check` completed without errors.

## Italiano

La versione 1.0.535 consolida il lavoro successivo alla 1.0.533: il
completamento dei cataloghi di traduzione della 1.0.534, la serializzazione
completa del ciclo CAT Hamlib, salvataggi piu' sicuri, aggiornamenti Live Map
piu' leggeri e una migliore selezione della GPU ibrida su Windows.

### 1.0.534 — Completamento dei cataloghi

- Aggiunte le 100 voci mancanti a tutti i 14 cataloghi di traduzione.
- Completate le stringhe inglesi, italiane e multilingue relative ad Auto Call,
  controlli SSB RTL-SDR, conferme LoTW/eQSL/QRZ, intelligenza della mappa e
  relative impostazioni.
- Allineati i contesti dei cataloghi per evitare fallback incoerenti alle
  stringhe italiane presenti nel codice sorgente.

### 1.0.535 — Serializzazione CAT e stabilita'

- Serializzato completamente il ciclo di disconnessione e riconnessione CAT
  Hamlib. Una nuova connessione parte solo dopo il completamento di `rig_close`,
  la terminazione del worker e il segnale `QThread::finished()`.
- Risolto il caso Windows in cui un secondo `rig_open` poteva raggiungere la
  stessa COM mentre il vecchio handle era ancora in chiusura, causando gli
  errori `serial port ... is already open` e `Access denied`.
- Applicata la stessa protezione alle modifiche del polling PWR/SWR, CI-V,
  split, audio TCI, retry CAT automatici e watchdog di connessione.
- Mantenuta la GUI non bloccante: la chiusura CAT normale ha il tempo di
  completarsi e resta disponibile un timeout di sicurezza limitato per i
  backend bloccati in I/O.
- Resa deterministica la gestione delle riconnessioni accodate: una
  disconnessione esplicita annulla i retry pendenti, mentre una riconnessione
  richiesta da una modifica dei parametri viene eseguita una sola volta dopo la
  pulizia completa.

### Reattivita' e sicurezza grafica

- Spostate diverse scritture delle impostazioni sul percorso asincrono, con
  meno lavoro sincrono su disco durante decodifica e interazione con i dialoghi.
- Accorpati gli aggiornamenti della Live Map in una finestra piu' leggera e
  dipendente dalla pressione del sistema; il repaint del grafico parte solo
  quando il grafico e' visibile e ha dimensioni valide.
- Aggiunti su Windows gli hint NVIDIA Optimus e AMD PowerXpress per preferire
  la GPU ad alte prestazioni prima della creazione del dispositivo Qt, senza
  sovrascrivere la scelta esplicita dell'utente nelle impostazioni grafiche di
  Windows.
- Il fix CAT non modifica i percorsi waterfall, panadapter o audio digitale.
  Le protezioni grafiche e della mappa riducono soltanto il lavoro UI evitabile.

### Verifica

- Build locale di `decodium_qml` completata correttamente.
- Test mirati completati correttamente: `test_cat4om_manager`,
  `test_callsign_intelligence`, `test_map_layer_service` e `test_rtlsdr_dsp`.
- `git diff --check` completato senza errori.

## Release assets

The release workflows publish the Windows x64 executable, macOS Intel and
Apple Silicon DMG packages, and Linux x86_64 and aarch64 AppImages together
with their checksums where provided by the workflow.
