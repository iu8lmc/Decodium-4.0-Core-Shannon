# Notas de Documentacion (Espanol) - 1.5.6

Este indice agrupa la documentacion de release del ciclo actual del fork.

- Release actual: `1.5.6`
- Ciclo de actualizacion: `1.5.5 -> 1.5.6`
- Foco principal: finalizacion del runtime promovido nativo C++ para FT8/FT4/FT2/Q65, decodificacion in-process con workers, hardening TX/build, validacion de paridad y packaging macOS/Linux alineado.

## Cambios Tecnicos Principales (`1.5.5 -> 1.5.6`)

- FT8, FT4, FT2 y Q65 usan ahora el runtime promovido nativo C++ sin orquestacion Fortran especifica en el camino activo de decode.
- los entrypoints `jt9` / `jt9a` y varias utilidades Q65/offline son nativos C++ en este arbol.
- el arranque principal ya no reserva el viejo bootstrap `jt9` basado en shared memory para la ruta worker FTX promovida.
- el TX FT2/FT4/Fox conserva snapshots de ondas precomputadas, lead-in mas conservador y trazas mas ricas en `debug.txt`.
- el build Linux/macOS ha sido endurecido frente a ciclos de librerias estaticas GNU `ld`, strictness de GCC 15, constructores Qt5 y huecos de compatibilidad C++11.
- la cobertura de paridad/regresion crece con nuevos stage-compare y tests helper mas amplios.
- se mantiene el layout macOS ya validado en el ultimo deploy correcto para Tahoe, Sequoia e Intel.

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

- `x86_64`, dual-core 2.0 GHz+
- 4 GB RAM minima (8 GB recomendados)
- 500 MB libres en disco
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

## Guia de Arranque

Cuarentena macOS:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Flujo Linux AppImage extract-run:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Ficheros Relacionados

- [README.md](../README.md)
- [README.en-GB.md](../README.en-GB.md)
- [README.it.md](../README.it.md)
- [README.es.md](../README.es.md)
- [RELEASE_NOTES_1.5.6.md](../RELEASE_NOTES_1.5.6.md)
- [doc/GITHUB_RELEASE_BODY_1.5.6.md](./GITHUB_RELEASE_BODY_1.5.6.md)
- [CHANGELOG.md](../CHANGELOG.md)
