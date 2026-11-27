# Decodium 4 FT2 1.0.369

Release cumulativa dalla 1.0.361 alla 1.0.369.

## Novita principali

- Pannelli principali interscambiabili: introdotta la struttura a slot per i tre pannelli classici, con re-parent controllato, resize non sticky e correzione degli handle duplicati.
- Pannelli TX e Waterfall estesi alla nuova logica di riordino, con fix della banda quando si passa da FT2 a FT8.
- MAM multi-stream nativo introdotto in modo progressivo:
  - infrastruttura del modulatore multi-stream FT8;
  - sequencer per QSO paralleli, lasciato disattivato di default;
  - wiring UI con toggle, spinbox e vista slot;
  - click stazione per aggiungere correttamente alla lista hunter TX1;
  - estensione a FT8, FT4 e FT2 con marker waterfall.
- Fix FT2 phase-lock breaker: aggiunto sfasamento automatico anti same-slot lock per ridurre blocchi o collisioni quando due stazioni restano agganciate allo stesso slot.
- Fix PTT ICOM CI-V:
  - correzione per casi di PTT bloccato in TX su IC-7851;
  - PTT-off best effort con tre retry no-throw;
  - consolidamento per evitare desync dello stato TX;
  - aggiunto toggle FT2 conservative TX window.
- Miglioramenti MAM window: evitata la chiusura accidentale e aggiunta X di chiusura coerente con il tema.
- Waterfall: ripristinati i colori highlight dei call e aggiunta maniglia drag al pulsante "Mostra controlli".
- World Clock: dopo i tentativi snap-header e pop-out, la soluzione finale e un overlay floating trascinabile ovunque con posizione X/Y libera.
- Versione applicazione, installer e metadati allineati alla 1.0.369.

## Dettaglio tecnico per versione

- `v1.0.362`: struttura iniziale dei pannelli interscambiabili a tre colonne classiche.
- `v1.0.363`: pannelli interscambiabili stadio 2 e 3, TX + Waterfall, fix band FT2 -> FT8 e integrazione del fix grafico `1.0.361`.
- `v1.0.364`: FT2 phase-lock breaker con sfasamento automatico anti same-slot lock.
- `v1.0.365`: primo fix PTT stuck in TX per ICOM CI-V, in particolare IC-7851.
- `v1.0.366`: PTT-off best effort con tre retry no-throw.
- `v1.0.367`: consolidamento PTT ICOM, MAM multi-stream, fix UI e marker waterfall.
- `v1.0.368`: fix PTT 367, protezione da desync stato TX e toggle FT2 conservative TX window.
- `v1.0.369`: World Clock convertito in overlay floating mobile, trascinabile ovunque.

## File principali modificati

- `DecodiumBridge.cpp`
- `DecodiumBridge.h`
- `Transceiver/HamlibTransceiver.cpp`
- `main_qml.cpp`
- `qml/decodium/Main.qml`
- `qml/decodium/components/MamPanel.qml`
- `qml/decodium/components/MamWindow.qml`
- `qml/decodium/components/SettingsDialog.qml`
- `qml/decodium/components/Waterfall.qml`
- `fork_release_version.txt`
- `installer/decodium_setup.iss`
- `installer/decodium_setup_nsis.nsi`

## Asset previsti nella release

- Source code GitHub per il tag `1.0.369`.
- Installer Windows x64 `.exe`.
- Pacchetti macOS Apple Silicon tramite runner GitHub.
- Pacchetti macOS Intel tramite runner GitHub.
- AppImage Linux x86_64.
- AppImage Linux aarch64.
