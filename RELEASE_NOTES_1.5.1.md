# Release Notes / Note di Rilascio / Notas de Lanzamiento - Fork 1.5.1

Date: 2026-03-19  
Scope: update cycle from `1.5.0` to `1.5.1`.

## English

### Summary

`1.5.1` focuses on practical on-air stability after `1.5.0`: direct-caller handling, late-signoff recovery, missed autolog paths, CQ/map cleanup, and an internal updater that can point users directly to the correct release asset.

### Detailed Changes (`1.5.0 -> 1.5.1`)

- Internal updater:
- added startup checks against the GitHub Releases API.
- added `Help > Check for updates...`.
- release prompts now choose the best matching macOS/Linux asset directly, with fallback to the release page only when no safe match is found.
- QSO sequencing and final-message correctness:
- fixed late `73/RR73` cases after local signoff where the app could repeat `73/RR73` instead of logging the completed QSO.
- fixed abandoned/timeout QSO paths so delayed final acknowledgements from the active partner can still recover and log correctly.
- fixed final-ack forwarding after own signoff so valid remote `73/RR73` messages are not dropped before `processMessage()`.
- AutoCQ and caller management:
- fixed stale DX-partner reuse when a new direct caller arrives after returning to CQ.
- fixed CQ-to-direct-reply transitions so a valid direct caller now arms `Tx2` immediately instead of leaving the app in CQ.
- fixed first direct FT2 replies using stale report values from previous QSOs instead of the current caller SNR.
- extended recent-work and abandoned-partner suppression so late/deferred closures do not immediately retrigger duplicate work on the same station.
- Map / identity / telemetry:
- fixed the live world map from drawing an active outgoing link while the station is transmitting plain CQ.
- aligned the PSK Reporter program identifier to the exact title-bar string shown by the application.
- Diagnostics and release process:
- expanded `debug.txt` tracing for direct-caller arming, stale-partner cleanup, late-signoff recovery, and updater asset selection.
- local UI version, release workflows, release notes, and GitHub publication defaults are aligned to semantic version `1.5.1`.

### Release Artifacts

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/experimental): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

### Linux Minimum Requirements

Hardware:

- Architecture: `x86_64` (64-bit)
- CPU: dual-core 2.0 GHz or better
- RAM: 4 GB minimum (8 GB recommended)
- Storage: at least 500 MB free
- Display: 1280x800 or better recommended
- Station hardware: CAT/audio/PTT equipment according to radio setup

Software:

- Linux with `glibc >= 2.35`
- `libfuse2` / FUSE2 support
- ALSA, PulseAudio, or PipeWire audio stack
- desktop environment capable of running Qt5 AppImages
- network access recommended for NTP, DX Cluster, PSK Reporter, and online features

### If macOS blocks startup

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

### Linux AppImage recommendation

To avoid issues caused by the AppImage read-only filesystem, it is recommended to start Decodium by extracting the AppImage first and then running the program from the extracted directory.

Run the following commands in a terminal:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Italiano

### Sintesi

`1.5.1` e' focalizzata sulla stabilita' pratica on-air dopo `1.5.0`: gestione caller diretti, recovery dei signoff tardivi, path di autolog mancati, pulizia CQ/mappa e updater interno capace di puntare direttamente all'asset corretto.

### Modifiche Dettagliate (`1.5.0 -> 1.5.1`)

- Updater interno:
- aggiunti controlli all'avvio contro la GitHub Releases API.
- aggiunta voce `Help > Check for updates...`.
- i prompt release scelgono ora direttamente l'asset macOS/Linux piu' adatto, con fallback alla pagina release solo quando non esiste un match sicuro.
- Sequenziamento QSO e correttezza messaggi finali:
- corretti i casi di `73/RR73` tardivi dopo il signoff locale, dove l'app poteva ripetere `73/RR73` invece di loggare il QSO chiuso.
- corretti i path di QSO abbandonati/per-timeout cosi' i final ack ritardati del partner attivo possono ancora essere recuperati e loggati.
- corretto l'inoltro del final ack dopo il proprio signoff, cosi' i validi `73/RR73` remoti non vengono scartati prima di `processMessage()`.
- AutoCQ e gestione caller:
- corretto il riuso di partner DX stantii quando arriva un nuovo caller diretto dopo il ritorno a CQ.
- corretta la transizione CQ->reply diretta, cosi' un caller diretto valido arma ora subito `Tx2` invece di lasciare l'app in CQ.
- corretto il primo reply FT2 diretto che poteva usare report stantii del QSO precedente invece dell'SNR del caller corrente.
- esteso il blocco recent-work e abandoned-partner cosi' chiusure tardive o differite non rilanciano subito un nuovo QSO duplicato sullo stesso nominativo.
- Mappa / identita' / telemetria:
- corretta la live world map che disegnava un link uscente attivo mentre la stazione trasmetteva un semplice CQ.
- allineato l'identificativo programma inviato a PSK Reporter esattamente alla stringa mostrata nella barra del titolo.
- Diagnostica e processo release:
- esteso `debug.txt` con tracing piu' ricco per armo caller diretto, cleanup partner stantio, recovery late-signoff e scelta asset updater.
- versione UI locale, workflow release, note di rilascio e default di pubblicazione GitHub allineati alla semver `1.5.1`.

