# Documentation Notes (English) - 1.5.8

This index groups the release-facing documentation for the current fork cycle.

- Current release: `1.5.8`
- Update cycle: `1.5.7 -> 1.5.8`
- Primary focus: full promoted native C++ runtime closure for the FTX family, macOS shutdown/data-path stability, Linux/GCC build hardening, and aligned release metadata.

## Key Technical Changes (`1.5.7 -> 1.5.8`)

- FT8, FT4, FT2, Q65, MSK144, SuperFox, and FST4/FST4W now use the promoted native C++ runtime path without mode-specific Fortran active/runtime residues.
- the remaining FST4/FST4W decode-core, LDPC, shared-DSP helpers, and reference/simulator chain are now native C++.
- the historical promoted-mode tree residues (`ana64`, `q65_subs`, MSK144/MSK40 snapshots, SuperFox Fortran subtree) have been removed after native promotion.
- native utility replacements now cover `encode77`, `hash22calc`, `msk144code`, `msk144sim`, `sfoxsim`, `sfrx`, `sftx`, `fst4sim`, `ldpcsim240_101`, and `ldpcsim240_74`.
- macOS close-time FFTW crashes and `MainWindow::dataSink` / `fastSink` crash-prone paths were hardened.
- Linux/GCC 15 build issues around `_q65_mask`, `pack28`, migrated symbol linkage, and MSK40 frame indexing were fixed.
- version metadata, workflow defaults, release docs, and GitHub release notes are aligned to `1.5.8`.

## Release Artifacts

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

- `x86_64` CPU with SSE2, dual-core 2.0 GHz+
- 4 GB RAM minimum (8 GB recommended)
- 500 MB free disk space
- `glibc >= 2.35`
- `libfuse2` / FUSE2 if mounting the AppImage directly
- ALSA, PulseAudio, or PipeWire
- X11 or Wayland desktop session capable of running Qt5 AppImages

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
- [RELEASE_NOTES_1.5.8.md](../RELEASE_NOTES_1.5.8.md)
- [doc/GITHUB_RELEASE_BODY_1.5.8.md](./GITHUB_RELEASE_BODY_1.5.8.md)
- [CHANGELOG.md](../CHANGELOG.md)
