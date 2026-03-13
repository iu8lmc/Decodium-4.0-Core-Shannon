# Documentation Notes (English) - v1.4.6

## Scope

Repository-specific notes for the Decodium macOS fork and Linux AppImage release flow.

## Current Release Context

- Current release: `v1.4.6`
- Update cycle: `v1.4.5 -> v1.4.6`
- Targets: Apple Silicon Tahoe, Apple Silicon Sequoia, Apple Intel Sequoia, Apple Intel Monterey (experimental), Linux x86_64 AppImage

## Key Technical Changes (`v1.4.5 -> v1.4.6`)

- AutoCQ/QSO sequencing hardening:
- restored FIFO caller queue behavior from v1.3.8 baseline.
- stronger active-partner continuity during active QSOs.
- deferred RR73/73 retry flow generalized across FT2/FT8/FT4/FST4/Q65/MSK144.
- payload-token partner matching strengthened for edge decode formats.
- Logging/signoff consistency:
- deferred autolog context recovery refined after retry windows.
- reduced cases of premature/no-confirmation logging.
- reduced delayed logging from stale pending-state transitions.
- Decode/right-pane continuity:
- robust payload extraction for variable marker spacing and optional marker frames.
- reduced cases where valid partner messages stayed only in Band Activity pane.
- UI/runtime:
- fixed macOS `View -> World Map` hide/show behavior.
- improved splitter pane sizing logic for secondary decode/map panels.
- Linux settings tabs now use scroll wrapping to keep dialog action buttons reachable.
- added startup audio stream refresh on macOS for already-selected devices.
- Remote/web dashboard:
- added remote `set_tx_frequency` command endpoint wiring.
- web app now supports Rx+Tx set, dedicated Rx/Tx set, and per-mode frequency presets.
- maintained localized minimum-password hint (12 chars, IT/EN).

## Build and Runtime

- Local build binary: `build/ft2.app/Contents/MacOS/ft2`
- Helper binaries in bundle: `jt9`, `wsprd`
- Shared memory backend on macOS remains file-backed `mmap` (no `.pkg` sysctl bootstrap required).

## Linux Minimum Requirements

- Architecture: `x86_64` (64-bit)
- CPU: dual-core 2.0 GHz or better
- RAM: 4 GB minimum (8 GB recommended)
- Storage: at least 500 MB free
- Runtime/software:
- Linux with `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio, or PipeWire

## Linux AppImage Recommendation

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l'AppImage e poi eseguendo il programma dalla cartella estratta.

Eseguire i seguenti comandi nel terminale:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Gatekeeper Quarantine Command

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

## References

- [CHANGELOG.md](../CHANGELOG.md)
- [RELEASE_NOTES_v1.4.6.md](../RELEASE_NOTES_v1.4.6.md)
- [doc/GITHUB_RELEASE_BODY_v1.4.6.md](./GITHUB_RELEASE_BODY_v1.4.6.md)
- [doc/WEBAPP_SETUP_GUIDE.en-GB.md](./WEBAPP_SETUP_GUIDE.en-GB.md)
- [doc/WEBAPP_SETUP_GUIDE.it.md](./WEBAPP_SETUP_GUIDE.it.md)
- [doc/WEBAPP_SETUP_GUIDE.es.md](./WEBAPP_SETUP_GUIDE.es.md)
