# Decodium 4.0 v1.0.515

Version 1.0.515 is a focused follow-up to v1.0.514. It improves recovery from a PipeWire audio-service interruption on Linux, makes the PSK Reporter history window configurable, and adds a quick callsign-copy action to the logbook.

## Changes from v1.0.514 to v1.0.515

### Linux audio recovery after PipeWire interruption

- Fixed the Linux PipeWire recovery path reached after a terminal audio I/O error and `StoppedState` condition.
- The failed `QAudioSource` is now synchronously disconnected and destroyed on its owning `SoundInput` thread before a replacement source is scheduled.
- The replacement input is still opened after a short 2-second settling delay, giving PipeWire and Qt Multimedia time to complete their teardown before capture resumes.
- This prevents the failed source from remaining queued for deferred deletion while it repeatedly emits invalid socket-notifier events, a condition that could starve the event loop and leave decoding stuck after the watchdog started recovery.
- This change is compiled only for the Linux/PipeWire recovery branch. The Windows and macOS audio paths, and the panadapter/waterfall rendering paths, are unchanged by this fix.

### Qt Quick diagnostic scope

- Field diagnostics on Wayland with NVIDIA/OpenGL identify a separate Qt Quick vsync/present synchronisation condition as the source of the previously observed one-second QSG sync stalls. Running with `QSG_NO_VSYNC=1` is a useful diagnostic workaround on an affected system, but is not enabled by Decodium itself in this release.
- v1.0.515 deliberately leaves the global render/present policy unchanged. That avoids introducing a cross-platform panadapter or waterfall regression while the compositor-specific condition is investigated separately.

### Configurable PSK Reporter history

- Added a **Query history** selector under **Settings > Reporting > Network Services > PSK Reporter**.
- The selectable look-back period runs from **5 to 60 minutes** in 5-minute increments; the default remains 60 minutes.
- The selected value is stored with the active Decodium settings profile and is restored on the next start.
- Callsign searches and **heard-by** queries both use the selected period, so map markers, search feedback and the PSK Reporter panel consistently describe the same time range.
- The status labels and tooltips now show the selected number of minutes instead of always referring to the last hour.

### Logbook usability

- Right-clicking a QSO row in the logbook now opens a compact context menu with **Copy Callsign**.
- The action copies the callsign of the selected row to the clipboard.
- The same action is available in both the docked log and the detached/floating log window.

## Release contents

- GitHub source archives for the complete v1.0.515 codebase.
- Windows x64 installer executable.
- macOS Apple Silicon and Intel DMG packages, with ZIP and SHA-256 checksum companions where produced by the runners.
- Linux x86_64 and aarch64 AppImages, each with its SHA-256 checksum file.

The release artifacts are built and uploaded by the GitHub Actions runners for their respective platforms.
