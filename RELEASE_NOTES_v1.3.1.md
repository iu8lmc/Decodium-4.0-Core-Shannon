# Release Notes / Note di Rilascio - Fork v1.3.1

Date: 2026-02-28  
Scope: version alignment, Linux CI/release completion, cross-platform packaging refresh.

## English

### Summary

- Program title now reads fork release version from `FORK_RELEASE_VERSION` (no legacy hardcoded `1.2.1`).
- Linux build chain hardened:
  - Hamlib fallback for environments without `rig_get_conf2`
  - `libudev` dependency alignment in CI
  - executable verification/packaging corrected to `ft2`
- Release automation expanded with Linux x86_64 AppImage publishing.
- macOS release compatibility check updated for Tahoe runner (`compat_macos=26.0`).

### Release Artifacts

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

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

- Il titolo programma legge ora la release fork da `FORK_RELEASE_VERSION` (senza hardcode legacy `1.2.1`).
- Catena build Linux resa piu' robusta:
  - fallback Hamlib per ambienti senza `rig_get_conf2`
  - allineamento dipendenza `libudev` in CI
  - verifica/packaging eseguibili corretti su `ft2`
- Automazione release estesa con pubblicazione Linux x86_64 AppImage.
- Controllo compatibilita' release macOS aggiornato per runner Tahoe (`compat_macos=26.0`).

### Artifact release

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

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
