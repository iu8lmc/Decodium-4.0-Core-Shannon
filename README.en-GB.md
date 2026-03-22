# Decodium (macOS/Linux Fork) - 1.5.4

This fork packages Decodium for macOS and Linux AppImage, with FT2 anti-ghost filtering, decoder sync upgrades across FT2/FT4/FT8, remote web-console parity controls, full language coverage, downloader hardening, secure settings protection, and corrected macOS bundle packaging.

Current stable release: `1.5.4`.

## Changes in 1.5.4 (`1.5.3 -> 1.5.4`)

- added FT2 anti-ghost handling for very weak malformed decodes, with `ghostPass` / `ghostFilt` tracing in `debug.txt`.
- refreshed decoder sync logic: FT2 `best 3 of 4 Costas`, FT4 `best 3 of 4` plus adaptive deeper subtraction, and FT8 `best 2 of 3` plus fourth subtraction pass.
- added web-app `Monitoring ON/OFF`, FT2 `ASYNC` dB display, and `Hide CQ` / `Hide 73` filters.
- the web app now follows the language selected in Decodium and covers all bundled languages.
- removed the duplicate `English (UK)` menu entry, localized the UTC/Astro date format, and kept `Decodium` while removing `v3.0 FT2 "Raptor"` from the user-facing title.
- corrected macOS release packaging: sounds now belong under `Contents/Resources`, bundled Hamlib helper tools are copied as real files, Frameworks/plugin dependencies are normalized to `@rpath`, and reused build trees are cleaned from stale legacy bundle artifacts.
- hardened secure settings fallback/import, file downloads, CAT exception logging, DXLab startup waits, and LoTW HTTPS defaults.
- extended automated coverage with RFC HOTP/TOTP vectors and dedicated downloader/secure-settings tests.

## Release Artifacts

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
- [RELEASE_NOTES_1.5.4.md](RELEASE_NOTES_1.5.4.md)
- [doc/GITHUB_RELEASE_BODY_1.5.4.md](doc/GITHUB_RELEASE_BODY_1.5.4.md)
- [CHANGELOG.md](CHANGELOG.md)
