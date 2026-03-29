# Documentation Notes (English) - 1.5.6

This index groups the release-facing documentation for the current fork cycle.

- Current release: `1.5.6`
- Update cycle: `1.5.5 -> 1.5.6`
- Primary focus: promoted native C++ runtime completion for FT8/FT4/FT2/Q65, worker-based in-process decoding, transmit/build hardening, parity validation, and aligned macOS/Linux release packaging.

## Key Technical Changes (`1.5.5 -> 1.5.6`)

- FT8, FT4, FT2, and Q65 now use the promoted native C++ runtime path without mode-specific Fortran orchestration on the active decode path.
- `jt9` / `jt9a` shell entrypoints and several Q65/offline utilities are native C++ in this tree.
- main application startup no longer provisions the old `jt9` shared-memory bootstrap for the promoted FTX worker path.
- FT2/FT4/Fox TX handling now keeps precomputed-wave snapshots, longer lead-in timing, and richer `debug.txt` tracing for waveform issues.
- GNU `ld` static-library cycles, GCC 15 strictness, Qt5 constructor issues, and C++11 compatibility gaps have been hardened in the Linux/macOS build path.
- parity/regression coverage has been expanded with stage-compare utilities and broader helper tests.
- the macOS packaging layout already validated in the previous successful deploy is retained for Tahoe, Sequoia, and Intel release targets.

## Release Artifacts

- `decodium3-ft2-1.5.6-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.6-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.6-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.6-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.6-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.6-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.6-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.6-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.6-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.6-macos-monterey-x86_64.dmg` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.6-macos-monterey-x86_64.zip` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.6-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.6-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.6-linux-x86_64.AppImage.sha256.txt`

## Linux Minimum Requirements

- `x86_64` CPU, dual-core 2.0 GHz+
- 4 GB RAM minimum (8 GB recommended)
- 500 MB free disk space
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio, or PipeWire

## Startup Guidance

macOS quarantine:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Linux AppImage extract-run flow:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Related Files

- [README.md](../README.md)
- [README.en-GB.md](../README.en-GB.md)
- [README.it.md](../README.it.md)
- [README.es.md](../README.es.md)
- [RELEASE_NOTES_1.5.6.md](../RELEASE_NOTES_1.5.6.md)
- [doc/GITHUB_RELEASE_BODY_1.5.6.md](./GITHUB_RELEASE_BODY_1.5.6.md)
- [CHANGELOG.md](../CHANGELOG.md)
