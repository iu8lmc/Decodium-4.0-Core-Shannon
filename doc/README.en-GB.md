# Documentation Notes (English) - v1.4.5

## Scope

Repository-specific notes for the Decodium macOS fork and Linux AppImage release flow.

## Current Release Context

- Current release: `v1.4.5`
- Update cycle: `v1.4.4 -> v1.4.5`
- Targets: Apple Silicon Tahoe, Apple Silicon Sequoia, Apple Intel Sequoia, Apple Intel Monterey (experimental), Linux x86_64 AppImage

## Key Technical Changes (`v1.4.4 -> v1.4.5`)

- FT2 AutoCQ/QSO sequencing hardened:
- partner lock reliability improved during active QSO.
- directed replies while CQ are now matched more robustly (base-call aware).
- non-partner `73` frames are ignored in auto-sequence.
- deferred stale autolog timers are safely dropped.
- caller queue skips recently worked stations.
- FT2 signoff/logging flow stabilized:
- deferred signoff path refined with pending log snapshot handoff.
- RR73/73 completion handling tuned before fallback to CQ.
- retry behavior tuned with `MAX_TX_RETRIES=5`.
- FT2 decode/runtime updates:
- `TΔ` display introduced in FT2 decode column.
- FT2 alignment fixes for marker and Unicode minus variants.
- weak async confirmations preserved before dedupe.
- decoder threshold update: `nharderror 35 -> 48`.
- TX timing fixes:
- FT2 ramp-up/ramp-down and latency handling fixed for TCI and soundcard paths.
- Remote/web and UI fixes:
- fixed remote dashboard startup/fetch path.
- fixed `View -> Display a cascata` / waterfall activation.
- restored localized web password hint (minimum 12 chars).
- UDP logger compatibility:
- FT2 UDP payload extraction fixed so logger integrations (RumLog) keep full callsign.

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
- [RELEASE_NOTES_v1.4.5.md](../RELEASE_NOTES_v1.4.5.md)
- [doc/GITHUB_RELEASE_BODY_v1.4.5.md](./GITHUB_RELEASE_BODY_v1.4.5.md)
- [doc/WEBAPP_SETUP_GUIDE.en-GB.md](./WEBAPP_SETUP_GUIDE.en-GB.md)
- [doc/WEBAPP_SETUP_GUIDE.it.md](./WEBAPP_SETUP_GUIDE.it.md)
- [doc/WEBAPP_SETUP_GUIDE.es.md](./WEBAPP_SETUP_GUIDE.es.md)
