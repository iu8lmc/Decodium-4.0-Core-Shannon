# Decodium v3.0 SE "Raptor" - Fork 9H1SR v1.3.8

English, Italian, and Spanish documentation for this fork is included in this repository.

## English

Fork release `v1.3.8` is based on upstream `iu8lmc/Decodium-3.0-Codename-Raptor` and adds macOS-focused operational hardening.

- Upstream base: Decodium v3.0 SE "Raptor"
- Fork release: `v1.3.8`
- App bundle/executable on macOS: `ft2.app` / `ft2`
- License: GPLv3

### What this fork adds

- Stable macOS runtime migration to `ft2.app` (including subprocess path/layout fixes).
- TX/PTT/audio robustness improvements for repeated FT2 operation.
- Persistent audio/radio behavior and safer settings restore on macOS.
- Startup microphone permission preflight to avoid delayed popup during operation.
- Robust DT/NTP strategy (weak-sync + deadband + confirmations + sparse jump guard + hold mode).
- Shared-memory path on macOS migrated to `mmap` backend; `.pkg` sysctl bootstrap is no longer required in standard release artifacts.
- Startup freeze/hang mitigation by moving heavy startup file I/O off the UI thread.
- CTY/grid/satellite/comments data loaders hardened with size limits, parser guards, and fallback recovery.
- Raptor baseline maintained with fork integrations from previous 1.1.x work.
- Imported upstream Raptor features:
  - B4 strikethrough in Band Activity for worked-on-band calls.
  - Auto CQ caller FIFO queue (max 20) with auto-continue on QSO completion.
  - TX slot red bracket overlay on waterfall (FT2/FT8/FT4).
  - Auto `cty.dat` refresh at startup when missing or older than 30 days.
  - FT2 decoder updates: adaptive `syncmin` (`0.90/0.85/0.80`), extended AP types for Tx3/Tx4, relaxed deep-search thresholds, OSD depth boost near `nfqso`.
- Fork `v1.3.5` UI/runtime refinements:
  - Startup mode auto-selection now scans full frequency list (fixes FT8/FT2 mismatch on launch).
  - Responsive top controls with automatic 2-row layout on small displays.
  - Light-theme progress/seconds bar rendering fixed for clear visibility.
- Fork `v1.3.5` security/concurrency hardening:
  - TCI binary frame parser now validates header/payload lengths before access (malformed frame drop).
  - TCI pseudo-sync nested event loops removed from `mysleep1..8` (no `QEventLoop::exec()` path).
  - `foxcom_.wave` transmission path hardened with guarded snapshot reads across UI/audio threads.
  - TCI C++/Fortran audio boundary hardened with `kin` clamps and bounded writes.
  - TOTP generation aligned to NTP-corrected time source.
  - `QRegExp` migration completed in critical runtime/network paths (`mainwindow`, `wsprnet`).
- Fork `v1.3.5` network/integrity hardening:
  - Cloudlog and LotW HTTPS/TLS handling tightened (no insecure fallback path).
  - UDP control packets now filtered by trusted sender and destination id rules.
  - ADIF generation/saving hardened with input sanitization and mutex-protected append paths.
  - Settings trace output now redacts sensitive keys (password/token/api key).
- Fork `v1.3.5` NTP + CTY reliability updates:
  - NTP bootstrap now tolerates constrained networks (single-server confirm mode, retry tuning, auto fallback pin to `time.apple.com`).
  - NTP status bar keeps displaying live offset during weak-sync hold states.
  - Startup `cty.dat` download is immediate when missing and now forced over HTTPS.
- Fork `v1.3.7` operational and UI updates (from `v1.3.6`):
  - Added clickable world map contacts with visual highlight and DX call/grid transfer to main TX controls.
  - Added configurable map click behavior: default single-click fills/calls, optional single-click starts TX.
  - Added `View -> World Map` persistent visibility toggle.
  - Added new `View -> Ionospheric Forecast` window (HamQSL solar/ionospheric feed, refresh timer, saved geometry).
  - Added new `View -> DX Cluster` window (band-aware, mode filtering, periodic refresh, saved geometry).
  - Improved day/night overlay rendering and map path lifecycle (end-of-QSO path cleanup to reduce stale lines).
  - Improved top controls layout management for compact/two-row mode on small displays.
  - Fixed FT decode sequence-start timestamp initialization to avoid startup decode misalignment.
  - Fixed cross-thread Qt signal delivery for `ModulatorState` with explicit metatype registration.
  - Set default Qt style to `Fusion` for more consistent rendering across macOS variants.
  - Removed hardcoded legacy revision suffix source so PSKReporter `Using:` no longer appends `mod by IU8LMC...`.
