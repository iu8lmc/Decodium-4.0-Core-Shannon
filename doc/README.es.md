# Notas de Documentacion (Espanol) - 1.5.9

Este indice agrupa la documentacion de release del ciclo actual del fork.

- Release actual: `1.5.9`
- Ciclo de actualizacion: `1.5.8 -> 1.5.9`
- Foco principal: fiabilidad TX Linux FT2/FT4 de baja latencia, estabilidad macOS al cerrar/UI, nueva iconografia FT2 y metadatos release alineados.

## Cambios Tecnicos Principales (`1.5.8 -> 1.5.9`)

- el TX estandar Linux FT2/FT4 ya no queda en cola detras del bloqueo innecesario del runtime Fortran del lado decode en el camino waveform C++ normal.
- Linux FT2/FT4 usa ahora arranque inmediato post-waveform, fallback CAT, cola de audio TX mas pequena y categoria de audio de baja latencia.
- FT2 Linux funciona ahora con cero retraso extra y cero lead-in extra en el camino waveform precomputado estandar.
- Linux FT2/FT4 termina con la finalizacion real de modulator/audio y no solo con el timing teorico del slot.
- los dumps waveform FT2/FT4 se escriben solo cuando el debug log esta activo.
- corregido el orden de ownership/destruccion de widgets `MainWindow` al cerrar en macOS.
- el boton `Band Hopping` ya no pinta en rojo el campo `QSOs to upload`.
- actualizada la iconografia FT2 para launcher/artefactos macOS y Linux.
- metadatos de version, defaults de workflow, documentos release y notas GitHub quedan alineados a `1.5.9`.

## Artefactos Release

- `decodium3-ft2-1.5.9-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.9-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.9-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.9-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.9-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.9-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.9-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.9-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.9-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.9-macos-monterey-x86_64.dmg` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.9-macos-monterey-x86_64.zip` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.9-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.9-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.9-linux-x86_64.AppImage.sha256.txt`

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

Para evitar problemas debidos al sistema de archivos de solo lectura de las AppImage, se recomienda iniciar Decodium extrayendo primero la AppImage y ejecutando despues el programa desde la carpeta extraida.

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
- [RELEASE_NOTES_1.5.9.md](../RELEASE_NOTES_1.5.9.md)
- [doc/GITHUB_RELEASE_BODY_1.5.9.md](./GITHUB_RELEASE_BODY_1.5.9.md)
- [CHANGELOG.md](../CHANGELOG.md)
