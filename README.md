# Decodium v3.0 SE "Raptor" - Fork 9H1SR v1.4.5

This repository contains the macOS-focused fork build of Decodium 3 FT2, including Linux AppImage release flow.

- Upstream base: `iu8lmc/Decodium-3.0-Codename-Raptor`
- Fork release: `v1.4.5`
- App bundle/executable on macOS: `ft2.app` / `ft2`
- Licence: GPLv3

## Release Scope (`v1.4.4 -> v1.4.5`)

- FT2 AutoCQ flow hardening:
- improved partner locking during active QSOs with manual override only on Ctrl/Shift.
- immediate pickup of directed replies while calling CQ (base-call aware matching).
- non-partner `73` frames are ignored in auto-sequence.
- stale deferred autolog triggers are ignored safely.
- queued callers are deduplicated after recent logs to avoid reworking same station.
- FT2 signoff/logging stability:
- deferred FT2 signoff path with safer snapshot-based autolog handoff.
- RR73/73 partner handling refined to avoid premature CQ fallback.
- retry behavior tuned for AutoCQ (`MAX_TX_RETRIES=5`) with cleaner reset on partner change.
- FT2 decode rendering/timing updates:
- decode column now shows `TΔ` (time since TX) in FT2.
- fixed FT2 column alignment (`~`, minus glyph normalization, fixed-width prefix reformat).
- tighter async confirmation + dedupe handling preserves weak valid replies.
- decoder runtime tuning update: `nharderror` threshold relaxed (`35 -> 48`).
- TX timing path fixes:
- improved FT2 TX ramp-up/ramp-down and latency handling on both TCI and soundcard paths.
- remote/web + UI fixes:
- remote dashboard startup/fetch sequence fixed.
- `View -> Display a cascata` / waterfall window activation fixed.
- localized web password hint restored (minimum 12 chars).
- FT2 UDP/logger compatibility fix:
- fixed FT2 UDP decode payload slicing so external loggers (RumLog) no longer lose first callsign character.

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
- Release notes (EN/IT/ES): [RELEASE_NOTES_v1.4.5.md](RELEASE_NOTES_v1.4.5.md)
- Changelog (EN/IT/ES): [CHANGELOG.md](CHANGELOG.md)
- GitHub release template (EN/IT/ES): [doc/GITHUB_RELEASE_BODY_v1.4.5.md](doc/GITHUB_RELEASE_BODY_v1.4.5.md)
- Docs index (EN): [doc/README.en-GB.md](doc/README.en-GB.md)
- Docs index (IT): [doc/README.it.md](doc/README.it.md)
- Docs index (ES): [doc/README.es.md](doc/README.es.md)
