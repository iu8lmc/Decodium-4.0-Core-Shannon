# Decodium - Fork 9H1SR 1.5.7

This repository contains the maintained macOS and Linux AppImage fork of Decodium, focused on FT2 decode quality, operator workflow reliability, and release engineering for the promoted native runtime tree.

- Upstream base: `iu8lmc/Decodium-3.0-Codename-Raptor`
- Current fork release: `1.5.7`
- Update cycle: `1.5.6 -> 1.5.7`
- Upstream sync baseline: `2603182239`
- App bundle/executable on macOS: `ft2.app` / `ft2`
- Licence: GPLv3

## Release Highlights (`1.5.6 -> 1.5.7`)

- FT2 decode sanity:
- added a plausibility gate for FT2 type-4 decodes so clearly impossible callsign-like payloads no longer surface as bogus messages such as `CAYOBTYZCV0` or `7SVPAYTXTIK`.
- FT2 operator workflow:
- fixed Band Activity double-click handling for FT2 standard `CQ` / `QRZ` lines, so valid callers such as `D2UY`, `K1RZ`, and `KL7J` arm reliably from the activity window.
- Regression coverage:
- added focused `test_qt_helpers` coverage for valid slash/special-event calls and for rejecting garbage FT2 type-4 outputs.
- Linux release continuity:
- restored the `wsprd` build target so Linux/AppImage workflows publish the expected runtime set again.
- Release alignment:
- local version metadata, workflow defaults, README/doc sets, changelog, release notes, and GitHub release body are aligned to semantic version `1.5.7`.
- Packaging continuity:
- macOS releases keep the folder/layout already validated by the previous successful deploy.

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

No `.pkg` installers are produced in this fork release line.

## Linux Minimum Requirements

Hardware:

- `x86_64` CPU
- dual-core 2.0 GHz or better
- 4 GB RAM minimum (8 GB recommended)
- at least 500 MB free disk space
- 1280x800 display or better recommended
- audio/CAT hardware suitable for amateur-radio weak-signal operation

Software:

- Linux `x86_64` with `glibc >= 2.35`
- `libfuse2` / FUSE2 support if you want to mount the AppImage directly
- ALSA, PulseAudio, or PipeWire audio stack
- a desktop session capable of running Qt5 AppImages
- network access recommended for NTP, PSK Reporter, DX Cluster, updater, and online station workflows

## Linux Local Build Note

The published AppImage already bundles the required Qt multimedia runtime. If you build Decodium locally on Ubuntu/Debian, install the minimum system multimedia packages too, otherwise the audio device lists may remain empty or disabled even though the AppImage works correctly.

Recommended minimum packages for local Ubuntu/Debian builds:

```bash
sudo apt update
sudo apt install \
  qtmultimedia5-dev \
  libqt5multimedia5 \
  libqt5multimedia5-plugins \
  libqt5multimediawidgets5 \
  libqt5multimediagsttools5 \
  libpulse-mainloop-glib0 \
  pulseaudio-utils \
  gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good
```

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

- English README: [README.en-GB.md](README.en-GB.md)
- Italian README: [README.it.md](README.it.md)
- Spanish README: [README.es.md](README.es.md)
- Release notes (EN/IT/ES): [RELEASE_NOTES_1.5.7.md](RELEASE_NOTES_1.5.7.md)
- Changelog (EN/IT/ES): [CHANGELOG.md](CHANGELOG.md)
- GitHub release body (EN/IT/ES): [doc/GITHUB_RELEASE_BODY_1.5.7.md](doc/GITHUB_RELEASE_BODY_1.5.7.md)
- Docs index (EN): [doc/README.en-GB.md](doc/README.en-GB.md)
- Docs index (IT): [doc/README.it.md](doc/README.it.md)
- Docs index (ES): [doc/README.es.md](doc/README.es.md)
