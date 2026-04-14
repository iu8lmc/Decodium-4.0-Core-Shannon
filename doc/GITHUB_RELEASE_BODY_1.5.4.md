# Decodium 1.5.4 (Fork 9H1SR)

## English

Release highlights (`1.5.3 -> 1.5.4`):

- added FT2 anti-ghost filtering for very weak decodes, with `ghostPass` / `ghostFilt` tracing and syntax cleanup for malformed weak payloads.
- upgraded decoder sync logic across FT2, FT4, and FT8 with `best 3 of 4` / `best 2 of 3` Costas handling, adaptive thresholds, and deeper subtraction passes.
- added web-app parity controls: `Monitoring ON/OFF`, FT2 `ASYNC` dB display, and `Hide CQ` / `Hide 73` activity filters.
- the web app now follows the UI language selected inside Decodium and covers all bundled app languages.
- improved title/version alignment, removed the duplicate `English (UK)` menu item, and localized the UTC/Astro date format to `Day Month Year` plus `UTC`.
- hardened secure settings fallback, file downloads, CAT exception logging, DXLab startup waits, and LoTW HTTPS defaults.
- corrected macOS app packaging so sounds live under `Contents/Resources/sounds`, Hamlib helper binaries are bundled as real files instead of Homebrew symlinks, Mach-O references are normalized to `@rpath`, and stale legacy bundle artifacts are cleaned from reused build trees.
- extended test coverage with RFC HOTP/TOTP vectors plus secure-settings and downloader integration tests.

Release assets:

- `decodium3-ft2-1.5.4-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.4-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.4-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.4-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.4-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.4-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.4-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.4-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.4-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.4-macos-monterey-x86_64.dmg` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.4-macos-monterey-x86_64.zip` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.4-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.4-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.4-linux-x86_64.AppImage.sha256.txt`

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

Punti principali (`1.5.3 -> 1.5.4`):

- aggiunto il filtro FT2 anti-ghost per decode molto deboli, con tracing `ghostPass` / `ghostFilt` e pulizia sintattica dei payload weak-signal malformati.
- aggiornato il sync decoder su FT2, FT4 e FT8 con logica Costas `best 3 of 4` / `best 2 of 3`, soglie adattive e pass di sottrazione piu' profondi.
- aggiunti alla web app i controlli di parita': `Monitoring ON/OFF`, indicatore FT2 `ASYNC` in dB e filtri attivita' `Hide CQ` / `Hide 73`.
- la web app segue ora la lingua selezionata dentro Decodium e copre tutte le lingue UI bundle.
- migliorato l'allineamento titolo/versione, rimossa la voce duplicata `English (UK)` e localizzato il formato data UTC/Astro in `Giorno Mese Anno` piu' `UTC`.
- irrigiditi fallback secure settings, download file, logging eccezioni CAT, wait di startup DXLab e default HTTPS LoTW.
- corretto il packaging dell'app macOS: i suoni sono ora in `Contents/Resources/sounds`, gli helper Hamlib sono inclusi come file reali e non symlink Homebrew, i riferimenti Mach-O sono normalizzati a `@rpath`, e i residui legacy del bundle vengono rimossi anche nelle build riutilizzate.
- estesa la copertura test con vettori RFC HOTP/TOTP e test integrazione per secure settings e downloader.

Asset release:

- `decodium3-ft2-1.5.4-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.4-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.4-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.4-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.4-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.4-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.4-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.4-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.4-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.4-macos-monterey-x86_64.dmg` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.4-macos-monterey-x86_64.zip` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.4-macos-monterey-x86_64-sha256.txt` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.4-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.4-linux-x86_64.AppImage.sha256.txt`

Requisiti minimi Linux:

- `x86_64`, dual-core 2.0 GHz+, RAM minima 4 GB, 500 MB liberi
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

Resumen (`1.5.3 -> 1.5.4`):

- anadido el filtro FT2 anti-ghost para decodes muy debiles, con trazas `ghostPass` / `ghostFilt` y limpieza sintactica de payloads weak-signal malformados.
- actualizado el sync decoder en FT2, FT4 y FT8 con logica Costas `best 3 of 4` / `best 2 of 3`, umbrales adaptativos y pases de sustraccion mas profundos.
- anadidos a la web app los controles de paridad: `Monitoring ON/OFF`, indicador FT2 `ASYNC` en dB y filtros de actividad `Hide CQ` / `Hide 73`.
- la web app sigue ahora el idioma seleccionado dentro de Decodium y cubre todas las lenguas UI bundle.
- mejorada la alineacion titulo/version, eliminada la entrada duplicada `English (UK)` y localizado el formato fecha UTC/Astro a `Dia Mes Ano` mas `UTC`.
- endurecidos fallback secure settings, descargas de archivos, logging de excepciones CAT, waits de arranque DXLab y defaults HTTPS de LoTW.
- corregido el empaquetado de la app macOS: los sonidos viven ahora en `Contents/Resources/sounds`, los helpers de Hamlib se incluyen como archivos reales y no como symlinks de Homebrew, las referencias Mach-O se normalizan a `@rpath`, y los residuos legacy del bundle se eliminan tambien en arboles de build reutilizados.
- ampliada la cobertura de test con vectores RFC HOTP/TOTP y tests de integracion para secure settings y downloader.

Artefactos release:

- `decodium3-ft2-1.5.4-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.4-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.4-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.4-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.4-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.4-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.4-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.4-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.4-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.4-macos-monterey-x86_64.dmg` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.4-macos-monterey-x86_64.zip` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.4-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.4-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.4-linux-x86_64.AppImage.sha256.txt`

Requisitos minimos Linux:

- `x86_64`, dual-core 2.0 GHz+, RAM minima 4 GB, 500 MB libres
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
