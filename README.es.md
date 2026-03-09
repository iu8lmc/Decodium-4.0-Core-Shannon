# Decodium v3.0 SE "Raptor" - Fork 9H1SR v1.4.1 (Espanol)

Para la version completa bilingue (English + Italiano), ver [README.md](README.md).

## Resumen (v1.4.1)

Esta version consolida todos los fixes y nuevas funciones del ciclo `v1.4.0 -> v1.4.1`:

- Estabilizacion del flujo de decode FT2:
  - split de lineas packed (sin filas fusionadas/desalineadas),
  - supresion near-duplicate con ventana de 5 segundos,
  - prioridad para mantener la linea con mejor SNR,
  - filtro coherente en decode normal y async.
- Correccion de comportamiento Async L2:
  - visible solo en FT2,
  - desactivacion automatica al salir de FT2.
- Corregidas regresiones de startup/cambio de modo:
  - auto-seleccion startup por frecuencia del rig ahora one-shot (sin re-forzado continuo),
  - respuesta inicial al cambio de modo restaurada,
  - sin enfoque forzado del waterfall al cambiar modo.
- Maduracion del dashboard web remoto:
  - ajustes LAN desde el menu de la app (bind/puerto/user/token),
  - login usuario/password,
  - mejor UX mobile/PWA,
  - controles principales mas claros (mode/band/rx/tx/auto-cq).
- Hardening CAT/UDP/TCI de releases anteriores mantenido.
- Funciones mapa/runtime mantenidas:
  - toggle greyline opcional,
  - distancia en ruta activa del mapa en km/mi.
- `.pkg` no necesario: releases solo DMG/ZIP/SHA256 (macOS) y AppImage/SHA256 (Linux).

## Objetivos de release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey (12.x, experimental/best-effort)
- Linux x86_64 AppImage

## Hamlib en builds de release

- En macOS los workflows ejecutan `brew update` + `brew upgrade hamlib` antes de compilar.
- En Linux los workflows compilan Hamlib desde la ultima release oficial de GitHub e instalan en `/usr/local` antes de compilar `ft2`.
- Los logs CI muestran siempre la version efectiva usada (`rigctl --version`, `pkg-config --modversion hamlib`).

## Requisitos minimos Linux

- Arquitectura: `x86_64` (64 bits)
- CPU: dual-core 2.0 GHz o superior
- RAM: 4 GB minimo (8 GB recomendado)
- Disco: al menos 500 MB libres (AppImage + logs + configuracion)
- Runtime:
  - Linux con `glibc >= 2.35`
  - `libfuse2` / FUSE2 para ejecutar AppImage
  - audio ALSA, PulseAudio o PipeWire
- Integracion de estacion: hardware CAT/audio segun configuracion de radio

## Recomendacion de arranque AppImage en Linux

Para evitar problemas por el sistema de archivos de solo lectura de AppImage:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Comando macOS (cuarentena)

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

## Documentacion

- [RELEASE_NOTES_v1.4.1.md](RELEASE_NOTES_v1.4.1.md)
- [CHANGELOG.md](CHANGELOG.md)
- [doc/GITHUB_RELEASE_BODY_v1.4.1.md](doc/GITHUB_RELEASE_BODY_v1.4.1.md)
- [doc/README.es.md](doc/README.es.md)
- [doc/WEBAPP_SETUP_GUIDE.es.md](doc/WEBAPP_SETUP_GUIDE.es.md)
- [doc/WEBAPP_SETUP_GUIDE.en-GB.md](doc/WEBAPP_SETUP_GUIDE.en-GB.md)
- [doc/WEBAPP_SETUP_GUIDE.it.md](doc/WEBAPP_SETUP_GUIDE.it.md)
- [doc/SECURITY_BUG_ANALYSIS_REPORT.md](doc/SECURITY_BUG_ANALYSIS_REPORT.md)
