# Documentation Notes (British English)

## Scope

Repository-specific notes for the macOS fork.

## Current Release Context

- Latest stable release: `v1.4.1`
- Targets: macOS Tahoe ARM64, Sequoia ARM64, Sequoia Intel, Monterey Intel (experimental), Linux x86_64 AppImage

## Build and Runtime Notes

### Build output

- `build/ft2.app/Contents/MacOS/ft2`
- helper executables in the same bundle path (`jt9`, `wsprd`)

### Shared memory on macOS

- This fork uses `SharedMemorySegment` with file-backed `mmap` on Darwin.
- The release flow no longer depends on System V shared-memory `sysctl` tuning (`kern.sysv.shmmax/shmall`).

### v1.4.1 consolidated highlights

- FT2 decode-flow stabilization with packed-row split + near-duplicate suppression (5-second window).
- Async L2 control shown only in FT2 mode and auto-disabled outside FT2.
- Startup auto mode-from-rig now one-shot; initial mode-switch responsiveness restored.
- Mode switch no longer forces waterfall to foreground.
- Remote web dashboard maturity improvements (LAN config, username/password auth, mobile/PWA behavior).
- Existing CAT/UDP/TCI hardening and map options (greyline + distance badge) retained.

### Release artifacts

- `decodium3-ft2-<version>-<asset-suffix>.dmg`
- `decodium3-ft2-<version>-<asset-suffix>.zip`
- `decodium3-ft2-<version>-<asset-suffix>-sha256.txt`
- `decodium3-ft2-<version>-linux-x86_64.AppImage`
- `decodium3-ft2-<version>-linux-x86_64.AppImage.sha256.txt`

### Linux minimum requirements

- Architecture: `x86_64`
- CPU: dual-core 2.0 GHz or better
- RAM: 4 GB minimum
- Runtime: `glibc >= 2.35`, `libfuse2`/FUSE2, ALSA/PulseAudio/PipeWire

### Linux AppImage recommendation

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

### Gatekeeper quarantine command

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

## References

- `CHANGELOG.md`
- `RELEASE_NOTES_v1.4.1.md`
- `doc/GITHUB_RELEASE_BODY_v1.4.1.md`
- `doc/WEBAPP_SETUP_GUIDE.en-GB.md`
- `doc/WEBAPP_SETUP_GUIDE.it.md`
- `doc/WEBAPP_SETUP_GUIDE.es.md`
- `doc/SECURITY_BUG_ANALYSIS_REPORT.md`
