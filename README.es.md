# Decodium (Fork macOS/Linux) - 1.5.6

Este repositorio contiene el fork mantenido de Decodium para macOS y Linux AppImage.

- Release estable actual: `1.5.6`
- Ciclo de actualizacion: `1.5.5 -> 1.5.6`

## Cambios en 1.5.6 (`1.5.5 -> 1.5.6`)

- completada la migracion del runtime promovido nativo C++ para FT8, FT4, FT2 y Q65, eliminando del camino activo la orquestacion Fortran especifica de esos modos.
- ampliada la arquitectura in-process con workers y retirada del bootstrap principal la antigua shared memory de `jt9` para los modos FTX promovidos.
- promovidas herramientas/frontends nativos C++ para `jt9`, `jt9a`, `q65sim`, `q65code`, `q65_ftn_test`, `q65params`, `test_q65` y `rtty_spec`.
- endurecido el TX FT2/FT4/Fox con snapshots de ondas precomputadas, lead-in mas prudente y trazas extra en `debug.txt`.
- corregidos fallos de compilacion/enlazado en GNU `ld`, GCC 15, Qt5 y C++11 dentro de bridges, herramientas y tests.
- ampliada la validacion de paridad/regresion con nuevos stage-compare y cobertura reforzada en `test_qt_helpers`.
- mantenido el layout/carpetas macOS ya validado en el ultimo deploy correcto, alineando las releases Tahoe, Sequoia, Intel Sequoia, Monterey y Linux AppImage.
- la UI publica de RTTY sigue oculta a la espera de validacion dedicada.

## Targets Release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey (best effort / experimental)
- Linux x86_64 AppImage

## Artefactos Release

- `decodium3-ft2-1.5.6-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.6-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.6-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.6-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.6-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.6-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.6-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.6-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.6-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.6-macos-monterey-x86_64.dmg` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.6-macos-monterey-x86_64.zip` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.6-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.6-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.6-linux-x86_64.AppImage.sha256.txt`

## Requisitos Minimos Linux

Hardware:

- CPU `x86_64`
- dual-core 2.0 GHz o superior
- minimo 4 GB RAM (8 GB recomendados)
- al menos 500 MB libres en disco
- pantalla 1280x800 o superior recomendada

Software:

- Linux `x86_64` con `glibc >= 2.35`
- `libfuse2` / soporte FUSE2 si se quiere montar directamente la AppImage
- ALSA, PulseAudio o PipeWire
- una sesion de escritorio capaz de ejecutar AppImages Qt5

## Arranque

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

## Documentacion Relacionada

- [RELEASE_NOTES_1.5.6.md](RELEASE_NOTES_1.5.6.md)
- [doc/GITHUB_RELEASE_BODY_1.5.6.md](doc/GITHUB_RELEASE_BODY_1.5.6.md)
- [CHANGELOG.md](CHANGELOG.md)
