# Decodium (macOS/Linux Fork) - 1.5.5

This repository carries the maintained macOS and Linux AppImage fork of Decodium.

- Current stable release: `1.5.5`
- Update cycle: `1.5.4 -> 1.5.5`

## Changes in 1.5.5 (`1.5.4 -> 1.5.5`)

- fixed macOS native `Preferences...` handling so menu-role heuristics no longer misroute to another action in translated UIs.
- made the `Settings` dialog scroll-safe on macOS, keeping the confirm buttons reachable on smaller displays and longer localized layouts.
- added persistent `jt9_subprocess.log` plus richer FT2 subprocess stdout/stderr diagnostics for decoder crash analysis.
- upgraded FT2 ADIF handling to `MODE=MFSK` + `SUBMODE=FT2` and migrate historical `MODE=FT2` records automatically with a backup file.
- improved audio-input recovery during startup/monitor activation so reopening `Settings > Audio` is no longer required to wake the stream.
- withdrew the incomplete experimental RTTY UI from the public release while keeping the work out of the end-user path.

## Release Targets

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey (best effort / experimental)
- Linux x86_64 AppImage

## Release Assets

- `decodium3-ft2-1.5.5-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.5-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.5-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.5-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.5-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.5-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.5-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.5-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.5-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.5-macos-monterey-x86_64.dmg` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.5-macos-monterey-x86_64.zip` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.5-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.5-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.5-linux-x86_64.AppImage.sha256.txt`

## Linux Minimum Requirements

Hardware:

- `x86_64` CPU
- dual-core 2.0 GHz or better
- 4 GB RAM minimum (8 GB recommended)
- at least 500 MB free disk space
- 1280x800 display or better recommended

Software:

- Linux `x86_64` with `glibc >= 2.35`
- `libfuse2` / FUSE2 support
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

- [RELEASE_NOTES_1.5.5.md](RELEASE_NOTES_1.5.5.md)
- [doc/GITHUB_RELEASE_BODY_1.5.5.md](doc/GITHUB_RELEASE_BODY_1.5.5.md)
- [CHANGELOG.md](CHANGELOG.md)
