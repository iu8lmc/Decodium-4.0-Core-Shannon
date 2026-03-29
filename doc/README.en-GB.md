# Documentation Notes (English) - 1.5.7

This index groups the release-facing documentation for the current fork cycle.

- Current release: `1.5.7`
- Update cycle: `1.5.6 -> 1.5.7`
- Primary focus: FT2 decode sanity filtering, reliable FT2 Band Activity caller selection, Linux release continuity, and aligned release metadata.

## Key Technical Changes (`1.5.6 -> 1.5.7`)

- FT2 type-4 outputs now pass through a plausibility filter before they are accepted into the active decode path.
- `tests/test_qt_helpers.cpp` now validates accepted special-event/slash patterns and rejected garbage examples for FT2 type-4 text.
- FT2 Band Activity double-click on standard `CQ` / `QRZ` lines now directly arms the selected caller instead of depending on the older generic path.
- Linux release packaging once again includes the restored `wsprd` target for AppImage/release publishing.
- version metadata, workflow defaults, release docs, and GitHub release notes are aligned to `1.5.7`.

## Release Artifacts

- `decodium3-ft2-1.5.7-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.7-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.7-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.7-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.7-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.7-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.7-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.7-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.7-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.7-macos-monterey-x86_64.dmg` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.7-macos-monterey-x86_64.zip` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.7-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.7-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.7-linux-x86_64.AppImage.sha256.txt`

## Linux Minimum Requirements

- `x86_64` CPU, dual-core 2.0 GHz+
- 4 GB RAM minimum (8 GB recommended)
- 500 MB free disk space
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio, or PipeWire

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
- [RELEASE_NOTES_1.5.7.md](../RELEASE_NOTES_1.5.7.md)
- [doc/GITHUB_RELEASE_BODY_1.5.7.md](./GITHUB_RELEASE_BODY_1.5.7.md)
- [CHANGELOG.md](../CHANGELOG.md)
