# Notas de Documentacion (Espanol) - v1.4.6

## Alcance

Notas tecnicas del fork macOS Decodium con flujo de release Linux AppImage.

## Contexto de Release

- Release actual: `v1.4.6`
- Ciclo de actualizacion: `v1.4.5 -> v1.4.6`
- Objetivos: Apple Silicon Tahoe, Apple Silicon Sequoia, Apple Intel Sequoia, Apple Intel Monterey (experimental), Linux x86_64 AppImage

## Cambios Tecnicos Principales (`v1.4.5 -> v1.4.6`)

- Hardening FT2/AutoCQ/secuencia QSO:
- restaurado comportamiento FIFO de cola callers de la baseline v1.3.8.
- lock de pareja activa reforzado durante QSO en curso.
- flujo deferred de reintentos RR73/73 aplicado de forma consistente en FT2/FT8/FT4/FST4/Q65/MSK144.
- matching de pareja por tokens payload normalizados reforzado en formatos decode limite.
- Estabilidad signoff/log:
- recovery de contexto deferred autolog mejorado tras ventanas de reintento.
- reducidos casos de log prematuro/sin confirmacion de pareja.
- reducidos retrasos de log por estados pending stale.
- Continuidad decode/panel Frecuencia Rx:
- extraccion de payload decode robusta con marcador variable/ausente.
- reducidos casos donde mensajes validos de pareja quedaban solo en Actividad de Banda.
- Fix UI/runtime:
- corregido hide/show `Vista -> World Map` en macOS.
- mejorada logica de tamano splitter para paneles secundarios decode/map.
- en Linux, pestañas de ajustes ahora con scroll wrapping (botones de accion siempre accesibles).
- agregado refresh startup de streams audio en macOS para dispositivos ya seleccionados.
- Dashboard remota/web:
- agregado endpoint remoto `set_tx_frequency`.
- web app con set Rx+Tx, set Rx/set Tx separados, y presets de frecuencia por modo.
- mantenido hint de password localizado con minimo 12 caracteres (IT/EN).

## Build y Runtime

- Binario local: `build/ft2.app/Contents/MacOS/ft2`
- Ejecutables auxiliares del bundle: `jt9`, `wsprd`
- En macOS se mantiene backend de memoria compartida `mmap` file-backed (no requiere bootstrap `.pkg`).

## Requisitos Minimos Linux

- Arquitectura: `x86_64` (64 bits)
- CPU: dual-core 2.0 GHz o superior
- RAM: 4 GB minimo (8 GB recomendado)
- Disco: al menos 500 MB libres
- Runtime/software:
- Linux con `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

## Recomendacion AppImage

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l'AppImage e poi eseguendo il programma dalla cartella estratta.

Eseguire i seguenti comandi nel terminale:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Comando Cuarentena Gatekeeper

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

## Referencias

- [CHANGELOG.md](../CHANGELOG.md)
- [RELEASE_NOTES_v1.4.6.md](../RELEASE_NOTES_v1.4.6.md)
- [doc/GITHUB_RELEASE_BODY_v1.4.6.md](./GITHUB_RELEASE_BODY_v1.4.6.md)
- [doc/WEBAPP_SETUP_GUIDE.es.md](./WEBAPP_SETUP_GUIDE.es.md)
- [doc/WEBAPP_SETUP_GUIDE.en-GB.md](./WEBAPP_SETUP_GUIDE.en-GB.md)
- [doc/WEBAPP_SETUP_GUIDE.it.md](./WEBAPP_SETUP_GUIDE.it.md)
