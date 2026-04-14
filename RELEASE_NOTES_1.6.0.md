# Release Notes / Note di Rilascio / Notas de Lanzamiento - Fork 1.6.0

Scope: update cycle from `1.5.9` to `1.6.0`.

## English

`1.6.0` focuses on pushing the legacy JT runtime further into native C++, fixing replies to special/non-standard callsigns, hardening GCC/Linux portability, and extending the release pipeline to Linux `aarch64` AppImage in addition to the existing macOS and Linux `x86_64` targets.

### Detailed Changes (`1.5.9 -> 1.6.0`)

- promoted the active JT65 runtime path to native C++, including decoder orchestration, JT65 DSP/IO helpers, and removal of the old JT65 Fortran active-path sources from the build.
- expanded native JT9 fast and wide decoder building blocks plus shared legacy DSP/IO helpers and compare/regression tooling for the remaining legacy JT migration work.
- fixed replies to non-standard or special-event callsigns that were incorrectly rejected with `*** bad message ***`.
- added Linux `aarch64` AppImage release support using a Debian Trixie ARM64 build path and a dedicated GitHub Actions ARM runner job.
- updated `build-arm.sh` so it is version-aware, CI-friendly, and excludes `build-arm-output` from source staging.
- permanently excluded `build-arm-output/` from git tracking.
- fixed the GCC 14 false-positive `stringop-overflow` issue in `LegacyDspIoHelpers.cpp` without breaking macOS Clang builds.
- fixed GCC/libstdc++ portability regressions in `jt9_wide_stage_compare.cpp` and `legacy_wsprio_compare.cpp`.
- fixed ADIF/QRZ upload failures caused by exporting a free-text `operator` field that QRZ interpreted as `my_call`; `operator` is now written only when it is a valid distinct callsign.
- fixed decode-window double-click behavior so signoff/control tokens (`RR73`, `73`, `RRR`, `R`, `TU`, `OOO`) and Maidenhead locators no longer trigger TX as if they were a station callsign.
- aligned local version metadata, workflow defaults, readmes, docs, changelog, release notes, package description, and GitHub release body to `1.6.0`.

### Release Targets

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey *(best effort / experimental)*
- Linux x86_64 AppImage
- Linux aarch64 AppImage *(Debian Trixie baseline)*

### Linux Minimum Requirements

Hardware:

- `x86_64` CPU with SSE2, or `aarch64` / ARM64 64-bit CPU
- dual-core 2.0 GHz or better, or equivalent modern ARM64 SoC
- 4 GB RAM minimum (8 GB recommended)
- 500 MB free disk space
- audio/CAT/serial/USB hardware suitable for weak-signal operation

Software:

- Qt5-capable X11 or Wayland desktop session
- Linux `x86_64` AppImage: `glibc >= 2.35`
- Linux `aarch64` AppImage: `glibc >= 2.38` *(Debian Trixie baseline)*
- `libfuse2` / FUSE2 for direct AppImage mounting
- ALSA, PulseAudio, or PipeWire
- serial/USB permissions as required by CAT and radio hardware

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

`1.6.0` e' focalizzata nel portare ulteriormente il runtime JT legacy verso il C++ nativo, nel correggere le risposte verso callsign speciali/non standard, nel rafforzare la portabilita' GCC/Linux e nell'estendere la pipeline release anche alla AppImage Linux `aarch64`, oltre ai target macOS e Linux `x86_64` gia' esistenti.

### Modifiche Dettagliate (`1.5.9 -> 1.6.0`)

- promosso a C++ nativo il path runtime attivo JT65, inclusi orchestrazione decode, helper DSP/IO JT65 e rimozione dal build dei vecchi sorgenti Fortran JT65 del path attivo.
- estesi i blocchi nativi decoder JT9 fast e wide, gli helper legacy DSP/IO condivisi e gli strumenti compare/regression per il lavoro residuo di migrazione JT legacy.
- corrette le risposte verso callsign non standard o special-event che venivano rigettate con `*** bad message ***`.
- aggiunto il supporto release Linux AppImage `aarch64` tramite build path ARM64 basato su Debian Trixie e job dedicato GitHub Actions ARM runner.
- aggiornato `build-arm.sh` in modo che sia sensibile alla versione, adatto alla CI ed escluda `build-arm-output` dallo staging dei sorgenti.
- esclusa in modo permanente da git la cartella `build-arm-output/`.
- corretto il falso positivo GCC 14 `stringop-overflow` in `LegacyDspIoHelpers.cpp` senza rompere le build macOS Clang.
- corrette le regressioni di portabilita' GCC/libstdc++ in `jt9_wide_stage_compare.cpp` e `legacy_wsprio_compare.cpp`.
- corretti i fallimenti upload ADIF/QRZ causati dall'esportazione di un campo `operator` testuale che QRZ interpretava come `my_call`; `operator` viene ora scritto solo se e' un nominativo valido e distinto.
- corretto il comportamento del doppio click nelle finestre decode: token di signoff/controllo (`RR73`, `73`, `RRR`, `R`, `TU`, `OOO`) e locator Maidenhead non mandano piu' in TX come se fossero il callsign di una stazione.
- allineati a `1.6.0` metadati versione locali, default workflow, readme, documentazione, changelog, note release, package description e body GitHub.

