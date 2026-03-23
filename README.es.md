# Decodium (Fork macOS/Linux) - 1.5.5

Este repositorio contiene el fork mantenido de Decodium para macOS y Linux AppImage.

- Release estable actual: `1.5.5`
- Ciclo de actualizacion: `1.5.4 -> 1.5.5`

## Cambios en 1.5.5 (`1.5.4 -> 1.5.5`)

- corregido el manejo nativo de `Preferencias...` en macOS para que las heuristicas de menu no abran acciones equivocadas en UIs traducidas.
- hecho scroll-safe el dialogo `Settings` tambien en macOS, manteniendo accesibles los botones finales en pantallas pequenas y layouts localizados mas largos.
- anadido el log persistente `jt9_subprocess.log` y una diagnostica stdout/stderr mas rica para analizar fallos del subprocess FT2.
- actualizado el ADIF FT2 a `MODE=MFSK` + `SUBMODE=FT2`, con migracion automatica de registros historicos `MODE=FT2` y backup del fichero original.
- mejorada la recuperacion del audio al arrancar/activar monitor, de forma que ya no haga falta reabrir `Settings > Audio` para despertar el stream.
- retirada de la release publica la UI experimental de RTTY, todavia incompleta, dejandola fuera del camino del usuario.

## Targets Release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey (best effort / experimental)
- Linux x86_64 AppImage

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

Hardware:

- CPU `x86_64`
- dual-core 2.0 GHz o superior
- minimo 4 GB RAM (8 GB recomendados)
- al menos 500 MB libres en disco
- pantalla 1280x800 o superior recomendada

Software:

- Linux `x86_64` con `glibc >= 2.35`
- `libfuse2` / soporte FUSE2
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

- [RELEASE_NOTES_1.5.5.md](RELEASE_NOTES_1.5.5.md)
- [doc/GITHUB_RELEASE_BODY_1.5.5.md](doc/GITHUB_RELEASE_BODY_1.5.5.md)
- [CHANGELOG.md](CHANGELOG.md)
