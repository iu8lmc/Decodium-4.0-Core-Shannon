# Decodium (Fork macOS/Linux) - 1.5.4

Questo fork confeziona Decodium per macOS e Linux AppImage, con filtro FT2 anti-ghost, aggiornamenti sync decoder FT2/FT4/FT8, controlli web app allineati, copertura completa delle lingue UI/web, hardening downloader, protezione secure settings e packaging macOS corretto.

Release stabile corrente: `1.5.4`.

## Novita' 1.5.4 (`1.5.3 -> 1.5.4`)

- aggiunto il filtro FT2 anti-ghost per decode molto deboli, con tracing `ghostPass` / `ghostFilt` in `debug.txt`.
- aggiornato il sync decoder: FT2 `best 3 of 4 Costas`, FT4 `best 3 of 4` con sottrazione piu' profonda e adattiva, FT8 `best 2 of 3` con quarto pass.
- aggiunti alla web app `Monitoring ON/OFF`, indicatore FT2 `ASYNC` in dB e filtri `Hide CQ` / `Hide 73`.
- la web app segue ora la lingua scelta in Decodium e copre tutte le lingue bundle.
- rimossa la voce duplicata `English (UK)`, localizzato il formato data UTC/Astro e mantenuto `Decodium` rimuovendo `v3.0 FT2 "Raptor"` dal titolo lato utente.
- corretto il packaging release macOS: i suoni vanno in `Contents/Resources`, i tool Hamlib inclusi sono file reali e non symlink, le dipendenze Frameworks/plugin vengono normalizzate a `@rpath`, e le build tree riusate vengono ripulite dai residui legacy del vecchio bundle.
- irrigiditi fallback/import secure settings, download file, logging eccezioni CAT, wait di startup DXLab e default HTTPS LoTW.
- estesa la copertura automatizzata con vettori RFC HOTP/TOTP e test dedicati per downloader e secure settings.

## Artifact Release

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

## Requisiti Minimi Linux

Hardware:

- CPU `x86_64`, dual-core 2.0 GHz+
- RAM minima 4 GB (consigliati 8 GB)
- almeno 500 MB liberi su disco
- display 1280x800 o superiore consigliato
- hardware radio/CAT/audio secondo la propria stazione

Software:

- Linux `x86_64` con `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire
- ambiente desktop capace di eseguire AppImage Qt5
- accesso rete consigliato per NTP, DX Cluster, PSK Reporter, updater e funzioni online

## Nota Build Locale Linux

L'AppImage pubblicata include gia' il runtime Qt multimedia richiesto. Se compili Decodium localmente su Ubuntu/Debian, installa anche i pacchetti multimedia minimi di sistema, altrimenti le liste delle periferiche audio possono restare vuote o disabilitate anche se l'AppImage funziona correttamente.

```bash
sudo apt update
sudo apt install \
  qtmultimedia5-dev \
  libqt5multimedia5 \
  libqt5multimedia5-plugins \
  libqt5multimediawidgets5 \
  libqt5multimediagsttools5 \
  libpulse-mainloop-glib0 \
  pulseaudio-utils \
  gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good
```

## Guida Avvio

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

## File Collegati

- [README.md](README.md)
- [README.en-GB.md](README.en-GB.md)
- [README.es.md](README.es.md)
- [RELEASE_NOTES_1.5.4.md](RELEASE_NOTES_1.5.4.md)
- [doc/GITHUB_RELEASE_BODY_1.5.4.md](doc/GITHUB_RELEASE_BODY_1.5.4.md)
- [CHANGELOG.md](CHANGELOG.md)
