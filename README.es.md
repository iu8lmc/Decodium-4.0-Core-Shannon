# Decodium (Fork macOS/Linux) - 1.6.0

- Release estable actual: `1.6.0`
- Ciclo de actualizacion: `1.5.9 -> 1.6.0`

## Cambios en 1.6.0 (`1.5.9 -> 1.6.0`)

- promovido a C++ nativo el camino runtime activo JT65, incluyendo orquestacion de decode, helpers DSP/IO JT65 y eliminacion del build de las viejas fuentes Fortran JT65 del camino activo.
- anadidos bloques nativos JT9 fast/wide y una cobertura mas amplia de helpers legacy DSP/IO y compare/regression para el trabajo restante de migracion JT.
- corregidas las respuestas a indicativos no estandar o special-event que se rechazaban como `*** bad message ***`.
- anadido soporte release Linux AppImage `aarch64` con camino de build ARM64 basado en Debian Trixie y runners ARM de GitHub Actions.
- hecho `build-arm.sh` sensible a la version, mas apto para CI, y excluida permanentemente de git la carpeta `build-arm-output/`.
- corregido el falso positivo GCC 14 `stringop-overflow` en `LegacyDspIoHelpers.cpp` sin romper builds macOS Clang.
- corregidas regresiones de portabilidad GCC/libstdc++ en `jt9_wide_stage_compare.cpp` y `legacy_wsprio_compare.cpp`.
- alineados metadatos locales de version, defaults de workflow, documentacion release y notas GitHub con la semver `1.6.0`.

## Targets Release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey *(best effort / experimental)*
- Linux x86_64 AppImage
- Linux aarch64 AppImage *(baseline Debian Trixie)*

## Artefactos Release

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

## Requisitos Minimos Linux

Hardware:

- CPU `x86_64` con SSE2, o CPU `aarch64` / ARM64 64-bit
- dual-core 2.0 GHz o mejor, o SoC ARM64 moderno equivalente
- minimo 4 GB RAM (8 GB recomendados)
- al menos 500 MB libres en disco
- hardware de audio/CAT/serie/USB adecuado para weak-signal

Software:

- sesion de escritorio X11 o Wayland capaz de ejecutar AppImage Qt5
- AppImage Linux `x86_64`: `glibc >= 2.35`
- AppImage Linux `aarch64`: `glibc >= 2.38` *(baseline Debian Trixie)*
- `libfuse2` / FUSE2 para montar directamente la AppImage
- ALSA, PulseAudio o PipeWire
- permisos de acceso serie/USB para CAT o dispositivos externos

## Guia de Arranque

Si macOS bloquea el inicio:

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

## Archivos Relacionados

- [README.md](README.md)
- [README.en-GB.md](README.en-GB.md)
- [README.it.md](README.it.md)
- [RELEASE_NOTES_1.6.0.md](RELEASE_NOTES_1.6.0.md)
- [doc/GITHUB_RELEASE_BODY_1.6.0.md](doc/GITHUB_RELEASE_BODY_1.6.0.md)
