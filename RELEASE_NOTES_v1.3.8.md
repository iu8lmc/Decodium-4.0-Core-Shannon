# Release Notes / Note di Rilascio / Notas de Lanzamiento - Fork v1.3.8

Date: 2026-03-05  
Scope: Consolidated fixes/features from `v1.3.7` to `v1.3.8`, release/documentation alignment, multi-platform artifacts.

## English

### Summary

- Hardened CAT/remote Configure behavior to avoid forced FT2 mode from generic control traffic.
- Hardened UDP control surface with direct target-id enforcement.
- Added optional world-map greyline toggle in Settings -> General.
- Added active map-path distance badge (km/mi based on unit setting).
- Refined compact top controls and DX-ped alignment on small displays.

### Detailed Changes

- CAT and remote control:
  - `remote_configure()` now respects the requested mode and no longer forces FT2 when external controllers are open.
  - Control messages (`Configure` and related commands) now require direct destination id; wildcard/global targets are rejected.
  - UI error diagnostics now include expected controller target id for easier setup debugging.
- Map and UX:
  - New configurable greyline toggle (`MapShowGreyline`) with runtime propagation to map renderer.
  - Active QSO map path now displays distance text badge in km/mi according to `Miles` preference.
  - Top-bar compact layout refined after DX-ped addition to reduce overlap/truncation on narrow displays.
- Release and docs:
  - Fork metadata aligned to `v1.3.8` in CMake/revision/workflows.
  - Documentation updated in English, Italian, and Spanish.

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
- Aggiunto toggle opzionale greyline mappa in Settings -> General.
- Aggiunto badge distanza sul path mappa attivo (km/mi secondo unita' impostata).
- Rifinito layout top compatto e area DX-ped su schermi piccoli.

### Modifiche Dettagliate

- CAT e controllo remoto:
  - `remote_configure()` rispetta la modalita' richiesta e non forza FT2 quando controller esterni sono aperti.
  - I messaggi di controllo (`Configure` e correlati) richiedono ora destination id diretto; target wildcard/globali vengono rifiutati.
  - La diagnostica UI include ora l'expected controller target id per semplificare il debug della configurazione.
- Mappa e UX:
  - Nuovo toggle configurabile greyline (`MapShowGreyline`) con propagazione runtime al renderer mappa.
  - Il path mappa del QSO attivo mostra ora badge distanza in km/mi secondo preferenza `Miles`.
  - Layout top compatto rifinito dopo aggiunta DX-ped per ridurre sovrapposizioni/troncamenti su display stretti.
- Release e documentazione:
  - Metadati fork allineati a `v1.3.8` in CMake/revision/workflow.
  - Documentazione aggiornata in inglese, italiano e spagnolo.

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
- Se anade toggle opcional greyline del mapa en Settings -> General.
- Se anade badge de distancia en la ruta activa del mapa (km/mi segun unidad configurada).
- Se refina layout superior compacto y zona DX-ped en pantallas pequenas.

### Cambios Detallados

- CAT y control remoto:
  - `remote_configure()` respeta el modo solicitado y ya no fuerza FT2 cuando hay controladores externos abiertos.
  - Mensajes de control (`Configure` y relacionados) ahora requieren destination id directo; objetivos wildcard/globales se rechazan.
  - El diagnostico UI ahora incluye expected controller target id para facilitar configuracion.
- Mapa y UX:
  - Nuevo toggle configurable greyline (`MapShowGreyline`) propagado en runtime al renderer del mapa.
  - La ruta activa del QSO muestra badge de distancia en km/mi segun preferencia `Miles`.
  - Layout superior compacto ajustado tras DX-ped para reducir solapes/cortes en pantallas estrechas.
- Release y documentacion:
  - Metadatos del fork alineados a `v1.3.8` en CMake/revision/workflows.
  - Documentacion actualizada en ingles, italiano y espanol.

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
