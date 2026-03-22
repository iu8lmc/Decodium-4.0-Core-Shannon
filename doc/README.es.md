# Notas de Documentacion (Espanol) - 1.5.3

Este indice agrupa la documentacion de release del ciclo actual del fork.

- Release actual: `1.5.3`
- Ciclo de actualizacion: `1.5.2 -> 1.5.3`
- Foco principal: compatibilidad CQRLOG, correccion Wait Features FT4/FT8, propagacion de version local, nuevas traducciones bundle DE/FR y alineacion release.

## Cambios Tecnicos Principales (`1.5.2 -> 1.5.3`)

- restaurada la interoperabilidad Linux con `CQRLOG wsjtx remote`, manteniendo el comportamiento historico de la listen port UDP y usando `WSJTX` como client id de compatibilidad.
- corregida la propagacion de version en las builds locales para que un cambio en `fork_release_version.txt` fuerce la version correcta dentro del binario compilado.
- endurecido `Wait Features + AutoSeq` en FT4/FT8 para que las colisiones sobre slots ocupados pausen el ciclo TX actual en lugar de transmitir sobre un QSO activo.
- anadidas traducciones UI bundle reales en aleman y frances y filtrado el menu de idiomas a las traducciones realmente incluidas.
- alineados a `1.5.3` los defaults release y la documentacion, incluido el default del workflow Linux Hamlib experimental.

## Artefactos Release

- `decodium3-ft2-1.5.3-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.3-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.3-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.3-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.3-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.3-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.3-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.3-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.3-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.3-macos-monterey-x86_64.dmg` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.3-macos-monterey-x86_64.zip` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.3-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.3-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.3-linux-x86_64.AppImage.sha256.txt`

## Requisitos Minimos Linux

- `x86_64`, dual-core 2.0 GHz+, 4 GB RAM minima, 500 MB libres
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

## Nota de Build Local Linux

La AppImage ya incluye el runtime Qt multimedia requerido. Para builds locales desde codigo fuente en Ubuntu/Debian, instala tambien los paquetes multimedia minimos del sistema; de lo contrario, los dispositivos de audio pueden aparecer vacios o deshabilitados.

```bash
sudo apt update
sudo apt install \
  qtmultimedia5-dev \
  libqt5multimedia5 \
  libqt5multimedia5-plugins \
  libqt5multimediawidgets5 \
  libqt5multimediagsttools5 \
  libpulse-mainloop-glib0 \
  pulseaudio-utils \
  gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good
```

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
- [RELEASE_NOTES_1.5.3.md](../RELEASE_NOTES_1.5.3.md)
- [doc/GITHUB_RELEASE_BODY_1.5.3.md](./GITHUB_RELEASE_BODY_1.5.3.md)
- [CHANGELOG.md](../CHANGELOG.md)
