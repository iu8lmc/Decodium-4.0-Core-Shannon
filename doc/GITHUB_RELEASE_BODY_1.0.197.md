# Decodium 4 FT2 1.0.197

Release focused on HRD compatibility, audio stability, AutoCQ sequencing, and Qt/QML user interface polish.

## Highlights

- Improve HRD connection behavior with WSJT-X-aligned protocol probing, IPv4 preference, stale-context refresh, and faster shutdown cleanup.
- Harden macOS CoreAudio shutdown/restart paths to avoid QtMultimedia crashes during device changes or background operation.
- Prevent Full Spectrum and Signal RX panels from going black when valid waterfall or decode data is still available.
- Fix AutoCQ final signoff handling so Decodium waits for the partner `73` after sending `RR73`.
- Modernize the log warning dialog and suppress false `DX Call field is empty` prompts when a valid QSO is already present.
- Reduce repeated RX audio restart noise and avoid resetting active capture when the same audio stream is already running.
- Fix Settings checkbox overlap with longer translations, including Italian labels in TX and Advanced tabs.
- Make the bottom `TX1`-`TX6` buttons compact instead of stretching across wide displays.

## Assets

Windows and macOS release assets are produced by GitHub Actions. Source archives are attached automatically by GitHub for tag `1.0.197`.