### Target Release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey *(best effort / sperimentale)*
- Linux x86_64 AppImage
- Linux aarch64 AppImage *(baseline Debian Trixie)*

### Requisiti Minimi Linux

Hardware:

- CPU `x86_64` con SSE2, oppure CPU `aarch64` / ARM64 64-bit
- dual-core 2.0 GHz o meglio, oppure SoC ARM64 moderno equivalente
- minimo 4 GB RAM (8 GB raccomandati)
- 500 MB liberi su disco
- hardware audio/CAT/seriale/USB adatto al weak-signal

Software:

- sessione desktop X11 o Wayland capace di eseguire AppImage Qt5
- AppImage Linux `x86_64`: `glibc >= 2.35`
- AppImage Linux `aarch64`: `glibc >= 2.38` *(baseline Debian Trixie)*
- `libfuse2` / FUSE2 per montare direttamente l'AppImage
- ALSA, PulseAudio o PipeWire
- permessi seriale/USB richiesti dall'hardware CAT/radio

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

`1.6.0` se centra en llevar aun mas el runtime JT legacy hacia C++ nativo, corregir las respuestas a indicativos especiales/no estandar, reforzar la portabilidad GCC/Linux y ampliar la pipeline release tambien a la AppImage Linux `aarch64`, ademas de los targets macOS y Linux `x86_64` ya existentes.

### Cambios Detallados (`1.5.9 -> 1.6.0`)

- promovido a C++ nativo el camino runtime activo JT65, incluyendo orquestacion decode, helpers DSP/IO JT65 y eliminacion del build de las viejas fuentes Fortran JT65 del camino activo.
- extendidos los bloques nativos decoder JT9 fast y wide, los helpers legacy DSP/IO compartidos y las herramientas compare/regression para el trabajo restante de migracion JT legacy.
- corregidas las respuestas a indicativos no estandar o special-event que se rechazaban con `*** bad message ***`.
- anadido soporte release Linux AppImage `aarch64` mediante camino de build ARM64 basado en Debian Trixie y job dedicado GitHub Actions ARM runner.
- actualizado `build-arm.sh` para que sea sensible a la version, apto para CI y excluya `build-arm-output` del staging de fuentes.
- excluida permanentemente de git la carpeta `build-arm-output/`.
- corregido el falso positivo GCC 14 `stringop-overflow` en `LegacyDspIoHelpers.cpp` sin romper builds macOS Clang.
- corregidas las regresiones de portabilidad GCC/libstdc++ en `jt9_wide_stage_compare.cpp` y `legacy_wsprio_compare.cpp`.
- corregidos los fallos de subida ADIF/QRZ causados por exportar un campo `operator` de texto libre que QRZ interpretaba como `my_call`; `operator` ahora solo se escribe si es un indicativo valido y distinto.
- corregido el comportamiento del doble click en las ventanas de decode: tokens de cierre/control (`RR73`, `73`, `RRR`, `R`, `TU`, `OOO`) y localizadores Maidenhead ya no disparan TX como si fueran el indicativo de una estacion.
- alineados con `1.6.0` los metadatos locales de version, defaults de workflow, readmes, documentacion, changelog, notas release, package description y body GitHub.

### Targets Release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey *(best effort / experimental)*
- Linux x86_64 AppImage
- Linux aarch64 AppImage *(baseline Debian Trixie)*

### Requisitos Minimos Linux

Hardware:

- CPU `x86_64` con SSE2, o CPU `aarch64` / ARM64 64-bit
- dual-core 2.0 GHz o mejor, o SoC ARM64 moderno equivalente
- minimo 4 GB RAM (8 GB recomendados)
- 500 MB libres en disco
- hardware de audio/CAT/serie/USB adecuado para weak-signal

Software:

- sesion de escritorio X11 o Wayland capaz de ejecutar AppImage Qt5
- AppImage Linux `x86_64`: `glibc >= 2.35`
- AppImage Linux `aarch64`: `glibc >= 2.38` *(baseline Debian Trixie)*
- `libfuse2` / FUSE2 para montar directamente la AppImage
- ALSA, PulseAudio o PipeWire
- permisos serie/USB requeridos por el hardware CAT/radio

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
