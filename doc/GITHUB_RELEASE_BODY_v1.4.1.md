# Decodium 3 FT2 v1.4.1 (Fork 9H1SR)

## English

Release highlights (`v1.4.0 -> v1.4.1`):

- Startup mode-switch responsiveness fixed (no delayed first mode changes).
- Startup FT advanced controls initialization fixed (no unexpected raw controls at boot).
- No forced waterfall foreground when switching FT2/FT4/FT8/FST4/FST4W.
- Startup auto mode-from-rig is now one-shot (prevents repeated CAT re-forcing).
- Polling transceiver tolerates transient poll failures better (fewer false CAT disconnects).
- Remote dashboard/API completed for Async L2, Dual Carrier, Alt 1/2 runtime control/state.
- World-map visibility persistence retained and stable in splitter layout.

Release assets:

- `decodium3-ft2-v1.4.1-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.1-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.1-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.1-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.1-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.1-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.1-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.1-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.1-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.1-macos-monterey-x86_64.dmg` *(best effort/experimental, if generated)*
- `decodium3-ft2-v1.4.1-macos-monterey-x86_64.zip` *(best effort/experimental, if generated)*
- `decodium3-ft2-v1.4.1-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, if generated)*
- `decodium3-ft2-v1.4.1-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.1-linux-x86_64.AppImage.sha256.txt`

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

Punti principali release (`v1.4.0 -> v1.4.1`):

- Corretto ritardo iniziale nel cambio modalita'.
- Corretta inizializzazione startup dei controlli FT avanzati (niente controlli grezzi inattesi all'avvio).
- Rimosso il passaggio forzato in primo piano del waterfall durante switch FT2/FT4/FT8/FST4/FST4W.
- Auto-selezione startup da frequenza radio resa one-shot (evita riforzatura CAT ripetuta).
- Polling transceiver piu' tollerante ai fallimenti transitori (meno false disconnessioni CAT).
- Dashboard/API remota completata per controllo/stato runtime Async L2, Dual Carrier, Alt 1/2.
- Persistenza visibilita' world-map confermata stabile nel layout splitter.

Asset release:

- `decodium3-ft2-v1.4.1-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.1-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.1-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.1-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.1-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.1-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.1-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.1-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.1-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.1-macos-monterey-x86_64.dmg` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-v1.4.1-macos-monterey-x86_64.zip` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-v1.4.1-macos-monterey-x86_64-sha256.txt` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-v1.4.1-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.1-linux-x86_64.AppImage.sha256.txt`

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

Resumen release (`v1.4.0 -> v1.4.1`):

- Corregido retraso inicial al cambiar modo.
- Corregida inicializacion startup de controles FT avanzados (sin controles "raw" inesperados al abrir).
- Eliminado enfoque forzado del waterfall al cambiar FT2/FT4/FT8/FST4/FST4W.
- Auto-seleccion startup por frecuencia de rig ahora one-shot (evita re-forzado CAT repetido).
- Polling transceiver mas tolerante a fallos transitorios (menos desconexiones CAT falsas).
- Dashboard/API remota completada para control/estado de Async L2, Dual Carrier y Alt 1/2.
- Persistencia de visibilidad world-map estable en layout splitter.

Artefactos release:

- `decodium3-ft2-v1.4.1-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.1-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.1-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.1-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.1-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.1-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.1-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.1-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.1-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.1-macos-monterey-x86_64.dmg` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.1-macos-monterey-x86_64.zip` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.1-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.1-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.1-linux-x86_64.AppImage.sha256.txt`

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
