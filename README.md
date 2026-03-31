# Decodium - Fork 9H1SR 1.5.9

This repository contains the maintained macOS and Linux AppImage fork of Decodium, focused on FT2/FT4 operator workflow reliability, low-latency Linux transmit behavior, macOS stability, and reproducible macOS/Linux release engineering.

- Upstream base: `iu8lmc/Decodium-3.0-Codename-Raptor`
- Current fork release: `1.5.9`
- Update cycle: `1.5.8 -> 1.5.9`
- Upstream sync baseline: `2603182239`
- App bundle/executable on macOS: `ft2.app` / `ft2`
- Licence: GPLv3

## Release Highlights (`1.5.8 -> 1.5.9`)

- Linux FT2/FT4 TX timing:
- fixed the intermittent "carrier first, payload much later" behavior seen on older Ubuntu systems by removing standard FT2/FT4 TX generation from unnecessary decoder-side Fortran runtime contention.
- Linux low-latency TX path:
- added immediate post-waveform TX start for FT2/FT4 on Linux, CAT/PTT fallback start when rig feedback is late, lower-latency audio queue defaults, and Linux-specific low-latency audio category selection.
- FT2 startup alignment:
- Linux FT2 now uses zero artificial `delay_ms` and zero extra lead-in on the standard precomputed-wave path, so payload starts as soon as the radio/audio path is ready.
- TX completion hardening:
- FT2/FT4 on Linux now stop on actual modulator/audio completion instead of relying only on theoretical slot expiry, preventing truncated or half-muted transmissions.
- Reduced TX-path overhead:
- FT2/FT4 debug waveform dumps are now written only when the debug log is enabled, removing unnecessary disk I/O during normal transmit cycles.
- macOS shutdown stability:
- fixed a close-time crash caused by Qt widget ownership/destruction ordering in `MainWindow`, especially around status-bar widgets and progress controls.
- UI polish:
- fixed `Band Hopping` so the `QSOs to upload` field no longer turns red together with the `Band Hopping` button.
- New launcher branding:
- updated the FT2 application icon set for macOS and Linux release artifacts.
- Release alignment:
- local version metadata, README/doc sets, changelog, release notes, and GitHub release body are aligned to semantic version `1.5.9`.

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

No `.pkg` installers are produced in this fork release line.

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
- `libfuse2` / FUSE2 support if you want to mount the AppImage directly
- ALSA, PulseAudio, or PipeWire audio stack
- X11 or Wayland desktop session capable of running Qt5 AppImages
- access permissions for serial/USB devices (`dialout` / `uucp` / distro equivalent) when CAT or external radios are used
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

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l'AppImage e poi eseguendo il programma dalla cartella estratta.

Eseguire i seguenti comandi nel terminale:

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
- Release notes (EN/IT/ES): [RELEASE_NOTES_1.5.9.md](RELEASE_NOTES_1.5.9.md)
- Changelog (EN/IT/ES): [CHANGELOG.md](CHANGELOG.md)
- GitHub release body (EN/IT/ES): [doc/GITHUB_RELEASE_BODY_1.5.9.md](doc/GITHUB_RELEASE_BODY_1.5.9.md)
- Docs index (EN): [doc/README.en-GB.md](doc/README.en-GB.md)
- Docs index (IT): [doc/README.it.md](doc/README.it.md)
- Docs index (ES): [doc/README.es.md](doc/README.es.md)
