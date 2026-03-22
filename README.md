# Decodium - Fork 9H1SR 1.5.4

This repository contains the macOS and Linux AppImage fork of Decodium, with FT2 anti-ghost filtering, FT2/FT4/FT8 decoder sync refinements, web-app parity controls, complete UI/web localization, secure settings hardening, safer download handling, and macOS bundle/release tooling maintained in this tree.

- Upstream base: `iu8lmc/Decodium-3.0-Codename-Raptor`
- Current fork release: `1.5.4`
- Update cycle: `1.5.3 -> 1.5.4`
- Upstream sync baseline: `2603182239`
- App bundle/executable on macOS: `ft2.app` / `ft2`
- Licence: GPLv3

## Release Highlights (`1.5.3 -> 1.5.4`)

- FT2 decoder hygiene:
- added a very-weak FT2 anti-ghost filter with payload cleanup, callsign sanity checks, and `ghostPass` / `ghostFilt` tracing.
- Decoder sync refresh:
- FT2 now uses `best 3 of 4 Costas sync`, FT4 uses `best 3 of 4` plus adaptive deeper subtraction, and FT8 uses `best 2 of 3` plus a fourth subtraction pass for weak-signal recovery.
- Web app parity:
- added `Monitoring ON/OFF`, FT2 `ASYNC` dB display, and `Hide CQ` / `Hide 73` filters to the remote console.
- Language and UI alignment:
- the web app now follows the language chosen in Decodium, all bundled languages are covered, the duplicate `English (UK)` menu entry is gone, and UTC/Astro dates now use localized month names.
- macOS packaging and release integrity:
- release bundles now normalize sounds to `Contents/Resources/sounds`, avoid shipping symlinked Hamlib helper tools, rewrite bundled Mach-O references to `@rpath`, and clean stale legacy bundle artifacts from reused build trees.
- Security and stability:
- secure settings fallback/import is hardened, downloader redirects and oversize transfers are guarded, LoTW defaults to `https`, and CAT exception logging is more explicit.
- Test and release coverage:
- HOTP/TOTP RFC vectors, secure settings tests, downloader tests, and Linux test-link fixes are included in this cycle.

## Release Targets

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey (best effort / experimental)
- Linux x86_64 AppImage

## Release Assets

- `decodium3-ft2-1.5.4-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.4-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.4-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.4-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.4-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.4-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.4-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.4-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.4-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.4-macos-monterey-x86_64.dmg` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.4-macos-monterey-x86_64.zip` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.4-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.4-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.4-linux-x86_64.AppImage.sha256.txt`

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
- Release notes (EN/IT/ES): [RELEASE_NOTES_1.5.4.md](RELEASE_NOTES_1.5.4.md)
- Changelog (EN/IT/ES): [CHANGELOG.md](CHANGELOG.md)
- GitHub release body (EN/IT/ES): [doc/GITHUB_RELEASE_BODY_1.5.4.md](doc/GITHUB_RELEASE_BODY_1.5.4.md)
- Docs index (EN): [doc/README.en-GB.md](doc/README.en-GB.md)
- Docs index (IT): [doc/README.it.md](doc/README.it.md)
- Docs index (ES): [doc/README.es.md](doc/README.es.md)
