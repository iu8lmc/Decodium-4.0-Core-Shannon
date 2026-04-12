# Release Notes / Note di Rilascio / Notas de Lanzamiento - Fork v1.4.9

Date: 2026-03-17  
Scope: update cycle from `v1.4.8` to `v1.4.9`.

## English

### Summary

`v1.4.9` focuses on FT2 decoder gains under fading conditions, FT2 async visual/runtime refinements, FT2 startup and AutoCQ correctness, Linux astronomical data portability, and UI/language usability improvements.

### Detailed Changes (`v1.4.8 -> v1.4.9`)

- FT2 decoder and channel handling:
- raised FT2 triggered-decode LLR scale from `2.83` to `3.2`.
- normalized all three FT2 LLR branches before LDPC decode.
- added adaptive FT2 channel estimation with MMSE-equalized bit metrics for fading HF paths.
- introduced `lib/ft2/ft2_channel_est.f90` and linked it into the FT2 Fortran build.
- FT2 async/runtime feedback:
- added a dedicated FT2 async visualizer widget in the main UI.
- FT2 async S-meter is now fed from the real FT2 decode path instead of stale UI state.
- reduced FT2 async polling from `750 ms` to `100 ms`.
- deferred `postDecode()` and `write_all()` fan-out to keep the async decode path responsive.
- preserved FT2 async fixed-column formatting and timestamps instead of trimming away leading spacing.
- FT2 startup / operator correctness:
- startup no longer hard-forces `FT2`; saved mode/frequency is respected again.
- FT2 now handles an immediate first directed reply while CQ is running, avoiding missed first callers.
- fresh AutoCQ partner acquisition now clears stale retry/miss counters before a new QSO starts, preventing premature partner switches.
- UI and usability:
- added a `Language` menu to the main menu bar.
- selected UI language is stored in settings and reused on next startup when no CLI language override is passed.
- DX Cluster columns are now manually resizable and header widths are persisted between sessions.
- Linux / astronomical data:
- `JPLEPH` lookup now covers AppImage paths, Linux share paths, current working directory, and out-of-source builds through `CMAKE_SOURCE_DIR`.
- Linux AppImage packaging now includes `contrib/Ephemeris/JPLEPH` in `usr/share/wsjtx`.
- astronomical error reporting now shows the searched paths for faster diagnostics.
- Release/build:
- release defaults, notes templates, and workflow version strings aligned to `v1.4.9`.

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

`v1.4.9` e' focalizzata su guadagni decoder FT2 in fading, rifiniture visual/runtime FT2 async, correttezza startup e AutoCQ FT2, portabilita' Linux dei dati astronomici e miglioramenti di usabilita' UI/lingua.

### Modifiche Dettagliate (`v1.4.8 -> v1.4.9`)

- Decoder FT2 e gestione canale:
- scalatura LLR del triggered decode FT2 aumentata da `2.83` a `3.2`.
- normalizzazione di tutti e tre i rami LLR FT2 prima del decode LDPC.
- aggiunta channel estimation adattiva FT2 con metriche MMSE-equalized per percorsi HF con fading.
- introdotto `lib/ft2/ft2_channel_est.f90` e collegato al build Fortran FT2.
- Feedback async/runtime FT2:
- aggiunto un visualizzatore dedicato FT2 async nella UI principale.
- l'S-meter async FT2 viene ora alimentato dal path reale di decode FT2 invece che da stato UI stantio.
- polling async FT2 ridotto da `750 ms` a `100 ms`.
- differito il fan-out `postDecode()` e `write_all()` per mantenere reattivo il path async.
- preservata la formattazione FT2 async a colonne fisse con timestamp, senza trim che rimuoveva gli spazi iniziali.
- Correttezza startup / operatore FT2:
- l'avvio non forza piu' `FT2`; modo/frequenza salvati vengono rispettati di nuovo.
- FT2 ora gestisce una prima risposta diretta immediata mentre CQ e' attivo, evitando di perdere il primo caller.
- l'aggancio di un nuovo partner AutoCQ ora azzera contatori retry/miss sporchi prima che parta un nuovo QSO, evitando cambi partner prematuri.
- UI e usabilita':
- aggiunto un menu `Language` nella barra menu principale.
- la lingua UI selezionata viene salvata nelle impostazioni e riutilizzata al riavvio quando non passi un override CLI.
- le colonne del DX Cluster sono ora ridimensionabili manualmente e le larghezze header vengono persistite tra sessioni.
- Linux / dati astronomici:
- il lookup di `JPLEPH` copre ora path AppImage, path share Linux, working directory corrente e build out-of-source tramite `CMAKE_SOURCE_DIR`.
- il packaging Linux AppImage include ora `contrib/Ephemeris/JPLEPH` in `usr/share/wsjtx`.
- il reporting degli errori astronomici mostra ora i path cercati per una diagnosi piu' rapida.
- Release/build:
- default release, template note e stringhe workflow allineati a `v1.4.9`.

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

`v1.4.9` se centra en ganancias del decoder FT2 en fading, refinamientos visual/runtime FT2 async, correccion startup y AutoCQ FT2, portabilidad Linux de datos astronomicos y mejoras de usabilidad UI/idioma.

### Cambios Detallados (`v1.4.8 -> v1.4.9`)

- Decoder FT2 y gestion de canal:
- escalado LLR del triggered decode FT2 aumentado de `2.83` a `3.2`.
- normalizacion de las tres ramas LLR FT2 antes del decode LDPC.
- anadida channel estimation adaptativa FT2 con metricas MMSE-equalized para trayectos HF con fading.
- introducido `lib/ft2/ft2_channel_est.f90` y enlazado al build Fortran FT2.
- Feedback async/runtime FT2:
- anadido un visualizador dedicado FT2 async en la UI principal.
- el S-meter async FT2 se alimenta ahora desde el path real de decode FT2 en lugar de un estado UI obsoleto.
- polling async FT2 reducido de `750 ms` a `100 ms`.
- diferido el fan-out `postDecode()` y `write_all()` para mantener reactivo el path async.
- preservada la formateacion FT2 async de columnas fijas con timestamp, sin trim que eliminaba espacios iniciales.
- Correccion startup / operador FT2:
- el arranque ya no fuerza `FT2`; se respetan de nuevo modo/frecuencia guardados.
- FT2 ahora gestiona una primera respuesta dirigida inmediata mientras CQ esta activo, evitando perder el primer caller.
- la adquisicion de un nuevo partner AutoCQ ahora limpia contadores retry/miss sucios antes de iniciar un nuevo QSO, evitando cambios prematuros de partner.
- UI y usabilidad:
- anadido un menu `Language` en la barra principal.
- el idioma UI seleccionado se guarda en settings y se reutiliza al reinicio cuando no se pasa override CLI.
- las columnas del DX Cluster ahora son redimensionables manualmente y las anchuras del header se persisten entre sesiones.
- Linux / datos astronomicos:
- el lookup de `JPLEPH` cubre ahora paths AppImage, paths share Linux, directorio de trabajo actual y builds out-of-source mediante `CMAKE_SOURCE_DIR`.
- el packaging Linux AppImage incluye ahora `contrib/Ephemeris/JPLEPH` en `usr/share/wsjtx`.
- el reporte de errores astronomicos muestra ahora los paths buscados para un diagnostico mas rapido.
- Release/build:
- defaults release, plantillas de notas y strings workflow alineados a `v1.4.9`.

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
