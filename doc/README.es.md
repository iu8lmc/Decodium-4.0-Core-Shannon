# Notas de Documentacion (Espanol) - 1.5.4

Este indice agrupa la documentacion de release del ciclo actual del fork.

- Release actual: `1.5.4`
- Ciclo de actualizacion: `1.5.3 -> 1.5.4`
- Foco principal: filtro FT2 anti-ghost, refresh sync decoder FT2/FT4/FT8, paridad de la web app, alineacion completa UI/web de idiomas, endurecimiento de downloader/secure settings y packaging release macOS corregido.

## Cambios Tecnicos Principales (`1.5.3 -> 1.5.4`)

- anadido el filtro FT2 anti-ghost para payloads muy debiles y malformados, con logging diagnostico `ghostPass` / `ghostFilt`.
- actualizado el sync decoder en FT2, FT4 y FT8 con logica Costas `best 3 of 4` / `best 2 of 3` y sustraccion adaptativa mas profunda.
- anadidos a la web app `Monitoring ON/OFF`, indicador FT2 `ASYNC` en dB y filtros de actividad `Hide CQ` / `Hide 73`.
- la web app sigue ahora el idioma seleccionado en la app de escritorio y cubre todas las lenguas bundle.
- eliminada la entrada duplicada `English (UK)` y localizado el formato fecha UTC/Astro.
- corregido el packaging release de macOS: sonidos en `Contents/Resources`, helpers Hamlib incluidos como ficheros reales, dependencias Framework/plugin normalizadas a `@rpath`, y limpieza de residuos legacy del bundle antiguo en arboles de build reutilizados.
- endurecidos fallback/import de secure settings, redirects/limites de tamano del downloader, logging de excepciones CAT, waits de arranque DXLab y defaults HTTPS de LoTW.
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
- [RELEASE_NOTES_1.5.4.md](../RELEASE_NOTES_1.5.4.md)
- [doc/GITHUB_RELEASE_BODY_1.5.4.md](./GITHUB_RELEASE_BODY_1.5.4.md)
- [CHANGELOG.md](../CHANGELOG.md)
