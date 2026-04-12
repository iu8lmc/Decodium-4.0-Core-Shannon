# Release Notes / Note di Rilascio / Notas de Lanzamiento - Fork v1.4.7

Date: 2026-03-15  
Scope: update cycle from `v1.4.6` to `v1.4.7`.

## English

### Summary

`v1.4.7` focuses on FT2 runtime/decode hardening, AutoCQ partner continuity, DxSpider-based AutoSpot/DX Cluster integration, and release alignment.

### Detailed Changes (`v1.4.6 -> v1.4.7`)

- FT2 runtime/decode:
- increased FT2 async Tx guard from `100 ms` to `300 ms`.
- added FT2 false-decode rejection based on `cty.dat` prefix validation.
- rewrote FT2 `TΔ` display using structured parsing rather than fixed-column overwrite.
- preserved FT2 decode-column alignment when `~` appears in the flow.
- AutoCQ/QSO correctness:
- stronger active-partner lock prevents station takeover during an ongoing QSO.
- missed-period handling now matches the real `5` retry budget.
- stale missed-period state is cleared when a valid caller/partner reply advances the QSO.
- after confirmed partner `73`, AutoCQ returns to CQ instead of transmitting an extra `RR73`.
- AutoSpot / DX Cluster:
- new Settings controls for AutoSpot host, port, and enable state.
- DX Cluster window migrated from the fixed HamQTH view to a configurable DxSpider telnet feed.
- improved telnet IAC handling, login/prompt detection, quiet `UNSET/DX` queries, and explicit submit/verify tracing in `autospot.log`.
- cluster feed and AutoSpot submit now share the configured endpoint, while the view can use a separate login when required.
- Web dashboard / remote control:
- added remote AutoSpot state exposure and dashboard AutoSpot toggle.
- improved command-status reporting, auth failure handling, and current-mode preset save/apply behavior.
- reduced Rx/Tx frequency field bounce with pending-frequency guards on the web panel.
- Desktop/runtime fixes:
- `Astronomical Data` menu item now unchecks correctly when the window is closed with the titlebar `X`.
- Linux settings dialog pages remain scrollable on small or HiDPI desktops, with bottom action buttons preserved.
- Release/build alignment:
- `FORK_RELEASE_VERSION` updated to `v1.4.7`.
- workflow defaults and release-note fallbacks aligned to `v1.4.7`.

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

To avoid issues caused by the AppImage read-only filesystem, start Decodium by extracting the AppImage first and then running the program from the extracted directory.

Run the following commands in a terminal:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Italiano

### Sintesi

`v1.4.7` e' focalizzata su hardening runtime/decode FT2, continuita' partner in AutoCQ, integrazione AutoSpot/DX Cluster basata su DxSpider e allineamento release.

### Modifiche Dettagliate (`v1.4.6 -> v1.4.7`)

