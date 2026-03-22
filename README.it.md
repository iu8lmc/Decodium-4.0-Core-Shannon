# Decodium 3 FT2 (Fork macOS/Linux) - 1.5.3

Questo fork confeziona Decodium 3 FT2 per macOS e Linux AppImage, con fix di chiusura QSO FT2/FT4/FT8, hardening AutoCQ, aggiornamenti decoder FT2 da upstream, recovery audio all'avvio, supporto updater, controlli web app allineati, traduzioni UI complete e tooling certificati Decodium mantenuti in questo repository.

Release stabile corrente: `1.5.3`.

## Novita' 1.5.3 (`1.5.2 -> 1.5.3`)

- irrigidito `Wait Features + AutoSeq` in FT4/FT8, cosi' una risposta tardiva o una stazione occupata mette in pausa il ciclo TX corrente invece di far chiamare sopra un QSO gia' attivo.
- ripristinata la compatibilita' pratica Linux con `CQRLOG wsjtx remote`, mantenendo il comportamento storico della listen port UDP e usando `WSJTX` come client id di compatibilita'.
- corretto l'allineamento versione delle build locali: cambiando `fork_release_version.txt`, la ricompilazione aggiorna davvero la versione nel binario e non lascia stringhe `1.5.x` stantie.
- aggiunte vere traduzioni UI bundle in tedesco e francese, con fallback pulito a inglese se nelle impostazioni era salvata una lingua non realmente inclusa.
- allineati a `1.5.3` i default release e la documentazione, incluso il default del build Linux sperimentale Hamlib.

## Artifact Release

- `decodium3-ft2-1.5.3-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.3-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.3-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.3-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.3-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.3-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.3-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.3-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.3-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.3-macos-monterey-x86_64.dmg` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.3-macos-monterey-x86_64.zip` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.3-macos-monterey-x86_64-sha256.txt` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.3-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.3-linux-x86_64.AppImage.sha256.txt`

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

Pacchetti minimi consigliati per build locali Ubuntu/Debian:

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
- [RELEASE_NOTES_1.5.3.md](RELEASE_NOTES_1.5.3.md)
- [doc/GITHUB_RELEASE_BODY_1.5.3.md](doc/GITHUB_RELEASE_BODY_1.5.3.md)
- [CHANGELOG.md](CHANGELOG.md)
