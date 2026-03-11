# Decodium 3 FT2 (macOS Fork) - v1.4.4

Fork maintained by **Salvatore Raccampo 9H1SR**.

## Project Description

This repository is a Decodium/WSJT-X fork focused on macOS and Linux packaging, FT2 runtime stability, and DXpedition workflows.

Latest stable release: `v1.4.4`.

## Changes in v1.4.4 (`v1.4.3 -> v1.4.4`)

- Added DXpedition certificate verification (`.dxcert`) with canonical payload hashing and HMAC-SHA256 signature checks.
- Added DXpedition certificate validity checks (activation window) and operator authorization checks against station callsign.
- Added `DXped` menu actions:
- `Load DXped Certificate...`
- `DXped Certificate Manager...`
- Added bundled DXped certificate tooling in `tools/` and install rules to ship tools with release artifacts.
- Enforced certificate gate before enabling DXped mode; DXped activation is blocked if certificate is missing/invalid/expired/unauthorized.
- Added DXped runtime protections:
- standard `processMessage()` flow is blocked while DXped mode FSM is active, avoiding AutoSeq collisions.
- DXped auto-sequencing is now called directly from decode paths.
- DXped CQ fallback now mirrors `tx6` into `tx5` when queue is empty and `tx5` is blank.
- FT2 Async L2 is now mandatory:
- forced ON in FT2 and forced OFF outside FT2.
- local and remote attempts to disable Async L2 in FT2 are ignored.
- Added ASYMX timing/visibility improvements:
- `GUARD`, `TX`, `RX`, `IDLE` progress states with dedicated coloring.
- 300 ms TX guard phase before first FT2 auto-TX attempt.
- explicit async buffer/counter state reset and timer state cleanup on toggle transitions.
- Improved decode UI correctness:
- fixed-pitch font enforcement in decode panes to preserve column alignment.
- AP/quality marker placement moved to line tail to avoid right-column misalignment.
- FT2 decode mode marker normalization (`~` to `+`) in both normal and async decode paths.
- safer double-click parsing by stripping right-side annotations before `DecodedText` parsing.
- Improved UDP multi-instance identity:
- UDP client id now derives from application name, improving distinction between parallel instances.
- Added Italian UI translations for async status and mandatory Async L2 messages.
- Updated release/version metadata and CI defaults to `v1.4.4`.

## Release Targets

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey (experimental / best effort)
- Linux x86_64 AppImage

## Linux Minimum Requirements

- Architecture: `x86_64` (64-bit)
- CPU: dual-core 2.0 GHz or better
- RAM: 4 GB minimum (8 GB recommended)
- Storage: at least 500 MB free for AppImage + logs/settings
- Runtime/software:
- Linux with `glibc >= 2.35` (Ubuntu 22.04 class or newer)
- `libfuse2` / FUSE2 support
- ALSA, PulseAudio, or PipeWire audio stack
- Station integration: CAT/audio hardware according to radio setup

## Linux AppImage Launch Recommendation

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l'AppImage e poi eseguendo il programma dalla cartella estratta.

Eseguire i seguenti comandi nel terminale:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## macOS Quarantine Command

If Gatekeeper blocks startup, run:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

## Build and Run (local)

```bash
cmake --build build -j6
./build/ft2.app/Contents/MacOS/ft2
```

## Documentation

- [README.md](README.md)
- [README.it.md](README.it.md)
- [README.es.md](README.es.md)
- [RELEASE_NOTES_v1.4.4.md](RELEASE_NOTES_v1.4.4.md)
- [CHANGELOG.md](CHANGELOG.md)
- [doc/GITHUB_RELEASE_BODY_v1.4.4.md](doc/GITHUB_RELEASE_BODY_v1.4.4.md)
- [doc/README.en-GB.md](doc/README.en-GB.md)
- [doc/README.it.md](doc/README.it.md)
- [doc/README.es.md](doc/README.es.md)

## Licence

GPLv3 inherited from upstream WSJT-X/Decodium codebase.
