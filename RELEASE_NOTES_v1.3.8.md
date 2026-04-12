# Release Notes / Note di Rilascio / Notas de Lanzamiento - Fork v1.3.8

Date: 2026-03-05  
Scope: Consolidated fixes/features from `v1.3.7` to `v1.3.8`, release/documentation alignment, multi-platform artifacts.

## English

### Summary

- Hardened CAT/remote Configure behavior to avoid forced FT2 mode from generic control traffic.
- Hardened UDP control surface with direct target-id enforcement.
- Hardened TCI parser/runtime path (safer websocket URL parsing, guarded command argument parsing, timer/null guards).
- Hardened network/download flow (safer redirects, null-manager guards, safer LotW query construction + password redaction in logs).
- Added optional world-map greyline toggle in Settings -> General.
- Added active map-path distance badge (km/mi based on unit setting).
- Refined compact top controls and DX-ped alignment on small displays.
- Added screen-safe geometry restore helper for external/5K monitor scenarios.
- Applied compatibility fixes for Linux CI + Hamlib 4.x/5.x build paths.

### Detailed Changes

- CAT and remote control:
  - `remote_configure()` now respects the requested mode and no longer forces FT2 when external controllers are open.
  - Control messages (`Configure` and related commands) now require direct destination id; wildcard/global targets are rejected.
  - UI error diagnostics now include expected controller target id for easier setup debugging.
- TCI stability and parsing:
  - WebSocket endpoint normalization improved (`ws`/`wss`, host/port fallback) to avoid malformed endpoint startup failures.
  - Safer command token parsing (`value()`/trimmed access instead of unchecked `at()` patterns).
  - Decimal tenths parsing for power/SWR made robust (avoids split/index edge crashes).
  - Added timer/null guards in several TCI state transitions.
- Network and privacy hardening:
  - LotW download URL construction moved to query API builder; password values are redacted in debug traces.
  - LotW download signal wiring now uses explicit disconnect + `Qt::UniqueConnection` to avoid duplicate callback chains.
  - `FileDownload` and sample downloader now guard null network manager/reply cases and use no-less-safe redirect policy.
  - Fox verifier logging now avoids leaking full URLs/payloads in logs.
- Map and UX:
  - New configurable greyline toggle (`MapShowGreyline`) with runtime propagation to map renderer.
  - Active QSO map path now displays distance text badge in km/mi according to `Miles` preference.
  - Top-bar compact layout refined after DX-ped addition to reduce overlap/truncation on narrow displays.
  - Auto-CQ queue robustness improved with missed-period timeout handling.
  - Added DX-ped 2-slot AutoCQ workflow controls and queue handling refinements.
- Geometry and multi-monitor resilience:
  - Added `WindowGeometryUtils` restore path to keep tool windows visible/on-screen after monitor layout changes.
  - Applied to key windows (`DX Cluster`, `Ionospheric Forecast`, log/export dialogs).
- Legacy tools hardening (`map65`/`qmap`):
  - Replaced risky string copies with bounded `snprintf`/safe helpers.
  - Added device index clamping and defensive list bounds checks.
  - Improved file path handling using `QFile::encodeName` for safer non-ASCII/long path cases.
- Release and docs:
  - Fork metadata aligned to `v1.3.8` in CMake/revision/workflows.
  - Documentation updated in English, Italian, and Spanish.
  - Linux workflows fixed for latest Hamlib tag parsing and Hamlib 4.x fallback compatibility.

### Release Artifacts

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/experimental): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

Note: `.pkg` installers are intentionally not produced.

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

To avoid issues caused by AppImage read-only filesystem mode:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Italiano

### Sintesi

