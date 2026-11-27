# Decodium 4 FT2 1.0.283

Release 1.0.283 is a packaging and field-fix release after 1.0.282. It focuses on cross-platform startup reliability, TX/RX recovery after transmit, and FT8/FT4/FT2 special-call handling seen in recent user logs.

## Main Changes Since 1.0.282

- Added native C++ file and folder dialogs exposed through `DecodiumBridge`, replacing direct QML `QtQuick.Dialogs` usage in the main UI, decode history export, logbook import/export, and settings flows.
- Fixed Linux/AppImage startup failures caused by missing `QtQuick.Dialogs` runtime modules on some distributions.
- Added `BOTA` as a valid directed-CQ modifier, so decodes such as `CQ BOTA 9H1SR JM68` are accepted instead of being discarded by the semantic decoder filter.
- Relaxed local self-echo suppression for special and non-standard calls: only exact canonical repeats of the last transmitted message are treated as local echo, preventing valid special-call replies from being blocked when token order is unusual.
- Improved TCI RX recovery after TX by avoiding unnecessary post-TX audio capture restarts when TCI capture is already active.
- Extended the empty-audio watchdog to cover TCI audio input, so missing TCI capture after TX is recovered instead of silently leaving RX inactive.
- Improved TX audio precompute logging for paths where PCM is not required, reporting `pcm=not-required` instead of a misleading zero-sized buffer.
- Added regression coverage for `BOTA` directed CQ and special-call FT message encoding/decoding paths.

## Analysis Notes

- Investigated reported FT8 TX3 stalls with long/special calls. The available log showed Decodium correctly remaining on TX3 while the remote station repeatedly sent only a bare report; no CPU or TX audio pipeline failure was found in that case.
- TX audio telemetry in the inspected log stayed clean, with no underruns or audio stalls.

## Platform Assets

- Windows x64 installer is built and attached by the Windows GitHub Actions runner.
- macOS Apple Silicon DMG/ZIP assets are built and attached by the macOS GitHub Actions runner.
- Linux x86_64 AppImage is built and attached by the Linux GitHub Actions runner.

## Notes

- This release intentionally does not force TX4 after repeated bare reports. The sequencer still waits for a valid acknowledgement (`R+NN`, `RR73`, `RRR`, or `73`) to avoid logging incomplete QSOs.
