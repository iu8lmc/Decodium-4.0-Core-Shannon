# Decodium 4 FT2 1.0.291

Release 1.0.291 is a field-fix and packaging release after 1.0.290. It focuses on macOS shutdown/TX backend stability, Linux/AppImage QML completeness, special callsign handling, data-download reliability, and several UI persistence fixes reported by users.

## Main Changes Since 1.0.290

- Hardened macOS shutdown of the async FT decode workers so the app stops pending decode callbacks, drains queued worker calls, requests thread interruption, and avoids the `QThread: Destroyed while thread ... is still running` crash seen in diagnostic reports.
- Made macOS use the standalone UDP TX path by default. `DECODIUM_FORCE_STANDALONE_UDP=1` is no longer needed on Mac; `DECODIUM_MAC_LEGACY_TX_BACKEND=1` remains available as an explicit legacy override.
- Fixed special/non-standard callsign message generation for hashed calls, including TX3 ordering, so contacts such as `ZL100C` and other bracketed calls no longer loop with an invalid `R-report` message.
- Added `BOTA` as a valid directed-CQ modifier so messages like `CQ BOTA 9H1SR JM68` pass the semantic decoder filter.
- Added a directed false-call guard for weak, low-confidence messages addressed to the local station when the caller/grid combination is implausible, reducing false Signal RX alerts such as the EA8DJR/Japan-grid case.
- Improved cty.dat download compatibility by adding accepted country-files endpoints, browser-safe request headers, payload validation, atomic save, retry/fallback handling, and clearer error messages instead of raw HTTP 406 failures.
- Added CALL3.TXT download status/error feedback, validation, atomic save, reload reporting, and explicit UI feedback while the download is running.
- Fixed TX watchdog persistence so custom watchdog mode, time, and count are saved, mirrored to legacy keys, and restored on restart instead of falling back to the default 6 minutes.
- Fixed waterfall decoded-calls visibility persistence and filtering behavior, plus MON/RX visual behavior so the RX progress bar no longer keeps advancing when monitor is disabled.
- Fixed the waterfall `Hide` button so it hides only controls and keeps the spectrum/panadapter area visible.
- Improved Settings dialog scrolling and layout on smaller screens, including the clipped alignment controls, the Decode Boost row, Data Download area spacing, and Linux mouse-wheel scrolling inside large combo popups.
- Recentered the `LOG` button content and translated remaining TX tooltip/history text that was still shown in Italian on English systems.
- Hardened Linux AppImage Qt 6.11 packaging by verifying and bundling `QtQuick.Controls.Material` in both runtime QML locations, preventing `module "QtQuick.Controls.Material" version 6.11 is not installed`.

## Verification Notes

- Local macOS build of `decodium_app` completed successfully after the decode-thread shutdown and standalone UDP changes.
- Local QML sync/build completed successfully after the Settings and Waterfall UI fixes.
- Local smoke startup with `DECODIUM_TX_SMOKE_TEST=1` exited cleanly and did not generate a new crash report.
- Added/kept regression coverage for `BOTA` directed CQ and special-call decode/message paths.

## Platform Assets

- Windows x64 installer is built and attached by the Windows GitHub Actions runner.
- macOS Apple Silicon DMG/ZIP assets are built and attached by the macOS GitHub Actions runner.
- Linux x86_64 AppImage is built and attached by the Linux Qt 6.11 GitHub Actions runner.

## Notes

- GitHub release source archives are published with the tag automatically.
- This release keeps the requested `1.0.291` version on the fork while preserving the local fixes accumulated after 1.0.290.
