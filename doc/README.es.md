# Notas de Documentacion (Espanol)

## Alcance

Notas especificas del fork macOS dentro de este repositorio.

## Contexto de release actual

- Ultima release estable: `v1.3.8`
- Objetivos: macOS Tahoe ARM64, Sequoia ARM64, Sequoia Intel, Monterey Intel (experimental), Linux x86_64 AppImage

## Notas de build y runtime

### Salida de compilacion

- `build/ft2.app/Contents/MacOS/ft2`
- ejecutables auxiliares en el mismo bundle (`jt9`, `wsprd`)

### Memoria compartida en macOS

- Este fork usa `SharedMemorySegment` con backend `mmap` en Darwin.
- El flujo de release ya no depende de ajustes `sysctl` System V (`kern.sysv.shmmax/shmall`).

### Hardening CAT/red y mejoras mapa/UI (v1.3.8)

- Hardening CAT/Configure remoto para evitar forzado FT2 por paquetes genericos.
- Los comandos de control UDP ahora exigen target id directo.
- Opcion greyline en Settings -> General.
- Distancia en ruta activa del mapa en km/mi segun unidad configurada.
- Ajustes de layout de controles superiores para alineacion DX-ped en pantallas pequenas.

### Artefactos de release

- `decodium3-ft2-<version>-<asset-suffix>.dmg`
- `decodium3-ft2-<version>-<asset-suffix>.zip`
- `decodium3-ft2-<version>-<asset-suffix>-sha256.txt`
- `decodium3-ft2-<version>-linux-x86_64.AppImage`
- `decodium3-ft2-<version>-linux-x86_64.AppImage.sha256.txt`

### Requisitos minimos Linux

- Arquitectura: `x86_64`
- CPU: dual-core 2.0 GHz o superior
- RAM: 4 GB minimo
- Runtime: `glibc >= 2.35`, `libfuse2`/FUSE2, ALSA/PulseAudio/PipeWire

### Recomendacion AppImage

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

### Comando cuarentena Gatekeeper

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

## Referencias

- `CHANGELOG.md`
- `RELEASE_NOTES_v1.3.8.md`
- `doc/GITHUB_RELEASE_BODY_v1.3.8.md`
- `doc/SECURITY_BUG_ANALYSIS_REPORT.md`
