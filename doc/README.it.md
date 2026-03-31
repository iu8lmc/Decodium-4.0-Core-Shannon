# Note di Documentazione (Italiano) - 1.5.9

Questo indice raccoglie la documentazione di rilascio del ciclo corrente del fork.

- Release corrente: `1.5.9`
- Ciclo aggiornamento: `1.5.8 -> 1.5.9`
- Focus principale: affidabilita' TX Linux FT2/FT4 a bassa latenza, stabilita' macOS in chiusura/UI, nuova iconografia FT2 e metadati release allineati.

## Cambi Tecnici Principali (`1.5.8 -> 1.5.9`)

- il TX standard Linux FT2/FT4 non resta piu' in coda dietro al contenzioso inutile del runtime Fortran lato decode sul path waveform C++ normale.
- Linux FT2/FT4 usa ora start immediato post-waveform, fallback CAT, coda audio TX piu' piccola e categoria audio a bassa latenza.
- FT2 Linux lavora ora con zero ritardo extra e zero lead-in extra sul path waveform precomputato standard.
- Linux FT2/FT4 termina sul completamento reale di modulator/audio e non solo sul timing teorico dello slot.
- i dump waveform FT2/FT4 vengono scritti solo quando il debug log e' attivo.
- corretto l'ordine ownership/distruzione dei widget `MainWindow` in chiusura su macOS.
- il pulsante `Band Hopping` non colora piu' in rosso il campo `QSOs to upload`.
- aggiornata l'icona FT2 per launcher/artifact macOS e Linux.
- metadati versione, default workflow, documenti release e note GitHub sono allineati a `1.5.9`.

## Artifact Release

- `decodium3-ft2-1.5.9-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.9-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.9-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.9-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.9-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.9-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.9-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.9-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.9-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.9-macos-monterey-x86_64.dmg` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.9-macos-monterey-x86_64.zip` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.9-macos-monterey-x86_64-sha256.txt` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.9-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.9-linux-x86_64.AppImage.sha256.txt`

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

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l'AppImage e poi eseguendo il programma dalla cartella estratta.

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
- [RELEASE_NOTES_1.5.9.md](../RELEASE_NOTES_1.5.9.md)
- [doc/GITHUB_RELEASE_BODY_1.5.9.md](./GITHUB_RELEASE_BODY_1.5.9.md)
- [CHANGELOG.md](../CHANGELOG.md)
