# Decodium 4 FT2 1.0.193

Release focused on HRD reliability, panadapter rendering stability, audio device handling, and Qt 6.11 Linux packaging.

## Highlights

- Align HRD startup probing with WSJT-X by using `get context` as the protocol probe.
- Make HRD disconnect/shutdown more robust by aborting stale sockets, stopping polling before cleanup, and using fast shutdown timeouts.
- Refresh the active HRD radio context before context-prefixed commands to avoid stale context IDs.
- Improve audio device handling for USB input/output selection.
- Harden Full Spectrum and Signal RX rendering paths against transient black panels when data is present.
- Keep the Linux AppImage Qt 6.11 packaging path aligned with the current runtime and GPU shader assets.

## Notes

Windows and macOS release assets are produced by GitHub Actions. Source archives are attached automatically by GitHub for tag `1.0.193`.