- Fork `v1.3.8` CAT/network/map updates (from `v1.3.7`):
  - CAT/remote Configure hardening: generic Configure packets no longer force FT2 mode.
  - UDP control hardening: control commands now require direct target id matching.
  - Added optional greyline toggle in `Settings -> General` for world map day/night overlay.
  - Added distance badge on active world-map path (km/mi based on current unit setting).
  - Refined compact top-controls layout with DX-ped button alignment improvements.

### Build (macOS)

```bash
cmake -B build-raptor-port -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DWSJT_GENERATE_DOCS=OFF \
  -DWSJT_SKIP_MANPAGES=ON \
  -DWSJT_BUILD_UTILS=OFF

cmake --build build-raptor-port --target wsjtx -j8
```

Run:

```bash
./build-raptor-port/ft2.app/Contents/MacOS/ft2
```

### macOS quarantine

If macOS blocks app startup, run:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

### Documentation

- Release notes (EN/IT/ES): [RELEASE_NOTES_v1.3.8.md](RELEASE_NOTES_v1.3.8.md)
- Changelog (EN/IT/ES for latest release): [CHANGELOG.md](CHANGELOG.md)
- Security and bug analysis report: [doc/SECURITY_BUG_ANALYSIS_REPORT.md](doc/SECURITY_BUG_ANALYSIS_REPORT.md)
- macOS porting details (EN/IT): [doc/MACOS_PORTING_v1.2.0.md](doc/MACOS_PORTING_v1.2.0.md)
- DT/NTP architecture (EN/IT): [doc/DT_NTP_ROBUST_SYNC_v1.2.0.md](doc/DT_NTP_ROBUST_SYNC_v1.2.0.md)
- GitHub release body template (EN/IT/ES): [doc/GITHUB_RELEASE_BODY_v1.3.8.md](doc/GITHUB_RELEASE_BODY_v1.3.8.md)

### CI release targets

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey (experimental / best effort)
- Linux x86_64 AppImage

### Hamlib version in release builds

- macOS CI now runs `brew update` + `brew upgrade hamlib` before packaging.
- Linux CI now builds Hamlib from the latest official GitHub release and installs it under `/usr/local` before compiling `ft2`.
- Each workflow prints the effective Hamlib version in logs (`rigctl --version`, `pkg-config --modversion hamlib`) so release provenance is explicit.

## Espanol

Tambien esta disponible un resumen en espanol:

- [README.es.md](README.es.md)
- [doc/README.es.md](doc/README.es.md)

### Linux minimum requirements

- Architecture: `x86_64` (64-bit)
- CPU: dual-core 2.0 GHz or better
- RAM: 4 GB minimum (8 GB recommended)
- Storage: at least 500 MB free for AppImage + logs/settings
- OS/runtime:
  - Linux with `glibc >= 2.35` (Ubuntu 22.04 class or newer)
  - `FUSE 2` support (`libfuse2`) or an AppImage-compatible launcher/runtime
  - ALSA/PulseAudio/PipeWire audio stack available
- Radio integration: CAT/audio interface hardware as required by station setup

### Linux AppImage launch recommendation

To avoid issues caused by AppImage read-only filesystem mode, run:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Italiano

La release fork `v1.3.8` e' basata su `iu8lmc/Decodium-3.0-Codename-Raptor` e aggiunge hardening operativo specifico per macOS.

- Base upstream: Decodium v3.0 SE "Raptor"
- Versione fork: `v1.3.8`
- Bundle/eseguibile su macOS: `ft2.app` / `ft2`
- Licenza: GPLv3

### Cosa aggiunge questo fork

