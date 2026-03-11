# Notas de Documentacion (Espanol) - v1.4.4

## Alcance

Notas tecnicas del fork macOS Decodium con flujo de release Linux AppImage.

## Contexto de Release

- Release actual: `v1.4.4`
- Ciclo de actualizacion: `v1.4.3 -> v1.4.4`
- Objetivos: Apple Silicon Tahoe, Apple Silicon Sequoia, Apple Intel Sequoia, Apple Intel Monterey (experimental), Linux x86_64 AppImage

## Cambios Tecnicos Principales (`v1.4.3 -> v1.4.4`)

- Anadido subsistema de certificados DXped (`DXpedCertificate.hpp`) con verificacion HMAC-SHA256 de payload JSON canonico.
- Anadidos controles runtime de certificado DXped:
- ventana temporal de validez.
- autorizacion de operador respecto al callsign local.
- Anadidas acciones de menu para cargar certificado y abrir manager DXped.
- Anadidas herramientas Python en `tools/` y reglas de instalacion en `CMakeLists.txt`.
- Activacion DXped ahora condicionada a certificado valido.
- Integrada auto-secuencia DXped en el flujo decode; bloqueado `processMessage()` estandar cuando la FSM DXped esta activa.
- Async L2 en FT2 ahora obligatorio (tambien en controles remotos).
- Nuevos estados de barra ASYMX (`GUARD/TX/RX/IDLE`) con temporizador de guardia.
- Mejoras de consistencia decode/UI:
- fuente monoespaciada forzada en paneles decode.
- marcador AP/calidad alineado al final de linea.
- normalizacion del marcador FT2 (`~` a `+`) en lineas decode.
- doble click mas robusto eliminando anotaciones de cola antes de parsear.
- ID de cliente UDP derivado del nombre de aplicacion para mejor separacion multiinstancia.
- Traducciones italianas actualizadas para estados async y mensaje Async L2 obligatorio.

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
- [RELEASE_NOTES_v1.4.4.md](../RELEASE_NOTES_v1.4.4.md)
- [doc/GITHUB_RELEASE_BODY_v1.4.4.md](./GITHUB_RELEASE_BODY_v1.4.4.md)
- [doc/WEBAPP_SETUP_GUIDE.es.md](./WEBAPP_SETUP_GUIDE.es.md)
- [doc/WEBAPP_SETUP_GUIDE.en-GB.md](./WEBAPP_SETUP_GUIDE.en-GB.md)
- [doc/WEBAPP_SETUP_GUIDE.it.md](./WEBAPP_SETUP_GUIDE.it.md)
