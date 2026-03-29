# Decodium 1.5.6 (Fork 9H1SR)

## English

Release highlights (`1.5.5 -> 1.5.6`):

- completed the promoted native C++ runtime for FT8, FT4, FT2, and Q65, removing mode-specific Fortran orchestration from the active path for those modes.
- expanded the in-process worker model and removed the old `jt9` shared-memory bootstrap from main startup for promoted FTX modes.
- promoted native C++ tools/frontends for `jt9`, `jt9a`, `q65sim`, `q65code`, `q65_ftn_test`, `q65params`, `test_q65`, and `rtty_spec`.
- hardened FT2/FT4/Fox TX with cached precomputed-wave snapshots, safer lead-in timing, and better `debug.txt` tracing.
- fixed GNU `ld`, GCC 15, Qt5, and C++11 build/link issues affecting tools, tests, and runtime bridges.
- expanded parity/regression coverage with new stage-compare binaries and wider `test_qt_helpers` validation.
- retained the macOS folder/layout changes already proven by the previous successful deploy.

Release assets:

- `decodium3-ft2-1.5.6-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.6-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.6-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.6-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.6-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.6-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.6-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.6-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.6-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.6-macos-monterey-x86_64.dmg` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.6-macos-monterey-x86_64.zip` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.6-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.6-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.6-linux-x86_64.AppImage.sha256.txt`

Linux minimum requirements:

- `x86_64`, dual-core 2.0 GHz+, 4 GB RAM minimum, 500 MB free disk
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio, or PipeWire

If macOS blocks startup:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

To avoid issues caused by the AppImage read-only filesystem, it is recommended to start Decodium by extracting the AppImage first and then running the program from the extracted directory.

Run the following commands in a terminal:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

No `.pkg` installers are produced.

## Italiano

Punti principali (`1.5.5 -> 1.5.6`):

- completato il runtime promosso nativo C++ per FT8, FT4, FT2 e Q65, rimuovendo dal path attivo l'orchestrazione Fortran specifica di quei modi.
- esteso il modello in-process a worker e rimosso il vecchio bootstrap `jt9` a shared memory dall'avvio principale per i modi FTX promossi.
- promossi tool/front-end nativi C++ per `jt9`, `jt9a`, `q65sim`, `q65code`, `q65_ftn_test`, `q65params`, `test_q65` e `rtty_spec`.
- irrobustito il TX FT2/FT4/Fox con snapshot di wave precompute, lead-in piu' sicuro e tracing migliore in `debug.txt`.
- corretti problemi di build/link con GNU `ld`, GCC 15, Qt5 e C++11 che impattavano tool, test e bridge runtime.
- ampliata la validazione di parita'/regressione con nuovi binari stage-compare e piu' copertura in `test_qt_helpers`.
- mantenuto il layout/cartelle macOS gia' dimostrato nell'ultimo deploy riuscito.

Asset release:

- `decodium3-ft2-1.5.6-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.6-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.6-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.6-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.6-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.6-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.6-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.6-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.6-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.6-macos-monterey-x86_64.dmg` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.6-macos-monterey-x86_64.zip` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.6-macos-monterey-x86_64-sha256.txt` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.6-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.6-linux-x86_64.AppImage.sha256.txt`

Requisiti minimi Linux:

- `x86_64`, dual-core 2.0 GHz+, minimo 4 GB RAM, 500 MB liberi
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

Se macOS blocca l'avvio:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l'AppImage e poi eseguendo il programma dalla cartella estratta.

Eseguire i seguenti comandi nel terminale:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

Non vengono prodotti installer `.pkg`.

## Espanol

Resumen (`1.5.5 -> 1.5.6`):

- completado el runtime promovido nativo C++ para FT8, FT4, FT2 y Q65, retirando del camino activo la orquestacion Fortran especifica de esos modos.
- ampliado el modelo in-process con workers y retirado el viejo bootstrap `jt9` de shared memory del arranque principal para los modos FTX promovidos.
- promovidas herramientas/frontends nativos C++ para `jt9`, `jt9a`, `q65sim`, `q65code`, `q65_ftn_test`, `q65params`, `test_q65` y `rtty_spec`.
- endurecido el TX FT2/FT4/Fox con snapshots de ondas precomputadas, lead-in mas seguro y mejor trazado en `debug.txt`.
- corregidos problemas de build/link con GNU `ld`, GCC 15, Qt5 y C++11 que afectaban herramientas, tests y bridges runtime.
- ampliada la validacion de paridad/regresion con nuevos binarios stage-compare y mas cobertura en `test_qt_helpers`.
- mantenido el layout/carpetas macOS ya demostrado por el ultimo deploy correcto.

Artefactos release:

- `decodium3-ft2-1.5.6-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.6-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.6-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.6-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.6-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.6-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.6-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.6-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.6-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.6-macos-monterey-x86_64.dmg` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.6-macos-monterey-x86_64.zip` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.6-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.6-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.6-linux-x86_64.AppImage.sha256.txt`

Requisitos minimos Linux:

- `x86_64`, dual-core 2.0 GHz+, minimo 4 GB RAM, 500 MB libres
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

Si macOS bloquea el inicio:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Para evitar problemas debidos al sistema de archivos de solo lectura de las AppImage, se recomienda iniciar Decodium extrayendo primero la AppImage y ejecutando despues el programa desde la carpeta extraida.

Ejecutar los siguientes comandos en la terminal:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

No se generan instaladores `.pkg`.
