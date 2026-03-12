# Notas de Documentacion (Espanol) - v1.4.5

## Alcance

Notas tecnicas del fork macOS Decodium con flujo de release Linux AppImage.

## Contexto de Release

- Release actual: `v1.4.5`
- Ciclo de actualizacion: `v1.4.4 -> v1.4.5`
- Objetivos: Apple Silicon Tahoe, Apple Silicon Sequoia, Apple Intel Sequoia, Apple Intel Monterey (experimental), Linux x86_64 AppImage

## Cambios Tecnicos Principales (`v1.4.4 -> v1.4.5`)

- Hardening FT2 AutoCQ/secuencia QSO:
- bloqueo de pareja activa mas fiable durante QSO.
- respuestas dirigidas durante CQ capturadas con matching base-call.
- `73` de estaciones no pareja ignorados en auto-secuencia.
- timers autolog stale/deferred filtrados de forma segura.
- cola de callers protegida contra duplicados recien trabajados.
- Estabilidad signoff/log FT2:
- flujo deferred signoff mejorado con handoff de snapshot log.
- cierre RR73/73 mas estable antes de volver a CQ.
- reintentos actualizados (`MAX_TX_RETRIES=5`).
- Mejoras decode/runtime FT2:
- columna FT2 actualizada con `TΔ` (tiempo desde ultimo TX).
- fix de alineacion de columnas FT2 para marcador y signos menos Unicode.
- confirmaciones async debiles preservadas antes de dedupe.
- umbral decoder `nharderror` actualizado (`35 -> 48`).
- Fix de temporizacion TX:
- corregidos ramp-up/ramp-down y latencia FT2 en rutas TCI y soundcard.
- Fix web/UI:
- corregido inicio dashboard remota y fetch init.
- corregida apertura `Vista -> Display a cascata` (waterfall).
- restaurado hint de password web localizado (minimo 12 caracteres).
- Compatibilidad logger UDP:
- corregido slicing payload FT2 por UDP para evitar perdida del primer caracter del callsign en RumLog.

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
- [RELEASE_NOTES_v1.4.5.md](../RELEASE_NOTES_v1.4.5.md)
- [doc/GITHUB_RELEASE_BODY_v1.4.5.md](./GITHUB_RELEASE_BODY_v1.4.5.md)
- [doc/WEBAPP_SETUP_GUIDE.es.md](./WEBAPP_SETUP_GUIDE.es.md)
- [doc/WEBAPP_SETUP_GUIDE.en-GB.md](./WEBAPP_SETUP_GUIDE.en-GB.md)
- [doc/WEBAPP_SETUP_GUIDE.it.md](./WEBAPP_SETUP_GUIDE.it.md)
