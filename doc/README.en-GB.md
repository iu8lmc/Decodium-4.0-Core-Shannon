# Documentation Notes (English) - v1.4.4

## Scope

Repository-specific notes for the Decodium macOS fork and Linux AppImage release flow.

## Current Release Context

- Current release: `v1.4.4`
- Update cycle: `v1.4.3 -> v1.4.4`
- Targets: Apple Silicon Tahoe, Apple Silicon Sequoia, Apple Intel Sequoia, Apple Intel Monterey (experimental), Linux x86_64 AppImage

## Key Technical Changes (`v1.4.3 -> v1.4.4`)

- Added DXped certificate subsystem (`DXpedCertificate.hpp`) with canonical JSON payload verification and HMAC-SHA256 signature validation.
- Added DXped certificate runtime checks:
- activation time window validation.
- operator authorization validation against local callsign.
- Added DXped UI actions to load certificate and launch certificate manager.
- Added bundled Python tooling under `tools/` and install rules in `CMakeLists.txt`.
- Enforced certificate requirement before DXped mode activation.
- Integrated DXped auto-sequencing directly into decode flow and blocked standard `processMessage()` while DXped FSM is active.
- Enforced mandatory Async L2 for FT2 (local + remote disable prevention).
- Added ASYMX progress states (`GUARD/TX/RX/IDLE`) with guard timing.
- Improved decode rendering and parsing (fixed-pitch font enforcement, AP marker alignment, FT2 marker normalization, safer double-click parsing).
- Updated UDP client id to include application-name-derived rig suffix.
- Updated translations and release/workflow version defaults to `v1.4.4`.

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
- [RELEASE_NOTES_v1.4.4.md](../RELEASE_NOTES_v1.4.4.md)
- [doc/GITHUB_RELEASE_BODY_v1.4.4.md](./GITHUB_RELEASE_BODY_v1.4.4.md)
- [doc/WEBAPP_SETUP_GUIDE.en-GB.md](./WEBAPP_SETUP_GUIDE.en-GB.md)
- [doc/WEBAPP_SETUP_GUIDE.it.md](./WEBAPP_SETUP_GUIDE.it.md)
- [doc/WEBAPP_SETUP_GUIDE.es.md](./WEBAPP_SETUP_GUIDE.es.md)
