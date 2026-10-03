# Decodium 4 FT2 1.0.300

Release 1.0.300 is a post-1.0.299 field-fix release. It keeps the fork aligned with Martino's 1.0.299 baseline and adds the local fixes made after that release for TX slot timing, FT2 AutoCQ handoff, DX Cluster band classification, 8 metre FT8 operation, and Linux AppImage QML packaging.

## Main Changes Since 1.0.299

- Fixed late manual FT8/FT4 TX arming: when TX is armed too late for the current sync window, Decodium now explicitly queues the next valid slot instead of silently waiting multiple periods.
- Added a retry serial guard to the sync TX scheduler so stale delayed callbacks cannot start a previous TX request after the operator or auto-sequence state has changed.
- Tightened deferred manual sync TX cleanup so cancelled or consumed deferred starts clear both the scheduled retry flag and its pending callback generation.
- Fixed the FT2 double-click TX1 race: an initial call started from a decoded callsign now remains period-gated, so the audio-completion callback cannot immediately restart TX1 on every FT2 slot without a real RX gap.
- Hardened the FT2 manual QSO latch after double-click/map/cluster selection: manual pre-signoff `TX1..TX3` transmissions now run one-shot and disarm after audio completion until a fresh partner decode advances the QSO, while AutoCQ and the TX4/TX5 signoff/autolog path keep their normal behavior.
- Fixed FT2 AutoCQ latch handling for direct replies in the form `MYCALL CALL GRID`: AutoCQ now immediately locks the caller/grid, exits raw CQ repeat behavior, and arms TX2 even if the previous CQ has just finished or audio cleanup is still running.
- Prevented FT2 AutoCQ from transmitting CQ every short FT2 period without a real RX opportunity. CQ repeat now respects the normal period check unless a QSO response is already in progress.
- Preserved FT2 AutoCQ partner state through the first reply/report transition, avoiding the loop where Decodium kept sending CQ or repeating the same report while the caller had already answered.
- Added an FT2 AutoCQ one-shot wait after TX2/TX3/TX4 audio completion. AutoCQ stays enabled, but Decodium now waits for a fresh decode from the locked partner before retransmitting or advancing, which prevents the macOS CoreAudio cleanup path from re-entering TX in a tight loop.
- Fixed the FT2 AutoCQ live-caller cooldown gate: a fresh `MYCALL CALL GRID` decode now clears the previous abandoned-caller marker and can arm TX2 immediately, while stations already worked or logged remain protected by the duplicate guard.
- Tightened the FT2 AutoCQ report latch on both generic and macOS PCM completion paths: after TX2/TX3/TX4 completes, TX is internally disarmed while AutoCQ remains enabled, and only a fresh partner decode newer than the completed transmission can rearm it.
- Mirrored deferred auto-sequence handoff on the macOS PCM completion path, so a caller decoded during an active CQ applies pending TX2 immediately when CQ audio ends instead of allowing one more CQ/retry cycle.
- Restored macOS startup to the legacy TX backend by default. `DECODIUM_FORCE_STANDALONE_UDP=1` now explicitly opts into standalone UDP, while `DECODIUM_FORCE_STANDALONE_UDP=0` or an unset variable leaves standalone UDP disabled.
- Avoided a macOS CoreAudio/Qt socket-notifier crash after TX audio completion by leaving retired CoreAudio TX sinks parked instead of stopping/deleting them during the unsafe cleanup window.
- Reworked DX Cluster band detection to use explicit amateur allocations instead of broad MHz thresholds. This fixes 70 MHz spots as `4M` rather than `2M` and introduces correct `8M` detection for 40-45 MHz.
- Added regression coverage for DX Cluster band mapping, including 40.680 MHz as `8M` and 70.154 MHz as `4M`.
- Added the 8 metre FT8 frequency preset at `40.680 MHz`.
- Added the `8M` band entry to the dashboard band controls and Band Selector, with the dashboard button labelled `8`.
- Updated bridge-side band derivation for ADIF/B4 logic to use the shared `Bands` table instead of the older approximate frequency ladder.
- Hardened Linux x86_64 AppImage packaging for Qt 6.11 by dereferencing bundled Qt QML symlinks, pruning unused optional QML modules such as QtQuick3D, bundling QML plugin library dependencies, setting runtime QML/plugin paths in `AppRun`, and validating the final AppImage payload for `QtQuick.Controls.Material`.

## Verification Notes

- Local macOS build target `decodium_qml` completed successfully after the version bump and all fixes.
- Local regression tests passed:
  - `test_tx_pipeline`
  - `test_dx_cluster_band`
  - `test_ft2_qso_sim`
- `git diff --check` completed cleanly before commit.

## Platform Assets

- Windows x64 installer is built and attached by the Windows GitHub Actions runner.
- macOS Apple Silicon DMG/ZIP assets are built and attached by the macOS GitHub Actions runner.
- Linux x86_64 AppImage is built and attached by the Linux Qt 6.11 GitHub Actions runner.

## Notes

- GitHub release source archives are published automatically from tag `1.0.300`.
- This release keeps the fork version aligned to `1.0.300` while preserving all local fixes made after `1.0.299`.
