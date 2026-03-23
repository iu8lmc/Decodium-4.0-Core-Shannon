# Documentation Notes (English) - 1.5.5

This index groups the release-facing documentation for the current fork cycle.

- Current release: `1.5.5`
- Update cycle: `1.5.4 -> 1.5.5`
- Primary focus: macOS Preferences reliability across all UI languages, scroll-safe Settings on macOS, FT2 subprocess diagnostics, FT2 ADIF migration, audio-start recovery, and withdrawal of the experimental RTTY UI from the public release path.

## Key Technical Changes (`1.5.4 -> 1.5.5`)

- fixed macOS app-menu role routing so only the intended native entries use macOS special roles, regardless of translation language.
- made `Settings` pages scrollable on macOS as well as Linux to keep `OK` reachable.
- added `jt9_subprocess.log` with persistent FT2 decoder launch/error/termination traces and richer popup diagnostics.
- replaced opaque FT2 subprocess failures with clearer shared-memory and stdout/stderr reporting paths.
- migrated FT2 ADIF handling to ADIF 3.17 style `MODE=MFSK` plus `SUBMODE=FT2`, including automatic historical-log migration with backup.
- improved audio startup and monitor recovery through settings-style reopen plus health checks.
- removed the incomplete RTTY user path from menus/settings for this public release.

## Release Artifacts

- `decodium3-ft2-1.5.5-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.5-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.5-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.5-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.5-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.5-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.5-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.5-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.5-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.5-macos-monterey-x86_64.dmg` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.5-macos-monterey-x86_64.zip` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.5-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.5-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.5-linux-x86_64.AppImage.sha256.txt`

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
- [RELEASE_NOTES_1.5.5.md](../RELEASE_NOTES_1.5.5.md)
- [doc/GITHUB_RELEASE_BODY_1.5.5.md](./GITHUB_RELEASE_BODY_1.5.5.md)
- [CHANGELOG.md](../CHANGELOG.md)
