# Decodium 3 FT2 (Fork macOS) - v1.4.5

Fork mantenido por **Salvatore Raccampo 9H1SR**.

Para la vista general bilingue, ver [README.md](README.md).

## Cambios en v1.4.5 (`v1.4.4 -> v1.4.5`)

- Correcciones FT2 AutoCQ y maquina de estados QSO:
- bloqueo de pareja activa mas robusto durante QSO (override manual solo con Ctrl/Shift).
- respuestas dirigidas durante CQ capturadas de forma mas fiable (matching por base callsign).
- mensajes `73` de estaciones no pareja ya no fuerzan cierres incorrectos.
- timers diferidos/stale de autolog ahora se ignoran con seguridad.
- cola de callers protegida contra duplicados recien trabajados tras log.
- Fiabilidad signoff/log en FT2:
- mejorado flujo deferred signoff FT2 con handoff de snapshot autolog.
- completado RR73/73 mas estable antes de volver a CQ.
- reintentos AutoCQ ajustados (`MAX_TX_RETRIES=5`) con reset limpio al cambiar DX.
- Mejoras decode/runtime FT2:
- columna FT2 ahora muestra `TΔ` (tiempo desde ultimo TX) en lugar de DT.
- corregida alineacion de columnas FT2 (`~`, signos menos Unicode, prefijo de ancho fijo).
- confirmaciones async debiles preservadas antes de la supresion near-duplicate.
- hardening del decoder con umbral `nharderror` relajado (`35 -> 48`).
- Correcciones de temporizacion TX:
- ajustados ramp-up/ramp-down y latencia TX FT2 en rutas TCI y soundcard.
- Correcciones web/UI:
- fix de inicio dashboard remota e inicializacion de fetch.
- fix de apertura `Vista -> Display a cascata` (waterfall).
- restaurado hint de password web localizado (minimo 12 caracteres).
- Fix compatibilidad logger UDP FT2:
- corregido el slicing del payload FT2 por UDP para que loggers externos (RumLog) no pierdan el primer caracter del callsign.

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
- [RELEASE_NOTES_v1.4.5.md](RELEASE_NOTES_v1.4.5.md)
- [CHANGELOG.md](CHANGELOG.md)
- [doc/GITHUB_RELEASE_BODY_v1.4.5.md](doc/GITHUB_RELEASE_BODY_v1.4.5.md)
- [doc/README.es.md](doc/README.es.md)
