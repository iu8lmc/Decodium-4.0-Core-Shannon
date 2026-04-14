# Release Notes / Note di Rilascio / Notas de Lanzamiento - Fork 1.5.9

Scope: update cycle from `1.5.8` to `1.5.9`.

## English

`1.5.9` focuses on making Linux FT2/FT4 transmit timing deterministic on older Ubuntu hosts, removing the remaining practical causes of "carrier first, payload later" behavior, hardening macOS shutdown/UI stability, and refreshing FT2 launcher branding.

### Detailed Changes (`1.5.8 -> 1.5.9`)

- fixed intermittent Linux FT2/FT4 cases where the payload started much later than PTT, sometimes near the end of the transmit slot.
- removed the standard FT2/FT4 Linux TX path from unnecessary global Fortran runtime mutex contention so decoder activity does not delay native C++ waveform transmission.
- added immediate post-waveform TX start and CAT/PTT fallback start on Linux when rig-state confirmation is late.
- reduced Linux TX audio queue defaults, selected a lower-latency audio category, and set FT2 Linux startup alignment to zero extra delay and zero extra lead-in on the standard waveform path.
- changed Linux FT2/FT4 stop handling to follow real modulator/audio completion instead of theoretical slot timing only.
- limited FT2/FT4 debug waveform dump writes to sessions where debug logging is enabled, reducing normal transmit-path disk I/O.
- fixed a macOS close-time crash caused by status-bar/member-widget ownership and destruction ordering in `MainWindow`.
- fixed the `Band Hopping` UI regression where `QSOs to upload` inherited the red warning styling meant only for the `Band Hopping` button.
- refreshed the FT2 application icon set for macOS and Linux release artifacts.
- aligned local version metadata, workflow defaults, readmes, docs, changelog, release notes, and GitHub release body to `1.5.9`.

### Release Targets

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey *(best effort / experimental)*
- Linux x86_64 AppImage

### Linux Minimum Requirements

Hardware:

- `x86_64` CPU with SSE2
- dual-core 2.0 GHz or better
- 4 GB RAM minimum
- 500 MB free disk space
- audio/CAT/serial/USB hardware suitable for weak-signal operation

Software:

- `glibc >= 2.35`
- `libfuse2` if mounting the AppImage directly
- ALSA, PulseAudio, or PipeWire
- X11 or Wayland desktop session capable of running Qt5 AppImages

### Startup Guidance

macOS quarantine workaround:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

To avoid issues caused by the AppImage read-only filesystem, it is recommended to start Decodium by extracting the AppImage first and then running the program from the extracted directory.

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Italiano

`1.5.9` e' focalizzata nel rendere deterministico il timing TX Linux FT2/FT4 sui sistemi Ubuntu piu' vecchi, eliminando le cause pratiche residue del comportamento "portante prima, payload dopo", rafforzando stabilita' macOS in chiusura/UI e aggiornando il branding dell'icona FT2.

### Modifiche Dettagliate (`1.5.8 -> 1.5.9`)

- corretti i casi intermittenti Linux FT2/FT4 in cui il payload partiva molto dopo il PTT, a volte quasi alla fine dello slot.
- rimosso il path TX Linux standard FT2/FT4 dal contenzioso non necessario con il mutex globale del runtime Fortran, cosi' l'attivita' di decode non ritarda piu' la trasmissione waveform nativa C++.
- aggiunti start immediato post-waveform e fallback CAT/PTT su Linux quando la conferma dello stato rig arriva in ritardo.
- ridotte le code audio TX Linux, selezionata una categoria audio a bassa latenza, e impostato FT2 Linux a zero ritardo extra e zero lead-in extra sul path waveform standard.
- cambiata la logica di stop Linux FT2/FT4 per seguire la fine reale di modulator/audio e non solo il timing teorico dello slot.
- limitata la scrittura dei dump waveform FT2/FT4 ai soli casi con debug log attivo, riducendo l'I/O su disco nel path normale di trasmissione.
- corretto un crash macOS in chiusura causato dall'ordine di ownership e distruzione dei widget status-bar/member in `MainWindow`.
- corretta la regressione UI `Band Hopping` per cui `QSOs to upload` ereditava il rosso di warning previsto solo per il pulsante `Band Hopping`.
- aggiornata la nuova icona FT2 per gli artifact release macOS e Linux.
- allineati a `1.5.9` metadati versione locali, default workflow, readme, documentazione, changelog, note release e body GitHub.

