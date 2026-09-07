# Decodium 4 FT2 1.0.468

Release 1.0.468 focuses on FT2-Link live radio diagnostics, macOS legacy audio capture, and connection reliability while preserving the current 1.0.467 UI and workflow behavior.

## FT2-Link macOS legacy RX

- Added FT2-Link RX tap support to the legacy macOS audio backend, so FT2-Link receives audio from the same capture path used by the classic FT modes.
- Reworked the legacy RX handoff to use a lightweight callback plus buffered timer drain instead of pushing decoder work directly from the audio callback.
- Kept the legacy callback feeding recorder and spectrum paths independently, so FT2-Link RX does not steal or block waterfall/panadapter updates.
- Added bounded pending sample storage for legacy FT2-Link RX to prevent decoder backlog growth during noisy or undecodable signals.
- Cleared stale pending FT2-Link legacy RX audio when leaving FT2-Link, stopping monitoring, tuning, or transmitting.

## FT2-Link decode observability

- Added live diagnostic flush support for FT2-Link messages so `tail -f decodium_diagnostic.log` shows RX/TX events immediately during field testing.
- Added `[Ft2Link][RXTAP]` diagnostics for both modern and legacy audio taps, including monitor/TX/tune gating and legacy pending-buffer state.
- Added `[Ft2Link][SESSION]` diagnostics when session state changes, including remote call, session state, event, profile, rate, and message count.
- Added richer RX failure diagnostics with buffer sizes and decode errors for NARROW, W500, and W2300 paths.
- Added detailed NARROW waveform failure metrics, including estimated center, frequency offset, quality score, and sample count when a burst is not found.

## FT2-Link decoder stability

- Capped and trimmed the NARROW live receive buffer to avoid runaway buffer growth after long busy-channel periods without successful decode.
- Kept idle NARROW buffers short when the channel is not busy, reducing unnecessary work while preserving enough context for resync.
- Preserved wide-mode W500/W2300 RX buffering and resync behavior while improving failure visibility.

## FT2-Link connection handling

- Added HELLO retry scheduling for radio handshakes so a connection attempt is not limited to a single unanswered transmission.
- Added retry timing and timeout diagnostics for pending HELLO sessions.
- Cleared HELLO retry state when a session is acknowledged or closed.
- Moved HELLO and live outbound retry timers onto a wall-clock scheduling base so retries are not triggered early by synthetic or externally supplied logical timestamps.
- Preserved application TX metadata across adaptive retry replans, including chat, file, form, mail, BBS, and broadcast identifiers.

## Logging and release behavior

- Added a diagnostic log flush API used only by live FT2-Link troubleshooting paths.
- Routed FT2-Link diagnostic messages to stderr plus the persistent diagnostic log without relying on the generic Qt message handler.
- Preserved normal diagnostic buffering for unrelated high-volume application logs.

## Validation

- Local QML/C++ target build: `cmake --build "/Users/salvo/Desktop/Decodium4-build" --parallel 4 --target decodium_qml`.
- Focused FT2-Link tests: `ctest --test-dir "/Users/salvo/Desktop/Decodium4-build" -R "test_ft2link$|test_ft2link_qml_adapter$" --output-on-failure`.
- Source hygiene: `git diff --check`.

## Expected release assets

- Windows x64 installer: `Decodium_1.0.468_Setup_x64.exe`.
- macOS Apple Silicon DMG/ZIP assets.
- macOS Intel DMG/ZIP assets.
- Linux x86_64 AppImage and checksum.
- Linux aarch64 AppImage and checksum.
