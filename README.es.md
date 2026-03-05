# Decodium v3.0 SE "Raptor" - Fork 9H1SR v1.3.8 (Espanol)

Para la version completa bilingue (English + Italiano), ver [README.md](README.md).

## Resumen (v1.3.8)

Esta version del fork completa los fixes desde `v1.3.7` con enfoque en robustez CAT/UDP, mapa y UX:

- hardening CAT/Configure remoto: paquetes Configure genericos ya no fuerzan FT2;
- hardening de control UDP: se exige target id directo en mensajes de control;
- opcion de greyline en `Settings -> General` (desactivada = mapa siempre iluminado);
- distancia sobre la ruta activa del mapa en km/mi segun la unidad configurada;
- ajustes de layout de controles superiores en pantallas pequenas y realineacion del area DX-ped;
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

- [RELEASE_NOTES_v1.3.8.md](RELEASE_NOTES_v1.3.8.md)
- [CHANGELOG.md](CHANGELOG.md)
- [doc/GITHUB_RELEASE_BODY_v1.3.8.md](doc/GITHUB_RELEASE_BODY_v1.3.8.md)
- [doc/README.es.md](doc/README.es.md)
- [doc/SECURITY_BUG_ANALYSIS_REPORT.md](doc/SECURITY_BUG_ANALYSIS_REPORT.md)
