# Decodium - Fork 9H1SR 1.5.8

This repository contains the maintained macOS and Linux AppImage fork of Decodium, focused on promoted native C++ runtime coverage for the FTX family, operator workflow reliability, and release engineering for macOS and Linux.

- Upstream base: `iu8lmc/Decodium-3.0-Codename-Raptor`
- Current fork release: `1.5.8`
- Update cycle: `1.5.7 -> 1.5.8`
- Upstream sync baseline: `2603182239`
- App bundle/executable on macOS: `ft2.app` / `ft2`
- Licence: GPLv3

## Release Highlights (`1.5.7 -> 1.5.8`)

- Native runtime closure:
- FT8, FT4, FT2, Q65, MSK144, SuperFox, and FST4/FST4W now ship from the promoted native C++ runtime path without mode-specific Fortran active/runtime residues.
- FST4 family promotion:
- migrated the remaining FST4/FST4W decode-core, LDPC, shared-DSP helpers, and reference/simulator chain to native C++, replacing the old `fst4_decode`, `decode240_*`, `osd*`, `fst4sim`, and `ldpcsim240_*` Fortran path.
- Q65/MSK144/SuperFox cleanup:
- removed the last promoted-mode tree residues such as `ana64`, `q65_subs`, MSK144/MSK40 legacy snapshots, and the historical SuperFox Fortran subtree after their native replacements were promoted.
- New native utilities:
- promoted native replacements for `encode77`, `hash22calc`, `msk144code`, `msk144sim`, `sfoxsim`, `sfrx`, `sftx`, `fst4sim`, `ldpcsim240_101`, and `ldpcsim240_74`.
- macOS stability:
- fixed the close-time crash caused by premature global `fftwf_cleanup()` while thread-local FFTW plans were still finalizing, and hardened `MainWindow::dataSink` / `fastSink`.
- Linux/GCC hardening:
- fixed Ubuntu/GCC build failures around `_q65_mask`, `pack28`, legacy tool linkage to migrated C++ symbols, and the MSK40 off-by-one bug in `decodeframe40_native`.
- Regression coverage:
- expanded `test_qt_helpers` and utility smoke coverage for shared DSP, FST4 parity/oracle behavior, native Q65 `ana64` compatibility, and promoted helper paths.
- Release alignment:
- local version metadata, workflow defaults, README/doc sets, changelog, release notes, and GitHub release body are aligned to semantic version `1.5.8`.

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

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l’AppImage e poi eseguendo il programma dalla cartella estratta.

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
- Release notes (EN/IT/ES): [RELEASE_NOTES_1.5.8.md](RELEASE_NOTES_1.5.8.md)
- Changelog (EN/IT/ES): [CHANGELOG.md](CHANGELOG.md)
- GitHub release body (EN/IT/ES): [doc/GITHUB_RELEASE_BODY_1.5.8.md](doc/GITHUB_RELEASE_BODY_1.5.8.md)
- Docs index (EN): [doc/README.en-GB.md](doc/README.en-GB.md)
- Docs index (IT): [doc/README.it.md](doc/README.it.md)
- Docs index (ES): [doc/README.es.md](doc/README.es.md)
