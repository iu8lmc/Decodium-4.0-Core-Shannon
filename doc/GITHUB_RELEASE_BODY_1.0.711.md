# Decodium 4 v1.0.711

## English (British)

This release contains the FT2/FT4 receiver improvements from v1.0.710 and the
follow-up fixes for CW/SSB window lifetime, graphics teardown and mode changes.
It is based on v1.0.709 and includes all changes through v1.0.710.

### FT2 and FT4 sync-word candidate search

- FT2 and FT4 now search for the protocol's known synchronisation symbols in
  addition to the strongest average-spectrum peaks.
- Weak signals close to strong signals are therefore offered to the decoder even
  when a simple peak search would hide them.
- FT2 candidate generation is separated from the Stage 7 decoder and the FT4
  receiver uses the same sync-word-led search strategy.
- The receiver scoreboard and the `tools/autoimprove` laboratory tools document
  the synthetic-signal, real-recording and noise checks used to compare the
  candidate strategies.
- The FT2 path reduces unnecessary candidate work while preserving the existing
  decode and SNR reporting flow.

### CW lifecycle and mode hand-off

- Leaving CW now performs a complete suspension: transmission is stopped, CAT
  PTT and remote keying are released, retry timers are cancelled, and local
  serial/WinKeyer resources are closed.
- Returning to CW starts the controller lazily again, including when the CW
  window remained loaded while another application mode was selected.
- This prevents a closed CW window from leaving keyer workers, ports or decoder
  state alive until application destruction.
- A regression test covers `start -> suspend -> restart -> transmit`.

### CW/SSB window and graphics teardown

- Closing CW or SSB now releases the associated Qt Quick graphics resources
  before unloading the asynchronous Loader item.
- Application shutdown no longer assumes that 75 ms is enough for every
  compositor or OpenGL/X11 render loop.
- Shutdown drains the secondary native-window Loaders over multiple event-loop
  turns and exits once they are gone, with a bounded one-second safety timeout.
- The main window also requests `releaseResources()` before the final quit.
- These changes target the freeze/black-window sequence reported after repeated
  CW-to-digital/SSB transitions, while preserving the normal lazy CW startup
  behaviour.

### Graphics and CAT reliability carried into this release

- Transactional graphics pipeline caches remain separated by application role,
  Qt/RHI identity and Windows D3D11 hardware/WARP mode.
- Interrupted graphics sessions discard incomplete cache data on the next launch.
- Windows runtime graphics-driver recovery and safe-renderer fallback remain
  protected by their existing diagnostics and tests.
- The previously delivered CAT, PTT, CW stop, SSB focus, Liquid Glass and
  dashboard-control safeguards are included in the release tree.

### Verification

- `decodium_qml` rebuilt successfully on the local macOS ARM64 development
  environment.
- Focused regression suite: 6/6 tests passed, including CW controller restart,
  graphics-cache recovery, CW/SSB lifecycle, CW window rendering, SSB focus and
  TX dispatch guards.
- QML syntax checking completed; the repository still reports its existing
  non-fatal layout and unqualified-access warnings.

### Release contents

- Source code for Decodium 4 v1.0.711.
- Windows x64 executable/installer built by the GitHub Windows runner.
- macOS Intel DMG builds for the supported compatibility targets.
- macOS Apple Silicon DMG builds.
- Linux x86_64 AppImage.
- Linux aarch64 AppImage.
- SHA-256 checksum files for the generated binary packages.

## Italiano

Questa release contiene i miglioramenti del ricevitore FT2/FT4 della v1.0.710
e i fix successivi sul ciclo di vita delle finestre CW/SSB, sul rilascio delle
risorse grafiche e sui cambi di modo. È basata sulla v1.0.709 e comprende tutte
le modifiche fino alla v1.0.710.

### Ricerca dei candidati tramite sincronismi FT2 e FT4

- FT2 e FT4 ora cercano i simboli di sincronismo noti del protocollo oltre ai
  picchi più forti dello spettro medio.
- I segnali deboli vicini a quelli forti vengono quindi proposti al decoder anche
  quando una semplice ricerca dei picchi li nasconderebbe.
- La generazione dei candidati FT2 è separata dal decoder Stage 7 e il ricevitore
  FT4 usa la stessa strategia guidata dai sincronismi.
- Lo scoreboard del ricevitore e gli strumenti di laboratorio in
  `tools/autoimprove` documentano i controlli con segnali sintetici,
  registrazioni reali e rumore usati per confrontare le strategie.
- Il percorso FT2 riduce il lavoro inutile sui candidati mantenendo il normale
  flusso di decodifica e di calcolo dell'SNR.

### Ciclo di vita CW e passaggio tra i modi

- Uscendo da CW viene eseguita una sospensione completa: la trasmissione viene
  fermata, PTT CAT e keying remoto vengono rilasciati, i timer di retry vengono
  annullati e le risorse seriali/WinKeyer vengono chiuse.
- Tornando in CW il controller viene riavviato in modo lazy, anche quando la
  finestra CW è rimasta caricata mentre era stato scelto un altro modo.
- In questo modo una finestra CW chiusa non lascia attivi worker, porte o stato
  del decoder fino alla distruzione del processo.
- È stato aggiunto un test di regressione per
  `start -> suspend -> restart -> transmit`.

### Chiusura delle finestre CW/SSB e risorse grafiche

- Alla chiusura di CW o SSB vengono ora rilasciate le risorse grafiche Qt Quick
  prima di scaricare il Loader asincrono.
- La chiusura dell'applicazione non presume più che 75 ms siano sufficienti per
  ogni compositor o render loop OpenGL/X11.
- Lo shutdown svuota i Loader delle finestre native secondarie su più turni del
  loop eventi ed esce quando sono realmente scomparsi, con timeout massimo di un
  secondo.
- Anche la finestra principale richiede `releaseResources()` prima dell'uscita.
- Le modifiche riguardano la sequenza di freeze/finestra nera segnalata dopo
  ripetuti passaggi CW-digitale/SSB, mantenendo l'avvio lazy del modulo CW.

### Affidabilità grafica e CAT inclusa nella release

- Le cache grafiche transazionali restano separate per ruolo della finestra,
  identità Qt/RHI e modalità Windows D3D11 hardware/WARP.
- Le sessioni grafiche interrotte eliminano al successivo avvio le cache
  incomplete.
- Restano attivi il recupero dai crash del driver grafico Windows e il fallback
  al renderer sicuro, con le relative diagnostiche e verifiche.
- Sono inclusi anche i fix già consegnati per CAT, PTT, stop CW, focus SSB,
  Liquid Glass e comandi radio dalla dashboard.

### Verifica

- `decodium_qml` ricompilato con successo nell'ambiente locale macOS ARM64.
- Suite di regressione mirata: 6/6 test superati, inclusi restart del controller
  CW, recupero della cache grafica, lifecycle CW/SSB, rendering della finestra
  CW, focus SSB e guardie del dispatch TX.
- Controllo sintattico QML completato; il repository continua a produrre gli
  avvisi non bloccanti già esistenti su layout e accessi non qualificati.

### Contenuto della release

- Codice sorgente di Decodium 4 v1.0.711.
- Eseguibile/installer Windows x64 generato dal runner GitHub Windows.
- DMG macOS Intel per i target di compatibilità supportati.
- DMG macOS Apple Silicon.
- AppImage Linux x86_64.
- AppImage Linux aarch64.
- File checksum SHA-256 per i pacchetti binari generati.
