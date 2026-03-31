# Decodium (Fork macOS/Linux) - 1.5.9

Este repositorio contiene el fork mantenido de Decodium para macOS y Linux AppImage.

- Release estable actual: `1.5.9`
- Ciclo de actualizacion: `1.5.8 -> 1.5.9`

## Cambios en 1.5.9 (`1.5.8 -> 1.5.9`)

- corregido el comportamiento intermitente Linux FT2/FT4 donde la radio entraba en TX pero el payload arrancaba mucho mas tarde, a veces casi al final de la ventana de transmision.
- eliminado en el camino TX estandar FT2/FT4 Linux el bloqueo innecesario con el mutex global del runtime Fortran, de modo que un decode lento ya no retrasa la salida del payload.
- anadidos arranque inmediato post-waveform y fallback CAT/PTT en Linux FT2/FT4 cuando el feedback del equipo llega tarde.
- reducida la latencia de la cola de audio TX Linux, seleccionada una categoria de audio de baja latencia, y llevado FT2 Linux a cero retraso/cero lead-in extra en el camino waveform estandar.
- cambiado el stop Linux FT2/FT4 para seguir la finalizacion real de audio/modulator en lugar de depender solo del fin teorico del slot.
- limitada la escritura de dumps waveform FT2/FT4 a sesiones con debug log activo, reduciendo I/O en disco durante TX normales.
- corregido un crash de cierre en macOS causado por el orden de ownership/destruccion de widgets Qt en `MainWindow`.
- corregido el color de la UI `Band Hopping`: `QSOs to upload` ya no se vuelve rojo junto con el boton `Band Hopping`.
- actualizada la nueva iconografia FT2 para los artefactos macOS y Linux.
- alineados metadatos locales de version, defaults de workflow, documentacion release y notas GitHub con la semver `1.5.9`.

## Targets Release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey (best effort / experimental)
- Linux x86_64 AppImage

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

Hardware:

- CPU `x86_64` con soporte SSE2
- dual-core 2.0 GHz o superior
- minimo 4 GB RAM (8 GB recomendados)
- al menos 500 MB libres en disco
- pantalla 1280x800 o superior recomendada
- hardware de audio/CAT/serial/USB adecuado para operacion weak-signal de radioaficion

Software:

- Linux `x86_64` con `glibc >= 2.35`
- `libfuse2` / soporte FUSE2 si se quiere montar directamente la AppImage
- ALSA, PulseAudio o PipeWire
- sesion de escritorio X11 o Wayland capaz de ejecutar AppImages Qt5
- permisos de acceso a dispositivos serie/USB (`dialout`, `uucp` o equivalente de la distro) si se usa CAT o radio externa
- acceso de red recomendado para NTP, PSK Reporter, DX Cluster, updater y flujos online

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

- [RELEASE_NOTES_1.5.9.md](RELEASE_NOTES_1.5.9.md)
- [doc/GITHUB_RELEASE_BODY_1.5.9.md](doc/GITHUB_RELEASE_BODY_1.5.9.md)
- [CHANGELOG.md](CHANGELOG.md)
