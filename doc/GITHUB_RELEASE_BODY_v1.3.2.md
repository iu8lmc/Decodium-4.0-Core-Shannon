# Decodium Fork v1.3.2 (base: Decodium v3.0 SE Raptor)

## English

Release focused on startup hang mitigation and startup data-file hardening.

### Highlights

- Program title/version alignment to fork release `v1.3.2`.
- Startup freeze mitigation: expensive startup file I/O moved off the UI thread.
- Async and guarded startup loading for `wsjtx.log`, `ignore.list`, `ALLCALL7.TXT`, and old-file cleanup.
- `CTY.DAT` hardening with parser validation, size limits, fallback recovery, and safer reload behavior.
- Additional defensive parsing checks for `grid.dat`, `sat.dat`, and `comments.txt`.
- macOS matrix release for Tahoe arm64, Sequoia arm64, Sequoia x86_64.
- Linux x86_64 AppImage release.

### Linux minimum requirements

- Architecture: `x86_64`
- CPU: dual-core 2.0 GHz or better
- RAM: 4 GB minimum
- Runtime: `glibc >= 2.35`, `libfuse2`/FUSE2, ALSA/PulseAudio/PipeWire

### macOS quarantine command

If Gatekeeper blocks the app, run:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

### Artifacts

- `decodium3-ft2-v1.3.2-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.3.2-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.3.2-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.3.2-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.3.2-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.3.2-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.3.2-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.3.2-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.3.2-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.3.2-linux-x86_64.AppImage`
- `decodium3-ft2-v1.3.2-linux-x86_64.AppImage.sha256.txt`

---

## Italiano

Release focalizzata su mitigazione hang in avvio e hardening dei file dati di startup.

### Highlight

- Allineamento titolo/versione programma alla release fork `v1.3.2`.
- Mitigazione freeze in avvio: I/O pesante dei file startup spostato fuori dal thread UI.
- Caricamento startup asincrono e protetto per `wsjtx.log`, `ignore.list`, `ALLCALL7.TXT` e cleanup file vecchi.
- Hardening `CTY.DAT` con validazione parser, limiti dimensione, fallback recovery e reload piu' sicuro.
- Check difensivi aggiuntivi su parsing `grid.dat`, `sat.dat` e `comments.txt`.
- Release matrix macOS per Tahoe arm64, Sequoia arm64, Sequoia x86_64.
- Release Linux x86_64 AppImage.

### Requisiti minimi Linux

- Architettura: `x86_64`
- CPU: dual-core 2.0 GHz o superiore
- RAM: minimo 4 GB
- Runtime: `glibc >= 2.35`, `libfuse2`/FUSE2, ALSA/PulseAudio/PipeWire

### Comando quarantena macOS

Se Gatekeeper blocca l'app:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```