- Runtime/decode FT2:
- guardia async Tx FT2 aumentata da `100 ms` a `300 ms`.
- aggiunto filtro false decode FT2 basato su validazione prefissi `cty.dat`.
- riscritta visualizzazione FT2 `TΔ` con parsing strutturato invece di overwrite a colonne fisse.
- preservato allineamento colonne decode FT2 quando nel flow compare `~`.
- Correttezza AutoCQ/QSO:
- lock partner attivo piu' rigoroso per impedire takeover di altre stazioni durante un QSO in corso.
- gestione periodi mancati allineata al budget reale di `5` retry.
- stato stale dei periodi mancati azzerato quando una risposta valida del caller/partner fa avanzare il QSO.
- dopo `73` confermato dal partner, AutoCQ torna a CQ invece di ritrasmettere un `RR73` extra.
- AutoSpot / DX Cluster:
- nuovi controlli impostazioni per host, porta e stato enable AutoSpot.
- finestra DX Cluster migrata dal feed HamQTH fisso a feed telnet DxSpider configurabile.
- migliorata gestione IAC telnet, riconoscimento login/prompt, query silenziose `UNSET/DX` e tracing esplicito submit/verify in `autospot.log`.
- feed cluster e submit AutoSpot ora condividono l'endpoint configurato, con possibilita' di login separato per la sola vista.
- Dashboard web / controllo remoto:
- aggiunta esposizione stato AutoSpot e relativo toggle nella dashboard.
- migliorato reporting stato comandi, gestione errori auth e comportamento save/apply dei preset del modo corrente.
- ridotto rimbalzo dei campi frequenza Rx/Tx sul pannello web con guardie su frequenze pending.
- Correzioni desktop/runtime:
- la voce menu `Dati Astronomici` si deseleziona correttamente quando la finestra viene chiusa con la `X` della titlebar.
- le pagine del dialogo impostazioni Linux restano scrollabili su desktop piccoli o HiDPI, mantenendo visibili i pulsanti finali.
- Allineamento release/build:
- `FORK_RELEASE_VERSION` aggiornato a `v1.4.7`.
- default workflow e fallback note release allineati a `v1.4.7`.

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

`v1.4.7` se centra en hardening runtime/decode FT2, continuidad de pareja en AutoCQ, integracion AutoSpot/DX Cluster basada en DxSpider y alineacion release.

### Cambios Detallados (`v1.4.6 -> v1.4.7`)

- Runtime/decode FT2:
- guardia async Tx FT2 aumentada de `100 ms` a `300 ms`.
- anadido filtro false decode FT2 basado en validacion de prefijos `cty.dat`.
- reescrita la visualizacion FT2 `TΔ` con parsing estructurado en lugar de overwrite de columnas fijas.
- preservada la alineacion de columnas decode FT2 cuando aparece `~` en el flujo.
- Correccion AutoCQ/QSO:
- lock de pareja activa mas fuerte para impedir takeover de otras estaciones durante un QSO en curso.
- manejo de periodos perdidos alineado con el presupuesto real de `5` reintentos.
- estado stale de periodos perdidos limpiado cuando una respuesta valida del caller/pareja hace avanzar el QSO.
- tras `73` confirmado por la pareja, AutoCQ vuelve a CQ en lugar de retransmitir un `RR73` extra.
- AutoSpot / DX Cluster:
- nuevos controles de ajustes para host, puerto y estado enable de AutoSpot.
- ventana DX Cluster migrada del feed HamQTH fijo a un feed telnet DxSpider configurable.
- mejorado manejo IAC telnet, deteccion login/prompt, consultas silenciosas `UNSET/DX` y trazado explicito submit/verify en `autospot.log`.
- feed cluster y submit AutoSpot ahora comparten el endpoint configurado, con opcion de login separado para la vista si hace falta.
- Dashboard web / control remoto:
- anadida exposicion del estado AutoSpot y su toggle en la dashboard.
- mejorado reporting del estado de comandos, gestion de errores auth y comportamiento save/apply de presets del modo actual.
- reducido rebote de campos de frecuencia Rx/Tx en el panel web mediante guardas de frecuencias pendientes.
- Correcciones desktop/runtime:
- la opcion de menu `Datos Astronomicos` se desmarca correctamente al cerrar la ventana con la `X` de la titlebar.
- las paginas del dialogo de ajustes en Linux siguen siendo scrollables en escritorios pequenos o HiDPI, manteniendo visibles los botones finales.
- Alineacion release/build:
- `FORK_RELEASE_VERSION` actualizado a `v1.4.7`.
- defaults de workflow y fallback de notas release alineados a `v1.4.7`.

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

Para evitar problemas debidos al sistema de archivos de solo lectura de las AppImage, se recomienda iniciar Decodium extrayendo primero la AppImage y ejecutando despues el programa desde la carpeta extraida.

Ejecutar los siguientes comandos en la terminal:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```
