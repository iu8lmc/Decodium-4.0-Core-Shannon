# Note di Documentazione (Italiano) - 1.5.8

Questo indice raccoglie la documentazione di rilascio del ciclo corrente del fork.

- Release corrente: `1.5.8`
- Ciclo aggiornamento: `1.5.7 -> 1.5.8`
- Focus principale: chiusura completa del runtime promosso nativo C++ per la famiglia FTX, stabilita' macOS in chiusura/percorso dati, hardening Linux/GCC e metadati release allineati.

## Cambi Tecnici Principali (`1.5.7 -> 1.5.8`)

- FT8, FT4, FT2, Q65, MSK144, SuperFox e FST4/FST4W usano ora il runtime promosso nativo C++ senza residui Fortran specifici del modo nel path attivo.
- la catena residua FST4/FST4W composta da decode-core, LDPC, helper DSP condivisi e reference/simulator e' ora nativa C++.
- i residui tree-only dei modi promossi (`ana64`, `q65_subs`, snapshot MSK144/MSK40, subtree SuperFox Fortran) sono stati rimossi dopo la promozione nativa.
- i sostituti utility nativi coprono ora `encode77`, `hash22calc`, `msk144code`, `msk144sim`, `sfoxsim`, `sfrx`, `sftx`, `fst4sim`, `ldpcsim240_101` e `ldpcsim240_74`.
- sono stati corretti il crash FFTW in chiusura su macOS e i percorsi fragili di `MainWindow::dataSink` / `fastSink`.
- sono stati corretti i problemi Linux/GCC 15 relativi a `_q65_mask`, `pack28`, link ai simboli migrati e indicizzazione frame MSK40.
- metadati versione, default workflow, documenti release e note GitHub sono allineati a `1.5.8`.

## Artifact Release

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

- `x86_64` con SSE2, dual-core 2.0 GHz+
- minimo 4 GB RAM (8 GB consigliati)
- 500 MB liberi su disco
- `glibc >= 2.35`
- `libfuse2` / FUSE2 se si vuole montare l'AppImage direttamente
- ALSA, PulseAudio o PipeWire
- sessione desktop X11 o Wayland capace di eseguire AppImage Qt5

## Guida Avvio

Quarantena macOS:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l’AppImage e poi eseguendo il programma dalla cartella estratta.

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
- [RELEASE_NOTES_1.5.8.md](../RELEASE_NOTES_1.5.8.md)
- [doc/GITHUB_RELEASE_BODY_1.5.8.md](./GITHUB_RELEASE_BODY_1.5.8.md)
- [CHANGELOG.md](../CHANGELOG.md)
