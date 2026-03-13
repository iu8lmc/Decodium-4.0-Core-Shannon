# Decodium 3 FT2 (Fork macOS) - v1.4.6

Fork mantenido por **Salvatore Raccampo 9H1SR**.

Para la vista general bilingue, ver [README.md](README.md).

## Cambios en v1.4.6 (`v1.4.5 -> v1.4.6`)

- Hardening AutoCQ y maquina de estados QSO:
- restaurado comportamiento FIFO estable de cola de callers (baseline de la logica v1.3.8).
- lock de pareja activa reforzado durante QSO para evitar takeover accidental.
- flujo deferred de reintentos RR73/73 aplicado de forma coherente a FT2, FT8, FT4, FST4, Q65 y MSK144.
- matching de pareja mejorado con tokens de payload normalizados, incluso en formatos decode limite.
- Correccion signoff/log:
- recuperado contexto deferred autolog tras ventanas de reintento.
- reducidos casos de log forzado sin confirmacion real de pareja.
- reducidos casos de log tardio por estados pending stale.
- Continuidad decode y panel Frecuencia Rx:
- extraccion de payload decode mas robusta con marcador variable/ausente.
- reducidos casos donde respuestas validas quedaban solo en Actividad de Banda sin pasar al flujo central Rx.
- Correcciones UI/runtime desktop:
- corregido toggle `Vista -> World Map` en macOS (mapa ahora sigue el estado del menu).
- mejorada gestion de splitter/paneles secundarios en Linux/macOS.
- en Linux, pestañas de ajustes largas ahora van en `QScrollArea` (boton OK siempre accesible).
- en macOS, refresco automatico de stream audio al inicio para evitar recarga manual de dispositivos.
- Actualizaciones dashboard remota/web app:
- soporte completo para comando remoto `set_tx_frequency`.
- webapp con set combinado Rx+Tx, set Rx/set Tx separados y presets por modo (save/apply).
- mantenido hint de password localizado con minimo 12 caracteres (IT/EN).

## Objetivos de release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey (experimental / best effort)
- Linux x86_64 AppImage

## Requisitos minimos Linux

- Arquitectura: `x86_64` (64 bits)
- CPU: dual-core 2.0 GHz o superior
- RAM: 4 GB minimo (8 GB recomendado)
- Disco: al menos 500 MB libres (AppImage + logs + configuracion)
- Runtime/software:
- Linux con `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire
- Integracion de estacion: hardware CAT/audio segun configuracion de radio

## Recomendacion de arranque AppImage en Linux

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l'AppImage e poi eseguendo il programma dalla cartella estratta.

Eseguire i seguenti comandi nel terminale:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Comando macOS (cuarentena)

Si Gatekeeper bloquea el inicio, ejecutar:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

## Build local

```bash
cmake --build build -j6
./build/ft2.app/Contents/MacOS/ft2
```

## Documentacion

- [README.en-GB.md](README.en-GB.md)
- [README.it.md](README.it.md)
- [RELEASE_NOTES_v1.4.6.md](RELEASE_NOTES_v1.4.6.md)
- [CHANGELOG.md](CHANGELOG.md)
- [doc/GITHUB_RELEASE_BODY_v1.4.6.md](doc/GITHUB_RELEASE_BODY_v1.4.6.md)
- [doc/README.es.md](doc/README.es.md)
