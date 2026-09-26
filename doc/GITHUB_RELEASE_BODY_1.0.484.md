# Decodium 4 FT2 1.0.484

## Release scope

This release focuses on UI responsiveness during the periodic FT4 and FT8
decode passes. It reduces main-thread work when early, final and deep decode
results arrive, prevents decode-list model resets and reserves CPU capacity for
the Qt Quick render and event threads.

## English

### Decode-time UI responsiveness

- Correlated the repeatable FT4 and FT8 pauses with the early, final and deep
  decode result windows rather than with waterfall rendering itself.
- Preserved the existing decode cadence, depth and weak-signal processing while
  reducing the amount of post-decode work performed by the GUI thread.
- Added focused main-thread timing around legacy mirroring and decode-model
  rebuilds so remaining stalls can be identified from `MAIN-TL` diagnostics.

### CPU scheduling

- The embedded FT2/FT4/FT8 decoder now honours the bridge's effective thread
  limit instead of automatically consuming every logical CPU.
- Automatic and manually configured decoder thread counts are capped so that
  logical cores remain available for the UI and audio pipeline.
- FT4 and FT8 decode worker threads run at low priority, reducing event-loop and
  render-thread starvation on Windows and other busy systems.

### Incremental decode-list updates

- Reworked `DecodeListModel` updates to avoid full model resets.
- Append, shrink, head shift, prepend with tail pruning and early-to-final
  middle replacement are now emitted as incremental row operations.
- Decode maps and stable match keys are cached once per snapshot, keeping the
  structural comparison linear for large 500-row decode histories.
- Existing delegates remain alive when a provisional early pass is replaced by
  final or deep results, avoiding a complete Qt Quick delegate rebuild.

### Legacy mirror and filtering

- Existing enriched decode entries are reused when their source data has not
  changed, avoiding repeated callsign, grid, DXCC, worked-before and highlight
  lookups across the complete history.
- World-map, PSK Reporter, history, MAM, Wait & Pounce and auto-sequence side
  effects are evaluated only for newly received rows.
- Decode filter settings are loaded as one snapshot from each backing store
  instead of repeatedly constructing `QSettings` objects for every key.
- Hot whitespace tokenization paths no longer create temporary regular
  expressions for every decoded message.
- Signal RX avoids a second full rebuild when the legacy mirror already owns
  the complete RX history.

### Qt Quick list rendering

- Hidden and detached decode views release their model binding while inactive.
- Off-screen delegate caching is reduced for the main and detached decode
  lists.
- Tail-follow callbacks are ignored for inactive views, preventing duplicate
  scrolling and layout work during pile-ups.

### Regression coverage and validation

- Added `test_decode_list_model` with append, shift, prepend/prune and
  early-to-final replacement coverage.
- A 500-row provisional-to-final replacement completes without `modelReset`
  and is measured at approximately 1 ms in the Release test build.
- `decodium_qml`, `test_decode_list_model`, `test_streaming_list_model` and
  `test_ftx_weak_decode` build and pass on macOS arm64 with Qt 6.11.
- `git diff --check` completes without whitespace errors.

### Release assets

GitHub Actions build the Windows x64 installer, macOS Apple Silicon DMGs,
macOS Intel DMGs, Linux x86_64 AppImage and Linux aarch64 AppImage. Matching ZIP
archives and checksums are attached where produced by the platform workflow.
The tagged source tree is available through GitHub's automatic source archives.

## Italiano

### Reattivita' della GUI durante le decodifiche

- Correlati i blocchi ripetibili FT4 e FT8 con le finestre di consegna dei
  risultati early, final e deep, anziche' con il rendering del waterfall.
- Mantenuti cadenza, profondita' e trattamento weak-signal esistenti, riducendo
  il lavoro successivo alla decodifica eseguito dal thread grafico.
- Aggiunte misure mirate del main thread per mirror legacy e ricostruzione dei
  modelli, visibili nei diagnostici `MAIN-TL`.

### Scheduling CPU

- Il decoder embedded FT2/FT4/FT8 rispetta ora il limite thread effettivo del
  bridge invece di utilizzare automaticamente tutte le CPU logiche.
- Sia il conteggio automatico sia quello configurato manualmente lasciano core
  disponibili alla GUI e alla pipeline audio.
- I worker FT4 e FT8 vengono avviati a priorita' bassa, riducendo l'affamamento
  dell'event loop e del render thread, in particolare su Windows.

### Aggiornamenti incrementali delle liste decode

- Rielaborato `DecodeListModel` eliminando i reset completi del modello.
- Append, riduzione, shift dalla testa, prepend con potatura della coda e
  sostituzione early-to-final sono ora operazioni incrementali sulle righe.
- Mappe e chiavi di confronto vengono calcolate una sola volta per snapshot,
  mantenendo lineare il confronto anche con cronologie da 500 righe.
- I delegate Qt Quick esistenti non vengono piu' distrutti quando una passata
  provvisoria viene sostituita dai risultati final o deep.

### Mirror legacy e filtri

- Le righe gia' arricchite vengono riutilizzate quando i dati sorgente non sono
  cambiati, evitando lookup ripetuti di nominativo, locator, DXCC, worked-before
  ed evidenziazione sull'intera cronologia.
- Mappa, PSK Reporter, history, MAM, Wait & Pounce e auto-sequenza vengono
  valutati soltanto per le righe realmente nuove.
- Le impostazioni dei filtri vengono lette in un unico snapshot per archivio,
  senza costruire ripetutamente oggetti `QSettings` per ogni chiave.
- I percorsi caldi di tokenizzazione non compilano piu' espressioni regolari
  temporanee per ogni messaggio.
- Signal RX evita una seconda ricostruzione completa quando il mirror legacy
  possiede gia' tutta la cronologia RX.

### Rendering delle liste Qt Quick

- Le viste decode nascoste o detached rilasciano il modello mentre non sono
  attive.
- Ridotta la cache dei delegate fuori schermo nelle viste principali e pop-out.
- I callback di inseguimento della coda vengono ignorati nelle viste inattive,
  evitando scroll e layout duplicati durante i pile-up.

### Copertura regressioni e validazione

- Aggiunto `test_decode_list_model` per append, shift, prepend/prune e
  sostituzione early-to-final.
- La sostituzione di una passata provvisoria su 500 righe avviene senza
  `modelReset` e richiede circa 1 ms nella build Release di test.
- `decodium_qml`, `test_decode_list_model`, `test_streaming_list_model` e
  `test_ftx_weak_decode` compilano e superano i test su macOS arm64 con Qt 6.11.
- `git diff --check` completato senza errori di whitespace.

### Asset della release

Le GitHub Actions generano installer Windows x64, DMG macOS Apple Silicon, DMG
macOS Intel, AppImage Linux x86_64 e AppImage Linux aarch64. Gli archivi ZIP e i
checksum vengono allegati quando prodotti dal relativo workflow. Il codice
sorgente taggato e' disponibile negli archivi automatici di GitHub.
