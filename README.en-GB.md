# Decodium 3 FT2 (macOS Fork) - v1.4.6

Fork maintained by **Salvatore Raccampo 9H1SR**.

## Project Description

This repository is a Decodium/WSJT-X fork focused on macOS and Linux packaging, FT2 runtime stability, AutoCQ reliability, and DXpedition workflows.

Latest stable release: `v1.4.6`.

## Changes in v1.4.6 (`v1.4.5 -> v1.4.6`)

- AutoCQ/QSO engine hardening:
- restored stable FIFO caller queue behavior from the trusted v1.3.8 logic.
- stronger active-partner lock during ongoing QSO to prevent accidental station takeover.
- deferred RR73/73 retry handling generalized across FT2, FT8, FT4, FST4, Q65, and MSK144 paths.
- improved partner matching from normalized payload tokens, including edge decode formats.
- Logging/signoff correctness:
- recovered deferred autolog context after retry windows.
- reduced forced/no-confirmation logging cases.
- reduced delayed log completion caused by stale pending state.
- Decode/right-pane continuity:
- robust payload extraction for variable decode marker spacing and optional marker layouts.
- reduced cases where valid partner replies stayed only in Band Activity and were not promoted to Rx pane flow.
- Desktop UI/runtime fixes:
- fixed `View -> World Map` toggle so map visibility now follows menu state on macOS.
- splitter/pane layout logic improved to keep secondary decode/map panes coherent on Linux/macOS.
- Linux settings dialog tabs are now scrollable on small/HiDPI desktops, keeping action buttons reachable.
- macOS startup audio refresh added so previously selected devices initialize without manual reload.
- Remote dashboard/web app:
- added remote `set_tx_frequency` command wiring end-to-end.
- web UI now supports Rx+Tx set, dedicated Rx/Tx set buttons, and per-mode preset save/apply.
- localized access-password hint remains explicit (minimum 12 characters, IT/EN).

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
- [RELEASE_NOTES_v1.4.6.md](RELEASE_NOTES_v1.4.6.md)
- [CHANGELOG.md](CHANGELOG.md)
- [doc/GITHUB_RELEASE_BODY_v1.4.6.md](doc/GITHUB_RELEASE_BODY_v1.4.6.md)
- [doc/README.en-GB.md](doc/README.en-GB.md)
- [doc/README.it.md](doc/README.it.md)
- [doc/README.es.md](doc/README.es.md)

## Licence

GPLv3 inherited from upstream WSJT-X/Decodium codebase.
