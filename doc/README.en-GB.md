# Documentation Notes (English) - 1.6.0

- Current release: `1.6.0`
- Update cycle: `1.5.9 -> 1.6.0`

## Key Technical Changes (`1.5.9 -> 1.6.0`)

- JT65 active runtime promoted to the native C++ decode path.
- Native JT9 fast/wide decoder building blocks and shared DSP/IO helpers expanded for the continuing legacy JT migration.
- Non-standard/special-event callsign replies no longer fail with `*** bad message ***`.
- Linux ARM64 AppImage release support added through a Debian Trixie-based build path and ARM GitHub runner.
- GCC 14 and GCC/libstdc++ portability fixes added without regressing macOS Clang.
- Version metadata, workflow defaults, release docs, and GitHub release notes are aligned to `1.6.0`.

## Release Assets

- `decodium3-ft2-1.6.0-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.6.0-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.6.0-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.6.0-macos-monterey-x86_64.dmg` *(best effort / experimental, if generated)*
- `decodium3-ft2-1.6.0-linux-x86_64.AppImage`
- `decodium3-ft2-1.6.0-linux-aarch64.AppImage`

## Linux Minimum Requirements

- `x86_64` with SSE2 or `aarch64` / ARM64 64-bit CPU
- 4 GB RAM minimum, 500 MB free disk
- Qt5-capable X11 or Wayland desktop session
- `glibc >= 2.35` for Linux `x86_64`
- `glibc >= 2.38` for Linux `aarch64` *(Debian Trixie baseline)*
- `libfuse2`, ALSA/PulseAudio/PipeWire, and serial/USB permissions as needed

## Startup Guidance

macOS quarantine workaround:

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

- [README.md](../README.md)
- [RELEASE_NOTES_1.6.0.md](../RELEASE_NOTES_1.6.0.md)
- [doc/GITHUB_RELEASE_BODY_1.6.0.md](./GITHUB_RELEASE_BODY_1.6.0.md)