### Target Release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey *(best effort / sperimentale)*
- Linux x86_64 AppImage

### Requisiti Minimi Linux

Hardware:

- CPU `x86_64` con SSE2
- dual-core 2.0 GHz o meglio
- minimo 4 GB RAM
- 500 MB liberi su disco
- hardware audio/CAT/seriale/USB adatto al weak-signal

Software:

- `glibc >= 2.35`
- `libfuse2` se si vuole montare direttamente l'AppImage
- ALSA, PulseAudio o PipeWire
- sessione desktop X11 o Wayland capace di eseguire AppImage Qt5

### Guida Avvio

Workaround quarantena macOS:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l'AppImage e poi eseguendo il programma dalla cartella estratta.

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Espanol

`1.5.9` se centra en hacer deterministico el timing TX Linux FT2/FT4 en sistemas Ubuntu antiguos, eliminando las causas practicas restantes del comportamiento "portadora primero, payload despues", reforzando la estabilidad macOS al cerrar/UI y actualizando el branding del icono FT2.

### Cambios Detallados (`1.5.8 -> 1.5.9`)

- corregidos los casos intermitentes Linux FT2/FT4 donde el payload arrancaba mucho despues del PTT, a veces casi al final del slot.
- eliminado el camino TX Linux estandar FT2/FT4 del bloqueo no necesario con el mutex global del runtime Fortran, de modo que la actividad de decode ya no retrasa la transmision waveform nativa C++.
- anadidos arranque inmediato post-waveform y fallback CAT/PTT en Linux cuando la confirmacion de estado del equipo llega tarde.
- reducidas las colas de audio TX Linux, seleccionada una categoria de audio de baja latencia, y ajustado FT2 Linux a cero retraso extra y cero lead-in extra en el camino waveform estandar.
- cambiada la logica de stop Linux FT2/FT4 para seguir la finalizacion real de modulator/audio y no solo el timing teorico del slot.
- limitada la escritura de dumps waveform FT2/FT4 a sesiones con debug log activo, reduciendo el I/O de disco en la ruta normal de transmision.
- corregido un crash macOS al cerrar causado por el orden de ownership y destruccion de widgets status-bar/member en `MainWindow`.
- corregida la regresion UI `Band Hopping` donde `QSOs to upload` heredaba el rojo de aviso previsto solo para el boton `Band Hopping`.
- actualizada la nueva iconografia FT2 para los artefactos release macOS y Linux.
- alineados con `1.5.9` los metadatos locales de version, defaults de workflow, readmes, documentacion, changelog, notas release y body GitHub.

### Targets Release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey *(best effort / experimental)*
- Linux x86_64 AppImage

### Requisitos Minimos Linux

Hardware:

- CPU `x86_64` con SSE2
- dual-core 2.0 GHz o superior
- minimo 4 GB RAM
- 500 MB libres en disco
- hardware de audio/CAT/serie/USB adecuado para weak-signal

Software:

- `glibc >= 2.35`
- `libfuse2` si se quiere montar la AppImage directamente
- ALSA, PulseAudio o PipeWire
- sesion de escritorio X11 o Wayland capaz de ejecutar AppImages Qt5

### Guia de Arranque

Workaround de cuarentena macOS:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Para evitar problemas debidos al sistema de archivos de solo lectura de las AppImage, se recomienda iniciar Decodium extrayendo primero la AppImage y ejecutando despues el programa desde la carpeta extraida.

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```
