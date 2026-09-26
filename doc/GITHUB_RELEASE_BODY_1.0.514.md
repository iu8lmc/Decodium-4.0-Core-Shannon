# Decodium 4.0 v1.0.514

Version 1.0.514 consolidates the local asynchronous runtime work, completes the first operational satellite and rotator services, strengthens offline behaviour and map operations, and fixes a reproducible FT8 decoder crash caused by a short valid callsign.

## Changes from v1.0.513 to v1.0.514

### FT8 decoder crash fix and asynchronous runtime safety

- Fixed a reproducible abort in the standard 28-bit callsign encoder when a valid short callsign such as `A1B` was decoded. The standard six-character field is now padded before indexed access, with an additional defensive length check.
- Added a regression data row covering the short callsign path.
- Kept settings persistence asynchronous through a single coalescing worker, preventing repeated QML timer writes from blocking the GUI thread while retaining an orderly final save during shutdown.
- Extended main-thread diagnostics with the QML source associated with slow timer deliveries, making startup latency easier to identify from field logs.
- Preserved asynchronous audio recovery and staged map/world-map updates so radio reception, decoder work and UI refreshes remain independent.

### Satellite tracking

- Added a dedicated satellite tracking service with cached TLE data, asynchronous refresh, local fallback and offline handling.
- Added SGP4 propagation, observer position handling, azimuth/elevation, range, range-rate, visibility and upcoming-pass prediction.
- Added Doppler calculation and optional frequency tracking, with safeguards that prevent frequency feedback loops during active transmission or tuning.
- Added satellite markers to the operational map and a lazy-loaded satellite tracking window.
- Connected satellite state to the current grid, frequency, QSO/map presentation and rotator controls.

### Rotator control

- Added an asynchronous UDP rotator abstraction supporting PSTRotator and CatRotator command paths.
- Added azimuth and elevation targets, feedback state, tracking intervals, emergency stop and optional parking.
- Added configurable azimuth/elevation safety limits and rejection of invalid or non-finite targets.
- Added automatic satellite/target tracking integration, status reporting and feedback timeout handling.

### Map providers, overlays and offline operation

- Added provider fallback and clearer provider/API failure status for the base map.
- Added cache invalidation and stale-cache reporting so obsolete tiles are not silently treated as current.
- Added support for specialized geographic layers including bathymetry and external forecast/overlay sources, with temporal legend data and source validity/decay information.
- Added import and management of a user-provided offline raster pack. The offline path does not copy or redistribute provider tiles.
- Added a coordinated offline mode: cloud lookups, remote callsign updates, DX Cluster, propagation downloads, updater checks and other network services pause together while local ADIF, logbook, cache and radio functions remain available.
- Added map controls for layer opacity, colour, line width, label density, temporal display and operational marker interaction.

### Map operations and QSO workflow

- Added active-QSO map fitting and restoration of the previous map view.
- Added independent hover handling for the two split-grid cells.
- Added map interaction hooks for satellite targets, rotator tracking and operational spots.
- Improved spot validity/expiry presentation and map-side status for forecast and external overlay data.
- Kept roster, intelligence snapshots, map statistics and live markers on the same incremental data path to avoid unnecessary full redraws.

### Callsign and network coordination

- Propagated offline state into callsign intelligence, DX Cluster, propagation and updater services.
- Prevented remote callsign/provider requests when offline while retaining local databases and cached records.
- Kept remote spot/reporting actions explicitly disabled in offline mode with user-visible status messages.

### Build and packaging integration

- Registered the satellite, SGP4 and rotator sources in the desktop and QML targets.
- Added the supporting service test targets and SGP4/rotator regression sources to the project layout; they were not executed for this release, as requested.
- Added third-party attribution for the bundled SGP4 implementation.

## Release contents

- GitHub source archives for the complete v1.0.514 codebase.
- Windows x64 installer executable.
- macOS Apple Silicon and Intel DMG packages, with the corresponding ZIP and checksum companions where produced by the runners.
- Linux AppImages for x86_64 and aarch64, each with a SHA-256 checksum file.

No local test suite was executed for this release, as requested. Packaging is delegated to GitHub Actions runners.
