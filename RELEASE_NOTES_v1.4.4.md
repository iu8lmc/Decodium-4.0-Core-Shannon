# Release Notes / Note di Rilascio / Notas de Lanzamiento - Fork v1.4.4

Date: 2026-03-11  
Scope: update cycle from `v1.4.3` to `v1.4.4`.

## English

### Summary

`v1.4.4` introduces a complete DXped certificate workflow, mandatory FT2 Async L2 behavior, ASYMX runtime/status improvements, and decode/UI correctness fixes.

### Detailed Changes (`v1.4.3 -> v1.4.4`)

- DXped certificate subsystem:
- added `DXpedCertificate.hpp` with canonical JSON payload normalization and HMAC-SHA256 signature validation.
- certificate metadata extraction now includes callsign, DXCC info, operators, max slots, and certificate fingerprint.
- activation window parsing/validation (`activation_start` and `activation_end`) with UTC normalization.
- operator authorization matching supports base-call comparison for portable variants.
- DXped gating and UX:
- DXped mode now requires a valid certificate for the local operator callsign.
- when activation is requested without a valid certificate, the UI prompts certificate load and blocks DXped enable if checks fail.
- added menu actions:
- `Load DXped Certificate...`
- `DXped Certificate Manager...`
- DXped certificate manager launch path detection was added for bundled and source-tree layouts.
- bundled DXped tooling (Python scripts + public key) is now installed in runtime data path.
- DXped runtime behavior:
- standard `processMessage()` flow is now blocked while DXped mode FSM is active, avoiding collisions with normal AutoSeq flow.
- DXped auto-sequencing hooks were added in decode paths to keep slot FSM in control.
- CQ fallback behavior improved: when queue is empty and `tx5` is blank, CQ text is mirrored from `tx6`.
- FT2 Async L2 behavior:
- Async L2 is now mandatory in FT2.
- local and remote disable requests are ignored in FT2 and forced back to ON.
- Async state reset is now explicit on toggle transitions (audio ring position, async rows, pending pins/messages, timers).
- ASYMX progress and timing:
- new FT2 progress bar states: `GUARD`, `TX`, `RX`, `IDLE`.
- added 300 ms guard window before first FT2 auto-TX transition.
- TX/RX elapsed timing is tracked explicitly to improve visual timing coherence.
- Decode correctness and UI consistency:
- fixed-pitch decode font enforcement in decode panes for column alignment.
- AP/quality tag handling moved to line tail to avoid right-column alignment drift.
- FT2 mode marker normalization (`~` to `+`) applied in both regular and async decode paths.
- safer double-click decode parsing: right-side appended annotations are stripped before `DecodedText` parsing.
- Networking/runtime identity:
- UDP message-client id now derives from application name (`Decodium - <rig>` style) for cleaner multi-instance separation.
- Localization:
- added Italian strings for async status labels (`IDLE/GUARD/TX/RX`) and mandatory Async L2 notices.
- Release/build alignment:
- fork version defaults updated to `v1.4.4` in build metadata and workflows.
- release docs and templates aligned to `v1.4.4`.

### Release Artifacts

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/experimental): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

### Linux Minimum Requirements

- Architecture: `x86_64` (64-bit)
- CPU: dual-core 2.0 GHz or better
- RAM: 4 GB minimum (8 GB recommended)
- Storage: at least 500 MB free
- Runtime/software:
- Linux with `glibc >= 2.35`
- `libfuse2` / FUSE2 support
- ALSA, PulseAudio, or PipeWire audio stack

### If macOS blocks startup

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

### Linux AppImage recommendation

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l'AppImage e poi eseguendo il programma dalla cartella estratta.

Eseguire i seguenti comandi nel terminale:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Italiano

### Sintesi

`v1.4.4` introduce un workflow completo di certificati DXped, rende Async L2 obbligatorio in FT2, aggiunge stati/timing ASYMX e corregge diversi punti di coerenza decode/UI.

### Modifiche Dettagliate (`v1.4.3 -> v1.4.4`)

- Sottosistema certificati DXped:
- aggiunto `DXpedCertificate.hpp` con normalizzazione payload JSON canonico e verifica firma HMAC-SHA256.
- estrazione metadati certificato: callsign, dati DXCC, operatori, max slot, fingerprint.
- parsing/validazione finestra temporale (`activation_start`, `activation_end`) con normalizzazione UTC.
- autorizzazione operatore con confronto anche base-call per varianti portatili.
- Gating e UX DXped:
- DXped mode ora richiede certificato valido per il callsign locale.
- se l'utente tenta l'attivazione senza certificato valido, UI propone il caricamento e blocca l'attivazione in caso di fallimento controlli.
- aggiunte azioni menu:
- `Load DXped Certificate...`
- `DXped Certificate Manager...`
- aggiunto rilevamento percorsi manager certificati per layout bundle e source tree.
- tool DXped (script Python + chiave pubblica) ora installati nel percorso dati runtime.
- Runtime DXped:
- il flusso standard `processMessage()` viene bloccato quando la FSM DXped e' attiva, evitando collisioni con AutoSeq standard.
- inseriti hook `dxpedAutoSequence` nei percorsi decode per mantenere il controllo FSM degli slot.
- migliorato fallback CQ: con coda vuota e `tx5` vuoto, il testo CQ viene specchiato da `tx6`.
- Comportamento FT2 Async L2:
- Async L2 ora e' obbligatorio in FT2.
- richieste locali/remoto di disattivazione in FT2 vengono ignorate e ripristinate ON.
- reset stato async esplicito su transizioni toggle (ring audio, righe async, pin/message pending, timer).
- Progress/timing ASYMX:
- nuovi stati barra progressi FT2: `GUARD`, `TX`, `RX`, `IDLE`.
- finestra guardia 300 ms prima del primo passaggio auto-TX in FT2.
- tracking esplicito tempi TX/RX per coerenza visiva.
- Correttezza decode e coerenza UI:
- font monospazio forzato sulle pane decode per mantenere allineamenti colonnari.
- marker AP/qualita' spostato in coda riga per evitare drift colonna destra.
- normalizzazione marker FT2 (`~` in `+`) applicata in decode normali e async.
- parsing double-click decode piu robusto: rimozione annotazioni appese a destra prima di `DecodedText`.
- Identita' runtime/network:
- id client UDP ora derivato dal nome applicazione (`Decodium - <rig>`) per separare meglio istanze multiple.
- Localizzazione:
- aggiunte stringhe italiane per stati async (`IDLE/GUARD/TX/RX`) e avviso Async L2 obbligatorio.
- Allineamento release/build:
- default versione fork allineati a `v1.4.4` in metadati build e workflow.
- documentazione/template release allineati a `v1.4.4`.

