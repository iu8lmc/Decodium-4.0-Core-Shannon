# Note di Documentazione (Italiano) - 1.5.7

Questo indice raccoglie la documentazione di rilascio del ciclo corrente del fork.

- Release corrente: `1.5.7`
- Ciclo aggiornamento: `1.5.6 -> 1.5.7`
- Focus principale: filtro di sanita' decode FT2, selezione affidabile del caller FT2 dalla Band Activity, continuita' della release Linux, e metadati release allineati.

## Cambi Tecnici Principali (`1.5.6 -> 1.5.7`)

- gli output FT2 type-4 passano ora in un filtro di plausibilita' prima di essere accettati nel path di decode attivo.
- `tests/test_qt_helpers.cpp` valida ora pattern special-event/slash ammessi e casi garbage FT2 da rigettare.
- il doppio click FT2 nella Band Activity sui messaggi standard `CQ` / `QRZ` arma ora direttamente il caller selezionato invece di dipendere dal vecchio path generico.
- il packaging Linux release include di nuovo il target `wsprd` ripristinato per la pubblicazione AppImage/release.
- metadati versione, default workflow, documenti release e note GitHub sono allineati a `1.5.7`.

## Artifact Release

- `decodium3-ft2-1.5.7-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.7-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.7-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.7-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.7-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.7-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.7-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.7-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.7-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.7-macos-monterey-x86_64.dmg` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.7-macos-monterey-x86_64.zip` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.7-macos-monterey-x86_64-sha256.txt` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.7-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.7-linux-x86_64.AppImage.sha256.txt`

## Requisiti Minimi Linux

- `x86_64`, dual-core 2.0 GHz+
- 4 GB RAM minima (8 GB consigliati)
- 500 MB liberi su disco
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

## Guida Avvio

Quarantena macOS:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l'AppImage e poi eseguendo il programma dalla cartella estratta.

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
- [RELEASE_NOTES_1.5.7.md](../RELEASE_NOTES_1.5.7.md)
- [doc/GITHUB_RELEASE_BODY_1.5.7.md](./GITHUB_RELEASE_BODY_1.5.7.md)
- [CHANGELOG.md](../CHANGELOG.md)
