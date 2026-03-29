# Decodium (Fork macOS/Linux) - 1.5.6

Questo repository contiene il fork Decodium mantenuto per macOS e Linux AppImage.

- Release stabile corrente: `1.5.6`
- Ciclo aggiornamento: `1.5.5 -> 1.5.6`

## Novita' 1.5.6 (`1.5.5 -> 1.5.6`)

- completata la migrazione del runtime promosso nativo C++ per FT8, FT4, FT2 e Q65, eliminando dal path attivo l'orchestrazione Fortran specifica di quei modi.
- estesa l'architettura in-process a worker e rimosso dal bootstrap principale il vecchio shared-memory bootstrap di `jt9` per i modi FTX promossi.
- promossi tool/front-end nativi C++ per `jt9`, `jt9a`, `q65sim`, `q65code`, `q65_ftn_test`, `q65params`, `test_q65` e `rtty_spec`.
- irrobustito il TX FT2/FT4/Fox con snapshot delle wave precompute, lead-in piu' prudente e tracing aggiuntivo in `debug.txt`.
- corretti problemi di build su GNU `ld`, GCC 15, Qt5 e C++11 nei bridge, nei tool e nei test.
- ampliata la copertura di parita'/regressione con nuovi stage-compare e un `test_qt_helpers` molto piu' esteso.
- mantenuto il layout/cartelle macOS gia' validato nell'ultimo deploy riuscito, allineando le release Tahoe, Sequoia, Intel Sequoia, Monterey e Linux AppImage.
- la UI RTTY pubblica resta nascosta in attesa di validazione dedicata.

## Target Release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey (best effort / sperimentale)
- Linux x86_64 AppImage

## Asset Release

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

Hardware:

- CPU `x86_64`
- dual-core 2.0 GHz o meglio
- minimo 4 GB RAM (8 GB consigliati)
- almeno 500 MB liberi su disco
- display 1280x800 o superiore consigliato

Software:

- Linux `x86_64` con `glibc >= 2.35`
- `libfuse2` / supporto FUSE2 se si vuole montare direttamente l'AppImage
- ALSA, PulseAudio o PipeWire
- sessione desktop capace di eseguire AppImage Qt5

## Avvio

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

## Documentazione Collegata

- [RELEASE_NOTES_1.5.6.md](RELEASE_NOTES_1.5.6.md)
- [doc/GITHUB_RELEASE_BODY_1.5.6.md](doc/GITHUB_RELEASE_BODY_1.5.6.md)
- [CHANGELOG.md](CHANGELOG.md)
