# Decodium (macOS/Linux Fork) - 1.5.9

This repository carries the maintained macOS and Linux AppImage fork of Decodium.

- Current stable release: `1.5.9`
- Update cycle: `1.5.8 -> 1.5.9`

## Changes in 1.5.9 (`1.5.8 -> 1.5.9`)

- fixed intermittent Linux FT2/FT4 cases where the radio entered TX but the payload started much later, sometimes near the end of the transmit window.
- removed standard FT2/FT4 Linux TX generation from unnecessary global Fortran runtime contention, so slow decode activity no longer delays the outgoing payload path.
- added immediate post-waveform TX start and CAT/PTT fallback start on Linux FT2/FT4 when rig state feedback is late.
- reduced Linux TX audio queue latency, selected a lower-latency audio category, and set Linux FT2 startup alignment to zero extra delay/lead-in on the standard waveform path.
- changed Linux FT2/FT4 stop behavior so transmit completion follows real audio/modulator completion instead of theoretical slot timing only.
- limited FT2/FT4 debug waveform dump writes to sessions where debug logging is enabled, reducing normal TX-path disk I/O.
- fixed a macOS close-time crash caused by Qt widget ownership/destruction ordering in `MainWindow`.
- fixed the `Band Hopping` UI so `QSOs to upload` no longer turns red together with the `Band Hopping` button.
- refreshed the FT2 launcher icon set for macOS and Linux artifacts.
- aligned local version metadata, workflow defaults, release docs, and GitHub release notes to semantic version `1.5.9`.

## Release Targets

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey (best effort / experimental)
- Linux x86_64 AppImage

## Release Assets

- `decodium3-ft2-1.5.9-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.9-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.9-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.9-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.9-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.9-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.9-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.9-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.9-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.9-macos-monterey-x86_64.dmg` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.9-macos-monterey-x86_64.zip` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.9-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.9-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.9-linux-x86_64.AppImage.sha256.txt`

## Linux Minimum Requirements

Hardware:

- `x86_64` CPU with SSE2 support
- dual-core 2.0 GHz or better
- 4 GB RAM minimum (8 GB recommended)
- at least 500 MB free disk space
- 1280x800 display or better recommended
- audio/CAT/serial/USB hardware suitable for amateur-radio weak-signal operation

Software:

- Linux `x86_64` with `glibc >= 2.35`
- `libfuse2` / FUSE2 support if mounting the AppImage directly
- ALSA, PulseAudio, or PipeWire
- X11 or Wayland desktop session capable of running Qt5 AppImages
- access permissions for serial/USB devices (`dialout` / `uucp` / distro equivalent) when CAT or external radios are used
- network access recommended for NTP, PSK Reporter, DX Cluster, updater, and online station workflows

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

- [RELEASE_NOTES_1.5.9.md](RELEASE_NOTES_1.5.9.md)
- [doc/GITHUB_RELEASE_BODY_1.5.9.md](doc/GITHUB_RELEASE_BODY_1.5.9.md)
- [CHANGELOG.md](CHANGELOG.md)
