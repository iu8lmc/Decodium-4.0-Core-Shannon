# Documentation Notes (English) - 1.5.4

This index groups the release-facing documentation for the current fork cycle.

- Current release: `1.5.4`
- Update cycle: `1.5.3 -> 1.5.4`
- Primary focus: FT2 anti-ghost filtering, FT2/FT4/FT8 decoder sync refresh, web-app parity controls, full UI/web language alignment, downloader/secure-settings hardening, and corrected macOS release packaging.

## Key Technical Changes (`1.5.3 -> 1.5.4`)

- added FT2 anti-ghost filtering for very weak malformed payloads, with diagnostic `ghostPass` / `ghostFilt` logging.
- refreshed decoder sync logic across FT2, FT4, and FT8 with `best 3 of 4` / `best 2 of 3` Costas handling and deeper adaptive subtraction.
- added web-app `Monitoring ON/OFF`, FT2 `ASYNC` dB display, and `Hide CQ` / `Hide 73` activity filters.
- the web app now follows the desktop UI language and covers all bundled app languages.
- removed the duplicate `English (UK)` menu entry and localized the UTC/Astro date format.
- corrected macOS release packaging so sounds live in `Contents/Resources`, bundled Hamlib helpers are real files, Framework/plugin references use `@rpath`, and reused build trees drop stale legacy bundle artifacts.
- hardened secure settings fallback/import, file download redirects/size limits, CAT exception logging, DXLab startup waits, and LoTW HTTPS defaults.
- extended automated coverage with RFC HOTP/TOTP vectors plus downloader and secure-settings tests.

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

- `x86_64` CPU, dual-core 2.0 GHz+
- 4 GB RAM minimum (8 GB recommended)
- 500 MB free disk space
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio, or PipeWire

## Linux Local Build Note

The AppImage already bundles the required Qt multimedia runtime. For local Ubuntu/Debian source builds, install the minimum system multimedia packages too, otherwise audio devices may appear empty or disabled.

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

macOS quarantine:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Linux AppImage extract-run flow:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Related Files

- [README.md](../README.md)
- [README.en-GB.md](../README.en-GB.md)
- [README.it.md](../README.it.md)
- [README.es.md](../README.es.md)
- [RELEASE_NOTES_1.5.4.md](../RELEASE_NOTES_1.5.4.md)
- [doc/GITHUB_RELEASE_BODY_1.5.4.md](./GITHUB_RELEASE_BODY_1.5.4.md)
- [CHANGELOG.md](../CHANGELOG.md)
