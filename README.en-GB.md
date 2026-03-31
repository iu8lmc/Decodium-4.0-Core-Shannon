# Decodium (macOS/Linux Fork) - 1.5.8

This repository carries the maintained macOS and Linux AppImage fork of Decodium.

- Current stable release: `1.5.8`
- Update cycle: `1.5.7 -> 1.5.8`

## Changes in 1.5.8 (`1.5.7 -> 1.5.8`)

- completed the promoted native C++ runtime for FT8, FT4, FT2, Q65, MSK144, SuperFox, and FST4/FST4W without mode-specific Fortran residues on the active path.
- migrated the remaining FST4/FST4W decode-core, LDPC, shared-DSP helpers, and reference/simulator chain to native C++.
- removed the old promoted-mode tree residues such as `ana64`, `q65_subs`, MSK144/MSK40 legacy snapshots, and the historical SuperFox Fortran subtree once their native replacements were promoted.
- promoted native utility replacements for `encode77`, `hash22calc`, `msk144code`, `msk144sim`, `sfoxsim`, `sfrx`, `sftx`, `fst4sim`, `ldpcsim240_101`, and `ldpcsim240_74`.
- fixed the macOS close-time FFTW crash and hardened `MainWindow::dataSink` / `fastSink`.
- fixed Linux/GCC 15 build failures around `_q65_mask`, `pack28`, legacy tool linkage to migrated symbols, and the MSK40 off-by-one bug in `decodeframe40_native`.
- added broader `test_qt_helpers` and utility smoke coverage for shared DSP, FST4 parity/oracle behavior, and native Q65 compatibility.
- aligned local version metadata, workflow defaults, release docs, and GitHub release notes to semantic version `1.5.8`.

## Release Targets

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey (best effort / experimental)
- Linux x86_64 AppImage

## Release Assets

- `decodium3-ft2-1.5.8-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.8-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.8-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.8-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.8-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.8-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.8-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.8-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.8-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.8-macos-monterey-x86_64.dmg` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.8-macos-monterey-x86_64.zip` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.8-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.8-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.8-linux-x86_64.AppImage.sha256.txt`

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

- [RELEASE_NOTES_1.5.8.md](RELEASE_NOTES_1.5.8.md)
- [doc/GITHUB_RELEASE_BODY_1.5.8.md](doc/GITHUB_RELEASE_BODY_1.5.8.md)
- [CHANGELOG.md](CHANGELOG.md)
