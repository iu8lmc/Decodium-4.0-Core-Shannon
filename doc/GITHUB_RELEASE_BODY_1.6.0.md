# Decodium 1.6.0 (Fork 9H1SR)

## English

Release highlights (`1.5.9 -> 1.6.0`):

- promoted the active JT65 runtime path to native C++, including decoder orchestration and JT65 DSP/IO helpers.
- expanded native JT9 fast/wide building blocks and compare/regression tooling for the remaining legacy JT migration.
- fixed replies to non-standard or special-event callsigns that were incorrectly rejected with `*** bad message ***`.
- added Linux `aarch64` AppImage release support using a Debian Trixie ARM64 build path and GitHub Actions ARM runner.
- made `build-arm.sh` version-aware, CI-friendly, and permanently excluded `build-arm-output/` from git tracking.
- fixed GCC 14 `stringop-overflow` false positives and GCC/libstdc++ compare-tool portability issues.
- aligned local version metadata, workflow defaults, readmes, docs, changelog, release notes, package description, and GitHub release body to semantic version `1.6.0`.

Release assets:

- `decodium3-ft2-1.6.0-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.6.0-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.6.0-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.6.0-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.6.0-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.6.0-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.6.0-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.6.0-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.6.0-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.6.0-macos-monterey-x86_64.dmg` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.6.0-macos-monterey-x86_64.zip` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.6.0-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.6.0-linux-x86_64.AppImage`
- `decodium3-ft2-1.6.0-linux-x86_64.AppImage.sha256.txt`
- `decodium3-ft2-1.6.0-linux-aarch64.AppImage`
- `decodium3-ft2-1.6.0-linux-aarch64.AppImage.sha256.txt`

Linux minimum requirements:

- `x86_64` with SSE2 or `aarch64` / ARM64 64-bit CPU
- 4 GB RAM minimum, 500 MB free disk
- Qt5-capable X11 or Wayland session
- `glibc >= 2.35` for Linux `x86_64`
- `glibc >= 2.38` for Linux `aarch64` *(Debian Trixie baseline)*
- `libfuse2`, ALSA/PulseAudio/PipeWire, and serial/USB permissions as needed

If macOS blocks startup:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

To avoid issues caused by the AppImage read-only filesystem, it is recommended to start Decodium by extracting the AppImage first and then running the program from the extracted directory.

Run the following commands in a terminal:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

No `.pkg` installers are produced.

## Italiano

Punti principali (`1.5.9 -> 1.6.0`):

- promosso a C++ nativo il path runtime attivo JT65, inclusi orchestrazione decode e helper DSP/IO JT65.
- estesi i blocchi nativi JT9 fast/wide e gli strumenti compare/regression per la migrazione JT legacy residua.
- corrette le risposte verso callsign non standard o special-event che venivano rigettate con `*** bad message ***`.
- aggiunto supporto release Linux AppImage `aarch64` tramite build path ARM64 Debian Trixie e runner GitHub Actions ARM.
- reso `build-arm.sh` sensibile alla versione, adatto alla CI, ed esclusa permanentemente da git `build-arm-output/`.
- corretti falsi positivi GCC 14 `stringop-overflow` e problemi di portabilita' GCC/libstdc++ nelle utility compare.
- allineati alla semver `1.6.0` metadati versione locali, default workflow, readme, documentazione, changelog, note release, package description e body GitHub.

Asset release:

- `decodium3-ft2-1.6.0-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.6.0-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.6.0-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.6.0-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.6.0-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.6.0-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.6.0-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.6.0-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.6.0-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.6.0-macos-monterey-x86_64.dmg` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.6.0-macos-monterey-x86_64.zip` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.6.0-macos-monterey-x86_64-sha256.txt` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.6.0-linux-x86_64.AppImage`
- `decodium3-ft2-1.6.0-linux-x86_64.AppImage.sha256.txt`
- `decodium3-ft2-1.6.0-linux-aarch64.AppImage`
- `decodium3-ft2-1.6.0-linux-aarch64.AppImage.sha256.txt`

Requisiti minimi Linux:

- `x86_64` con SSE2 oppure CPU `aarch64` / ARM64 64-bit
- minimo 4 GB RAM, 500 MB liberi su disco
- sessione Qt5 su X11 o Wayland
- `glibc >= 2.35` per Linux `x86_64`
- `glibc >= 2.38` per Linux `aarch64` *(baseline Debian Trixie)*
- `libfuse2`, ALSA/PulseAudio/PipeWire e permessi seriale/USB secondo necessita'

Se macOS blocca l'avvio:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l'AppImage e poi eseguendo il programma dalla cartella estratta.

Eseguire i seguenti comandi nel terminale:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

Non vengono prodotti installer `.pkg`.

## Espanol

Resumen (`1.5.9 -> 1.6.0`):

- promovido a C++ nativo el camino runtime activo JT65, incluyendo orquestacion decode y helpers DSP/IO JT65.
- extendidos los bloques nativos JT9 fast/wide y las herramientas compare/regression para la migracion JT legacy restante.
- corregidas las respuestas a indicativos no estandar o special-event que se rechazaban con `*** bad message ***`.
- anadido soporte release Linux AppImage `aarch64` mediante camino de build ARM64 Debian Trixie y runner ARM de GitHub Actions.
- hecho `build-arm.sh` sensible a la version, apto para CI, y excluida permanentemente de git `build-arm-output/`.
- corregidos falsos positivos GCC 14 `stringop-overflow` y problemas de portabilidad GCC/libstdc++ en las utilidades compare.
- alineados con la semver `1.6.0` metadatos locales de version, defaults de workflow, readmes, documentacion, changelog, notas release, package description y body GitHub.

Artefactos release:

- `decodium3-ft2-1.6.0-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.6.0-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.6.0-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.6.0-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.6.0-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.6.0-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.6.0-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.6.0-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.6.0-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.6.0-macos-monterey-x86_64.dmg` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.6.0-macos-monterey-x86_64.zip` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.6.0-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.6.0-linux-x86_64.AppImage`
- `decodium3-ft2-1.6.0-linux-x86_64.AppImage.sha256.txt`
- `decodium3-ft2-1.6.0-linux-aarch64.AppImage`
- `decodium3-ft2-1.6.0-linux-aarch64.AppImage.sha256.txt`

Requisitos minimos Linux:

- `x86_64` con SSE2 o CPU `aarch64` / ARM64 64-bit
- minimo 4 GB RAM, 500 MB libres en disco
- sesion Qt5 en X11 o Wayland
- `glibc >= 2.35` para Linux `x86_64`
- `glibc >= 2.38` para Linux `aarch64` *(baseline Debian Trixie)*
- `libfuse2`, ALSA/PulseAudio/PipeWire y permisos serie/USB segun necesidad

Si macOS bloquea el inicio:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Para evitar problemas debidos al sistema de archivos de solo lectura de las AppImage, se recomienda iniciar Decodium extrayendo primero la AppImage y ejecutando despues el programa desde la carpeta extraida.

Ejecutar los siguientes comandos en la terminal:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

No se generan instaladores `.pkg`.
