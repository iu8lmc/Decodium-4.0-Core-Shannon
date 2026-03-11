# Decodium v3.0 SE "Raptor" - Fork 9H1SR v1.4.4

This repository contains the macOS-focused fork build of Decodium 3 FT2, including Linux AppImage release flow.

- Upstream base: `iu8lmc/Decodium-3.0-Codename-Raptor`
- Fork release: `v1.4.4`
- App bundle/executable on macOS: `ft2.app` / `ft2`
- Licence: GPLv3

## Release Scope (`v1.4.3 -> v1.4.4`)

- New DXpedition certificate workflow:
- certificate verification (`.dxcert`) with HMAC-SHA256 signature and canonical JSON payload.
- activation window and authorized-operator checks before DXped mode can start.
- new menu actions to load certificate and open DXped certificate manager.
- bundled DXped tools installed with release packages.
- New DXped runtime safeguards:
- standard `processMessage()` path blocked while DXped FSM is active.
- DXped auto-sequence integrated directly in decode paths.
- CQ fallback for empty queue (`tx5` mirrored from `tx6`).
- FT2 Async L2 hardening and UX updates:
- Async L2 is mandatory in FT2 (local/remote disable attempts are ignored).
- new ASYMX progress states: `GUARD`, `TX`, `RX`, `IDLE`.
- 300 ms guard window before first FT2 auto-TX attempt.
- explicit async state reset on mode/toggle transitions.
- Decode and UI consistency fixes:
- fixed-pitch font enforcement in decode panes.
- right-side AP/quality marker placement corrected.
- FT2 decode marker normalization (`~` to `+`).
- safer double-click decode parsing by stripping trailing annotations.
- UDP client id now derives from application name for cleaner multi-instance operation.
- Translations updated (`wsjtx_it.ts`) for async status labels and FT2 mandatory Async L2 messages.

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
- Linux with `glibc >= 2.35`
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

If macOS blocks startup, run:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

## Build and Run

```bash
cmake --build build -j6
./build/ft2.app/Contents/MacOS/ft2
```

## Documentation

- English README: [README.en-GB.md](README.en-GB.md)
- Italian README: [README.it.md](README.it.md)
- Spanish README: [README.es.md](README.es.md)
- Release notes (EN/IT/ES): [RELEASE_NOTES_v1.4.4.md](RELEASE_NOTES_v1.4.4.md)
- Changelog (EN/IT/ES): [CHANGELOG.md](CHANGELOG.md)
- GitHub release template (EN/IT/ES): [doc/GITHUB_RELEASE_BODY_v1.4.4.md](doc/GITHUB_RELEASE_BODY_v1.4.4.md)
- Docs index (EN): [doc/README.en-GB.md](doc/README.en-GB.md)
- Docs index (IT): [doc/README.it.md](doc/README.it.md)
- Docs index (ES): [doc/README.es.md](doc/README.es.md)
