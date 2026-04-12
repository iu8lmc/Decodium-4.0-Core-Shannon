# Notas de Documentacion (Espanol) - 1.6.0

- Release actual: `1.6.0`
- Ciclo de actualizacion: `1.5.9 -> 1.6.0`

## Cambios Tecnicos Principales (`1.5.9 -> 1.6.0`)

- JT65 runtime activo promovido al camino decode nativo C++.
- Extendidos los bloques nativos JT9 fast/wide y los helpers DSP/IO compartidos para la migracion JT legacy restante.
- Las respuestas a indicativos no estandar o special-event ya no fallan con `*** bad message ***`.
- Anadido soporte release Linux AppImage ARM64 mediante camino de build Debian Trixie y runner GitHub ARM.
- Anadidos fixes de portabilidad GCC 14 y GCC/libstdc++ sin romper macOS Clang.
- Metadatos de version, defaults de workflow, documentos release y notas GitHub quedan alineados a `1.6.0`.

## Artefactos Release

- `decodium3-ft2-1.6.0-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.6.0-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.6.0-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.6.0-macos-monterey-x86_64.dmg` *(best effort / experimental, si se genera)*
- `decodium3-ft2-1.6.0-linux-x86_64.AppImage`
- `decodium3-ft2-1.6.0-linux-aarch64.AppImage`

## Requisitos Minimos Linux

- CPU `x86_64` con SSE2 o CPU `aarch64` / ARM64 64-bit
- minimo 4 GB RAM, 500 MB libres en disco
- sesion de escritorio X11 o Wayland capaz de ejecutar AppImage Qt5
- `glibc >= 2.35` para Linux `x86_64`
- `glibc >= 2.38` para Linux `aarch64` *(baseline Debian Trixie)*
- `libfuse2`, ALSA/PulseAudio/PipeWire y permisos serie/USB segun necesidad

## Guia de Arranque

Workaround de cuarentena macOS:

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

- [README.es.md](../README.es.md)
- [RELEASE_NOTES_1.6.0.md](../RELEASE_NOTES_1.6.0.md)
- [doc/GITHUB_RELEASE_BODY_1.6.0.md](./GITHUB_RELEASE_BODY_1.6.0.md)
