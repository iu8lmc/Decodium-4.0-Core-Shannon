# Decodium 4.0 v1.0.503

Version 1.0.503 completes the repository cleanup started in 1.0.502, makes the
release layout and version contract reproducible across all supported
platforms, fixes WSJT-X Client ID propagation on all three UDP destinations and
improves native macOS audio/panadapter cadence.

## Changes from 1.0.502 to 1.0.503

### UDP reporting and Client ID

- The configured Client ID is normalized once and serialized consistently in
  every WSJT-X protocol packet sent to the primary, secondary and tertiary UDP
  destinations.
- Blank identifiers fall back to `WSJTX`, repeated whitespace is normalized
  and protocol identifiers are bounded to 64 characters.
- Client ID changes are applied immediately and produce a fresh heartbeat,
  without requiring an application restart.
- The legacy backend now has a complete secondary `MessageClient`, providing
  status, decode, WSPR and clear notifications with the same protocol behavior
  as the primary and tertiary destinations.
- Secondary logged-QSO forwarding is independently controlled, while existing
  ADIF forwarding controls remain available.
- Changes made in Setup refresh the embedded legacy UDP clients in place.
- Diagnostic output now records Client ID, server, port, interface and TTL for
  all three endpoints.
- New loopback tests deserialize heartbeat datagrams from three local UDP
  receivers and verify both initial and live-updated Client IDs.

### macOS audio and panadapter fluidity

- Native AudioQueue capture now uses a callback quantum of approximately 20 ms
  by default instead of inheriting a generic buffer that could delay fresh
  spectrum samples by roughly 170 ms.
- Four AudioQueue buffers remain queued to preserve capture resilience.
- `DECODIUM_MAC_AUDIO_QUEUE_FRAMES` can override the callback size for
  diagnostics and hardware-specific testing.
- The spectrum timer now uses `Qt::PreciseTimer`.
- The GPU-accelerated legacy panadapter can honor the selected 15/20/30 FPS cap
  during normal operation.
- Adaptive cadence remains active during DEEP decoding and CPU pressure, while
  CPU fallback rendering keeps the more conservative timing policy.
- Runtime diagnostics report requested frames, effective frames and callback
  duration for native macOS capture.

### Repository structure

- Required runtime databases and lookup files now live under
  `resources/runtime`.
- Configured CMake templates now live under `CMake/templates`.
- Linux desktop metadata and package descriptions now live under `packaging`.
- Maintained acknowledgements and technical reference files were moved under
  `doc`.
- Obsolete Cirrus, Qt 5, duplicate Linux and hard-coded legacy macOS workflows
  were removed.
- Unused root placeholders, obsolete generated resource files and the orphaned
  `aethersdr` gitlink were removed.
- Installed runtime filenames remain unchanged, preserving upgrade and lookup
  compatibility on Windows, macOS and Linux.

### Release and packaging contract

- `fork_release_version.txt` is the single release-version source for local
  builds and GitHub Actions.
- A shared resolver rejects tags and manual workflow inputs that do not match
  the repository version.
- A repository-layout validator checks mandatory runtime files, templates,
  packaging metadata and matching release notes before long builds begin.
- Windows, macOS Apple Silicon, macOS Intel, Linux x86_64 and Linux aarch64
  workflows use the same validation contract.
- Windows packaging copies and verifies required runtime data from the
  maintained source directory before creating the installer.
- CAT frequency-rejection handling remains source-compatible with Linux
  distributions that still ship Hamlib 4.5.
- CMake install rules, local build scripts and AppImage/DMG packaging scripts
  were updated for the new repository layout.

### Setup UI and runtime lookup

- Added internal horizontal padding to TCP port, working-frequency, station
  offset and decode-color fields.
- Source-tree fallbacks for `CALL3.TXT`, `cty.dat` and `sat.dat` now resolve
  through `resources/runtime`.
- Documentation now describes the maintained source, runtime, packaging and
  release layout.

## Validation

- Repository layout and release-version contract validation.
- macOS `decodium_qml` build.
- Targeted UDP Client ID and FT runtime policy tests.
- CTest regression run for the configured local build: 16/18 test executables
  passed. The unchanged synthetic FT8 `-27 dB` hinted-decode limit and two
  pre-existing assertions inside `test_qt_helpers` remain documented test
  limitations; the new UDP and runtime-policy tests pass.
- Git diff whitespace validation.
- GitHub Actions packaging for Windows x64, macOS Apple Silicon, macOS Intel,
  Linux x86_64 and Linux aarch64.

---

## Italiano