- Migrazione runtime stabile su macOS verso `ft2.app` (con fix su path/layout dei sottoprocessi).
- Miglioramenti di robustezza TX/PTT/audio in uso FT2 ripetuto.
- Persistenza piu affidabile di audio/radio e ripristino impostazioni su macOS.
- Preflight permesso microfono all'avvio per evitare popup tardivo durante l'uso.
- Strategia DT/NTP robusta (weak-sync + deadband + conferme + filtro sparse jump + hold mode).
- Path shared memory su macOS migrato a backend `mmap`; bootstrap sysctl via `.pkg` non piu' richiesto negli artifact release standard.
- Mitigazione freeze/hang in avvio spostando I/O pesante di startup fuori dal thread UI.
- Loader dati CTY/grid/satellite/comments irrobustiti con limiti di dimensione, guardie parser e recovery fallback.
- Baseline Raptor mantenuta con integrazioni fork ereditate dal lavoro 1.1.x.
- Funzionalita' Raptor upstream integrate:
  - Testo barrato B4 in Band Activity per stazioni gia' lavorate in banda.
  - Coda FIFO Auto CQ (max 20) con prosecuzione automatica a fine QSO.
  - Overlay waterfall con parentesi rosse sullo slot TX (FT2/FT8/FT4).
  - Aggiornamento automatico `cty.dat` all'avvio se mancante o piu vecchio di 30 giorni.
  - Aggiornamenti decoder FT2: `syncmin` adattivo (`0.90/0.85/0.80`), AP esteso su Tx3/Tx4, soglie deep-search rilassate, OSD potenziato vicino a `nfqso`.
- Rifiniture fork `v1.3.5` su UI/runtime:
  - Auto-selezione modalita' all'avvio basata su lista frequenze completa (fix mismatch FT8/FT2).
  - Controlli top responsivi con passaggio automatico a 2 righe su schermi piccoli.
  - Rendering barra progressi/secondi corretto nel tema chiaro.
- Hardening sicurezza/concorrenza fork `v1.3.5`:
  - Parser frame binari TCI con validazione completa header/payload prima dell'accesso (drop frame malformati).
  - Rimossi loop annidati pseudo-sync in `mysleep1..8` (eliminato il percorso `QEventLoop::exec()`).
  - Percorso TX `foxcom_.wave` irrobustito con snapshot protetti tra thread UI/audio.
  - Boundary audio TCI C++/Fortran irrobustito con clamp `kin` e scritture limitate.
  - Generazione TOTP allineata a sorgente tempo corretta NTP.
  - Migrazione `QRegExp` completata nei percorsi runtime/network critici (`mainwindow`, `wsprnet`).
- Hardening rete/integrita' fork `v1.3.5`:
  - Gestione HTTPS/TLS di Cloudlog e LotW irrigidita (niente fallback insicuri).
  - Pacchetti controllo UDP filtrati con regole trusted sender + destination id.
  - Generazione/salvataggio ADIF irrobustiti con sanitizzazione input e append protetto da mutex.
  - Output trace impostazioni con redazione chiavi sensibili (password/token/api key).
- Aggiornamenti affidabilita' NTP + CTY fork `v1.3.5`:
  - Bootstrap NTP piu' robusto su reti vincolate (conferma single-server, tuning retry, auto fallback pin su `time.apple.com`).
  - Status bar NTP mantiene la visualizzazione offset live anche durante weak-sync hold.
  - Download startup `cty.dat` immediato se mancante e ora forzato su HTTPS.
- Aggiornamenti operativi/UI fork `v1.3.7` (da `v1.3.6`):
  - Aggiunti click mappa sui contatti con highlight visivo e trasferimento nominativo/griglia nei controlli TX.
  - Aggiunta opzione configurabile comportamento click mappa: default compila/chiama, opzionale single-click avvia TX.
  - Aggiunto toggle persistente `View -> World Map`.
  - Aggiunta nuova finestra `View -> Ionospheric Forecast` (feed HamQSL solar/ionosfera, refresh periodico, geometria salvata).
  - Aggiunta nuova finestra `View -> DX Cluster` (allineata banda corrente, filtro modo, refresh periodico, geometria salvata).
  - Migliorato rendering overlay giorno/notte e ciclo vita path mappa (pulizia linee stale su fine QSO).
  - Migliorata gestione layout controlli top in modalita' compatta/2 righe su schermi piccoli.
  - Corretto timestamp sequence-start decode FT per evitare disallineamenti all'avvio.
  - Corretto delivery segnali cross-thread `ModulatorState` con registrazione metatype esplicita.
  - Impostato stile Qt predefinito `Fusion` per rendering piu' coerente tra varianti macOS.
  - Rimossa sorgente hardcoded del suffisso revision legacy: PSKReporter `Using:` non appende piu' `mod by IU8LMC...`.
