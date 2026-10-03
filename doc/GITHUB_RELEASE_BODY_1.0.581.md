# Decodium 4 FT2 v1.0.581

This fork release consolidates the changes delivered from v1.0.576 through
v1.0.580 and adds the local fixes and packaging improvements prepared for
v1.0.581.

## English (British)

### DecoPort transmission and remote-radio control

The DecoPort path has evolved from remote monitoring into a complete remote
radio control path:

- v1.0.577 sends the modem waveform to the remote radio in timestamped 40 ms
  frames, buffered ahead of playback to protect FT8 timing from network jitter.
  The remote gateway controls the USB codec and PTT, with an automatic PTT
  deadline so a lost client or audio stream cannot leave the transmitter keyed.
- `tools/decoport_probe.py --serve` measures received transmit frames, audio
  duration, peak level, late frames and PTT duration without putting a signal on
  air.
- v1.0.578 avoids constructing local PCM when a remote radio is in use, so a
  machine without a suitable local output device can still reach the DecoPort
  transmit path. Transmit diagnostics now record whether the remote radio and
  link were available.
- v1.0.579 promotes DecoPort to a proper CAT backend. Frequency, mode and PTT
  commands use the network radio, the configured local CAT backend is restored
  afterwards, and split operation is deliberately refused because DecoPort
  carries one frequency.
- The gateway can resolve a USB radio through Hamlib's runtime catalogue and
  open it on its own worker thread, while refusing to compete with the
  application's existing CAT port.

### Data-mode correctness and clearer transmit diagnostics

- v1.0.580 translates the configured local radio mode into the DecoPort data
  vocabulary: lower-sideband modes request `DIGL`, while other modes request
  `DIGU`. This prevents a remote radio from being left in plain USB, where the
  modem audio would be sent to the microphone path instead of the data codec.
  The requested and configured modes are recorded in the log, and redundant
  mode commands are avoided.
- A transmit attempt with no station callsign now explains that the callsign and
  locator must be filled in under Settings > Station, rather than misleadingly
  reporting only that no TX message is selected.
- Hamlib diagnostic tracing is reduced at the driver boundary to avoid the
  synchronous stderr traffic that was consuming audio-processing time on an
  active gateway.

### v1.0.581 local fixes and release packaging

- Fixed the Settings > Reporting responsive layout for Linux/KDE/Wayland
  geometries. The page now chooses four columns only when the actual page
  viewport, after the settings sidebar, can contain the complete grid. Narrower
  viewports fall back to two columns, keeping the QRZ, LoTW and Logging
  controls visible instead of placing the right-hand controls outside the
  viewport. This addresses the missing controls reported in issue #66.
- macOS release packaging now creates and publishes only DMG files and their
  SHA-256 checksums. The additional application ZIP packages are no longer
  created or uploaded.
- Removed three unused lambda captures in the DecoPort bridge so AppleClang's
  `-Werror,-Wunused-lambda-capture` cannot block the macOS release builds.
- The release source tree and version metadata are updated to v1.0.581.

### Platform packages

The release workflows publish:

- Windows x64 installer (`.exe`);
- macOS Apple Silicon DMG packages for the supported runner targets;
- macOS Intel DMG packages for the supported runner targets;
- Linux x86_64 and aarch64 AppImage packages;
- GitHub's generated source-code downloads for the tagged codebase.

## Italiano

Questa release del fork raccoglie le modifiche dalla v1.0.576 alla v1.0.580 e
aggiunge i fix locali e i miglioramenti al packaging preparati per la v1.0.581.

### Trasmissione DecoPort e controllo della radio remota

Il percorso DecoPort è passato dal semplice ascolto remoto al controllo completo
della radio remota:

- La v1.0.577 invia la forma d'onda del modem alla radio remota in frame da
  40 ms con timestamp, spediti in anticipo e bufferizzati per proteggere il
  sincronismo FT8 dal jitter di rete. Il gateway remoto gestisce codec USB e
  PTT, con una scadenza automatica che impedisce di lasciare il trasmettitore
  inserito se client o audio si interrompono.
- `tools/decoport_probe.py --serve` misura frame ricevuti, durata dell'audio,
  livello di picco, frame arrivati in ritardo e durata del PTT senza trasmettere
  alcun segnale in aria.
- La v1.0.578 evita di costruire il PCM locale quando è in uso una radio remota:
  anche un computer senza un dispositivo audio di uscita adatto può quindi
  raggiungere il percorso di trasmissione DecoPort. I diagnostici registrano
  anche lo stato della radio remota e del collegamento.
- La v1.0.579 trasforma DecoPort in un vero backend CAT. Frequenza, modo e PTT
  usano la radio di rete, il backend CAT locale configurato viene ripristinato
  dopo il rilascio della radio remota e lo split viene rifiutato volutamente,
  perché DecoPort trasporta una sola frequenza.
- Il gateway risolve la radio USB tramite il catalogo Hamlib disponibile a
  runtime e la apre su un proprio thread, evitando di competere con la porta
  CAT già utilizzata dall'applicazione.

### Correttezza del modo dati e diagnostica TX più chiara

- La v1.0.580 traduce il modo configurato per la radio locale nel vocabolario
  dati di DecoPort: i modi a banda laterale inferiore richiedono `DIGL`, tutti
  gli altri `DIGU`. Si evita così di lasciare la radio remota in USB normale,
  dove l'audio del modem finirebbe nel percorso del microfono invece che nel
  codec dati. Il log registra modo richiesto e modo configurato, evitando anche
  comandi ridondanti.
- Un tentativo di trasmissione senza nominativo ora spiega che bisogna compilare
  nominativo e locator in Impostazioni > Stazione, invece di mostrare soltanto
  il messaggio fuorviante che non è stato selezionato alcun messaggio TX.
- Il tracing diagnostico di Hamlib viene ridotto all'ingresso del driver, così
  il traffico sincrono su stderr non sottrae tempo all'elaborazione audio del
  gateway attivo.

### Fix locali e packaging della v1.0.581

- Corretto il layout responsive di Impostazioni > Reporting per le geometrie
  Linux/KDE/Wayland. La pagina usa quattro colonne solo quando la viewport reale,
  dopo la sidebar delle impostazioni, contiene l'intera griglia. Nelle viewport
  più strette passa a due colonne e mantiene visibili i controlli QRZ, LoTW e
  Logging invece di collocare i controlli di destra fuori dall'area visibile.
  Questo risolve i controlli mancanti segnalati nell'issue #66.
- Il packaging macOS ora crea e pubblica solo i pacchetti DMG e i relativi
  checksum SHA-256. I pacchetti applicazione ZIP aggiuntivi non vengono più
  creati né caricati.
- Rimossi tre capture lambda inutilizzati nel bridge DecoPort, così il controllo
  AppleClang `-Werror,-Wunused-lambda-capture` non blocca più le build macOS di
  release.
- Il codice sorgente e i metadati di versione sono aggiornati alla v1.0.581.

### Pacchetti della piattaforma

I workflow di release pubblicano:

- installer Windows x64 (`.exe`);
- pacchetti DMG macOS Apple Silicon per i runner supportati;
- pacchetti DMG macOS Intel per i runner supportati;
- pacchetti AppImage Linux x86_64 e aarch64;
- i download del codice sorgente generati automaticamente da GitHub per il
  codebase taggato.
