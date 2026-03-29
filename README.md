# Decodium - Fork 9H1SR 1.5.6

This repository contains the maintained macOS and Linux AppImage fork of Decodium, focused on FT2 and on native C++ promoted-runtime migration for FT8, FT4, FT2, and Q65, plus packaging, build hardening, and release engineering.

- Upstream base: `iu8lmc/Decodium-3.0-Codename-Raptor`
- Current fork release: `1.5.6`
- Update cycle: `1.5.5 -> 1.5.6`
- Upstream sync baseline: `2603182239`
- App bundle/executable on macOS: `ft2.app` / `ft2`
- Licence: GPLv3

## Release Highlights (`1.5.5 -> 1.5.6`)

- Native promoted runtime:
- completed the active C++ runtime migration for FT8, FT4, FT2, and Q65, so the promoted decode path no longer depends on mode-specific Fortran orchestration for those modes.
- Worker architecture:
- expanded the in-process worker model across promoted FTX modes and removed the old `jt9` shared-memory bootstrap from main app startup for those paths.
- Native tools and utilities:
- promoted native C++ frontends and helpers for `jt9` / `jt9a`, `q65sim`, `q65code`, `q65_ftn_test`, `q65params`, `test_q65`, and `rtty_spec`.
- TX stability:
- hardened FT2/FT4/Fox/SuperFox transmit handling with precomputed-wave snapshots, safer lead-in timing, and additional `debug.txt` tracing.
- Build and packaging:
- fixed GNU `ld` static-library cycles, GCC 15 / Qt5 / C++11 compatibility issues, and kept the macOS bundle layout already validated by the previous successful deploy.
- Validation:
- expanded parity and regression coverage with `ft8_stage_compare`, `ft4_stage_compare`, `ft2_stage_compare`, `ft2_equalized_compare`, `q65_stage_compare`, `ft2_make_test_wav`, and broader `test_qt_helpers` coverage.
- Public release hygiene:
- the experimental/public RTTY user path remains hidden pending dedicated validation.

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
- Release notes (EN/IT/ES): [RELEASE_NOTES_1.5.6.md](RELEASE_NOTES_1.5.6.md)
- Changelog (EN/IT/ES): [CHANGELOG.md](CHANGELOG.md)
- GitHub release body (EN/IT/ES): [doc/GITHUB_RELEASE_BODY_1.5.6.md](doc/GITHUB_RELEASE_BODY_1.5.6.md)
- Docs index (EN): [doc/README.en-GB.md](doc/README.en-GB.md)
- Docs index (IT): [doc/README.it.md](doc/README.it.md)
- Docs index (ES): [doc/README.es.md](doc/README.es.md)
