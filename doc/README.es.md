# Notas de Documentacion (Espanol) - 1.5.7

Este indice agrupa la documentacion de release del ciclo actual del fork.

- Release actual: `1.5.7`
- Ciclo de actualizacion: `1.5.6 -> 1.5.7`
- Foco principal: filtro de cordura FT2, seleccion fiable del caller FT2 desde Band Activity, continuidad de la release Linux y metadatos release alineados.

## Cambios Tecnicos Principales (`1.5.6 -> 1.5.7`)

- las salidas FT2 type-4 pasan ahora por un filtro de plausibilidad antes de aceptarse en el camino activo de decode.
- `tests/test_qt_helpers.cpp` valida ahora patrones special-event/slash aceptados y casos FT2 garbage que deben rechazarse.
- el doble click FT2 en Band Activity sobre mensajes estandar `CQ` / `QRZ` arma ahora directamente el caller seleccionado en lugar de depender del viejo camino generico.
- el packaging Linux release vuelve a incluir el target `wsprd` restaurado para la publicacion AppImage/release.
- metadatos de version, defaults de workflow, documentos release y notas GitHub quedan alineados a `1.5.7`.

## Artefactos Release

- `decodium3-ft2-1.5.7-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.7-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.7-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.7-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.7-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.7-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.7-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.7-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.7-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.7-macos-monterey-x86_64.dmg` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.7-macos-monterey-x86_64.zip` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.7-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.7-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.7-linux-x86_64.AppImage.sha256.txt`

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
- [RELEASE_NOTES_1.5.7.md](../RELEASE_NOTES_1.5.7.md)
- [doc/GITHUB_RELEASE_BODY_1.5.7.md](./GITHUB_RELEASE_BODY_1.5.7.md)
- [CHANGELOG.md](../CHANGELOG.md)
