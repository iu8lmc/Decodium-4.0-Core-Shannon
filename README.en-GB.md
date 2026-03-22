# Decodium 3 FT2 (macOS/Linux Fork) - 1.5.3

This fork packages Decodium 3 FT2 for macOS and Linux AppImage, with FT2/FT4/FT8 QSO-closing fixes, AutoCQ hardening, FT2 decoder updates from upstream, startup audio recovery, updater support, web-app parity controls, complete UI translations, and Decodium certificate tooling maintained in this repository.

Current stable release: `1.5.3`.

## Changes in 1.5.3 (`1.5.2 -> 1.5.3`)

- tightened `Wait Features + AutoSeq` in FT4/FT8 so a busy or late partner reply pauses the current TX cycle instead of letting Decodium call over an active QSO.
- restored practical Linux `CQRLOG wsjtx remote` compatibility by keeping the historical UDP listen-port behaviour and using `WSJTX` as the compatibility client id.
- fixed local rebuild propagation so changing `fork_release_version.txt` updates the compiled app version instead of leaving stale `1.5.x` strings in local builds.
- added real bundled German and French UI translations and made the language menu fall back cleanly to English when a saved language is not bundled.
- aligned release defaults and documentation to `1.5.3`, including the experimental Linux Hamlib build default.

## Release Artifacts

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

## Linux Minimum Requirements

Hardware:

- `x86_64` CPU, dual-core 2.0 GHz+
- 4 GB RAM minimum (8 GB recommended)
- at least 500 MB free disk space
- 1280x800 display or better recommended
- radio/CAT/audio hardware according to station setup

Software:

- Linux `x86_64` with `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio, or PipeWire
- Qt5-capable desktop environment
- network access recommended for NTP, DX Cluster, PSK Reporter, updater, and online features

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

If macOS blocks startup:

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

## Related Files

- [README.md](README.md)
- [README.it.md](README.it.md)
- [README.es.md](README.es.md)
- [RELEASE_NOTES_1.5.3.md](RELEASE_NOTES_1.5.3.md)
- [doc/GITHUB_RELEASE_BODY_1.5.3.md](doc/GITHUB_RELEASE_BODY_1.5.3.md)
- [CHANGELOG.md](CHANGELOG.md)
