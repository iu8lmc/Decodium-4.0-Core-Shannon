# Decodium 4 FT2 1.0.485

## Release scope

This cumulative release covers the work completed after 1.0.483, including the
1.0.484 decode-delivery changes and the additional warm-run stability work in
1.0.485. The main goal is to keep FT2, FT4 and FT8 decoding responsive without
emptying Full Spectrum or starving the Qt Quick render and audio threads when
several decode passes arrive in the same period.

## English

### Decode-time UI responsiveness

- Correlated the repeatable FT4 and FT8 visual pauses with early, final and deep
  decode result delivery rather than with waterfall rendering itself.
- Reduced work performed by the GUI thread after each decode while preserving
  the existing decode cadence, depth and weak-signal processing.
- Added focused `MAIN-TL`, dispatch and worker diagnostics for legacy mirroring,
  model updates, active thread counts and CPU topology.
- FT4 and FT8 workers continue to run at low priority so the event loop, render
  thread, audio capture and operating-system compositor retain execution time.

### Adaptive cross-platform decoder scheduling

- Added a shared adaptive thread-budget policy for embedded FT2, FT4 and FT8
  decode work.
- The policy scales from low-core computers to larger Intel, AMD and Apple
  systems instead of relying on one fixed thread count.
- Small systems retain at least one core for interactive work; larger systems
  reserve progressively more capacity for the GUI, audio and compositor.
- Manual thread limits and runtime CPU-pressure reductions remain authoritative:
  the adaptive policy can reduce their effective decode count but never exceed
  them.
- FT8 fast, conservative, sign-off and deep follow-up passes use the same
  interactive budget, preventing a later pass from consuming all logical CPUs
  after the application has been open for several minutes.
- Dispatch logs now report selected decode threads, the normal configured limit
  and detected logical-core count for field diagnosis on Windows, macOS and
  Linux.

### Stable Full Spectrum updates

- Reworked `DecodeListModel` snapshot replacement to update overlapping rows in
  place before inserting or removing any difference in row count.
- A completely disjoint replacement no longer removes every row and exposes a
  transient empty model to QML.
- Full Spectrum therefore keeps its delegates alive while early rows are
  replaced by final or deep results, avoiding the visible `No decodes` flash and
  delayed repopulation seen under load.
- Append, shrink, head shift, prepend with tail pruning and middle replacement
  remain incremental and do not issue `modelReset`.
- Decode maps and stable match keys are cached per snapshot, keeping structural
  comparisons linear for large histories.

### Bounded legacy decode mirroring

- The legacy widget-to-QML bridge now exports a recent live window instead of
  reparsing an unbounded text-document history on every revision.
- The 384-row bridge window leaves filtering margin above the 250 rows retained
  by the visible QML panes while keeping warm-run cost bounded.
- Band Activity and Signal RX use the same bounded snapshot policy.
- A band snapshot already built during the current synchronization pass is
  reused by Signal RX rather than mirrored and enriched a second time.
- Existing enriched rows are reused when source data is unchanged, avoiding
  repeated callsign, locator, DXCC, worked-before and highlight lookups.
- World map, reporting, history, MAM, Wait & Pounce and auto-sequence side
  effects are evaluated only for genuinely new rows.

### Qt Quick rendering and filtering

- Hidden and detached decode views release their model binding while inactive.
- Off-screen delegate caching is reduced for the main and detached decode lists.
- Tail-follow callbacks are ignored for inactive views, avoiding duplicate
  scrolling and layout work during busy decode periods.
- Decode filter settings are loaded as one snapshot per backing store instead of
  constructing settings objects repeatedly for individual keys.
- Hot whitespace-tokenization paths avoid per-message temporary regular
  expressions.

### Regression coverage and validation

- Extended `test_decode_list_model` with a disjoint 180-row replacement that
  proves the model never becomes empty and never resets.
- Added `test_ft_decode_thread_budget` for 1, 2, 4, 6, 8, 10, 16 and 24 logical
  CPU configurations, including manual and pressure-reduced limits.
- Added `test_legacy_decode_window` for short histories, bounded warm-run
  histories and explicit empty-window behavior.
- `decodium_qml`, `test_decode_list_model`, `test_ft_decode_thread_budget` and
  `test_legacy_decode_window` build and pass on macOS Apple Silicon with Qt 6.11.
- An extended FT8 runtime check on a 10-core Apple system selected 6 active
  decoder threads from an 8-thread normal limit and remained free of main-event
  loop stalls while Full Spectrum stayed populated.
- `git diff --check` completes without whitespace errors.

### Release assets

GitHub Actions build the Windows x64 installer, macOS Apple Silicon DMGs, macOS
Intel DMGs, Linux x86_64 AppImage and Linux aarch64 AppImage. Matching ZIP
archives and checksums are attached where produced by each platform workflow.
The tagged source tree is available through GitHub's automatic source archives.

## Italiano

### Reattivita' durante le decodifiche

- I piccoli blocchi ripetibili di FT4 e FT8 sono stati correlati alla consegna
  dei risultati early, final e deep, non al rendering del waterfall.
