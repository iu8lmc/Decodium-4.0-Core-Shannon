# Note di Documentazione (Italiano) - 1.5.6

Questo indice raccoglie la documentazione di rilascio del ciclo corrente del fork.

- Release corrente: `1.5.6`
- Ciclo aggiornamento: `1.5.5 -> 1.5.6`
- Focus principale: completamento del runtime promosso nativo C++ per FT8/FT4/FT2/Q65, decoder worker in-process, hardening TX/build, validazione di parita', e packaging macOS/Linux allineato.

## Cambi Tecnici Principali (`1.5.5 -> 1.5.6`)

- FT8, FT4, FT2 e Q65 usano ora il runtime promosso nativo C++ senza orchestrazione Fortran specifica nel path di decode attivo.
- gli entrypoint `jt9` / `jt9a` e diversi tool Q65/offline sono nativi C++ in questo tree.
- l'avvio dell'app principale non alloca piu' il vecchio bootstrap `jt9` basato su shared memory per il path worker FTX promosso.
- il TX FT2/FT4/Fox conserva snapshot delle wave precompute, lead-in piu' conservativo e tracing piu' ricco in `debug.txt`.
- il build Linux/macOS e' stato irrobustito contro cicli di librerie statiche GNU `ld`, strictness GCC 15, costruttori Qt5 e gap di compatibilita' C++11.
- la copertura di parita'/regressione cresce con nuovi stage-compare e test helper piu' ampi.
- viene mantenuto il layout macOS gia' validato nell'ultimo deploy riuscito per i target Tahoe, Sequoia e Intel.

## Artifact Release

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
- [RELEASE_NOTES_1.5.6.md](../RELEASE_NOTES_1.5.6.md)
- [doc/GITHUB_RELEASE_BODY_1.5.6.md](./GITHUB_RELEASE_BODY_1.5.6.md)
- [CHANGELOG.md](../CHANGELOG.md)
