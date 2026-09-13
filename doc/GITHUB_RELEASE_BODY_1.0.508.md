# Decodium 4.0 v1.0.508

Version 1.0.508 consolidates the work completed after 1.0.506, with a new
operational Band Activity dashboard, stronger multi-callsign profile isolation,
correct handling of special callsigns and transmitted reports, safer WAV
retention defaults, and complete Live Map localisation.

## Changes from 1.0.506 to 1.0.508

### Operational Band Activity

- Added a dedicated Activity page to Map Intelligence for choosing the most
  useful operating band from current traffic instead of relying on raw spot
  totals alone.
- Added selectable 1, 6, 12 and 24 hour analysis windows with persistent user
  selection.
- Added separate counters for local RX, local TX, network RX and network TX
  activity.
- Added 15-minute timeline charts, per-band ranking, unique-call statistics,
  average SNR, recency and RX/TX balance.
- Added a 0-100 band score and a Best Band recommendation derived from traffic
  volume, diversity, signal quality, recency and agreement between local and
  network observations.
- Extended the persistent map database with spot direction and activity indexes
  so the dashboard remains incremental and responsive as history grows.

### Multi-callsign profile isolation

- Theme, accent, density and custom colours now follow the active Decodium
  profile, making personal and special-event configurations immediately
  distinguishable.
- Cloudlog settings now remain isolated per active profile, including endpoint,
  API key and station ID.
- LoTW credentials and upload-age preferences are resolved through the active
  profile with compatibility migration for older setting names.
- Logging credentials use callsign-scoped secure storage, preventing one
  callsign profile from silently reusing another profile's credentials.
- Existing settings are migrated on first use so established installations keep
  their configuration while gaining profile separation.

### Special callsigns and QSO logging

- Corrected FT8 packing of bracketed hash-addressed special callsigns, so the
  transmitted payload matches the TX message shown by the interface.
- Added shared extraction of transmitted signal reports from normal and
  bracketed directed messages.
- Preserved the actual sent report in automatic and manual QSO log snapshots,
  including late logging after the final exchange.
- Hardened pending-log recovery so the destination callsign and report remain
  available after the sequencer advances.

### WAV retention and storage safety

- Embedded operation now defaults to `Save None` for automatic period WAV
  capture on every supported platform.
- Legacy `Save decoded` and `Save all` settings are explicitly disabled when
  Decodium runs through the integrated interface.
- The embedded backend no longer inherits old automatic WAV-retention settings,
  preventing unnoticed long-running recordings from consuming large amounts of
  disk space.

### Live Map localisation

- Completed localisation of the Live Map, roster, statistics, awards, filters
  and layer descriptions in all fourteen supported languages.
- Added the previously missing translation entries while preserving standard
  amateur-radio abbreviations and placeholder formatting.

### Distribution

- Source code is published with the `v1.0.508` tag.
- Windows x64 installer is built with Qt 6.11.0.
- macOS packages are produced for Apple Silicon and Intel systems.
- Linux AppImages are produced for x86_64 and aarch64 systems.
