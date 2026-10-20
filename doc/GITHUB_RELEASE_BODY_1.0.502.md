# Decodium 4.0 v1.0.502

Version 1.0.502 reorganizes the native codebase into clear subsystem
directories, removes generated distribution files from source control, improves
LoTW user-data refresh behavior and hardens Windows packaging against missing
runtime data.

## Changes from 1.0.501 to 1.0.502

### Native source layout

- Moved application bootstrap and configuration code to `src/app`.
- Moved QML bridge, diagnostics, logging and legacy compatibility code to
  `src/bridge`.
- Grouped common runtime helpers under `src/core`.
- Grouped decode models, radio backends, security, online services and UI
  renderers under `src/models`, `src/radio`, `src/security`, `src/services`
  and `src/ui`.
- Updated CMake targets, include paths, tests and web-server integration to use
  the new source layout.
- Added `doc/REPOSITORY_LAYOUT.md` as the maintained repository map.

### Repository and packaging cleanup

- Moved Docker definitions to `packaging/docker` and Windows installer sources
  to `packaging/windows`.
- Moved maintained native build helpers to `scripts/build`.
- Archived useful legacy development notes and analysis tools under `doc` and
  `tools`.
- Removed the checked-in generated Windows runtime tree. Release workflows now
  reconstruct a clean distribution from the selected Qt toolchain.
- Removed unused vendored theme sources, obsolete signing material and retired
  local build helpers.
- Hardened `.gitignore` for build outputs, release artifacts, local diagnostics
  and sensitive signing files without hiding maintained source directories.

### LoTW user activity

- Added automatic startup loading and refresh of the LoTW user-activity cache.
- Introduced a shared, profile-independent cache and migration from older
  profile-local cache locations.
- Moved CSV loading and parsing off the UI thread.
- Added a seven-day freshness policy, atomic cache writes and a 30-second
  network timeout.
- Preserved the most recent valid cache when the network is unavailable or a
  refresh fails.

### Windows runtime data

- Made `cty.dat`, `sat.dat` and `ALLCALL7.TXT` mandatory inputs to the Windows
  package.
- Added workflow validation that stops the release build when required runtime
  data is missing, empty or copied incorrectly.
- Updated the Inno Setup source paths for the reorganized packaging tree.
- Kept Qt 6.11.0 as the enforced Windows release toolchain.

### CI integration

- Updated Linux ARM packaging to use the maintained script under
  `scripts/build`.
- Updated Docker and installer paths used by release automation.
- Kept generated installers, AppImages, DMGs and runtime bundles out of the
  source tree; GitHub Actions remains the authoritative packaging path.

## Validation

- Local macOS build:
  `cmake --build "/Users/salvo/Desktop/Decodium4-build" --parallel 4 --target decodium_qml`
- CTest run: 15 of 17 entries passed. The unchanged local FST4 native-emission
  fixture and the single-trial FT8 synthetic decode at -27 dB did not pass on
  this macOS toolchain; neither test nor its DSP implementation changed in this
  release.
- `git diff --cached --check`.
- GitHub Actions release builds for Windows x64, macOS Apple Silicon, macOS
  Intel, Linux x86_64 and Linux aarch64.

---

## Italiano

La versione 1.0.502 riorganizza il codice nativo in cartelle dedicate ai
diversi sottosistemi, elimina dal controllo versione i pacchetti generati,
migliora l'aggiornamento degli utenti LoTW e rende il packaging Windows più
rigoroso rispetto ai dati runtime obbligatori.

### Organizzazione dei sorgenti

- Il bootstrap e la configurazione dell'applicazione sono ora in `src/app`.
- Bridge QML, diagnostica, logging e compatibilità legacy sono in `src/bridge`.
- Helper comuni e componenti runtime sono raggruppati in `src/core`.
- Modelli, backend radio, sicurezza, servizi online e renderer UI sono separati
  in `src/models`, `src/radio`, `src/security`, `src/services` e `src/ui`.
- CMake, include, test e server web sono stati aggiornati per il nuovo layout.
- Aggiunto `doc/REPOSITORY_LAYOUT.md` come mappa mantenuta del repository.

### Pulizia del repository e packaging

- Spostate le definizioni Docker in `packaging/docker` e le sorgenti
  dell'installer Windows in `packaging/windows`.
- Spostati gli script di build nativi mantenuti in `scripts/build`.
- Archiviati sotto `doc` e `tools` gli appunti e gli strumenti legacy ancora
  utili.
- Rimossa dal repository la distribuzione Windows generata: i workflow
  ricostruiscono ora un pacchetto pulito usando il toolchain Qt selezionato.
- Rimossi tema vendorizzato non utilizzato, materiale di firma obsoleto e
  vecchi helper locali.
- Rafforzato `.gitignore` senza escludere le nuove cartelle dei sorgenti.

### Attività utenti LoTW

- Caricamento e aggiornamento automatico della cache LoTW all'avvio.
- Cache condivisa tra i profili e migrazione delle precedenti cache locali.
- Lettura e parsing CSV spostati fuori dal thread UI.
- Validità della cache impostata a sette giorni, scrittura atomica e timeout di
  rete di 30 secondi.
- In assenza di rete o in caso di errore resta disponibile l'ultima cache
  valida.

### Dati runtime Windows

- `cty.dat`, `sat.dat` e `ALLCALL7.TXT` sono ora obbligatori nel pacchetto
  Windows.
- Il workflow interrompe la release se un file richiesto manca, è vuoto o non
  viene copiato correttamente.
- Aggiornati i percorsi Inno Setup in base alla nuova struttura.
- Qt 6.11.0 resta il toolchain imposto per la release Windows.

### Integrazione CI

- Il packaging Linux ARM usa ora lo script mantenuto in `scripts/build`.
- Aggiornati i percorsi Docker e installer usati dall'automazione.
- Installer, AppImage, DMG e runtime generati restano fuori dal repository e
  vengono prodotti in modo ripetibile dai runner GitHub Actions.
