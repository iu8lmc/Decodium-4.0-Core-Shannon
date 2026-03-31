# Decodium (Fork macOS/Linux) - 1.5.8

Este repositorio contiene el fork mantenido de Decodium para macOS y Linux AppImage.

- Release estable actual: `1.5.8`
- Ciclo de actualizacion: `1.5.7 -> 1.5.8`

## Cambios en 1.5.8 (`1.5.7 -> 1.5.8`)

- completado el runtime promovido nativo C++ para FT8, FT4, FT2, Q65, MSK144, SuperFox y FST4/FST4W sin residuos Fortran especificos del modo en el camino activo.
- migrada a C++ nativo la cadena residual FST4/FST4W de decode-core, LDPC, helpers DSP compartidos y referencia/simulador.
- eliminados residuos tree-only de los modos promovidos como `ana64`, `q65_subs`, snapshots legacy MSK144/MSK40 y el subtree historico SuperFox Fortran.
- promovidos reemplazos nativos para `encode77`, `hash22calc`, `msk144code`, `msk144sim`, `sfoxsim`, `sfrx`, `sftx`, `fst4sim`, `ldpcsim240_101` y `ldpcsim240_74`.
- corregido el crash de cierre en macOS ligado a `fftwf_cleanup()` y reforzados `MainWindow::dataSink` y `fastSink`.
- corregidos los fallos Linux/GCC 15 alrededor de `_q65_mask`, `pack28`, enlazado legacy a simbolos C++ migrados y el bug off-by-one MSK40 en `decodeframe40_native`.
- ampliada la cobertura `test_qt_helpers` y smoke-tests de utilidades para DSP compartido, paridad FST4/oracle y compatibilidad Q65 nativa.
- alineados metadatos locales de version, defaults de workflow, documentacion release y notas GitHub con la semver `1.5.8`.

## Targets Release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey (best effort / experimental)
- Linux x86_64 AppImage

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

- [RELEASE_NOTES_1.5.8.md](RELEASE_NOTES_1.5.8.md)
- [doc/GITHUB_RELEASE_BODY_1.5.8.md](doc/GITHUB_RELEASE_BODY_1.5.8.md)
- [CHANGELOG.md](CHANGELOG.md)