- Hardening CAT/Configure remoto per evitare forzature FT2 da traffico di controllo generico.
- Hardening superficie controllo UDP con enforcement target-id diretto.
- Hardening percorso TCI/runtime (parsing endpoint websocket piu' sicuro, parsing comandi protetto, guardie timer/null).
- Hardening flusso rete/download (redirect piu' sicuri, guardie null-manager, costruzione query LotW sicura + redazione password nei log).
- Aggiunto toggle opzionale greyline mappa in Settings -> General.
- Aggiunto badge distanza sul path mappa attivo (km/mi secondo unita' impostata).
- Rifinito layout top compatto e area DX-ped su schermi piccoli.
- Aggiunto restore geometria screen-safe per scenari monitor esterni/5K.
- Applicati fix compatibilita' CI Linux + build Hamlib 4.x/5.x.

### Modifiche Dettagliate

- CAT e controllo remoto:
  - `remote_configure()` rispetta la modalita' richiesta e non forza FT2 quando controller esterni sono aperti.
  - I messaggi di controllo (`Configure` e correlati) richiedono ora destination id diretto; target wildcard/globali vengono rifiutati.
  - La diagnostica UI include ora l'expected controller target id per semplificare il debug della configurazione.
- Stabilita' TCI e parsing:
  - Normalizzazione endpoint WebSocket migliorata (`ws`/`wss`, fallback host/porta) per evitare failure su endpoint malformati.
  - Parsing token comandi reso sicuro (`value()`/trim) al posto di accessi non protetti.
  - Parsing decimali potenza/SWR reso robusto (evita crash su split/index irregolari).
  - Aggiunte guardie timer/null in diverse transizioni stato TCI.
- Hardening rete e privacy:
  - Costruzione URL download LotW migrata a query API; password ora redatte nei trace log.
  - Connessioni segnali download LotW con disconnect esplicito + `Qt::UniqueConnection` (evita callback duplicati).
  - `FileDownload` e sample downloader ora gestiscono null network manager/reply e policy redirect no-less-safe.
  - Logging Fox verifier ripulito per evitare leak di URL/payload completi.
- Mappa e UX:
  - Nuovo toggle configurabile greyline (`MapShowGreyline`) con propagazione runtime al renderer mappa.
  - Il path mappa del QSO attivo mostra ora badge distanza in km/mi secondo preferenza `Miles`.
  - Layout top compatto rifinito dopo aggiunta DX-ped per ridurre sovrapposizioni/troncamenti su display stretti.
  - Robustezza coda Auto-CQ migliorata con timeout su periodi senza risposta.
  - Aggiunto workflow DX-ped 2-slot con controlli dedicati e gestione coda affinata.
- Geometria e resilienza multi-monitor:
  - Aggiunto `WindowGeometryUtils` per mantenere le finestre visibili dopo cambi layout monitor.
  - Applicato a finestre principali (`DX Cluster`, `Ionospheric Forecast`, log/export).
- Hardening tool legacy (`map65`/`qmap`):
  - Sostituzione copie stringa rischiose con `snprintf`/helper bounded.
  - Clamp indici device e check bounds difensivi.
  - Gestione path file migliorata con `QFile::encodeName` per casi non-ASCII/path lunghi.
- Release e documentazione:
  - Metadati fork allineati a `v1.3.8` in CMake/revision/workflow.
  - Documentazione aggiornata in inglese, italiano e spagnolo.
  - Workflow Linux corretti per parsing latest tag Hamlib e compatibilita' fallback Hamlib 4.x.

### Artifact Release

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/sperimentale): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

Nota: i pacchetti `.pkg` non vengono prodotti intenzionalmente.

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

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Espanol

### Resumen

- Hardening CAT/Configure remoto para evitar forzado FT2 por trafico de control generico.
- Hardening de superficie de control UDP con target-id directo obligatorio.
- Hardening de ruta TCI/runtime (parsing WebSocket mas seguro, parsing de comandos protegido, guardas timer/null).
- Hardening de red/descarga (redirects mas seguros, guardas null-manager, construccion segura de query LotW + redaccion de password en logs).
- Se anade toggle opcional greyline del mapa en Settings -> General.
- Se anade badge de distancia en la ruta activa del mapa (km/mi segun unidad configurada).
- Se refina layout superior compacto y zona DX-ped en pantallas pequenas.
- Se anade restore de geometria screen-safe para escenarios con monitor externo/5K.
- Se aplican fixes de compatibilidad CI Linux + build Hamlib 4.x/5.x.

### Cambios Detallados

- CAT y control remoto:
  - `remote_configure()` respeta el modo solicitado y ya no fuerza FT2 cuando hay controladores externos abiertos.
  - Mensajes de control (`Configure` y relacionados) ahora requieren destination id directo; objetivos wildcard/globales se rechazan.
  - El diagnostico UI ahora incluye expected controller target id para facilitar configuracion.
- Estabilidad TCI y parsing:
  - Normalizacion de endpoint WebSocket mejorada (`ws`/`wss`, fallback host/puerto) para evitar fallos con endpoints malformados.
  - Parsing de tokens de comandos reforzado (`value()`/trim) en lugar de accesos sin proteccion.
  - Parsing decimal de potencia/SWR robusto (evita caidas por split/index irregular).
  - Guardas timer/null agregadas en varias transiciones de estado TCI.
- Hardening de red y privacidad:
  - Construccion de URL LotW movida a API de query; password redactada en trazas.
  - Conexiones de senales LotW con disconnect explicito + `Qt::UniqueConnection` para evitar callbacks duplicados.
  - `FileDownload` y sample downloader ahora cubren null manager/reply y redirect no-less-safe.
  - Logging de Fox verifier saneado para no filtrar URL/payload completos.
- Mapa y UX:
  - Nuevo toggle configurable greyline (`MapShowGreyline`) propagado en runtime al renderer del mapa.
  - La ruta activa del QSO muestra badge de distancia en km/mi segun preferencia `Miles`.
  - Layout superior compacto ajustado tras DX-ped para reducir solapes/cortes en pantallas estrechas.
  - Robustez de cola Auto-CQ mejorada con timeout en periodos sin respuesta.
  - Flujo DX-ped 2-slot agregado con controles dedicados y cola refinada.
- Geometria y resiliencia multi-monitor:
  - `WindowGeometryUtils` agregado para mantener ventanas visibles tras cambios de layout de pantallas.
  - Aplicado a ventanas clave (`DX Cluster`, `Ionospheric Forecast`, log/export).
- Hardening de herramientas legacy (`map65`/`qmap`):
  - Reemplazo de copias de string riesgosas por `snprintf`/helpers bounded.
  - Clamp de indices de dispositivos y comprobaciones defensivas de limites.
  - Manejo de rutas con `QFile::encodeName` para casos no-ASCII/rutas largas.
- Release y documentacion:
  - Metadatos del fork alineados a `v1.3.8` en CMake/revision/workflows.
  - Documentacion actualizada en ingles, italiano y espanol.
  - Workflows Linux corregidos para parsing latest tag de Hamlib y fallback compatible Hamlib 4.x.

### Artefactos de Release

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/experimental): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

Nota: no se generan instaladores `.pkg`.

### Requisitos Minimos Linux

- Arquitectura: `x86_64` (64 bits)
- CPU: dual-core 2.0 GHz o superior
- RAM: 4 GB minimo (8 GB recomendado)
- Espacio en disco: al menos 500 MB libres
- Runtime/software:
  - Linux con `glibc >= 2.35`
  - soporte `libfuse2` / FUSE2
  - audio ALSA, PulseAudio o PipeWire

### Si macOS bloquea el inicio

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

### Recomendacion AppImage en Linux

Para evitar problemas por el sistema de archivos de solo lectura de AppImage:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```
