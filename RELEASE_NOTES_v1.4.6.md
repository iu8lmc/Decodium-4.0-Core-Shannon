# Release Notes / Note di Rilascio / Notas de Lanzamiento - Fork v1.4.6

Date: 2026-03-14  
Scope: update cycle from `v1.4.5` to `v1.4.6`.

## English

### Summary

`v1.4.6` focuses on AutoCQ/signoff reliability across FT modes, correct QSO-flow promotion to the Rx pane, and desktop/web control parity.

### Detailed Changes (`v1.4.5 -> v1.4.6`)

- AutoCQ and signoff flow:
- restored stable FIFO caller-queue behavior (v1.3.8 logic baseline).
- strengthened active partner lock continuity during in-progress QSO.
- generalized deferred RR73/73 retry handling across FT2, FT8, FT4, FST4, Q65, and MSK144.
- improved partner detection from normalized payload tokens in edge decode formats.
- Logging correctness:
- deferred autolog context recovery hardened after retry windows.
- reduced forced/premature logging without confirmed partner signoff.
- reduced delayed logging caused by stale pending-state carry-over.
- Decode/Rx pane continuity:
- robust decode payload extraction for variable marker spacing and optional marker position.
- reduced cases where valid partner reports/signoff remained only in Band Activity and did not follow the QSO in Rx pane.
- Remote/dashboard:
- added explicit remote command handling for Tx frequency (`set_tx_frequency`).
- web dashboard now supports:
- combined Rx+Tx set action,
- dedicated Rx and Tx set actions,
- per-mode Rx/Tx preset save/apply.
- Desktop UX/runtime:
- fixed macOS `View -> World Map` toggle behavior.
- improved splitter sizing to preserve secondary decode/map panes.
- Linux settings dialog tabs now scroll correctly on small/HiDPI desktops (OK button remains reachable).
- added startup audio stream refresh on macOS to avoid manual device reload.
- Release/build alignment:
- `FORK_RELEASE_VERSION` updated to `v1.4.6`.
- workflow defaults and release-note fallback templates aligned to `v1.4.6`.

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

`v1.4.6` e' focalizzata su affidabilita' AutoCQ/signoff multi-modo, corretta promozione flusso QSO nel pannello Frequenza Rx e parita' controlli desktop/web.

### Modifiche Dettagliate (`v1.4.5 -> v1.4.6`)

- Flusso AutoCQ e signoff:
- ripristinato comportamento FIFO stabile della coda caller (baseline logica v1.3.8).
- rafforzata continuita' lock partner attivo durante QSO in corso.
- estesa gestione deferred retry RR73/73 su FT2, FT8, FT4, FST4, Q65 e MSK144.
- migliorato riconoscimento partner da token payload normalizzati anche su formati decode edge.
- Correttezza logging:
- hardening recovery contesto deferred autolog dopo finestre retry.
- ridotti casi di logging forzato/prematuro senza conferma reale del signoff partner.
- ridotti ritardi log causati da propagazione stati pending stale.
- Continuita' decode/pannello Rx:
- estrazione payload decode robusta con marker a spaziatura variabile e marker opzionale.
- ridotti casi in cui report/signoff validi del partner restavano solo su Attivita' di Banda senza seguire il QSO su Frequenza Rx.
- Remoto/dashboard:
- aggiunta gestione comando remoto esplicito frequenza Tx (`set_tx_frequency`).
- dashboard web ora supporta:
- azione combinata set Rx+Tx,
- azioni separate set Rx e set Tx,
- preset frequenze Rx/Tx per modo con save/apply.
- UX/runtime desktop:
- corretto comportamento toggle `Vista -> World Map` su macOS.
- migliorato dimensionamento splitter per preservare pannelli decode secondario/mappa.
- tab dialogo impostazioni Linux ora scrollabili correttamente su desktop piccoli/HiDPI (pulsante OK sempre raggiungibile).
- aggiunto refresh stream audio in avvio su macOS per evitare reload manuale periferiche.
- Allineamento release/build:
- `FORK_RELEASE_VERSION` aggiornato a `v1.4.6`.
- default workflow e fallback template release allineati a `v1.4.6`.

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

`v1.4.6` se centra en fiabilidad AutoCQ/signoff en varios modos, promocion correcta del flujo QSO en panel Frecuencia Rx y paridad de controles desktop/web.

### Cambios Detallados (`v1.4.5 -> v1.4.6`)

- Flujo AutoCQ y signoff:
- restaurado comportamiento FIFO estable de cola callers (baseline logica v1.3.8).
- reforzada continuidad del lock de pareja activa durante QSO en curso.
- extendido manejo deferred de reintentos RR73/73 en FT2, FT8, FT4, FST4, Q65 y MSK144.
- mejorado reconocimiento de pareja con tokens payload normalizados en formatos decode limite.
- Correccion logging:
- hardening en recovery de contexto deferred autolog tras ventanas de reintento.
- reducidos casos de log forzado/prematuro sin confirmacion real de signoff.
- reducidos retrasos de log por arrastre de estados pending stale.
- Continuidad decode/panel Rx:
- extraccion de payload decode robusta con espaciado variable y marcador opcional.
- reducidos casos donde reportes/signoff validos quedaban solo en Actividad de Banda y no seguian el flujo QSO en Frecuencia Rx.
- Remoto/dashboard:
- anadido manejo explicito de comando remoto de frecuencia Tx (`set_tx_frequency`).
- dashboard web ahora soporta:
- accion combinada set Rx+Tx,
- acciones separadas set Rx y set Tx,
- presets Rx/Tx por modo con save/apply.
- UX/runtime desktop:
- corregido toggle `Vista -> World Map` en macOS.
- mejorado tamano splitter para preservar panel decode secundario/mapa.
- pestañas de ajustes Linux ahora con scroll correcto en escritorios pequenos/HiDPI (boton OK siempre accesible).
- anadido refresh de stream audio al inicio en macOS para evitar recarga manual de dispositivos.
- Alineacion release/build:
- `FORK_RELEASE_VERSION` actualizado a `v1.4.6`.
- defaults de workflow y fallback templates de release alineados a `v1.4.6`.

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
