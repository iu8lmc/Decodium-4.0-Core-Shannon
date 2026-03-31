# Notas de Documentacion (Espanol) - 1.5.8

Este indice agrupa la documentacion de release del ciclo actual del fork.

- Release actual: `1.5.8`
- Ciclo de actualizacion: `1.5.7 -> 1.5.8`
- Foco principal: cierre completo del runtime promovido nativo C++ para la familia FTX, estabilidad macOS al cerrar/ruta de datos, endurecimiento Linux/GCC y metadatos release alineados.

## Cambios Tecnicos Principales (`1.5.7 -> 1.5.8`)

- FT8, FT4, FT2, Q65, MSK144, SuperFox y FST4/FST4W usan ahora el runtime promovido nativo C++ sin residuos Fortran especificos del modo en el camino activo.
- la cadena residual FST4/FST4W de decode-core, LDPC, helpers DSP compartidos y referencia/simulador es ahora nativa C++.
- los residuos tree-only de los modos promovidos (`ana64`, `q65_subs`, snapshots MSK144/MSK40, subtree SuperFox Fortran) se eliminaron tras la promocion nativa.
- las utilidades nativas ahora cubren `encode77`, `hash22calc`, `msk144code`, `msk144sim`, `sfoxsim`, `sfrx`, `sftx`, `fst4sim`, `ldpcsim240_101` y `ldpcsim240_74`.
- se corrigieron el crash FFTW al cerrar en macOS y las rutas fragiles de `MainWindow::dataSink` / `fastSink`.
- se corrigieron problemas Linux/GCC 15 alrededor de `_q65_mask`, `pack28`, enlazado a simbolos migrados e indexacion de frames MSK40.
- metadatos de version, defaults de workflow, documentos release y notas GitHub quedan alineados a `1.5.8`.

## Artefactos Release

- `decodium3-ft2-1.5.8-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.8-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.8-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.8-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.8-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.8-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.8-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.8-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.8-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.8-macos-monterey-x86_64.dmg` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.8-macos-monterey-x86_64.zip` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.8-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.8-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.8-linux-x86_64.AppImage.sha256.txt`

## Requisitos Minimos Linux

- `x86_64` con SSE2, dual-core 2.0 GHz+
- minimo 4 GB RAM (8 GB recomendados)
- 500 MB libres en disco
- `glibc >= 2.35`
- `libfuse2` / FUSE2 si se quiere montar la AppImage directamente
- ALSA, PulseAudio o PipeWire
- sesion de escritorio X11 o Wayland capaz de ejecutar AppImages Qt5

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
- [RELEASE_NOTES_1.5.8.md](../RELEASE_NOTES_1.5.8.md)
- [doc/GITHUB_RELEASE_BODY_1.5.8.md](./GITHUB_RELEASE_BODY_1.5.8.md)
- [CHANGELOG.md](../CHANGELOG.md)