- Ridotto il lavoro eseguito dal thread grafico dopo ogni decode, mantenendo
  cadenza, profondita' e trattamento weak-signal esistenti.
- Aggiunti diagnostici mirati `MAIN-TL`, dispatch e worker per mirror legacy,
  aggiornamenti modello, thread attivi e topologia CPU.
- I worker FT4 e FT8 restano a priorita' bassa per lasciare tempo di esecuzione
  a event loop, render thread, audio e compositor del sistema operativo.

### Scheduling adattivo multipiattaforma

- Introdotta una politica condivisa e adattiva per il budget thread dei decoder
  embedded FT2, FT4 e FT8.
- Il calcolo si adatta a computer con pochi core e a sistemi Intel, AMD e Apple
  piu' grandi, evitando un unico valore fisso.
- I sistemi piccoli conservano almeno un core per il lavoro interattivo; quelli
  piu' grandi riservano progressivamente piu' capacita' a GUI, audio e compositor.
- Limiti manuali e riduzioni dovute alla pressione CPU restano prioritari: il
  budget adattivo puo' ridurli ma non superarli.
- Le passate FT8 fast, conservative, sign-off e deep follow-up usano lo stesso
  budget interattivo, impedendo alle passate tardive di saturare tutte le CPU
  logiche dopo diversi minuti di funzionamento.
- I log di dispatch riportano thread scelti, limite normale e numero di core
  rilevati per facilitare la diagnosi su Windows, macOS e Linux.

### Full Spectrum stabile

- `DecodeListModel` aggiorna ora le righe sovrapposte in-place prima di inserire
  o rimuovere l'eventuale differenza di dimensione.
- La sostituzione di due snapshot completamente diversi non elimina piu'
  temporaneamente tutte le righe esponendo un modello vuoto al QML.
- Full Spectrum mantiene quindi vivi i delegate durante il passaggio da early a
  final o deep, senza il flash `No decodes` e senza ripopolamento ritardato.
- Append, riduzione, shift dalla testa, prepend con potatura e sostituzione
  centrale restano incrementali e non generano `modelReset`.
- Mappe e chiavi stabili vengono calcolate una volta per snapshot, mantenendo
  lineare il confronto anche con cronologie ampie.

### Mirror legacy con costo limitato

- Il bridge dal widget legacy al QML esporta una finestra recente invece di
  rileggere una cronologia testuale senza limite a ogni revisione.
- La finestra di 384 righe lascia margine ai filtri rispetto alle 250 righe
  visibili nel QML e mantiene costante il costo dopo molte ore di utilizzo.
- Band Activity e Signal RX seguono la stessa politica bounded.
- Lo snapshot di banda gia' creato nello stesso ciclo viene riutilizzato da
  Signal RX, evitando un secondo mirror con arricchimento completo.
- Le righe gia' arricchite vengono riutilizzate se i dati sorgente non cambiano,
  riducendo lookup ripetuti di call, locator, DXCC, worked-before e highlight.
- Mappa, reporting, history, MAM, Wait & Pounce e auto-sequenza vengono elaborati
  soltanto per le righe realmente nuove.

### Rendering Qt Quick e filtri

- Le viste decode nascoste o detached rilasciano il model binding quando non
  sono attive.
- Ridotta la cache dei delegate fuori schermo nelle liste principali e pop-out.
- I callback di inseguimento della coda vengono ignorati nelle viste inattive,
  evitando scroll e layout duplicati durante i periodi affollati.
- Le impostazioni dei filtri vengono lette in uno snapshot per archivio, senza
  creare ripetutamente oggetti settings per ogni chiave.
- I percorsi caldi di tokenizzazione evitano espressioni regolari temporanee per
  ogni messaggio.

### Test e validazione

- Esteso `test_decode_list_model` con una sostituzione disgiunta di 180 righe che
  dimostra l'assenza di modello vuoto e di reset.
- Aggiunto `test_ft_decode_thread_budget` per configurazioni da 1, 2, 4, 6, 8,
  10, 16 e 24 CPU logiche, inclusi limiti manuali e ridotti dalla pressione CPU.
- Aggiunto `test_legacy_decode_window` per cronologie corte, warm-run bounded e
  finestra vuota esplicita.
- `decodium_qml`, `test_decode_list_model`, `test_ft_decode_thread_budget` e
  `test_legacy_decode_window` compilano e superano i test su macOS Apple Silicon
  con Qt 6.11.
- In una prova FT8 prolungata su Apple 10-core sono stati selezionati 6 thread
  attivi su un limite normale di 8, senza stall del main event loop e con Full
  Spectrum sempre popolato.
- `git diff --check` completato senza errori di whitespace.

### Asset della release

Le GitHub Actions generano installer Windows x64, DMG macOS Apple Silicon, DMG
macOS Intel, AppImage Linux x86_64 e AppImage Linux aarch64. ZIP e checksum
vengono allegati quando prodotti dal relativo workflow. Il codice sorgente
taggato e' disponibile negli archivi automatici di GitHub.
