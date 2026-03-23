# Decodium - Fork 9H1SR 1.5.5

This repository contains the macOS and Linux AppImage fork of Decodium, focused on FT2 plus FT4/FT8 decoder parity, macOS/Linux packaging, UI hardening, and release engineering maintained in this tree.

- Upstream base: `iu8lmc/Decodium-3.0-Codename-Raptor`
- Current fork release: `1.5.5`
- Update cycle: `1.5.4 -> 1.5.5`
- Upstream sync baseline: `2603182239`
- App bundle/executable on macOS: `ft2.app` / `ft2`
- Licence: GPLv3

## Release Highlights (`1.5.4 -> 1.5.5`)

- macOS Preferences handling:
- fixed the native macOS app-menu routing so `Preferences...` always opens real `Settings`, regardless of UI language.
- macOS Settings usability:
- the `Settings` dialog now wraps tab pages in scroll areas on macOS too, so `OK` stays reachable even with extended General-tab content.
- FT2 decoder diagnostics:
- added persistent `jt9_subprocess.log`, richer stdout/stderr capture, and clearer shared-memory failure reporting for FT2 decoder subprocess crashes.
- ADIF alignment:
- FT2 logging is now standardized as `MODE=MFSK` with `SUBMODE=FT2`, and older `MODE=FT2` entries are migrated automatically with backup preservation.
- Audio recovery:
- improved startup/monitor audio reopening and health checks so audio streams recover without requiring a manual round-trip through `Settings > Audio`.
- UI release hygiene:
- the experimental RTTY UI has been withdrawn from public access in `1.5.5` pending further validation.

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

No `.pkg` installers are produced in this fork release line.

## Linux Minimum Requirements

Hardware:

- `x86_64` CPU
- dual-core 2.0 GHz or better
- 4 GB RAM minimum (8 GB recommended)
- at least 500 MB free disk space
- 1280x800 display or better recommended
- radio/CAT/audio hardware according to your station setup

Software:

- Linux `x86_64` with `glibc >= 2.35`
- `libfuse2` / FUSE2 support for AppImage mounting
- ALSA, PulseAudio, or PipeWire audio stack
- a desktop session capable of running Qt5 AppImages
- network access recommended for NTP, DX Cluster, PSK Reporter, updater, and online station workflows

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
- Release notes (EN/IT/ES): [RELEASE_NOTES_1.5.5.md](RELEASE_NOTES_1.5.5.md)
- Changelog (EN/IT/ES): [CHANGELOG.md](CHANGELOG.md)
- GitHub release body (EN/IT/ES): [doc/GITHUB_RELEASE_BODY_1.5.5.md](doc/GITHUB_RELEASE_BODY_1.5.5.md)
- Docs index (EN): [doc/README.en-GB.md](doc/README.en-GB.md)
- Docs index (IT): [doc/README.it.md](doc/README.it.md)
- Docs index (ES): [doc/README.es.md](doc/README.es.md)
