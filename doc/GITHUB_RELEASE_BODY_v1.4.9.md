# Decodium 3 FT2 v1.4.9 (Fork 9H1SR)

## English

Release highlights (`v1.4.8 -> v1.4.9`):

- FT2 decoder:
- triggered-decode LLR scaling raised from `2.83` to `3.2`.
- all three FT2 LLR branches normalized before LDPC decode.
- adaptive channel estimation with MMSE-equalized FT2 bit metrics added via `ft2_channel_est.f90`.
- FT2 async/runtime:
- new FT2 async visualizer widget.
- S-meter now fed from the real FT2 decode path.
- FT2 async polling reduced to `100 ms`.
- deferred `postDecode()` / `write_all()` fan-out for lower UI latency.
- FT2 startup / AutoCQ:
- startup no longer forces FT2 mode.
- immediate first directed FT2 reply while CQ is now handled correctly.
- fresh AutoCQ partner lock now clears stale retry/miss counters.
- UI / Linux portability:
- new `Language` menu with persisted UI language.
- DX Cluster columns are now user-resizable and persistent.
- `JPLEPH` lookup extended for AppImage, Linux share paths, current working directory, and `CMAKE_SOURCE_DIR`.
- Linux AppImage now bundles `JPLEPH`.

Release assets:

- `decodium3-ft2-v1.4.9-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.9-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.9-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.9-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.9-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.9-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.9-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.9-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.9-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.9-macos-monterey-x86_64.dmg` *(best effort/experimental, if generated)*
- `decodium3-ft2-v1.4.9-macos-monterey-x86_64.zip` *(best effort/experimental, if generated)*
- `decodium3-ft2-v1.4.9-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, if generated)*
- `decodium3-ft2-v1.4.9-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.9-linux-x86_64.AppImage.sha256.txt`

Linux minimum requirements:

- `x86_64` CPU (dual-core 2.0 GHz+)
- 4 GB RAM minimum (8 GB recommended)
- at least 500 MB free disk space
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio, or PipeWire

If macOS blocks startup:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Linux AppImage recommendation:

To avoid issues caused by the AppImage read-only filesystem, start Decodium by extracting the AppImage first and then running the program from the extracted directory.

Run the following commands in a terminal:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

No `.pkg` installers are produced.

## Italiano

Punti principali (`v1.4.8 -> v1.4.9`):

- Decoder FT2:
- scalatura LLR triggered decode aumentata da `2.83` a `3.2`.
- normalizzazione di tutti e tre i rami LLR FT2 prima del decode LDPC.
- aggiunta channel estimation adattiva con metriche FT2 MMSE-equalized tramite `ft2_channel_est.f90`.
- Runtime / async FT2:
- nuovo visualizzatore FT2 async.
- S-meter alimentato dal decode FT2 reale.
- polling async FT2 ridotto a `100 ms`.
- fan-out `postDecode()` / `write_all()` differito per ridurre la latenza UI.
- Startup / AutoCQ FT2:
- l'avvio non forza piu' la modalita' FT2.
- la prima risposta diretta FT2 mentre CQ e' attivo viene ora gestita correttamente.
- il nuovo lock partner AutoCQ pulisce i contatori retry/miss sporchi.
- UI / portabilita' Linux:
- nuovo menu `Language` con lingua UI persistente.
- colonne DX Cluster ora ridimensionabili e persistenti.
- lookup `JPLEPH` esteso per AppImage, path share Linux, working directory e `CMAKE_SOURCE_DIR`.
- Linux AppImage ora include `JPLEPH`.

Asset release:

- `decodium3-ft2-v1.4.9-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.9-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.9-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.9-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.9-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.9-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.9-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.9-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.9-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.9-macos-monterey-x86_64.dmg` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-v1.4.9-macos-monterey-x86_64.zip` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-v1.4.9-macos-monterey-x86_64-sha256.txt` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-v1.4.9-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.9-linux-x86_64.AppImage.sha256.txt`

Requisiti minimi Linux:

- CPU `x86_64` (dual-core 2.0 GHz+)
- RAM minima 4 GB (consigliati 8 GB)
- almeno 500 MB liberi su disco
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

Se macOS blocca l'avvio:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Avvio consigliato AppImage Linux:

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l'AppImage e poi eseguendo il programma dalla cartella estratta.

Eseguire i seguenti comandi nel terminale:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

I pacchetti `.pkg` non vengono prodotti.

## Espanol

Resumen (`v1.4.8 -> v1.4.9`):

- Decoder FT2:
- escalado LLR del triggered decode aumentado de `2.83` a `3.2`.
- normalizacion de las tres ramas LLR FT2 antes del decode LDPC.
- anadida channel estimation adaptativa con metricas FT2 MMSE-equalized via `ft2_channel_est.f90`.
- Runtime / async FT2:
- nuevo visualizador FT2 async.
- S-meter alimentado desde el decode FT2 real.
- polling async FT2 reducido a `100 ms`.
- fan-out `postDecode()` / `write_all()` diferido para reducir latencia UI.
- Startup / AutoCQ FT2:
- el arranque ya no fuerza el modo FT2.
- la primera respuesta dirigida FT2 con CQ activo se gestiona ahora correctamente.
- el nuevo lock partner AutoCQ limpia contadores retry/miss sucios.
- UI / portabilidad Linux:
- nuevo menu `Language` con idioma UI persistente.
- columnas DX Cluster ahora redimensionables y persistentes.
- lookup `JPLEPH` extendido para AppImage, paths share Linux, working directory y `CMAKE_SOURCE_DIR`.
- Linux AppImage ahora incluye `JPLEPH`.

Artefactos release:

- `decodium3-ft2-v1.4.9-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.9-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.9-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.9-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.9-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.9-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.9-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.9-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.9-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.9-macos-monterey-x86_64.dmg` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.9-macos-monterey-x86_64.zip` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.9-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.9-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.9-linux-x86_64.AppImage.sha256.txt`

Requisitos minimos Linux:

- CPU `x86_64` (dual-core 2.0 GHz+)
- RAM 4 GB minimo (8 GB recomendado)
- al menos 500 MB libres en disco
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

Si macOS bloquea el inicio:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Recomendacion AppImage Linux:

Para evitar problemas debidos al sistema de archivos de solo lectura de las AppImage, se recomienda iniciar Decodium extrayendo primero la AppImage y ejecutando despues el programa desde la carpeta extraida.

Ejecutar los siguientes comandos en la terminal:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

No se generan instaladores `.pkg`.
