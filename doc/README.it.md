# Note di Documentazione (Italiano) - 1.6.0

- Release corrente: `1.6.0`
- Ciclo aggiornamento: `1.5.9 -> 1.6.0`

## Cambi Tecnici Principali (`1.5.9 -> 1.6.0`)

- JT65 runtime attivo promosso al path decode nativo C++.
- Estesi i blocchi nativi JT9 fast/wide e gli helper DSP/IO condivisi per la migrazione JT legacy ancora residua.
- Le risposte verso callsign non standard o special-event non falliscono piu' con `*** bad message ***`.
- Aggiunto supporto release Linux AppImage ARM64 tramite build path Debian Trixie e runner GitHub ARM.
- Aggiunti fix di portabilita' GCC 14 e GCC/libstdc++ senza regressioni su macOS Clang.
- Metadati versione, default workflow, documenti release e note GitHub sono allineati a `1.6.0`.

## Artifact Release

- `decodium3-ft2-1.6.0-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.6.0-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.6.0-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.6.0-macos-monterey-x86_64.dmg` *(best effort / sperimentale, se generato)*
- `decodium3-ft2-1.6.0-linux-x86_64.AppImage`
- `decodium3-ft2-1.6.0-linux-aarch64.AppImage`

## Requisiti Minimi Linux

- CPU `x86_64` con SSE2 oppure CPU `aarch64` / ARM64 64-bit
- minimo 4 GB RAM, 500 MB liberi su disco
- sessione desktop X11 o Wayland capace di eseguire AppImage Qt5
- `glibc >= 2.35` per Linux `x86_64`
- `glibc >= 2.38` per Linux `aarch64` *(baseline Debian Trixie)*
- `libfuse2`, ALSA/PulseAudio/PipeWire e permessi seriale/USB secondo necessita'

## Guida Avvio

Workaround quarantena macOS:

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

- [README.it.md](../README.it.md)
- [RELEASE_NOTES_1.6.0.md](../RELEASE_NOTES_1.6.0.md)
- [doc/GITHUB_RELEASE_BODY_1.6.0.md](./GITHUB_RELEASE_BODY_1.6.0.md)
