# Documentation Notes (British English)

## Scope

Repository-specific notes for the macOS fork.

## Current Release Context

- Latest stable release: `v1.3.8`
- Targets: macOS Tahoe ARM64, Sequoia ARM64, Sequoia Intel, Monterey Intel (experimental), Linux x86_64 AppImage

## Build and Runtime Notes

### Build output

- `build/ft2.app/Contents/MacOS/ft2`
- helper executables in the same bundle path (`jt9`, `wsprd`)

### Shared memory on macOS

- This fork now uses `SharedMemorySegment` with file-backed `mmap` on Darwin.
- The release flow no longer depends on System V shared-memory `sysctl` tuning (`kern.sysv.shmmax/shmall`).

### Security/concurrency and CAT/network updates (v1.3.8)

- CAT/remote Configure is hardened to avoid forced FT2 mode from generic control packets.
- UDP control commands now require direct target id matching.
- Optional greyline toggle is available in Settings -> General.
- Active map path can render a distance badge in km/mi according to user units.
- Compact top controls were refined for DX-ped alignment on small displays.

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
- `RELEASE_NOTES_v1.3.8.md`
- `doc/GITHUB_RELEASE_BODY_v1.3.8.md`
- `doc/README.es.md`
- `doc/SECURITY_BUG_ANALYSIS_REPORT.md`
