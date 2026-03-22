# Decodium v3.0 SE "Raptor" - Fork 9H1SR 1.5.3

This repository contains the macOS and Linux AppImage fork of Decodium 3 FT2, with FT2/FT4/FT8 runtime fixes, AutoCQ hardening, FT2 decoder updates from upstream, startup audio recovery, updater support, web-app parity controls, complete UI translations, and Decodium certificate tooling maintained in this tree.

- Upstream base: `iu8lmc/Decodium-3.0-Codename-Raptor`
- Current fork release: `1.5.3`
- Update cycle: `1.5.2 -> 1.5.3`
- Upstream sync baseline: `2603182239`
- App bundle/executable on macOS: `ft2.app` / `ft2`
- Licence: GPLv3

## Release Highlights (`1.5.2 -> 1.5.3`)

- FT4 / FT8 Wait Features:
- tightened the `Wait Features + AutoSeq` busy-slot protection so an active QSO can pause the current TX cycle instead of transmitting over a late partner reply.
- Linux / CQRLOG compatibility:
- restored practical `wsjtx remote` compatibility by keeping the historical UDP listen-port behaviour and switching the compatibility client id to `WSJTX`.
- local build version alignment:
- fixed local rebuilds so changing `fork_release_version.txt` actually propagates to the compiled app instead of leaving stale version strings behind.
- UI translations:
- added real bundled German and French UI translations and made the language menu fall back cleanly to English when a non-bundled language was saved in settings.
- platform consistency:
- release defaults, documentation, and experimental build defaults are aligned to `1.5.3`.

## Release Targets

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey (best effort / experimental)
- Linux x86_64 AppImage

## Release Assets

- `decodium3-ft2-1.5.3-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.3-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.3-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.3-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.3-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.3-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.3-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.3-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.3-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.3-macos-monterey-x86_64.dmg` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.3-macos-monterey-x86_64.zip` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.3-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.3-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.3-linux-x86_64.AppImage.sha256.txt`

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
- Release notes (EN/IT/ES): [RELEASE_NOTES_1.5.3.md](RELEASE_NOTES_1.5.3.md)
- Changelog (EN/IT/ES): [CHANGELOG.md](CHANGELOG.md)
- GitHub release body (EN/IT/ES): [doc/GITHUB_RELEASE_BODY_1.5.3.md](doc/GITHUB_RELEASE_BODY_1.5.3.md)
- Docs index (EN): [doc/README.en-GB.md](doc/README.en-GB.md)
- Docs index (IT): [doc/README.it.md](doc/README.it.md)
- Docs index (ES): [doc/README.es.md](doc/README.es.md)
