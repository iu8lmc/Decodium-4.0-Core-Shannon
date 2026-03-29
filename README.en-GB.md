# Decodium (macOS/Linux Fork) - 1.5.6

This repository carries the maintained macOS and Linux AppImage fork of Decodium.

- Current stable release: `1.5.6`
- Update cycle: `1.5.5 -> 1.5.6`

## Changes in 1.5.6 (`1.5.5 -> 1.5.6`)

- completed the promoted native C++ runtime migration for FT8, FT4, FT2, and Q65, removing mode-specific Fortran orchestration from the active path for those modes.
- expanded the in-process worker architecture and removed the old `jt9` shared-memory bootstrap from main-app startup for promoted FTX modes.
- promoted native C++ tools/frontends for `jt9`, `jt9a`, `q65sim`, `q65code`, `q65_ftn_test`, `q65params`, `test_q65`, and `rtty_spec`.
- hardened FT2/FT4/Fox transmit handling with precomputed-wave snapshots, safer lead-in timing, and extra `debug.txt` tracing.
- fixed GNU `ld` static-library cycle issues plus GCC 15 / Qt5 / C++11 compatibility breaks in tools, tests, and bridge code.
- expanded parity/regression validation with new stage-compare utilities and broader `test_qt_helpers` coverage.
- kept the macOS folder/layout changes already validated by the last successful deploy and aligned Tahoe, Sequoia, Intel Sequoia, Monterey, and Linux AppImage release targets.
- the incomplete public RTTY user path remains hidden pending dedicated validation.

## Release Targets

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey (best effort / experimental)
- Linux x86_64 AppImage

## Release Assets

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

Hardware:

- `x86_64` CPU
- dual-core 2.0 GHz or better
- 4 GB RAM minimum (8 GB recommended)
- at least 500 MB free disk space
- 1280x800 display or better recommended

Software:

- Linux `x86_64` with `glibc >= 2.35`
- `libfuse2` / FUSE2 support if mounting the AppImage directly
- ALSA, PulseAudio, or PipeWire
- a desktop session capable of running Qt5 AppImages

## Startup Guidance

If macOS blocks startup, run:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

To avoid issues caused by the AppImage read-only filesystem, it is recommended to start Decodium by extracting the AppImage first and then running the program from the extracted directory.

Run the following commands in a terminal:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Related Documentation

- [RELEASE_NOTES_1.5.6.md](RELEASE_NOTES_1.5.6.md)
- [doc/GITHUB_RELEASE_BODY_1.5.6.md](doc/GITHUB_RELEASE_BODY_1.5.6.md)
- [CHANGELOG.md](CHANGELOG.md)
