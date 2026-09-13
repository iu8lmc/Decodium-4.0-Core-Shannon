# Decodium 4 FT2 1.0.489

## English

This release covers the performance and interface work completed after 1.0.488.
Its main goal is to keep FT8 and FT4 decoding responsive on Windows, macOS and
Linux while preserving decode depth, weak-signal sensitivity and immediate QSO
automation.

### Time-budgeted FT8 result delivery

- The embedded FT8 worker now returns structured, immutable decoded entries
  instead of forcing the GUI thread to parse the same text rows again.
- Messages directed to the local station, signal reports, `RRR`, `RR73` and
  `73` are placed in the priority queue so AutoSeq and active QSO handling are
  serviced before background display work.
- Remaining rows are drained in short event-loop turns, limited to six rows and
  approximately five milliseconds per cycle. Work resumes with a zero-delay
  timer instead of blocking one render frame with a complete decode batch.
- Logging, transport publication, map updates, DXCC enrichment and reporting
  use independent low-priority queues. A busy Full Spectrum update can no
  longer delay a directed reply.
- FT8 dispatch diagnostics now report wall time, CPU time per cycle and event
  loop resume lag in the normal diagnostic log.

### Incremental native decode models

- Band Activity, Signal RX and Full Spectrum now have separate native models.
  Full Spectrum no longer shares every synchronous update with Band Activity.
- Filtering, sorting, ghost rejection, RX membership and snapshot preparation
  run on a dedicated QtConcurrent worker rather than on the GUI thread.
- Legacy decode mirroring consumes only newly appended rows whenever possible;
  full reconstruction remains available as a safe fallback after mode, band or
  filter changes.
- Model changes are applied incrementally with a per-cycle row budget. Existing
  rows remain visible while a new snapshot is installed, avoiding transient
  empty lists and large delegate rebuilds.
- QML counters, tail-follow and decode labels are coalesced after snapshot
  completion. Hidden and detached panes no longer repeat the same update work.
- Full Spectrum refreshes are deferred briefly during the critical decode
  delivery window and then drained independently.

### Adaptive decoder and render scheduling

- Native FT4 and FT8 workers reserve at least one logical core for GUI, audio
  and rendering work.
- Decoder worker and OpenMP threads use utility or below-normal scheduling on
  macOS, Windows and Linux.
- FT8 DEEP mode enables a micro-stall guard after at least three event-loop
  stalls of 90 ms or more within 30 seconds. Only the DEEP pass loses one
  worker thread; normal operation is restored after 16 clean periods.
- The first 15 seconds after startup are excluded from adaptive decisions so
  shader, cache and device initialization cannot reduce the steady-state
  decoder budget.
- The legacy panadapter uses a 66 ms interval while the decoder is idle, 125 ms
  during DEEP and 180-250 ms only under measured pressure.
- Map updates are deferred during TX, tuning and decode bursts and are resumed
  through a bounded low-priority queue.

### FT4 startup and hash preparation

- FT4 hash seeds are stored in a compact persistent JSON cache with source
  path, size and modification-time validation.
- A valid cache avoids rescanning historical `ALL.TXT` files at every startup.
  Changed sources automatically invalidate and rebuild the relevant data.
- FT4 also uses the shared UI-core reservation and native worker scheduling
  policy without changing its decoder depth or on-air timing.

### Toolbar and decode-window refinements

- Transmission toolbar actions now use consistent compact widths.
- Horizontal padding and icon-to-label spacing were reduced so more controls
  fit without truncation on smaller displays.
- The FT2-Link mode selector retains enough width to show its complete name.
- Duplicate snapshot handlers in the detached decode window were removed, so
  each completed model update produces one coalesced UI refresh.

### Validation

- Added regression coverage for budgeted snapshot replacement, append, target
  supersession and per-cycle row limits.
- Extended decoder scheduling tests with UI-core reservation, FT8 micro-stall
  activation/restoration and adaptive panadapter intervals.
- A sustained FT8 runtime check showed typical UI batches around 4 ms, with
  approximately 0.8-1.2 ms CPU work per event-loop cycle. The previous
  25-32 ms synchronous snapshot callback no longer appeared.
- CAT and legacy macOS audio capture remained stable during the clean
  single-instance FT8 validation period.

### Release assets

GitHub Actions build the Windows x64 installer, macOS Apple Silicon DMGs,
macOS Intel DMGs, Linux x86_64 AppImage and Linux aarch64 AppImage. ZIP archives
and SHA-256 files are attached where produced by each platform workflow. The
tagged source is available through GitHub's automatic source archives.

## Italiano

