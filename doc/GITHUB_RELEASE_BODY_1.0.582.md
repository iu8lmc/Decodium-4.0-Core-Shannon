# Decodium 4 FT2 v1.0.582

This fork release follows v1.0.581 and delivers the definitive correction for
the Reporting settings layout reported in issue #66.

## English (British)

### Definitive KDE Reporting layout correction

- Settings > Reporting now always uses a two-column label/control layout.
  It no longer selects a four-column grid from the calculated viewport width.
- This removes the failure mode seen with fixed-size Settings windows on KDE
  Plasma/Wayland when running through XCB: the right-hand controls could be
  laid out beyond the clipped page even on a wide ultrawide display.
- The QRZ Logbook `Replace duplicates` option, LoTW `Password` and `Days
  Upload` fields, and the Logging `Auto Log`, `4-digit Grids` and `Spec Op
  Cmts` options remain in the normal vertical page flow at every window size.
- Existing settings and stored credentials are unchanged. No configuration
  reset or migration is required; the page simply uses additional vertical
  scrolling where necessary.

### Packaging and source availability

- The tagged source code is available through GitHub's generated source-code
  downloads for v1.0.582.
- Release workflows publish the Windows x64 installer, Linux x86_64 and
  aarch64 AppImages with SHA-256 checksums, and macOS DMGs with SHA-256
  checksums for the supported Apple Silicon and Intel runner targets.
- macOS application ZIP files remain intentionally excluded; only DMG packages
  and their checksums are published.

## Italiano

### Correzione definitiva del layout Reporting su KDE

- Impostazioni > Reporting usa ora sempre un layout a due colonne,
  etichetta/controllo. Non sceglie più una griglia a quattro colonne in base
  alla larghezza calcolata della viewport.
- Viene così eliminato il caso osservato con finestre Impostazioni a dimensione
  fissa in KDE Plasma/Wayland eseguito tramite XCB: i controlli di destra
  potevano essere collocati oltre l'area visibile della pagina anche su un
  monitor ultrawide.
- L'opzione QRZ Logbook `Replace duplicates`, i campi LoTW `Password` e `Days
  Upload`, e le opzioni di Logging `Auto Log`, `4-digit Grids` e `Spec Op
  Cmts` restano nel normale flusso verticale della pagina a ogni dimensione
  della finestra.
- Le impostazioni e le credenziali già memorizzate non cambiano. Non è
  necessario alcun reset o migrazione: quando serve, la pagina usa soltanto
  più scorrimento verticale.

### Packaging e disponibilità del sorgente

- Il codice sorgente taggato è disponibile tramite i download del codice
  generati da GitHub per la v1.0.582.
- I workflow di release pubblicano l'installer Windows x64, le AppImage Linux
  x86_64 e aarch64 con checksum SHA-256, e i DMG macOS con checksum SHA-256
  per i runner Apple Silicon e Intel supportati.
- Gli ZIP dell'applicazione macOS restano esclusi intenzionalmente: vengono
  pubblicati soltanto i pacchetti DMG e i rispettivi checksum.
