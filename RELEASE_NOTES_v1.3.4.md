# Release Notes / Note di Rilascio - Fork v1.3.4

Date: 2026-02-28  
Scope: TCI security/concurrency hardening, audio resilience, regex modernization, release alignment.

## English

### Summary

- Critical TCI binary parsing hardening:
  - strict binary header/payload length checks before frame access.
  - malformed/truncated frames are rejected safely.
- TCI pseudo-sync wait refactor:
  - removed nested `QEventLoop::exec()` in `mysleep1..8`.
  - replaced with cooperative wait logic and explicit timeout handling.
- TX concurrency hardening:
  - `foxcom_.wave` now read from guarded snapshots in audio/transceiver paths.
- C++/Fortran boundary hardening in TCI audio ingest:
  - `kin` clamping and bounded writes to shared decode buffers.
- macOS audio robustness:
  - Sequoia-safe output stop path.
  - input underrun handling no longer silently ignored.
- Time/auth consistency:
  - TOTP generation now uses NTP-corrected time.
- Qt6 migration progress:
  - `QRegExp` migrated to `QRegularExpression` in critical paths (`mainwindow`, `wsprnet`).

### Release Artifacts

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/experimental): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

Note: `.pkg` installers are intentionally not produced.

### Linux minimum requirements

- Architecture: `x86_64` (64-bit)
- CPU: dual-core 2.0 GHz or better
- RAM: 4 GB minimum (8 GB recommended)
- Storage: at least 500 MB free
- Runtime/software:
  - `glibc >= 2.35`
  - `libfuse2` / FUSE2 support
  - ALSA, PulseAudio, or PipeWire audio stack

### If macOS blocks startup

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

## Italiano

### Sintesi

- Hardening critico parser binario TCI:
  - controlli rigorosi su dimensione header/payload prima dell'accesso ai frame.
  - frame malformati/troncati scartati in sicurezza.
- Refactor attese pseudo-sync TCI:
  - rimossi i loop annidati `QEventLoop::exec()` in `mysleep1..8`.
  - sostituiti con attese cooperative con timeout esplicito.
- Hardening concorrenza TX:
  - `foxcom_.wave` letto tramite snapshot protetti nei percorsi audio/transceiver.
- Hardening boundary C++/Fortran in ingest audio TCI:
  - clamp `kin` e scritture buffer condiviso limitate.
- Robustezza audio macOS:
  - percorso stop output compatibile Sequoia.
  - gestione underrun input non piu' ignorata silenziosamente.
- Coerenza tempo/autenticazione:
  - la generazione TOTP usa ora il tempo corretto NTP.
- Avanzamento migrazione Qt6:
  - `QRegExp` migrato a `QRegularExpression` nei percorsi critici (`mainwindow`, `wsprnet`).

### Artifact release

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/sperimentale): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

Nota: i pacchetti `.pkg` non vengono prodotti intenzionalmente.

### Requisiti minimi Linux

- Architettura: `x86_64` (64 bit)
- CPU: dual-core 2.0 GHz o superiore
- RAM: minimo 4 GB (consigliati 8 GB)
- Spazio disco: almeno 500 MB liberi
- Runtime/software:
  - `glibc >= 2.35`
  - supporto `libfuse2` / FUSE2
  - stack audio ALSA, PulseAudio o PipeWire

### Se macOS blocca l'avvio

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```