- Aggiornamenti CAT/rete/mappa fork `v1.3.8` (da `v1.3.7`):
  - Hardening CAT/Configure remoto: pacchetti Configure generici non forzano piu' FT2.
  - Hardening UDP controllo: i comandi di controllo richiedono target id diretto.
  - Aggiunto toggle opzionale greyline in `Settings -> General`.
  - Aggiunto badge distanza sul path mappa attivo (km/mi in base all'unita' configurata).
  - Rifinito il layout controlli top compatti con allineamento pulsante DX-ped.

### Compilazione (macOS)

```bash
cmake -B build-raptor-port -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DWSJT_GENERATE_DOCS=OFF \
  -DWSJT_SKIP_MANPAGES=ON \
  -DWSJT_BUILD_UTILS=OFF

cmake --build build-raptor-port --target wsjtx -j8
```

Esecuzione:

```bash
./build-raptor-port/ft2.app/Contents/MacOS/ft2
```

### Quarantena macOS

Se macOS blocca l'avvio dell'app, eseguire:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

### Documentazione

- Note di rilascio (EN/IT/ES): [RELEASE_NOTES_v1.3.8.md](RELEASE_NOTES_v1.3.8.md)
- Changelog (EN/IT/ES per release attuale): [CHANGELOG.md](CHANGELOG.md)
- Report analisi sicurezza e bug: [doc/SECURITY_BUG_ANALYSIS_REPORT.md](doc/SECURITY_BUG_ANALYSIS_REPORT.md)
- Porting macOS (EN/IT): [doc/MACOS_PORTING_v1.2.0.md](doc/MACOS_PORTING_v1.2.0.md)
- Architettura DT/NTP (EN/IT): [doc/DT_NTP_ROBUST_SYNC_v1.2.0.md](doc/DT_NTP_ROBUST_SYNC_v1.2.0.md)
- Template release GitHub (EN/IT/ES): [doc/GITHUB_RELEASE_BODY_v1.3.8.md](doc/GITHUB_RELEASE_BODY_v1.3.8.md)

### Target CI release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey (sperimentale / best effort)
- Linux x86_64 AppImage

### Versione Hamlib nelle build release

- La CI macOS esegue ora `brew update` + `brew upgrade hamlib` prima del packaging.
- La CI Linux compila Hamlib dall'ultima release ufficiale GitHub e la installa in `/usr/local` prima della compilazione di `ft2`.
- Ogni workflow stampa nei log la versione Hamlib effettiva (`rigctl --version`, `pkg-config --modversion hamlib`) per rendere tracciabile la provenienza della release.

### Requisiti minimi Linux

- Architettura: `x86_64` (64 bit)
- CPU: dual-core 2.0 GHz o superiore
- RAM: minimo 4 GB (consigliati 8 GB)
- Spazio disco: almeno 500 MB liberi per AppImage + log/impostazioni
- OS/runtime:
  - Linux con `glibc >= 2.35` (classe Ubuntu 22.04 o successiva)
  - supporto `FUSE 2` (`libfuse2`) o launcher/runtime compatibile AppImage
  - stack audio ALSA/PulseAudio/PipeWire disponibile
- Integrazione radio: hardware CAT/interfaccia audio secondo setup stazione

### Avvio consigliato AppImage su Linux

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, eseguire:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Credits

- WSJT-X by Joe Taylor K1JT and the WSJT Development Group
- Decodium v3.0 SE Raptor by IU8LMC
- Fork integration and macOS porting by Salvatore Raccampo 9H1SR
