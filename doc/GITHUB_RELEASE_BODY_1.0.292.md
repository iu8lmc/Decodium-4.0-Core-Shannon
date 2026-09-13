# Decodium 4 FT2 1.0.292

Release 1.0.292 is a focused TX stability release after 1.0.291. It addresses the macOS Apple Silicon TX audio path where the first transmission could carry payload but later transmissions could become silent, and it tightens FT8/FT4 slot timing so generated audio starts at the correct sync point instead of drifting late inside the slot.

## Main Changes Since 1.0.291

- Reworked the modern macOS TX audio path to send a precomputed PCM buffer directly through `SoundOutput`, avoiding the Qt/CoreAudio sink lifetime interaction that could drop the payload after the first TX.
- Added explicit macOS PCM TX diagnostics that report mode, message, generated samples, payload length, slot elapsed time, lead-in silence, PCM size, and target audio device state.
- Fixed repeated TX payload loss on Apple Silicon by retiring old CoreAudio sinks without stopping a retired sink while a replacement TX sink is already active.
- Added guarded `finishPlayback()` handling for TX audio shutdown so Decodium finishes/parks the current audio stream without using the more destructive reset/stop path during normal TX completion.
- Tightened FT8/FT4 late-start protection: sync-mode TX now keeps the Costas start near the nominal 500 ms slot lead-in and defers to the next valid TX period instead of transmitting a shifted payload around +1.4 seconds.
- Fixed the deferred FT8/FT4 auto-TX budget path. If there is no longer enough time to wait for a decode grace period, Decodium now either starts immediately while still inside the safe window or explicitly schedules the next valid TX slot instead of silently skipping multiple slots.
- Preserved FT2 behavior while keeping the stricter FT8/FT4 timing rules, so async/fast FT2 operation is not penalized by the FT8 slot guard.
- Reduced diagnostic log noise by making per-chunk TX pump tracing opt-in through `DECODIUM_TX_PUMP_TRACE`; normal logs now keep the high-value TX timing and sink-open messages without flooding the diagnostic file.
- Kept the manual/sync TX retry scheduler active for late manual starts, Auto CQ, and auto-sequence retries so late clicks are queued instead of being lost.

## Verification Notes

- Local macOS build of `decodium_app` completed successfully after the TX audio and timing changes.
- Field logs confirmed the corrected FT8 start pattern: `slotElapsedMs` plus `initialSilenceMs` places the payload at approximately 500 ms in the slot.
- The diagnostic path remains available for deeper TX pump analysis by launching with `DECODIUM_TX_PUMP_TRACE=1`.

## Platform Assets

- Windows x64 installer is built and attached by the Windows GitHub Actions runner.
- macOS Apple Silicon DMG/ZIP assets are built and attached by the macOS GitHub Actions runner.
- Linux x86_64 AppImage is built and attached by the Linux Qt 6.11 GitHub Actions runner.

## Notes

- GitHub release source archives are published automatically from tag `1.0.292`.
- This release keeps the fork version aligned to `1.0.292` while preserving all local fixes made after `1.0.291`.
