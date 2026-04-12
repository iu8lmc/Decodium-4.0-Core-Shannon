# Release Notes / Note di Rilascio - Fork v1.3.2

Date: 2026-02-28  
Scope: startup hang mitigation, startup file hardening, release/documentation alignment.

## English

### Summary

- Startup I/O rework to reduce UI hangs on macOS systems affected by long/blocking file reads.
- Asynchronous/safe loading introduced for:
  - `wsjtx.log` startup scan
  - `ignore.list`
  - `ALLCALL7.TXT`
  - old saved-audio cleanup tasks
- `CTY.DAT` hardening:
  - parser validation and oversize guards
  - fallback load from bundled file
  - safer reload flow and stale/bad file handling
- Additional defensive startup parsing for `grid.dat`, `sat.dat`, and `comments.txt`.
- Release baseline/version/docs aligned to `v1.3.2` (English + Italian).

### Release Artifacts

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

Note: `.pkg` is not required for this fork release flow after shared-memory mmap migration on macOS.

### Linux minimum requirements

- Architecture: `x86_64` (64-bit)
- CPU: dual-core 2.0 GHz or better
- RAM: 4 GB minimum (8 GB recommended)
- Storage: 500 MB free minimum
- Runtime:
  - `glibc >= 2.35`
  - `libfuse2` / FUSE2 support
  - ALSA, PulseAudio, or PipeWire audio stack

### If macOS blocks startup

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

## Italiano

### Sintesi

- Rework I/O di startup per ridurre hang UI su macOS nei sistemi colpiti da letture file lunghe/bloccanti.
- Caricamento asincrono/sicuro introdotto per:
  - scansione startup `wsjtx.log`
  - `ignore.list`
  - `ALLCALL7.TXT`
  - cleanup dei vecchi file audio salvati
- Hardening `CTY.DAT`:
  - validazione parser e guardie su file oversized
  - fallback di caricamento dal file bundle
  - flusso di reload piu' sicuro e gestione file bad/stale
- Parsing startup difensivo aggiuntivo su `grid.dat`, `sat.dat` e `comments.txt`.
- Baseline release/versione/documentazione allineata a `v1.3.2` (inglese + italiano).

### Artifact release

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

Nota: `.pkg` non e' richiesto nel flusso release di questo fork dopo la migrazione shared-memory a mmap su macOS.

### Requisiti minimi Linux

- Architettura: `x86_64` (64 bit)
- CPU: dual-core 2.0 GHz o superiore
- RAM: minimo 4 GB (consigliati 8 GB)
- Spazio disco: minimo 500 MB liberi
- Runtime:
  - `glibc >= 2.35`
  - supporto `libfuse2` / FUSE2
  - stack audio ALSA, PulseAudio o PipeWire

### Se macOS blocca l'avvio

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```
