# Decodium 4 v1.0.505

## English

Release highlights (`v1.0.503 -> v1.0.505`):

### Decodium Live Map

- Introduced a native Map Intelligence architecture with persistent SQLite
  storage, background ADIF processing, incremental updates and a generic,
  persistent layer model.
- Added operational layers for live traffic, historical and confirmed QSOs,
  active and missing Maidenhead grids, PSK spots, POTA, US states and
  counties, IOTA, WPX, Moon data and propagation or geographic overlays.
- Added external overlay management for radar, lightning, MUF, foF2,
  sporadic-E, aurora, tropo, earthquakes and wildfires, with explicit offline
  behavior and provider status.
- Added local and online base-map providers, offline mode, refresh controls,
  persistent layer choices and restored detached-map visibility after restart.
- Expanded the GPU map renderer with geographic boundaries, coverage cells,
  operational markers, hover and selection details, map projections, UTC time
  zones, split-grid display, live decay and push-pin views.
- Added responsive Map Intelligence views for map controls, call roster,
  logbook, statistics, awards and alerts.
- Added grid detail dialogs, call lookup actions, filtering by band, mode,
  period, continent, DXCC and source, and independent query paths for live
  decoder traffic and ADIF history.

### Logbook, roster and awards

- Connected statistics, worked or confirmed state and award progress to the
  active ADIF logbook and the persistent map database.
- Added incremental ADIF import, filtered logbook views, export, sorting and
  operational comparisons.
- Added an actionable call roster with configurable columns, retention,
  sorting, CQ and text filters, wanted or confirmation states, watch and
  ignore rules, lookup actions and QSO preparation.
- Added award tracking and operational targets for DXCC, Maidenhead, WAZ,
  WAS, ITU zones, US48, WAC and the bundled Decodium award catalog.
- Added live spot analytics, heatmap, timeline, path correlation and alert
  rules for new grids, new DXCC entities, CQ activity and callsign patterns.

### Radio workflow and decode runtime

- Improved roster-driven QSO arming so a prepared call enters the normal
  slot-aligned transmit workflow instead of showing a false early TX state.
- Continued the portable QSO sequencer split, including deferred and pending
  state, progression decisions and the sequencer sink boundary.
- Improved FT4 and FT8 worker scheduling, lifecycle handling and diagnostic
  coverage while preserving supported weak-signal decode behavior.
- Hardened runtime-data lookup after repository reorganization, including the
  false-call database in installed and source-tree layouts.
- Kept the Windows installer language aligned with the operating-system
  language and removed residual fixed-language installer text.

### Test status

- Corrected the false-call bridge test by restoring lookup of
  `resources/runtime/ALLCALL7.TXT`; the test remains mandatory.
- The FST4 native synthetic 15-second fixture is explicitly accepted as a
  test-bench limitation when the native decoder produces no output row. The
  bridge comparison still runs whenever native output is available.
- FT8 very-deep decode at `-27 dB` is explicitly marked as an accepted
  experimental limit. The supported `-26 dB` weak-decode test remains
  mandatory and is not skipped.

### Release assets

- Windows x64 installer.
- macOS Apple Silicon DMG and ZIP for Tahoe and Sequoia.
- macOS Intel DMG and ZIP for Ventura, Sonoma and Sequoia.
- Linux x86_64 AppImage and SHA-256 file.
- Linux aarch64 AppImage and SHA-256 file.
- GitHub source archives.

## Italiano

Novita principali (`v1.0.503 -> v1.0.505`):

### Decodium Live Map

- Introdotta un'architettura Map Intelligence nativa con persistenza SQLite,
  elaborazione ADIF in background, aggiornamenti incrementali e modello
  generico dei layer con stato persistente.
- Aggiunti layer operativi per traffico live, QSO storici e confermati,
  griglie Maidenhead attive e mancanti, spot PSK, POTA, stati e contee USA,
  IOTA, WPX, dati lunari e overlay geografici o di propagazione.
- Aggiunta la gestione degli overlay esterni per radar, fulmini, MUF, foF2,
  Es sporadico, aurora, tropo, terremoti e incendi, con comportamento offline
  esplicito e stato dei provider.
- Aggiunti provider mappa locali e online, modalita offline, controlli di
  aggiornamento, persistenza dei layer e ripristino della mappa separata dopo
  il riavvio.
- Esteso il renderer GPU con confini geografici, celle di copertura, marker
  operativi, dettagli hover e selezione, proiezioni, fusi UTC, split grid,
  decadimento del traffico live e visualizzazione push-pin.
- Aggiunte viste responsive per controlli mappa, roster, logbook, statistiche,
  award e alert.
- Aggiunti dettagli locator, lookup nominativi e filtri per banda, modo,
  periodo, continente, DXCC e sorgente, mantenendo separate le query dei
  decode live e dello storico ADIF.

### Logbook, roster e award

- Collegate statistiche, stato worked o confirmed e progressione award al
  logbook ADIF attivo e al database persistente della mappa.
- Aggiunti import ADIF incrementale, logbook filtrabile, export, ordinamento e
  confronti operativi.
- Aggiunto un call roster operativo con colonne configurabili, retention,
  ordinamento, filtri CQ e testuali, stati wanted o confirmed, regole watch e
  ignore, lookup e preparazione del QSO.
- Aggiunto il tracking per DXCC, Maidenhead, WAZ, WAS, zone ITU, US48, WAC e
  per il catalogo award incluso in Decodium.
- Aggiunte analisi degli spot live, heatmap, timeline, correlazione percorsi e
  regole di alert per nuove griglie, nuovi DXCC, CQ e pattern nominativo.

### Flusso radio e runtime decode

- Migliorato l'arming del QSO dal roster: la chiamata preparata entra nel
  normale flusso TX allineato allo slot senza mostrare una falsa TX anticipata.
- Proseguita la separazione del sequencer QSO portabile, inclusi stato
  deferred e pending, decisioni di avanzamento e interfaccia del sink.
- Migliorati scheduling, ciclo di vita e diagnostica dei worker FT4 e FT8,
  conservando il comportamento weak-signal supportato.
- Reso robusto il lookup dei dati runtime dopo la riorganizzazione del
  repository, incluso il database anti-false-call nei layout installati e nel
  source tree.
- Allineata la lingua dell'installer Windows a quella del sistema operativo ed
  eliminato il testo residuo a lingua fissa.

### Stato dei test

- Corretto il test del bridge anti-false-call ripristinando il lookup di
  `resources/runtime/ALLCALL7.TXT`; il test resta obbligatorio.
- Il fixture sintetico FST4 nativo da 15 secondi e dichiarato esplicitamente
  come limite del banco di prova quando il decoder nativo non produce righe.
  Il confronto del bridge viene comunque eseguito quando l'output nativo e
  disponibile.
- Il decode FT8 very-deep a `-27 dB` e dichiarato esplicitamente come limite
  sperimentale accettato. Il test weak-decode supportato a `-26 dB` resta
  obbligatorio e non viene saltato.

### Asset della release

- Installer Windows x64.
- DMG e ZIP macOS Apple Silicon per Tahoe e Sequoia.
- DMG e ZIP macOS Intel per Ventura, Sonoma e Sequoia.
- AppImage Linux x86_64 e relativo SHA-256.
- AppImage Linux aarch64 e relativo SHA-256.
- Archivi sorgente GitHub.
