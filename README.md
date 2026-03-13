# Decodium v3.0 SE "Raptor" - Fork 9H1SR v1.4.6

This repository contains the macOS-focused fork build of Decodium 3 FT2, including Linux AppImage release flow.

- Upstream base: `iu8lmc/Decodium-3.0-Codename-Raptor`
- Fork release: `v1.4.6`
- App bundle/executable on macOS: `ft2.app` / `ft2`
- Licence: GPLv3

## Release Scope (`v1.4.5 -> v1.4.6`)

- AutoCQ/QSO sequencing reliability:
- restored stable FIFO caller-queue behavior (v1.3.8 logic baseline).
- strengthened active-partner lock to avoid call takeover during in-progress QSOs.
- deferred RR73/73 retry flow applied consistently across FT2, FT8, FT4, FST4, Q65 and MSK144 AutoCQ paths.
- improved partner-message matching from normalized payload tokens to avoid missing valid reports/73.
- FT8/FT2 logging correctness:
- recovered deferred autolog context safely after signoff retries.
- prevented forced logging before confirmed partner signoff.
- reduced late-log edge cases caused by stale pending-state transitions.
- Decode/right-pane continuity:
- improved payload extraction and message parsing robustness (variable marker spacing, optional mode marker).
- reduced cases where valid partner frames were left only on band-activity pane and not promoted to Rx flow.
- UI/runtime fixes:
- fixed `View -> World Map` visibility toggle behavior on macOS.
- improved splitter/layout handling to keep secondary decode pane and map pane stable on Linux/macOS.
- fixed oversized Linux settings dialog pages by wrapping tab contents in scroll areas (OK button always reachable).
- added macOS startup audio-stream refresh to avoid manual reload of already-selected devices.
- Remote dashboard/web app:
- added explicit remote `set_tx_frequency` command handling.
- web panel now supports Rx+Tx set, separate Rx/Tx set, and mode presets (save/apply per mode).
- kept localized password minimum-length hint (12 characters, IT/EN).

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
- Release notes (EN/IT/ES): [RELEASE_NOTES_v1.4.6.md](RELEASE_NOTES_v1.4.6.md)
- Changelog (EN/IT/ES): [CHANGELOG.md](CHANGELOG.md)
- GitHub release template (EN/IT/ES): [doc/GITHUB_RELEASE_BODY_v1.4.6.md](doc/GITHUB_RELEASE_BODY_v1.4.6.md)
- Docs index (EN): [doc/README.en-GB.md](doc/README.en-GB.md)
- Docs index (IT): [doc/README.it.md](doc/README.it.md)
- Docs index (ES): [doc/README.es.md](doc/README.es.md)
