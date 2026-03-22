# Note di Documentazione (Italiano) - 1.5.4

Questo indice raccoglie la documentazione di rilascio del ciclo corrente del fork.

- Release corrente: `1.5.4`
- Ciclo aggiornamento: `1.5.3 -> 1.5.4`
- Focus principale: filtro FT2 anti-ghost, refresh sync decoder FT2/FT4/FT8, parita' della web app, allineamento completo UI/web delle lingue, hardening di downloader/secure settings e packaging release macOS corretto.

## Cambi Tecnici Principali (`1.5.3 -> 1.5.4`)

- aggiunto il filtro FT2 anti-ghost per payload molto deboli e malformati, con logging diagnostico `ghostPass` / `ghostFilt`.
- aggiornato il sync decoder su FT2, FT4 e FT8 con logica Costas `best 3 of 4` / `best 2 of 3` e sottrazione adattiva piu' profonda.
- aggiunti alla web app `Monitoring ON/OFF`, indicatore FT2 `ASYNC` in dB e filtri attivita' `Hide CQ` / `Hide 73`.
- la web app segue ora la lingua scelta nell'app desktop e copre tutte le lingue bundle.
- rimossa la voce duplicata `English (UK)` e localizzato il formato data UTC/Astro.
- corretto il packaging release macOS: suoni in `Contents/Resources`, helper Hamlib inclusi come file reali, dipendenze Framework/plugin normalizzate a `@rpath`, e rimozione dei residui legacy del vecchio bundle nelle build tree riusate.
- irrigiditi fallback/import secure settings, redirect/limiti size del downloader, logging eccezioni CAT, wait di startup DXLab e default HTTPS LoTW.
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

- `x86_64`, dual-core 2.0 GHz+, 4 GB RAM minima, 500 MB liberi
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

## Nota Build Locale Linux

L'AppImage include gia' il runtime Qt multimedia richiesto. Per le build locali da sorgente su Ubuntu/Debian, installa anche i pacchetti multimedia minimi di sistema, altrimenti le periferiche audio possono comparire vuote o disabilitate.

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

Quarantena macOS:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Flusso Linux AppImage extract-run:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## File Collegati

- [README.md](../README.md)
- [README.en-GB.md](../README.en-GB.md)
- [README.it.md](../README.it.md)
- [README.es.md](../README.es.md)
- [RELEASE_NOTES_1.5.4.md](../RELEASE_NOTES_1.5.4.md)
- [doc/GITHUB_RELEASE_BODY_1.5.4.md](./GITHUB_RELEASE_BODY_1.5.4.md)
- [CHANGELOG.md](../CHANGELOG.md)
