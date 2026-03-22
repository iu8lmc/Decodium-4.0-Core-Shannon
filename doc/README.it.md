# Note di Documentazione (Italiano) - 1.5.3

Questo indice raccoglie la documentazione di rilascio del ciclo corrente del fork.

- Release corrente: `1.5.3`
- Ciclo aggiornamento: `1.5.2 -> 1.5.3`
- Focus principale: compatibilita' CQRLOG, correttezza Wait Features FT4/FT8, propagazione versione locale, nuove traduzioni bundle DE/FR e allineamento release.

## Cambi Tecnici Principali (`1.5.2 -> 1.5.3`)

- ripristinata l'interoperabilita' Linux con `CQRLOG wsjtx remote`, mantenendo il comportamento storico della listen port UDP e usando `WSJTX` come client id di compatibilita'.
- corretta la propagazione versione nelle build locali, cosi' un cambio in `fork_release_version.txt` forza la versione corretta nel binario compilato.
- irrigidito `Wait Features + AutoSeq` in FT4/FT8, cosi' le collisioni su slot occupati mettono in pausa il ciclo TX corrente invece di chiamare sopra un QSO attivo.
- aggiunte traduzioni UI bundle reali in tedesco e francese e filtrato il menu lingue alle sole traduzioni realmente incluse.
- allineati a `1.5.3` i default release e la documentazione, incluso il default del workflow Linux Hamlib sperimentale.

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
- [RELEASE_NOTES_1.5.3.md](../RELEASE_NOTES_1.5.3.md)
- [doc/GITHUB_RELEASE_BODY_1.5.3.md](./GITHUB_RELEASE_BODY_1.5.3.md)
- [CHANGELOG.md](../CHANGELOG.md)
