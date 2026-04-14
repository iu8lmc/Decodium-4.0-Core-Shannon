# Release Notes / Note di Rilascio / Notas de Lanzamiento - Fork v1.4.1

Date: 2026-03-09  
Scope: consolidated updates from `v1.4.0` to `v1.4.1`, with runtime/mode-switch fixes, CAT robustness, web dashboard controls, and release/docs alignment.

## English

### Summary

`v1.4.1` finalizes the stabilization cycle after `v1.4.0` with targeted fixes requested during live operation:

- Startup mode-switch responsiveness fixed (no delayed first mode changes).
- Startup advanced FT controls initialization fixed (no unexpected raw controls shown at boot).
- No forced waterfall foreground on mode switch.
- Startup auto-mode-from-rig made one-shot (prevents repeated CAT re-forcing loop).
- Additional CAT polling robustness and remote dashboard control surface completion.

### Detailed Changes (`v1.4.0 -> v1.4.1`)

- Startup/runtime behavior:
  - forced full startup mode initialization to avoid partially initialized UI controls.
  - fixed startup mode responsiveness regression caused by repeated auto mode forcing from rig frequency polling.
  - startup auto mode selection now disables itself after first valid match.
  - startup audio stream re-open safety timer retained for driver wakeup edge cases.
- Mode switching/UI:
  - removed automatic `wideGraph` foregrounding during FT2/FT4/FT8/FST4/FST4W switches.
  - Async L2 visibility remains FT2-only; hidden active-state leakage prevented outside FT2.
  - Alt 1/2 visibility behavior remains mode-aware.
- CAT/transceiver robustness:
  - polling transceiver now tolerates short transient poll failures before marking offline.
  - reduces false CAT disconnects under external logbook/CAT bridge contention.
- Remote Web Dashboard / API:
  - runtime state now exposes Async L2 / Dual Carrier / Alt 1/2 flags.
  - remote command handlers added for `set_async_l2`, `set_dual_carrier`, `set_alt_12`.
  - waterfall latest-frame HTTP endpoint available for remote UI rendering.
- Layout/map persistence:
  - world-map visibility persisted and restored cleanly in splitter layout.
  - single-column decode flow behavior preserved.

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

### Web App Setup Guide

Detailed novice setup is available in:

- `doc/WEBAPP_SETUP_GUIDE.en-GB.md`
- `doc/WEBAPP_SETUP_GUIDE.it.md`
- `doc/WEBAPP_SETUP_GUIDE.es.md`

## Italiano

### Sintesi

`v1.4.1` chiude il ciclo di stabilizzazione dopo `v1.4.0` con fix mirati richiesti in uso reale:

- corretto ritardo iniziale nel cambio modalita'.
- corretta inizializzazione startup dei controlli FT avanzati (niente controlli grezzi inattesi all'avvio).
- rimosso il passaggio forzato in primo piano del waterfall durante il cambio modalita'.
- auto-selezione modalita' da frequenza radio resa one-shot (evita loop di riforzatura CAT).
- ulteriore robustezza polling CAT e completamento controlli dashboard remota.

### Modifiche Dettagliate (`v1.4.0 -> v1.4.1`)

- Comportamento startup/runtime:
  - forzata inizializzazione completa della modalita' in avvio per evitare controlli UI parzialmente inizializzati.
  - risolta regressione di responsiveness iniziale causata da riforzatura ripetuta della modalita' via polling frequenza rig.
  - auto-selezione modalita' startup disattivata dopo il primo match valido.
  - mantenuto timer di riapertura stream audio in startup per edge-case driver/risveglio periferiche.
- Cambio modalita'/UI:
  - rimosso `wideGraph->show()` automatico su switch FT2/FT4/FT8/FST4/FST4W.
  - visibilita' Async L2 confermata solo in FT2; prevenuto stato attivo nascosto fuori FT2.
  - comportamento visibilita' Alt 1/2 mantenuto mode-aware.
- Robustezza CAT/transceiver:
  - polling transceiver ora tollera brevi failure transitori prima di segnare offline.
  - ridotte false disconnessioni CAT con software esterni (logbook/CAT bridge).
- Dashboard Web Remota / API:
  - stato runtime esporta anche Async L2 / Dual Carrier / Alt 1/2.
  - aggiunti handler comandi remoti `set_async_l2`, `set_dual_carrier`, `set_alt_12`.
  - endpoint HTTP waterfall frame latest disponibile per rendering UI remota.
- Persistenza layout/mappa:
  - visibilita' world-map persistita e ripristinata correttamente nello splitter.
  - comportamento flusso decode a colonna singola preservato.

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

### Guida Web App

Guida dettagliata per neofiti disponibile in:

- `doc/WEBAPP_SETUP_GUIDE.it.md`
- `doc/WEBAPP_SETUP_GUIDE.en-GB.md`
- `doc/WEBAPP_SETUP_GUIDE.es.md`

## Espanol

### Resumen

`v1.4.1` cierra el ciclo de estabilizacion tras `v1.4.0` con fixes concretos solicitados en operacion real:

- corregido retraso inicial en cambio de modo.
- corregida inicializacion de controles FT avanzados al arranque (sin controles "raw" inesperados).
- eliminado enfoque forzado del waterfall al cambiar modo.
- auto-seleccion de modo por frecuencia del rig ahora one-shot (evita bucle de re-forzado CAT).
- robustez adicional de polling CAT y cierre de superficie de control del dashboard remoto.

### Cambios Detallados (`v1.4.0 -> v1.4.1`)

- Comportamiento startup/runtime:
  - inicializacion completa de modo al arranque para evitar controles UI parcialmente inicializados.
  - resuelta regresion de respuesta inicial causada por re-forzado repetido de modo via polling de frecuencia del rig.
  - auto-seleccion de modo startup se desactiva tras el primer match valido.
  - se mantiene temporizador de reapertura de stream de audio en startup para edge-cases de drivers.
- Cambio de modo/UI:
  - eliminado `wideGraph->show()` automatico en FT2/FT4/FT8/FST4/FST4W.
  - Async L2 sigue visible solo en FT2; evitado estado activo oculto fuera de FT2.
  - comportamiento de visibilidad Alt 1/2 mantenido segun modo.
- Robustez CAT/transceiver:
  - polling transceiver tolera fallos transitorios cortos antes de marcar offline.
  - reduce desconexiones CAT falsas con software externo (logbook/CAT bridge).
- Dashboard Web Remoto / API:
  - estado runtime incluye Async L2 / Dual Carrier / Alt 1/2.
  - nuevos handlers de comando remoto `set_async_l2`, `set_dual_carrier`, `set_alt_12`.
  - endpoint HTTP de ultimo frame waterfall para render remoto.
- Persistencia layout/mapa:
  - visibilidad de world-map persistida/restaurada correctamente en splitter.
  - comportamiento de flujo decode en columna unica preservado.

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

### Guia Web App

Guia detallada para principiantes disponible en:

- `doc/WEBAPP_SETUP_GUIDE.es.md`
- `doc/WEBAPP_SETUP_GUIDE.en-GB.md`
- `doc/WEBAPP_SETUP_GUIDE.it.md`
