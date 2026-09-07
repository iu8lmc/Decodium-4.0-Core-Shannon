# Decodium 4.0 v1.0.510

Version 1.0.510 consolidates the Live Map intelligence layer into an
operational logbook, roster, award, propagation and statistics engine. It
also adds callsign intelligence services, improves map operations and removes
the repeated QML layout warnings seen during startup.

## Changes from 1.0.508 to 1.0.510

### One coherent ADIF source for map, roster, statistics and awards

- ADIF records are normalized into the shared `map_qso` data source used by
  coverage, roster, award progress, propagation analytics and historical
  statistics.
- QSO metadata is retained consistently for callsign, operator callsign,
  band, mode, grid, DXCC, continent, zones, confirmation status, source and
  propagation mode.
- Historical QSO data, live spots and roster state now remain separate while
  being queried through one consistent filter model.

### Complete operational call roster

- Added wanted and exception matrices for callsign, grid, DXCC, WPX, POTA,
  CQ zone, ITU zone, state, county and continent.
- Added filters for LoTW, eQSL, OQRS, “spotted me”, same DXCC, minimum SNR and
  maximum DT.
- Added roster scopes for current band, current mode, digital modes, all bands
  and the selected award.
- NEW entries now expose the precise reason they are new, including the
  missing entity type and the relevant band/mode or confirmation condition.
- RR73 can be treated as CQ through a configurable roster option.
- Added roster sorting, status, retention, text filtering, hunt scope and
  per-rule enablement controls.

### Executable award engine

- The existing award catalog is now evaluated against imported QSOs instead of
  being only a definition catalog.
- Award rules account for band, mode, confirmation type, callsign and date
  interval.
- Added endorsement selection and award-specific goals.
- Added worked, confirmed and missing detail views.
- Missing award entities can be selected to open the corresponding roster
  view and focus the map.
- Award calculations share the same QSO filters as the roster and statistics
  views.

### Propagation as queryable QSO data

- QSOs are classified by propagation type and the classification is retained
  in the shared data source.
- Added propagation filtering, summaries, grouped statistics and timeline
  data for the supported propagation categories.
- Propagation information is available alongside band, mode, source and
  confirmation analytics.

### Live Map operational refinements

- Added per-layer opacity, color, stroke width, label density and display
  style controls.
- Added automatic focus on the active QSO with restoration of the previous
  viewport.
- Added independent hover details for the historical and live halves of split
  grid cells.
- Added temporal legend data and source-specific decay handling.
- Added complete map configuration export/import, including layer settings,
  viewport, roster state and map presets.
- Extended map operations for selected grids, QSO focus, roster opening and
  map-linked detail windows.

### Historical logbook statistics

- Added award progress over time.
- Added rankings for bands, modes, DXCC, WPX and grids.
- Added period-to-period comparison data.
- Added per-profile and per-callsign statistics.
- Added drill-down from aggregates to the QSO records involved.
- Added export-oriented result structures for the statistics views.

### Callsign intelligence

- Added a provider-based lookup service with fallback ordering and caching.
- Added local database support for FCC ULS, LoTW, eQSL and Club Log OQRS
  information.
- Added optional automatic lookup when a QSO starts and automatic close after
  logging.
- Added optional enrichment of missing grid, name and QTH fields.
- Added callsign intelligence settings and a dedicated lookup window.

### Startup and UI reliability

- Removed the circular `TabBar`/`TabButton` width bindings in the Live Map
  intelligence panel. The tab controls now use Qt's own sizing logic, so the
  repeated `Binding loop detected` warnings no longer appear at startup.
- Added the callsign intelligence service to the legacy `wsjtx` packaging
  target as well as the QML frontend target, so all release configurations link
  the complete callsign feature set.
- Preserved the Metal-first graphics path and the CPU fallback paths for the
  panadapter and Live Map.

### Included 1.0.509 changes

- Completed translations for the Live Map band activity panel in all supported
  languages.
- Corrected the malformed save-directory error message so its placeholder and
  translation lookup are valid.
- Audited the translation catalogs for matching placeholders and complete
  entries.

## Release artifacts

- Windows x64 installer and portable executable bundle.
- macOS Apple Silicon DMG and ZIP packages for the supported macOS targets.
- macOS Intel x86_64 DMG and ZIP packages for Ventura, Sonoma and Sequoia.
- Linux x86_64 Qt 6.11 AppImage.
- Linux aarch64 Qt 6.11 AppImage.
- SHA-256 checksum files for the macOS and Linux packages.
- Complete source code at tag `v1.0.510`.

No local test suite was executed for this release, as requested. Platform
packaging is delegated to the configured GitHub Actions runners.
