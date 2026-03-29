# Decodium (Fork macOS/Linux) - 1.5.7

Este repositorio contiene el fork mantenido de Decodium para macOS y Linux AppImage.

- Release estable actual: `1.5.7`
- Ciclo de actualizacion: `1.5.6 -> 1.5.7`

## Cambios en 1.5.7 (`1.5.6 -> 1.5.7`)

- anadido un filtro de plausibilidad FT2 type-4 que evita que payloads ruidosos aparezcan en Band Activity como falsos nominativos.
- corregido el doble click FT2 en Band Activity para mensajes estandar `CQ` / `QRZ`, de forma que llamadas validas como `D2UY`, `K1RZ` y `KL7J` se armen de manera fiable.
- anadida cobertura dirigida en `test_qt_helpers` para formas validas special-event/slash y para salidas FT2 garbage que deben rechazarse.
- restaurado el target Linux `wsprd`, de modo que los jobs Linux/AppImage vuelvan a publicar el conjunto binario esperado.
- alineados metadatos locales de version, defaults de workflow, documentacion release y notas GitHub con la semver `1.5.7`.
- mantenido el layout/carpetas macOS ya validado en el ultimo deploy correcto.

## Targets Release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey (best effort / experimental)
- Linux x86_64 AppImage

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

- [RELEASE_NOTES_1.5.7.md](RELEASE_NOTES_1.5.7.md)
- [doc/GITHUB_RELEASE_BODY_1.5.7.md](doc/GITHUB_RELEASE_BODY_1.5.7.md)
- [CHANGELOG.md](CHANGELOG.md)
