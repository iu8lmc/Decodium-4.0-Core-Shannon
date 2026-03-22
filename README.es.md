# Decodium 3 FT2 (Fork macOS/Linux) - 1.5.3

Este fork empaqueta Decodium 3 FT2 para macOS y Linux AppImage, con fixes de cierre de QSO FT2/FT4/FT8, endurecimiento AutoCQ, actualizaciones de decoder FT2 desde upstream, recuperacion de audio al arrancar, soporte updater, controles web app alineados, traducciones UI completas y tooling de certificados Decodium mantenidos en este repositorio.

Release estable actual: `1.5.3`.

## Cambios en 1.5.3 (`1.5.2 -> 1.5.3`)

- endurecido `Wait Features + AutoSeq` en FT4/FT8 para que una respuesta tardia o una estacion ocupada pause el ciclo TX actual en lugar de transmitir encima de un QSO ya activo.
- restaurada la compatibilidad practica Linux con `CQRLOG wsjtx remote`, manteniendo el comportamiento historico de la listen port UDP y usando `WSJTX` como client id de compatibilidad.
- corregida la alineacion de version de las builds locales: al cambiar `fork_release_version.txt`, la recompilacion actualiza de verdad la version del binario y no deja cadenas `1.5.x` obsoletas.
- anadidas traducciones UI bundle reales en aleman y frances, con fallback limpio a ingles cuando en ajustes habia una lengua guardada no incluida realmente.
- alineados a `1.5.3` los defaults release y la documentacion, incluido el default del build Linux experimental Hamlib.

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

Hardware:

- CPU `x86_64`, dual-core 2.0 GHz+
- RAM 4 GB minimo (8 GB recomendado)
- al menos 500 MB libres en disco
- pantalla 1280x800 o superior recomendada
- hardware radio/CAT/audio segun la estacion

Software:

- Linux `x86_64` con `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire
- entorno de escritorio capaz de ejecutar AppImage Qt5
- acceso a red recomendado para NTP, DX Cluster, PSK Reporter, updater y funciones online

## Nota de Build Local Linux

La AppImage publicada ya incluye el runtime Qt multimedia necesario. Si compilas Decodium localmente en Ubuntu/Debian, instala tambien los paquetes multimedia minimos del sistema; de lo contrario, las listas de dispositivos de audio pueden quedar vacias o deshabilitadas aunque la AppImage funcione correctamente.

Paquetes minimos recomendados para builds locales Ubuntu/Debian:

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

## Ficheros Relacionados

- [README.md](README.md)
- [README.en-GB.md](README.en-GB.md)
- [README.it.md](README.it.md)
- [RELEASE_NOTES_1.5.3.md](RELEASE_NOTES_1.5.3.md)
- [doc/GITHUB_RELEASE_BODY_1.5.3.md](doc/GITHUB_RELEASE_BODY_1.5.3.md)
- [CHANGELOG.md](CHANGELOG.md)
