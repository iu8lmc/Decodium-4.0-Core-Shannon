# Decodium (Fork macOS/Linux) - 1.5.8

Questo repository contiene il fork Decodium mantenuto per macOS e Linux AppImage.

- Release stabile corrente: `1.5.8`
- Ciclo aggiornamento: `1.5.7 -> 1.5.8`

## Novita' 1.5.8 (`1.5.7 -> 1.5.8`)

- completato il runtime promosso nativo C++ per FT8, FT4, FT2, Q65, MSK144, SuperFox e FST4/FST4W senza residui Fortran specifici del modo nel path attivo.
- migrata a C++ nativo la catena residua FST4/FST4W composta da decode-core, LDPC, helper DSP condivisi e reference/simulator.
- rimossi i residui tree-only dei modi promossi, inclusi `ana64`, `q65_subs`, gli snapshot legacy MSK144/MSK40 e l'intero subtree storico SuperFox Fortran.
- promossi i sostituti nativi per `encode77`, `hash22calc`, `msk144code`, `msk144sim`, `sfoxsim`, `sfrx`, `sftx`, `fst4sim`, `ldpcsim240_101` e `ldpcsim240_74`.
- corretto il crash macOS in chiusura legato a `fftwf_cleanup()` e resi piu' robusti `MainWindow::dataSink` e `fastSink`.
- corretti i fallimenti Linux/GCC 15 relativi a `_q65_mask`, `pack28`, link legacy ai simboli C++ migrati e bug off-by-one MSK40 in `decodeframe40_native`.
- ampliata la copertura `test_qt_helpers` e smoke-test utility per DSP condiviso, parita' FST4/oracle e compatibilita' Q65 nativa.
- allineati metadati versione locali, default workflow, documentazione release e note GitHub alla semver `1.5.8`.

## Target Release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey (best effort / sperimentale)
- Linux x86_64 AppImage

## Asset Release

- `decodium3-ft2-1.5.8-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.8-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.8-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.8-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.8-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.8-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.8-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.8-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.8-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.8-macos-monterey-x86_64.dmg` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.8-macos-monterey-x86_64.zip` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.8-macos-monterey-x86_64-sha256.txt` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.8-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.8-linux-x86_64.AppImage.sha256.txt`

## Requisiti Minimi Linux

Hardware:

- CPU `x86_64` con supporto SSE2
- dual-core 2.0 GHz o meglio
- minimo 4 GB RAM (8 GB consigliati)
- almeno 500 MB liberi su disco
- display 1280x800 o superiore consigliato
- hardware audio/CAT/seriale/USB adatto all'operativita' weak-signal radioamatoriale

Software:

- Linux `x86_64` con `glibc >= 2.35`
- `libfuse2` / supporto FUSE2 se si vuole montare direttamente l'AppImage
- ALSA, PulseAudio o PipeWire
- sessione desktop X11 o Wayland capace di eseguire AppImage Qt5
- permessi di accesso ai device seriali/USB (`dialout`, `uucp` o equivalente distro) se si usa CAT o radio esterna
- accesso rete consigliato per NTP, PSK Reporter, DX Cluster, updater e workflow online

## Avvio

Se macOS blocca l'avvio:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l’AppImage e poi eseguendo il programma dalla cartella estratta.

Eseguire i seguenti comandi nel terminale:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Documentazione Collegata

- [RELEASE_NOTES_1.5.8.md](RELEASE_NOTES_1.5.8.md)
- [doc/GITHUB_RELEASE_BODY_1.5.8.md](doc/GITHUB_RELEASE_BODY_1.5.8.md)
- [CHANGELOG.md](CHANGELOG.md)