### Artifact Release

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/sperimentale): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

### Requisiti Minimi Linux

- Architettura: `x86_64` (64 bit)
- CPU: dual-core 2.0 GHz o superiore
- RAM: minimo 4 GB (consigliati 8 GB)
- Spazio disco: almeno 500 MB liberi
- Runtime/software:
- Linux con `glibc >= 2.35`
- supporto `libfuse2` / FUSE2
- stack audio ALSA, PulseAudio o PipeWire

### Se macOS blocca l'avvio

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

### Avvio consigliato AppImage su Linux

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l'AppImage e poi eseguendo il programma dalla cartella estratta.

Eseguire i seguenti comandi nel terminale:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Espanol

### Resumen

`v1.4.4` introduce un flujo completo de certificados DXped, hace Async L2 obligatorio en FT2, agrega estados/timing ASYMX y corrige coherencia decode/UI.

### Cambios Detallados (`v1.4.3 -> v1.4.4`)

- Subsistema de certificados DXped:
- anadido `DXpedCertificate.hpp` con normalizacion de payload JSON canonico y validacion HMAC-SHA256.
- extraccion de metadatos: callsign, datos DXCC, operadores, max slots, fingerprint.
- parsing/validacion de ventana temporal (`activation_start`, `activation_end`) con normalizacion UTC.
- validacion de operador con comparacion base-call para variantes portables.
- Gating y UX DXped:
- DXped mode ahora requiere certificado valido para el callsign local.
- al intentar activar sin certificado valido, la UI propone cargarlo y bloquea activacion si fallan los controles.
- anadidas acciones de menu:
- `Load DXped Certificate...`
- `DXped Certificate Manager...`
- deteccion de rutas del manager para layout bundle y arbol source.
- herramientas DXped (scripts Python + clave publica) ahora incluidas en instalacion runtime.
- Runtime DXped:
- flujo estandar `processMessage()` bloqueado cuando la FSM DXped esta activa para evitar colisiones con AutoSeq.
- hooks de `dxpedAutoSequence` anadidos en rutas decode para mantener control FSM de slots.
- fallback CQ mejorado: con cola vacia y `tx5` vacio, se refleja texto CQ desde `tx6`.
- Comportamiento FT2 Async L2:
- Async L2 ahora obligatorio en FT2.
- intentos locales/remotos de desactivar en FT2 se ignoran y se fuerza ON.
- reset explicito del estado async en transiciones toggle (ring audio, filas async, pendientes y timers).
- Progress/timing ASYMX:
- nuevos estados de barra FT2: `GUARD`, `TX`, `RX`, `IDLE`.
- ventana de guardia de 300 ms antes del primer auto-TX FT2.
- tracking explicito de tiempos TX/RX para coherencia visual.
- Correcciones decode/UI:
- fuente monoespaciada forzada en paneles decode para mantener columnas.
- marcador AP/calidad movido al final de linea para evitar drift de columna derecha.
- normalizacion de marcador FT2 (`~` a `+`) en decode normal y async.
- doble click decode mas robusto eliminando anotaciones de cola antes de parsear `DecodedText`.
- Identidad runtime/network:
- id de cliente UDP derivado del nombre de aplicacion (`Decodium - <rig>`) para separar mejor instancias multiples.
- Localizacion:
- anadidas cadenas italianas para estados async (`IDLE/GUARD/TX/RX`) y aviso Async L2 obligatorio.
- Alineacion release/build:
- defaults de version alineados a `v1.4.4` en metadatos build y workflows.
- docs/templates release alineados a `v1.4.4`.

### Artefactos de Release

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/experimental): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

### Requisitos Minimos Linux

- Arquitectura: `x86_64` (64 bits)
- CPU: dual-core 2.0 GHz o superior
- RAM: 4 GB minimo (8 GB recomendado)
- Espacio: al menos 500 MB libres
- Runtime/software:
- Linux con `glibc >= 2.35`
- soporte `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

### Si macOS bloquea el inicio

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

### Recomendacion AppImage Linux

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l'AppImage e poi eseguendo il programma dalla cartella estratta.

Eseguire i seguenti comandi nel terminale:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```
