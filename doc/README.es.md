# Notas de Documentacion (Espanol) - 1.5.5

Este indice agrupa la documentacion de release del ciclo actual del fork.

- Release actual: `1.5.5`
- Ciclo de actualizacion: `1.5.4 -> 1.5.5`
- Foco principal: fiabilidad de Preferencias en macOS para todas las lenguas UI, `Settings` con scroll en macOS, diagnostica del subprocess FT2, migracion ADIF FT2, recuperacion de audio al arranque y retirada de la UI RTTY experimental del camino publico.

## Cambios Tecnicos Principales (`1.5.4 -> 1.5.5`)

- corregido el enrutado de roles nativos de menu en macOS para que solo las entradas previstas usen roles especiales, sin depender del idioma traducido.
- hechas scrollables tambien en macOS las paginas de `Settings`, manteniendo accesible `OK`.
- anadido `jt9_subprocess.log` con trazas persistentes de arranque/errores/finalizacion del decoder FT2 y popups diagnosticos mas ricos.
- sustituidos los fallos opacos de FT2 por rutas de reporte mas claras para shared memory y stdout/stderr.
- llevado el ADIF FT2 al estilo ADIF 3.17 `MODE=MFSK` + `SUBMODE=FT2`, con migracion automatica del historico y backup.
- mejorada la recuperacion del audio al arrancar y al activar monitor mediante reopen estilo Settings mas health checks.
- retirada de la ruta de usuario la UI RTTY incompleta para esta release publica.

## Artefactos Release

- `decodium3-ft2-1.5.5-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.5-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.5-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.5-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.5-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.5-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.5-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.5-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.5-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.5-macos-monterey-x86_64.dmg` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.5-macos-monterey-x86_64.zip` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.5-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.5-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.5-linux-x86_64.AppImage.sha256.txt`

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
- [RELEASE_NOTES_1.5.5.md](../RELEASE_NOTES_1.5.5.md)
- [doc/GITHUB_RELEASE_BODY_1.5.5.md](./GITHUB_RELEASE_BODY_1.5.5.md)
- [CHANGELOG.md](../CHANGELOG.md)
