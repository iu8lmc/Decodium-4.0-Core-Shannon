# Decodium 3 FT2 (macOS Fork) - v1.4.5

Fork maintained by **Salvatore Raccampo 9H1SR**.

## Project Description

This repository is a Decodium/WSJT-X fork focused on macOS and Linux packaging, FT2 runtime stability, and DXpedition workflows.

Latest stable release: `v1.4.5`.

## Changes in v1.4.5 (`v1.4.4 -> v1.4.5`)

- FT2 AutoCQ and QSO state machine fixes:
- stronger active-partner lock during ongoing QSO (manual override only with Ctrl/Shift).
- directed replies during CQ are now captured more reliably (base callsign aware).
- non-partner `73` messages no longer force wrong signoff transitions.
- stale delayed autolog timers are ignored safely.
- caller queue now avoids recently worked duplicates after log completion.
- FT2 signoff/logging reliability:
- improved deferred FT2 signoff path and snapshot-based autolog handoff.
- better RR73/73 completion handling before returning to CQ.
- retry behavior tuned (`MAX_TX_RETRIES=5`) with clean reset when DX call changes.
- FT2 decode/runtime quality improvements:
- FT2 decode column now reports `TΔ` (time since TX) instead of DT.
- fixed FT2 column alignment and marker handling (`~`, Unicode minus variants, fixed-width prefix reformat).
- weak async confirmations are preserved before near-duplicate suppression.
- decoder hardening update with relaxed `nharderror` threshold (`35 -> 48`).
- TX timing fixes:
- corrected FT2 TX ramp-up/ramp-down and timing latency for both TCI and soundcard operation.
- Remote/web and UI fixes:
- fixed remote dashboard startup/fetch initialization path.
- fixed `View -> Display a cascata` / waterfall activation behavior.
- restored localized web password hint (minimum 12 characters).
- FT2 UDP logger fix:
- fixed FT2 UDP decode payload extraction so external loggers (RumLog) do not lose the first callsign character.

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
- [RELEASE_NOTES_v1.4.5.md](RELEASE_NOTES_v1.4.5.md)
- [CHANGELOG.md](CHANGELOG.md)
- [doc/GITHUB_RELEASE_BODY_v1.4.5.md](doc/GITHUB_RELEASE_BODY_v1.4.5.md)
- [doc/README.en-GB.md](doc/README.en-GB.md)
- [doc/README.it.md](doc/README.it.md)
- [doc/README.es.md](doc/README.es.md)

## Licence

GPLv3 inherited from upstream WSJT-X/Decodium codebase.
