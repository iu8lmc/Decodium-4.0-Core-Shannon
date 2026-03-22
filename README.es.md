# Decodium (Fork macOS/Linux) - 1.5.4

Este fork empaqueta Decodium para macOS y Linux AppImage, con filtro FT2 anti-ghost, mejoras sync decoder en FT2/FT4/FT8, controles web app alineados, cobertura completa de idiomas UI/web, endurecimiento del downloader, proteccion de secure settings y packaging macOS corregido.

Release estable actual: `1.5.4`.

## Cambios en 1.5.4 (`1.5.3 -> 1.5.4`)

- anadido el filtro FT2 anti-ghost para decodes muy debiles, con trazas `ghostPass` / `ghostFilt` en `debug.txt`.
- actualizado el sync decoder: FT2 `best 3 of 4 Costas`, FT4 `best 3 of 4` con sustraccion mas profunda y adaptativa, FT8 `best 2 of 3` con cuarto pase.
- anadidos a la web app `Monitoring ON/OFF`, indicador FT2 `ASYNC` en dB y filtros `Hide CQ` / `Hide 73`.
- la web app sigue ahora el idioma elegido en Decodium y cubre todas las lenguas bundle.
- eliminada la entrada duplicada `English (UK)`, localizado el formato fecha UTC/Astro y mantenido `Decodium` quitando `v3.0 FT2 "Raptor"` del titulo visible al usuario.
- corregido el packaging release de macOS: los sonidos quedan en `Contents/Resources`, las herramientas Hamlib incluidas se copian como ficheros reales y no symlinks, las dependencias de Frameworks/plugins se normalizan a `@rpath`, y los arboles de build reutilizados se limpian de residuos legacy del bundle antiguo.
- endurecidos fallback/import de secure settings, descargas de archivos, logging de excepciones CAT, waits de arranque DXLab y defaults HTTPS de LoTW.
- ampliada la cobertura automatizada con vectores RFC HOTP/TOTP y tests dedicados para downloader y secure settings.

## Artefactos Release

- `decodium3-ft2-1.5.4-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.4-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.4-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.4-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.4-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.4-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.4-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.4-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.4-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.4-macos-monterey-x86_64.dmg` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.4-macos-monterey-x86_64.zip` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.4-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.4-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.4-linux-x86_64.AppImage.sha256.txt`

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
- [RELEASE_NOTES_1.5.4.md](RELEASE_NOTES_1.5.4.md)
- [doc/GITHUB_RELEASE_BODY_1.5.4.md](doc/GITHUB_RELEASE_BODY_1.5.4.md)
- [CHANGELOG.md](CHANGELOG.md)
