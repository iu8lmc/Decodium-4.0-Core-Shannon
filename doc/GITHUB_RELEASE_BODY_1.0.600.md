# Decodium 4 FT2 v1.0.600

Version 1.0.600 is a focused maintenance release based on v1.0.599. It keeps
the main dashboard maximised while confirming a QSO on Windows, makes the QSO
logging fallback follow the selected interface language, places the compact
working-frequency selector before the band buttons, and hardens the macOS
Intel build when OpenMP is unavailable.

## English (UK)

### QSO logging window on Windows

- Opening the themed **Confirm QSO logging** window no longer calls `show()`
  again on an already visible host window. On Windows that operation could
  change a maximised `QWindow` back to its stored normal geometry, making the
  whole Decodium dashboard suddenly appear smaller.
- The logging path records whether its host was maximised and applies a
  deferred state guard after showing the native transient dialog. Window
  managers that report the transition asynchronously can therefore no longer
  leave the dashboard in windowed mode.
- A genuinely minimised host is still restored before presenting the prompt,
  and the existing raise/activation behaviour is retained.

### Consistent language in the logging prompt

- The emergency QWidget logging dialog remains available if no visible QML
  TX panel acknowledges the request. Its title, field labels, cluster state,
  and **Add/Skip** buttons now use the same `TxPanel` translation context as
  the themed QML dialog.
- This removes the apparent language change that could occur when one QSO used
  the Italian QML prompt and a later QSO fell back to the formerly hard-coded
  English safety dialog.
- The `Grid` label is now translatable in QML and is rendered as `Locator` in
  the Italian interface.

### Compact working-frequency selector

- The working-frequency selector has moved from beside the operating-mode
  control to the upper band row, immediately before the `160` metre button.
  The FT8/mode row is consequently less crowded, while the selected dial
  frequency remains visible at a glance.
- The band list starts after the fixed compact selector and retains horizontal
  scrolling on narrow windows. Space is reserved for the dock drag handle so
  it does not cover the frequency value.
- The selector still uses the existing Working Frequencies database and the
  normal bridge/CAT QSY path introduced in v1.0.599; this release changes its
  placement, not its tuning semantics.

### macOS Intel packaging resilience

- The FT8 Stage 4 worker-count value is explicitly allowed to be unused in
  builds where OpenMP is not enabled. This prevents the macOS Intel release
  configuration from failing under warnings-as-errors while leaving threaded
  builds and decoder behaviour unchanged.

### Validation and downloads

- The native Qt target builds successfully, the modified QML passes
  `qmllint`, and the source changes pass whitespace validation.
- GitHub Actions builds the Windows x64 installer, macOS Apple Silicon DMGs
  for Tahoe and Sequoia, macOS Intel DMGs for Ventura, Sonoma and Sequoia, and
  Linux AppImages for x86_64 and aarch64. DMG and AppImage downloads include
  SHA-256 companion files.
- GitHub's generated `.zip` and `.tar.gz` archives for tag `v1.0.600` are the
  source-code downloads for this release.

---

## Italiano

La versione 1.0.600 è un aggiornamento di manutenzione mirato basato sulla
v1.0.599. Mantiene massimizzata la dashboard durante la conferma di un QSO su
Windows, rende coerente la lingua anche nel dialogo di sicurezza, sposta il
selettore compatto delle frequenze prima dei pulsanti di banda e rende più
robusta la build macOS Intel quando OpenMP non è disponibile.

### Finestra di registrazione QSO su Windows

- L'apertura della finestra a tema **Conferma registrazione QSO** non richiama
  più `show()` su una finestra principale già visibile. Su Windows questa
  operazione poteva riportare una `QWindow` massimizzata alla geometria normale
  salvata, facendo diventare improvvisamente piccola l'intera dashboard.
- Il percorso di registrazione memorizza se la finestra ospite era
  massimizzata e applica un controllo differito dopo l'apertura del dialogo
  nativo transitorio. Anche i window manager che comunicano il cambio di stato
  in ritardo non possono quindi lasciare la dashboard in modalità finestra.
- Una finestra realmente minimizzata continua a essere ripristinata prima di
  mostrare la conferma; restano inoltre attivi il sollevamento e l'attivazione
  della finestra.

### Lingua coerente nella conferma di log

- Il dialogo QWidget di emergenza resta disponibile quando nessun pannello TX
  QML visibile conferma di avere raccolto la richiesta. Titolo, etichette,
  stato del cluster e pulsanti **Aggiungi/Salta** usano ora lo stesso contesto
  di traduzione `TxPanel` della finestra QML a tema.
- Viene così eliminato l'apparente cambio di lingua che poteva verificarsi
  quando un QSO usava la conferma QML italiana e un contatto successivo finiva
  nel dialogo di sicurezza precedentemente scritto direttamente in inglese.
- L'etichetta `Grid` è ora traducibile anche in QML e nell'interfaccia italiana
  viene visualizzata come `Locator`.

### Selettore compatto delle frequenze operative

- Il selettore della frequenza operativa è stato spostato dalla riga della
  modalità alla riga superiore delle bande, immediatamente prima del pulsante
  `160` metri. La riga FT8/modalità risulta meno affollata e la frequenza di
  dial resta subito visibile.
- La lista delle bande inizia dopo il selettore compatto fisso e mantiene lo
  scorrimento orizzontale nelle finestre strette. È riservato lo spazio per la
  maniglia di trascinamento del pannello, che non copre più il valore.
- Il selettore continua a usare il database Working Frequencies e il normale
  percorso QSY bridge/CAT introdotti nella v1.0.599: questa versione ne cambia
  la posizione, non il comportamento di sintonia.

### Robustezza del pacchetto macOS Intel

- Il valore del numero di worker FT8 Stage 4 può ora risultare esplicitamente
  inutilizzato nelle build senza OpenMP. Questo evita il fallimento della
  configurazione macOS Intel con warnings-as-errors, senza modificare il
  comportamento del decoder o delle build parallele.

### Verifica e download

- Il target Qt nativo compila correttamente, il QML modificato supera
  `qmllint` e le modifiche sorgenti superano il controllo degli spazi.
- GitHub Actions produce l'installer Windows x64, i DMG macOS Apple Silicon per
  Tahoe e Sequoia, i DMG macOS Intel per Ventura, Sonoma e Sequoia e le
  AppImage Linux x86_64 e aarch64. I DMG e le AppImage sono accompagnati dai
  rispettivi file SHA-256.
- Gli archivi `.zip` e `.tar.gz` generati da GitHub per il tag `v1.0.600`
  costituiscono i download del codice sorgente della release.
