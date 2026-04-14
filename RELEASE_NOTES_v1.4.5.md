# Release Notes / Note di Rilascio / Notas de Lanzamiento - Fork v1.4.5

Date: 2026-03-12  
Scope: update cycle from `v1.4.4` to `v1.4.5`.

## English

### Summary

`v1.4.5` focuses on FT2 AutoCQ state-machine stability, decode/UDP correctness, and release workflow synchronization.

### Detailed Changes (`v1.4.4 -> v1.4.5`)

- FT2 AutoCQ sequencing:
- strengthened partner continuity during active QSOs, with manual override only via Ctrl/Shift.
- improved directed-reply detection while calling CQ (full/base callsign aware).
- prevented non-partner `73` frames from hijacking signoff state.
- prevented stale deferred autolog timers from forcing late state changes.
- added recent-log dedupe in caller queue to avoid immediate rework loops.
- FT2 signoff and log stability:
- deferred signoff and pending-autolog snapshot handoff hardened.
- RR73/73 completion path tuned before fallback to CQ.
- retry window tuned for AutoCQ (`MAX_TX_RETRIES=5`), with reset on partner call changes.
- FT2 decode/runtime:
- introduced `TΔ` display in FT2 decode column (time since TX).
- fixed FT2 alignment with variable marker spacing and Unicode minus variants.
- preserved weak async confirmations before near-duplicate suppression.
- decoder threshold update: `nharderror` relaxed from `35` to `48`.
- TX timing:
- fixed FT2 TX ramp-up/ramp-down and latency behavior for both TCI and soundcard paths.
- Remote/web and UI:
- fixed remote dashboard startup/fetch initialization path.
- fixed `View -> Display a cascata` / waterfall activation behavior.
- restored localized web password hint requiring minimum 12 characters.
- UDP/logging integration:
- fixed FT2 UDP decode payload extraction so external loggers (RumLog and similar) do not lose first callsign character.
- Release/build alignment:
- `FORK_RELEASE_VERSION` updated to `v1.4.5`.
- workflow defaults and release-note fallback templates aligned to `v1.4.5`.

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

`v1.4.5` e' una release focalizzata su stabilita' macchina stati FT2 AutoCQ, correttezza decode/UDP e allineamento workflow release.

### Modifiche Dettagliate (`v1.4.4 -> v1.4.5`)

- Sequenza FT2 AutoCQ:
- rafforzata continuita' partner durante QSO attivo, con override manuale solo Ctrl/Shift.
- migliorato riconoscimento risposte dirette durante CQ (matching callsign completo/base).
- impedito che `73` non-partner alterino il percorso di signoff.
- bloccati timer deferred autolog stale che causavano transizioni tardive.
- introdotta deduplica caller recenti in coda dopo log per evitare richiami immediati.
- Stabilita' signoff e log FT2:
- hardening percorso deferred signoff con handoff snapshot autolog pending.
- rifinito completamento RR73/73 prima del fallback a CQ.
- retry AutoCQ aggiornati (`MAX_TX_RETRIES=5`) con reset su cambio chiamata partner.
- Decode/runtime FT2:
- introdotta visualizzazione `TΔ` in colonna decode FT2 (tempo dall'ultimo TX).
- corretto allineamento FT2 con marker a spaziatura variabile e segni meno Unicode.
- preservate conferme async deboli prima della soppressione near-duplicate.
- aggiornata soglia decoder: `nharderror` da `35` a `48`.
- Timing TX:
- corretto comportamento FT2 su ramp-up/ramp-down e latenza TX per percorsi TCI e soundcard.
- Web/UI:
- corretta inizializzazione startup/fetch dashboard remota.
- corretta apertura `Vista -> Display a cascata` (waterfall).
- ripristinato hint password web localizzato con minimo 12 caratteri.
- Integrazione UDP/logger:
- corretto payload decode FT2 via UDP: logger esterni (RumLog e simili) non perdono piu il primo carattere del callsign.
- Allineamento release/build:
- `FORK_RELEASE_VERSION` aggiornato a `v1.4.5`.
- default workflow e fallback template release allineati a `v1.4.5`.

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

`v1.4.5` se centra en estabilidad de la maquina de estados FT2 AutoCQ, correccion decode/UDP y sincronizacion de workflows de release.

### Cambios Detallados (`v1.4.4 -> v1.4.5`)

- Secuencia FT2 AutoCQ:
- reforzada continuidad de pareja durante QSO activo, con override manual solo Ctrl/Shift.
- mejor deteccion de respuestas dirigidas durante CQ (matching por callsign completo/base).
- evitado que `73` de no-pareja secuestre el estado de signoff.
- bloqueados timers deferred autolog stale que causaban cambios tardios.
- dedupe de callers recientes en cola despues del log para evitar reintentos inmediatos.
- Estabilidad signoff y log FT2:
- hardening del flujo deferred signoff con handoff de snapshot autolog pending.
- cierre RR73/73 afinado antes de volver a CQ.
- reintentos AutoCQ ajustados (`MAX_TX_RETRIES=5`) con reset al cambiar pareja.
- Decode/runtime FT2:
- anadida visualizacion `TΔ` en columna decode FT2 (tiempo desde ultimo TX).
- corregida alineacion FT2 con espaciado variable del marcador y signos menos Unicode.
- confirmaciones async debiles preservadas antes de supresion near-duplicate.
- umbral decoder actualizado: `nharderror` de `35` a `48`.
- Timing TX:
- corregido comportamiento FT2 de ramp-up/ramp-down y latencia TX en rutas TCI y soundcard.
- Web/UI:
- corregida inicializacion startup/fetch de dashboard remota.
- corregida apertura `Vista -> Display a cascata` (waterfall).
- restaurado hint de password web localizado con minimo 12 caracteres.
- Integracion UDP/logger:
- corregido payload decode FT2 por UDP: loggers externos (RumLog y similares) ya no pierden el primer caracter del callsign.
- Alineacion release/build:
- `FORK_RELEASE_VERSION` actualizado a `v1.4.5`.
- defaults de workflow y fallback templates de release alineados a `v1.4.5`.

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
