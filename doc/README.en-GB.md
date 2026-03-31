# Documentation Notes (English) - 1.5.9

This index groups the release-facing documentation for the current fork cycle.

- Current release: `1.5.9`
- Update cycle: `1.5.8 -> 1.5.9`
- Primary focus: low-latency Linux FT2/FT4 transmit reliability, macOS shutdown/UI stability, refreshed FT2 branding, and aligned release metadata.

## Key Technical Changes (`1.5.8 -> 1.5.9`)

- standard Linux FT2/FT4 TX no longer queues behind unnecessary decoder-side Fortran runtime contention on the normal C++ waveform path.
- Linux FT2/FT4 now use immediate post-waveform start, CAT fallback start, smaller TX audio queue defaults, and low-latency audio category selection.
- Linux FT2 now runs with zero extra delay and zero extra lead-in on the standard precomputed-wave path.
- Linux FT2/FT4 stop on real modulator/audio completion rather than slot timing only.
- FT2/FT4 debug waveform dumps are now limited to sessions with debug logging enabled.
- macOS close-time widget ownership/destruction ordering in `MainWindow` was fixed.
- the `Band Hopping` button no longer paints the `QSOs to upload` field red.
- macOS/Linux launcher icons were refreshed for FT2.
- version metadata, workflow defaults, release docs, and GitHub release notes are aligned to `1.5.9`.

## Release Artifacts

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

To avoid issues caused by the AppImage read-only filesystem, it is recommended to start Decodium by extracting the AppImage first and then running the program from the extracted directory.

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
- [RELEASE_NOTES_1.5.9.md](../RELEASE_NOTES_1.5.9.md)
- [doc/GITHUB_RELEASE_BODY_1.5.9.md](./GITHUB_RELEASE_BODY_1.5.9.md)
- [CHANGELOG.md](../CHANGELOG.md)