Questa release raccoglie il lavoro di prestazioni e interfaccia completato dopo
la 1.0.488. L'obiettivo principale e' mantenere fluide le decodifiche FT8 e FT4
su Windows, macOS e Linux senza ridurre profondita', sensibilita' weak-signal o
reattivita' dell'automazione QSO.

### Consegna FT8 con budget temporale

- Il worker FT8 embedded restituisce ora risultati strutturati e immutabili,
  evitando una seconda analisi delle stesse righe sul thread grafico.
- Messaggi diretti alla stazione locale, rapporti, `RRR`, `RR73` e `73` entrano
  nella coda prioritaria, servendo subito AutoSeq e il QSO attivo.
- Le altre righe vengono elaborate in turni brevi dell'event loop, limitati a
  sei righe e circa cinque millisecondi per ciclo, con ripresa tramite timer a
  ritardo zero.
- Log, trasporto, mappa, arricchimento DXCC e reporting utilizzano code
  indipendenti a bassa priorita'. Un aggiornamento Full Spectrum intenso non
  puo' piu' ritardare una risposta diretta.
- Le metriche FT8 riportano nel diagnostic log tempo totale, CPU per ciclo e
  ritardo di ripresa dell'event loop.

### Modelli decode nativi e incrementali

- Band Activity, Signal RX e Full Spectrum usano modelli nativi separati.
- Filtri, ordinamento, rifiuto ghost, appartenenza RX e preparazione snapshot
  vengono eseguiti da un worker QtConcurrent dedicato.
- Il mirror legacy usa solo le nuove righe quando possibile e conserva la
  ricostruzione completa come fallback dopo cambi di modo, banda o filtri.
- Gli aggiornamenti sono applicati per delta con un limite di righe per ciclo;
  le righe esistenti restano visibili mentre arriva il nuovo snapshot.
- Contatori QML, inseguimento della coda e label decode vengono coalesciuti dopo
  il completamento dello snapshot.
- Full Spectrum viene rinviato brevemente durante la finestra critica di
  consegna e poi aggiornato in modo indipendente.

### Scheduling adattivo di decoder e rendering

- I worker FT4 e FT8 riservano almeno un core logico a GUI, audio e rendering.
- Worker e thread OpenMP usano priorita' utility o below-normal su macOS,
  Windows e Linux.
- FT8 DEEP attiva una protezione dopo almeno tre stalli da 90 ms in 30 secondi:
  viene ridotto di uno solo il numero di thread DEEP e il budget normale torna
  dopo 16 periodi puliti.
- I primi 15 secondi di avvio non influenzano la politica adattiva.
- Il panadapter legacy usa 66 ms a decoder libero, 125 ms durante DEEP e
  180-250 ms soltanto sotto pressione misurata.
- Gli aggiornamenti mappa vengono rinviati durante TX, tune e burst decode e
  ripresi tramite una coda limitata a bassa priorita'.

### Avvio FT4 e preparazione hash

- I seed hash FT4 vengono salvati in una cache JSON compatta con validazione di
  percorso, dimensione e data di modifica dei file sorgente.
- Una cache valida evita di rileggere tutti gli `ALL.TXT` a ogni avvio; le
  sorgenti cambiate vengono rilevate e ricostruite automaticamente.
- FT4 usa anche la riserva core e la priorita' worker condivise senza cambiare
  profondita' del decoder o timing radio.

### Toolbar e finestre decode

- I comandi della toolbar TX hanno ora larghezze compatte e uniformi.
- Ridotti padding orizzontale e distanza tra simbolo e testo.
- Il selettore FT2-Link conserva lo spazio necessario al nome completo.
- Eliminati handler snapshot duplicati nella finestra decode separata: ogni
  snapshot completato genera un solo aggiornamento grafico coalesciuto.

### Verifica

- Aggiunti test per sostituzione e append a tranche, nuovo target durante un
  aggiornamento e limite righe per ciclo.
- Estesi i test di scheduling per riserva core, attivazione/ripristino della
  protezione FT8 e intervalli adattivi del panadapter.
- In una prova FT8 prolungata i batch UI tipici sono risultati intorno a 4 ms,
  con circa 0,8-1,2 ms di CPU per ciclo. Il precedente callback sincrono da
  25-32 ms non e' piu' comparso.
- CAT e acquisizione audio legacy macOS sono rimaste stabili durante la prova
  pulita con una singola istanza.

### Asset della release

Le GitHub Actions generano installer Windows x64, DMG macOS Apple Silicon, DMG
macOS Intel, AppImage Linux x86_64 e AppImage Linux aarch64. ZIP e checksum
SHA-256 vengono allegati quando prodotti dal workflow. Il sorgente taggato e'
disponibile negli archivi automatici di GitHub.
