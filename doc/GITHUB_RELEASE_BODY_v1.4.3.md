# Decodium 3 FT2 v1.4.3 (Fork 9H1SR)

## English

Release highlights (`v1.4.2 -> v1.4.3`):

- Fixed Linux FT2 Async L2 crash seen at first decode.
- Hardened FT2 Async L2 C++/Fortran boundary:
  - strict max-row guards in Fortran (`ndecodes/nout <= 100`);
  - fixed-length safe parsing of async decode rows in C++;
  - explicit async buffer reset and row-count reset on each decode/toggle cycle.
- Tightened Wait Features + AutoSeq active-QSO protection:
  - active partner lock now uses runtime partner (`m_hisCall`) first,
  - lock engages earlier (`REPLYING`..`SIGNOFF`) when Wait Features + AutoSeq are enabled.
- Version/docs/workflows aligned to `v1.4.3`.

Release assets:

- `decodium3-ft2-v1.4.3-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.3-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.3-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.3-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.3-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.3-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.3-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.3-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.3-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.3-macos-monterey-x86_64.dmg` *(best effort/experimental, if generated)*
- `decodium3-ft2-v1.4.3-macos-monterey-x86_64.zip` *(best effort/experimental, if generated)*
- `decodium3-ft2-v1.4.3-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, if generated)*
- `decodium3-ft2-v1.4.3-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.3-linux-x86_64.AppImage.sha256.txt`

Linux minimum requirements:

- `x86_64` CPU (2 cores, 2.0 GHz+)
- 4 GB RAM minimum (8 GB recommended)
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio, or PipeWire

If macOS blocks startup:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Linux AppImage recommendation (read-only FS workaround):

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

No `.pkg` installers are produced.

## Italiano

Punti principali release (`v1.4.2 -> v1.4.3`):

- Corretto crash Linux FT2 Async L2 osservato al primo decode.
- Hardening boundary FT2 Async L2 C++/Fortran:
  - guardie rigide numero righe in Fortran (`ndecodes/nout <= 100`);
  - parsing sicuro a lunghezza fissa delle righe async in C++;
  - reset esplicito buffer async e contatori righe a ogni ciclo decode/toggle.
- Irrigidita protezione QSO attivo con Wait Features + AutoSeq:
  - lock partner attivo ora usa prima il partner runtime (`m_hisCall`),
  - lock attivo anticipato su `REPLYING`..`SIGNOFF` con Wait Features + AutoSeq abilitati.
- Allineamento versione/docs/workflow a `v1.4.3`.

Asset release:

- `decodium3-ft2-v1.4.3-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.3-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.3-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.3-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.3-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.3-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.3-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.3-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.3-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.3-macos-monterey-x86_64.dmg` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-v1.4.3-macos-monterey-x86_64.zip` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-v1.4.3-macos-monterey-x86_64-sha256.txt` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-v1.4.3-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.3-linux-x86_64.AppImage.sha256.txt`

Requisiti minimi Linux:

- CPU `x86_64` (2 core, 2.0 GHz+)
- RAM minima 4 GB (consigliati 8 GB)
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

Se macOS blocca l'avvio:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Avvio consigliato AppImage Linux (workaround filesystem read-only):

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

I pacchetti `.pkg` non vengono prodotti.

## Espanol

Resumen release (`v1.4.2 -> v1.4.3`):

- Corregido crash Linux FT2 Async L2 observado en el primer decode.
- Hardening del limite FT2 Async L2 C++/Fortran:
  - guardas estrictas de filas en Fortran (`ndecodes/nout <= 100`);
  - parsing seguro de longitud fija de filas async en C++;
  - reset explicito de buffers async y contadores por ciclo decode/toggle.
- Reforzada proteccion de QSO activo con Wait Features + AutoSeq:
  - lock de pareja activa ahora usa primero pareja runtime (`m_hisCall`),
  - lock activo desde `REPLYING` hasta `SIGNOFF` con Wait Features + AutoSeq habilitados.
- Version/docs/workflows alineados a `v1.4.3`.

Artefactos release:

- `decodium3-ft2-v1.4.3-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.3-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.3-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.3-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.3-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.3-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.3-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.3-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.3-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.3-macos-monterey-x86_64.dmg` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.3-macos-monterey-x86_64.zip` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.3-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.3-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.3-linux-x86_64.AppImage.sha256.txt`

Requisitos minimos Linux:

- CPU `x86_64` (2 nucleos, 2.0 GHz+)
- RAM 4 GB minimo (8 GB recomendado)
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

Si macOS bloquea el inicio:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Recomendacion AppImage Linux (workaround filesystem solo lectura):

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

No se generan instaladores `.pkg`.
