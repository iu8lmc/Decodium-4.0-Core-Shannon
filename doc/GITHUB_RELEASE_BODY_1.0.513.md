# Decodium 4.0 v1.0.513

Version 1.0.513 brings the upstream v1.0.512 language and interface updates into the local release line and adds a performance and diagnostics pass for the Live Map, map intelligence snapshots, startup loading and main-thread responsiveness.

## Changes from v1.0.511 to v1.0.512

### Callsign Intelligence and Live Map statistics localisation

- Added the missing catalog entries for the Callsign Intelligence window, local database and Club Log OQRS settings, logbook statistics, award progression, period comparison, QSO drill-down and Live Map layer controls.
- Completed the translation catalogs for the supported languages, including the strings introduced by the Callsign Intelligence, roster, statistics and Live Map work.
- Removed the remaining cases where Italian source text could appear in an English interface because a catalog entry was missing.

### QML interface corrections

- Fixed the RX auto-level `AUTO` label so it no longer references a colour property that does not exist in the main window.
- Fixed the ALC target tooltip so it uses the property exposed by its `HoverHandler` and can appear correctly.

## Changes in v1.0.513

### Map intelligence snapshot efficiency

- Added per-domain change detection to `MapIntelligenceService` before emitting QML notifications.
- Roster, awards, alerts, propagation, band activity, statistics, filters, matrices and coverage now notify consumers only when their data actually changed.
- Coverage rendering is rebuilt only when coverage-related values change, reducing unnecessary model and scene-graph work after decode cycles.

### Live Map incremental updates

- Removed the full world-map contact clear and replay from the periodic spot synchronisation path.
- Kept decoder contacts incremental through the existing contact-added flow while spot paths and TX state are refreshed independently.
- Avoided full contact replays caused by snapshot filter notifications, preventing repeated map rebuilds during FT slots.

### Startup and settings loading

- Removed the automatic delayed warmup of `SettingsDialog` during startup.
- Settings remain lazy-loaded and are initialised when the user opens the dialog or one of its tabs, reducing startup activity and avoiding background dialog work during reception.

### Main-thread responsiveness diagnostics

- Added application-level timing around GUI-thread event dispatch.
- Slow event deliveries of at least 90 ms now record the event type, receiver class, object name and elapsed time with a `[MAINDISPATCH]` diagnostic entry.
- Added stable object names to the bridge timers for UTC display, spectrum processing, process usage and UI-stall diagnostics, making field logs easier to interpret.

### Panadapter and FT8 recovery policy

- Kept the FT8 micro-stall protection responsive while limiting GPU panadapter FFT suspension to genuinely severe main-thread stalls of at least 750 ms.
- Preserved the asynchronous CPU fallback and the clean-period recovery path, avoiding unnecessary GPU pauses for short QML or model updates.
- Extended the guard diagnostics with the GPU pause threshold used by the current recovery decision.

## Release contents

- GitHub source archives for the complete v1.0.513 codebase.
- Windows x64 installer executable.
- macOS Intel DMG packages for Ventura, Sonoma and Sequoia.
- macOS Apple Silicon DMG packages for Tahoe and Sequoia.
- Linux AppImages for x86_64 and aarch64, each with a SHA-256 checksum file.
- ZIP and checksum companions for the macOS packages where produced by the corresponding runner.

No local test suite was executed for this release, as requested. Packaging was delegated to the GitHub Actions release runners.