### Artifact Release

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/sperimentale): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

### Requisiti Minimi Linux

Hardware:

- Architettura: `x86_64` (64 bit)
- CPU: dual-core 2.0 GHz o superiore
- RAM: minimo 4 GB (consigliati 8 GB)
- Spazio disco: almeno 500 MB liberi
- Display: 1280x800 o superiore consigliato
- Hardware stazione: CAT/audio/PTT secondo il setup radio

Software:

- Linux con `glibc >= 2.35`
- supporto `libfuse2` / FUSE2
- stack audio ALSA, PulseAudio o PipeWire
- ambiente desktop capace di eseguire AppImage Qt5
- accesso rete consigliato per NTP, DX Cluster, PSK Reporter e feature online

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

`1.5.1` se centra en la estabilidad practica on-air tras `1.5.0`: manejo de callers directos, recovery de signoff tardio, paths de autolog perdidos, limpieza CQ/mapa y updater interno capaz de apuntar directamente al asset correcto.

### Cambios Detallados (`1.5.0 -> 1.5.1`)

- Updater interno:
- anadidos checks al arrancar contra la GitHub Releases API.
- anadida accion `Help > Check for updates...`.
- los prompts release eligen ahora directamente el asset macOS/Linux mas adecuado, con fallback a la pagina release solo cuando no hay un match seguro.
- Secuenciacion QSO y correccion de mensajes finales:
- corregidos casos de `73/RR73` tardios tras el signoff local, donde la app podia repetir `73/RR73` en lugar de logear el QSO cerrado.
- corregidos los paths de QSO abandonados/por-timeout para que los final ack retrasados del partner activo todavia puedan recuperarse y logearse.
- corregido el forwarding del final ack tras el propio signoff para que `73/RR73` remotos validos no se descarten antes de `processMessage()`.
- AutoCQ y gestion caller:
- corregido el reuso de partner DX obsoleto cuando llega un nuevo caller directo despues de volver a CQ.
- corregida la transicion CQ->reply directa para que un caller directo valido arme ahora inmediatamente `Tx2` en vez de dejar la app en CQ.
- corregido el primer reply FT2 directo que podia usar reportes obsoletos del QSO anterior en lugar del SNR del caller actual.
- ampliada la supresion recent-work y abandoned-partner para que cierres tardios o diferidos no relancen enseguida un QSO duplicado con el mismo callsign.
- Mapa / identidad / telemetria:
- corregido el live world map que dibujaba un enlace saliente activo mientras la estacion transmitia un CQ simple.
- alineado el identificador de programa enviado a PSK Reporter exactamente con la cadena mostrada en la barra de titulo.
- Diagnostica y proceso release:
- ampliado `debug.txt` con mas trazas para armado de caller directo, limpieza de partner obsoleto, recovery late-signoff y eleccion de asset updater.
- version UI local, workflows release, notas de lanzamiento y defaults de publicacion GitHub alineados con la semver `1.5.1`.

### Artefactos de Release

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/experimental): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

### Requisitos Minimos Linux

Hardware:

- Arquitectura: `x86_64` (64 bits)
- CPU: dual-core 2.0 GHz o superior
- RAM: 4 GB minimo (8 GB recomendado)
- Espacio: al menos 500 MB libres
- Pantalla: 1280x800 o superior recomendada
- Hardware de estacion: CAT/audio/PTT segun el setup de radio

Software:

- Linux con `glibc >= 2.35`
- soporte `libfuse2` / FUSE2
- stack audio ALSA, PulseAudio o PipeWire
- entorno de escritorio capaz de ejecutar AppImage Qt5
- acceso a red recomendado para NTP, DX Cluster, PSK Reporter y funciones online

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
