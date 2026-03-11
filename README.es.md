# Decodium 3 FT2 (Fork macOS) - v1.4.4

Fork mantenido por **Salvatore Raccampo 9H1SR**.

Para la vista general bilingue, ver [README.md](README.md).

## Cambios en v1.4.4 (`v1.4.3 -> v1.4.4`)

- Anadido sistema de certificados DXpedition (`.dxcert`) con verificacion HMAC-SHA256 sobre payload canonico.
- Anadidos controles de validez temporal del certificado y autorizacion del operador segun callsign local.
- Anadidas nuevas acciones de menu DXped:
- `Load DXped Certificate...`
- `DXped Certificate Manager...`
- Anadidas herramientas DXped en `tools/` e instalacion en artefactos release.
- DXped mode ahora exige certificado valido: activacion bloqueada si falta, es invalido, expirado o no autorizado.
- Mejoras en runtime DXped:
- flujo estandar `processMessage()` bloqueado cuando la FSM DXped esta activa, evitando colisiones con AutoSeq.
- `dxpedAutoSequence` llamado directamente desde rutas de decode.
- fallback CQ mejorado: si `tx5` esta vacio se copia desde `tx6`.
- Async L2 en FT2 ahora es obligatorio:
- ON forzado en FT2 y OFF forzado fuera de FT2.
- intentos locales/remotos de desactivarlo en FT2 se ignoran.
- Mejoras ASYMX/progress bar:
- nuevos estados `GUARD`, `TX`, `RX`, `IDLE` con color dedicado.
- guardia TX de 300 ms antes del primer auto-TX FT2.
- reset explicito de buffers/counters/timers async en transiciones de toggle.
- Mejoras de consistencia en decodes:
- fuente monoespaciada forzada en paneles decode para mantener columnas.
- marcador AP/calidad movido al final de linea para no romper alineacion de columna derecha.
- normalizacion de marcador FT2 (`~` a `+`) en decode normal y async.
- doble click mas robusto: elimina anotaciones de la derecha antes de parsear `DecodedText`.
- Mejor identificacion UDP multiinstancia:
- client id derivado del nombre de aplicacion para diferenciar instancias paralelas.
- Anadidas traducciones italianas de estados async y mensaje "Async L2 obligatorio".
- Metadatos de release/workflows/documentacion alineados a `v1.4.4`.

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
- [RELEASE_NOTES_v1.4.4.md](RELEASE_NOTES_v1.4.4.md)
- [CHANGELOG.md](CHANGELOG.md)
- [doc/GITHUB_RELEASE_BODY_v1.4.4.md](doc/GITHUB_RELEASE_BODY_v1.4.4.md)
- [doc/README.es.md](doc/README.es.md)
