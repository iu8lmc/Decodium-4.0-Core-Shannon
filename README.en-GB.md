# Decodium (macOS/Linux Fork) - 1.5.7

This repository carries the maintained macOS and Linux AppImage fork of Decodium.

- Current stable release: `1.5.7`
- Update cycle: `1.5.6 -> 1.5.7`

## Changes in 1.5.7 (`1.5.6 -> 1.5.7`)

- added an FT2 type-4 plausibility filter so nonsense callsign-like payloads no longer leak into Band Activity as bogus decodes.
- fixed FT2 Band Activity double-click handling for standard `CQ` / `QRZ` messages, so valid callers such as `D2UY`, `K1RZ`, and `KL7J` can be armed reliably.
- added focused `test_qt_helpers` regression coverage for valid special-event/slash forms and invalid garbage FT2 outputs.
- restored the Linux `wsprd` target so Linux/AppImage release jobs can publish the expected binary set again.
- aligned local version metadata, workflow defaults, release docs, and GitHub release notes to semantic version `1.5.7`.
- kept the macOS folder/layout changes already validated by the last successful deploy.

## Release Targets

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey (best effort / experimental)
- Linux x86_64 AppImage

## Release Assets

- `decodium3-ft2-1.5.7-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.7-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.7-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.7-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.7-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.7-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.7-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.7-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.7-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.7-macos-monterey-x86_64.dmg` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.7-macos-monterey-x86_64.zip` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.7-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.7-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.7-linux-x86_64.AppImage.sha256.txt`

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

- [RELEASE_NOTES_1.5.7.md](RELEASE_NOTES_1.5.7.md)
- [doc/GITHUB_RELEASE_BODY_1.5.7.md](doc/GITHUB_RELEASE_BODY_1.5.7.md)
- [CHANGELOG.md](CHANGELOG.md)
