# Decodium (macOS/Linux Fork) - 1.6.0

- Current stable release: `1.6.0`
- Update cycle: `1.5.9 -> 1.6.0`

## Changes in 1.6.0 (`1.5.9 -> 1.6.0`)

- promoted the active JT65 runtime path to native C++, including decoder orchestration, JT65 DSP/IO helpers, and removal of the old JT65 Fortran active-path sources from the build.
- added native JT9 fast/wide decoder building blocks plus broader legacy DSP/IO helpers and compare/regression utilities for the remaining JT migration work.
- fixed replies to non-standard or special-event callsigns that were incorrectly rejected as `*** bad message ***`.
- added Linux `aarch64` AppImage release support using a Debian Trixie ARM64 build path and GitHub Actions ARM runners.
- made `build-arm.sh` version-aware and CI-friendly, and permanently excluded `build-arm-output/` from git tracking.
- fixed the GCC 14 false-positive `stringop-overflow` issue in `LegacyDspIoHelpers.cpp` without breaking macOS Clang.
- fixed GCC/libstdc++ portability regressions in `jt9_wide_stage_compare.cpp` and `legacy_wsprio_compare.cpp`.
- aligned local version metadata, workflow defaults, release docs, and GitHub release notes to semantic version `1.6.0`.

## Release Targets

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey *(best effort / experimental)*
- Linux x86_64 AppImage
- Linux aarch64 AppImage *(Debian Trixie baseline)*

## Release Assets

- `decodium3-ft2-1.6.0-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.6.0-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.6.0-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.6.0-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.6.0-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.6.0-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.6.0-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.6.0-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.6.0-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.6.0-macos-monterey-x86_64.dmg` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.6.0-macos-monterey-x86_64.zip` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.6.0-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.6.0-linux-x86_64.AppImage`
- `decodium3-ft2-1.6.0-linux-x86_64.AppImage.sha256.txt`
- `decodium3-ft2-1.6.0-linux-aarch64.AppImage`
- `decodium3-ft2-1.6.0-linux-aarch64.AppImage.sha256.txt`

## Linux Minimum Requirements

Hardware:

- `x86_64` CPU with SSE2, or `aarch64` / ARM64 64-bit CPU
- dual-core 2.0 GHz or better, or equivalent modern ARM64 SoC
- 4 GB RAM minimum (8 GB recommended)
- 500 MB free disk space
- audio/CAT/serial/USB hardware suitable for weak-signal operation

Software:

- Qt5-capable X11 or Wayland desktop session
- Linux `x86_64` AppImage: `glibc >= 2.35`
- Linux `aarch64` AppImage: `glibc >= 2.38` *(Debian Trixie baseline)*
- `libfuse2` / FUSE2 for direct AppImage mounting
- ALSA, PulseAudio, or PipeWire
- serial/USB access permissions for CAT or external devices

## Startup Guidance

If macOS blocks startup:

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

## Related Files

- [README.md](README.md)
- [README.it.md](README.it.md)
- [README.es.md](README.es.md)
- [RELEASE_NOTES_1.6.0.md](RELEASE_NOTES_1.6.0.md)
- [doc/GITHUB_RELEASE_BODY_1.6.0.md](doc/GITHUB_RELEASE_BODY_1.6.0.md)
