# Decodium 4.0 — v1.0.511

Release 1.0.511 consolidates the audio-watchdog and panadapter-recovery work introduced after v1.0.510. The focus is preserving an active receive path while preventing recovery actions from creating new Qt Multimedia or main-thread instability.

## Audio RX recovery and watchdog behavior

- Added a non-destructive watchdog recovery path for an already active `SoundInput` stream. When the selected device, format, channel and sink are unchanged, the existing audio source is reset and resumed/restarted instead of being immediately destroyed and recreated.
- Added a short delayed health check after the safe recovery. If PCM callbacks return, the capture stays alive; if no callback returns, the watchdog escalates to the existing full stop-and-reopen path.
- Added cancellation sequencing for delayed recovery work, so a deliberate mode change, device change or capture stop cannot be followed by an obsolete watchdog escalation.
- Preserved the full reopen fallback for cases where the stream is not reusable or no PCM callback is received after the non-destructive attempt.
- Changed the audio health handler so a stream that is still delivering PCM is retained even when its content is silent, clipped or square-like. The condition is logged for diagnosis while capture recovery remains reserved for paths that have actually lost callbacks.
- Added recovery diagnostics and status reporting identifying the selected input device and whether the safe watchdog path is active.

## FT8 and panadapter stability

- Added a GPU panadapter FFT stall guard tied to the FT8 main-thread micro-stall protection. When a real stall is detected, the scene-graph GPU FFT path is paused and the asynchronous CPU fallback remains available.
- Prevented forced or accelerated GPU panadapter work while low-CPU mode is active or while the stall guard is engaged.
- Re-enabled GPU FFT probing only after the FT8 adaptive guard observes the configured clean periods, avoiding an immediate return to the path that caused the stall pressure.
- Added bridge diagnostics for GPU FFT suspension and later retry eligibility.

## Release contents

- GitHub source archives for the complete v1.0.511 codebase.
- Windows x64 installer executable.
- macOS Intel DMG packages for Ventura, Sonoma and Sequoia.
- macOS Apple Silicon DMG packages for Tahoe and Sequoia.
- Linux AppImages for x86_64 and aarch64, each with a SHA-256 checksum file.
- ZIP and checksum companions are included for the macOS packages where produced by the corresponding runner.

No local test suite was executed for this release, as requested. The packaging jobs were started through GitHub Actions to produce the published release artifacts.