La versione 1.0.503 completa la pulizia del repository iniziata nella 1.0.502,
rende riproducibili struttura e versione delle release su tutte le piattaforme,
corregge la propagazione del Client ID WSJT-X sulle tre destinazioni UDP e
migliora la fluidita' del percorso audio/panadapter nativo macOS.

### Reporting UDP e Client ID

- Il Client ID configurato viene normalizzato una sola volta e serializzato in
  modo coerente in tutti i pacchetti WSJT-X inviati alle destinazioni primaria,
  secondaria e terziaria.
- Un identificatore vuoto torna a `WSJTX`, gli spazi ripetuti vengono
  normalizzati e la lunghezza massima e' limitata a 64 caratteri.
- Le modifiche al Client ID sono applicate immediatamente e generano un nuovo
  heartbeat senza richiedere il riavvio.
- Il backend legacy dispone ora di un client UDP secondario completo per
  status, decode, WSPR e notifiche di cancellazione.
- L'invio dei QSO registrati sulla destinazione secondaria e' controllabile
  separatamente e restano disponibili i controlli ADIF esistenti.
- Le modifiche effettuate in Setup aggiornano direttamente i client UDP del
  backend legacy integrato.
- Il log diagnostico riporta Client ID, server, porta, interfaccia e TTL per
  tutte e tre le destinazioni.
- Nuovi test loopback deserializzano gli heartbeat su tre ricevitori UDP e
  verificano anche il cambio Client ID a runtime.

### Audio macOS e fluidita' del panadapter

- AudioQueue usa ora callback di circa 20 ms invece di ereditare buffer generici
  che potevano ritardare i campioni del panadapter di circa 170 ms.
- Restano accodati quattro buffer per mantenere stabile l'acquisizione.
- `DECODIUM_MAC_AUDIO_QUEUE_FRAMES` permette di modificare la dimensione dei
  callback per diagnostica.
- Il timer dello spettro usa ora `Qt::PreciseTimer`.
- Il percorso legacy accelerato GPU puo' rispettare il limite selezionato di
  15/20/30 FPS durante il funzionamento normale.
- Rimane attivo il rallentamento adattivo durante DEEP e sotto pressione CPU;
  il fallback CPU conserva una politica piu' prudente.
- La diagnostica riporta frame richiesti, frame effettivi e durata dei callback.

### Struttura del repository

- Database e file lookup runtime obbligatori sono stati spostati in
  `resources/runtime`.
- I template CMake configurati sono ora in `CMake/templates`.
- Metadati desktop Linux e descrizioni dei pacchetti sono ora in `packaging`.
- Ringraziamenti e riferimenti tecnici mantenuti sono stati spostati in `doc`.
- Rimossi workflow Cirrus, Qt 5, Linux duplicati e vecchi workflow macOS con
  versioni hard-coded.
- Rimossi placeholder inutilizzati, risorse generate obsolete e il gitlink
  orfano `aethersdr`.
- I nomi installati dei file runtime restano invariati per non interrompere
  aggiornamenti e lookup sulle tre piattaforme.

### Contratto di release e packaging

- `fork_release_version.txt` e' l'unica fonte della versione per build locali e
  GitHub Actions.
- Un resolver condiviso blocca tag o input manuali diversi dalla versione del
  repository.
- Un validatore controlla file runtime, template, metadati di packaging e note
  della release prima dell'avvio delle build lunghe.
- Windows, macOS Apple Silicon, macOS Intel, Linux x86_64 e Linux aarch64 usano
  lo stesso contratto.
- Il packaging Windows copia e verifica i dati runtime dalla cartella mantenuta
  prima di creare l'installer.
- La gestione dei rifiuti CAT durante il cambio frequenza resta compilabile
  anche sulle distribuzioni Linux che forniscono ancora Hamlib 4.5.
- Regole CMake, script locali e packaging AppImage/DMG sono stati aggiornati per
  il nuovo layout.

### Setup e lookup runtime

- Aggiunto padding interno ai campi porta TCP, frequenza operativa, offset di
  stazione e colori decode.
- I fallback sorgente per `CALL3.TXT`, `cty.dat` e `sat.dat` usano ora
  `resources/runtime`.
- La documentazione descrive la struttura mantenuta di sorgenti, runtime,
  packaging e release.

### Validazione

- Build macOS del target `decodium_qml` completata.
- Test mirati Client ID UDP e policy runtime FT completati.
- Suite CTest: 16/18 eseguibili di test superati. Restano il limite sintetico
  FT8 hinted decode a `-27 dB` e due asserzioni preesistenti nel contenitore
  `test_qt_helpers`; i nuovi test introdotti dalla 1.0.503 sono superati.
- Validazione del layout, del contratto di versione e del diff completata.
