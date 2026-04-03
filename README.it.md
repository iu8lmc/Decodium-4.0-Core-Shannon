# Decodium (Fork macOS/Linux) - 1.6.0

- Release stabile corrente: `1.6.0`
- Ciclo aggiornamento: `1.5.9 -> 1.6.0`

## Novita' 1.6.0 (`1.5.9 -> 1.6.0`)

- promosso a C++ nativo il path runtime attivo JT65, inclusi orchestrazione decode, helper DSP/IO JT65 e rimozione dal build dei vecchi sorgenti Fortran JT65 del path attivo.
- aggiunti i blocchi nativi JT9 fast/wide e una copertura piu' ampia di helper legacy DSP/IO e compare/regression per il lavoro residuo di migrazione JT.
- corrette le risposte verso callsign non standard o special-event che venivano rigettate come `*** bad message ***`.
- aggiunto il supporto release Linux AppImage `aarch64` con build path ARM64 basato su Debian Trixie e runner GitHub Actions ARM.
- reso `build-arm.sh` sensibile alla versione, piu' adatto alla CI, ed esclusa permanentemente da git la cartella `build-arm-output/`.
- corretto il falso positivo GCC 14 `stringop-overflow` in `LegacyDspIoHelpers.cpp` senza rompere le build macOS Clang.
- corrette le regressioni di portabilita' GCC/libstdc++ in `jt9_wide_stage_compare.cpp` e `legacy_wsprio_compare.cpp`.
- allineati metadati versione locali, default workflow, documentazione release e note GitHub alla semver `1.6.0`.

## Target Release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey *(best effort / sperimentale)*
- Linux x86_64 AppImage
- Linux aarch64 AppImage *(baseline Debian Trixie)*

## Artifact Release

- `decodium3-ft2-1.6.0-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.6.0-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.6.0-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.6.0-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.6.0-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.6.0-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.6.0-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.6.0-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.6.0-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.6.0-macos-monterey-x86_64.dmg` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.6.0-macos-monterey-x86_64.zip` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.6.0-macos-monterey-x86_64-sha256.txt` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.6.0-linux-x86_64.AppImage`
- `decodium3-ft2-1.6.0-linux-x86_64.AppImage.sha256.txt`
- `decodium3-ft2-1.6.0-linux-aarch64.AppImage`
- `decodium3-ft2-1.6.0-linux-aarch64.AppImage.sha256.txt`

## Requisiti Minimi Linux

Hardware:

- CPU `x86_64` con SSE2, oppure CPU `aarch64` / ARM64 64-bit
- dual-core 2.0 GHz o meglio, oppure SoC ARM64 moderno equivalente
- minimo 4 GB RAM (8 GB raccomandati)
- almeno 500 MB liberi su disco
- hardware audio/CAT/seriale/USB adatto al weak-signal

Software:

- sessione desktop X11 o Wayland capace di eseguire AppImage Qt5
- AppImage Linux `x86_64`: `glibc >= 2.35`
- AppImage Linux `aarch64`: `glibc >= 2.38` *(baseline Debian Trixie)*
- `libfuse2` / FUSE2 per montare direttamente l'AppImage
- ALSA, PulseAudio o PipeWire
- permessi di accesso seriale/USB per CAT o dispositivi esterni

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

## File Correlati

- [README.md](README.md)
- [README.en-GB.md](README.en-GB.md)
- [README.es.md](README.es.md)
- [RELEASE_NOTES_1.6.0.md](RELEASE_NOTES_1.6.0.md)
- [doc/GITHUB_RELEASE_BODY_1.6.0.md](doc/GITHUB_RELEASE_BODY_1.6.0.md)
