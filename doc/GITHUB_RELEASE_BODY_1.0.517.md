# Decodium 4.0 v1.0.517

Version 1.0.517 consolidates the v1.0.516 localisation update and adds the
latest logbook, digital-mode, Linux audio and station-configuration refinements.

## Changes from v1.0.515 to v1.0.517

### v1.0.516 localisation update

- The satellite tracking, rotator control and offline-mode strings introduced
  in the previous releases are now translated consistently in every supported
  language.
- The translated catalogues cover pass prediction, azimuth and elevation,
  Doppler, TLE age, rotator host, park and stop commands, rotor feedback,
  propagation forecast layers, temporal legends and offline-service messages.
- The catalogues retain consistent placeholders and matching entry counts so
  that changing language does not alter the available controls or data fields.

### v1.0.517 ADIF and logbook improvements

- Newly logged QSOs now receive valid `CQZ` and `ITUZ` ADIF fields when the
  callsign database provides those zones. Unknown or invalid values are left
  unset instead of writing a misleading zero.
- The same zone enrichment is applied to the standard ADIF output and the
  persistent Decodium ADIF log, keeping exported records and internal records
  consistent.
- The ADIF UDP forwarding description and status reporting now clearly identify
  the N1MM-compatible stream used by HRD Logbook QSO Forwarding and EasyLog.
- The settings panel includes an HRD Logbook preset that enables the output and
  selects `127.0.0.1:2333`, while retaining editable server and port controls.

### v1.0.517 digital-mode and station workflow improvements

- Operator-selected CQ modifiers such as `CQ WWA` survive automatic TX-message
  regeneration when callsign, grid or mode data changes.
- The standard-message reset action explicitly clears the custom CQ modifier,
  so the operator can return to the normal generated sequence.
- Linux PulseAudio/PipeWire monitor sources are now available as selectable
  capture inputs. They are labelled separately from ordinary sources for clear
  browser/WebSDR and KiwiSDR audio routing.
- Station settings now provide clearer labels, placeholders and tooltips for
  the active callsign, optional operator callsign, station label, radio
  description and Linux monitor capture sources.
- UDP logging status messages report queued datagrams and the selected ADIF
  destination more precisely.
- The map-service fixture comment and compatibility expectation now reflect
  that imported ADIF records may omit country, continent and zone fields while
  the application can derive them from the callsign database.

## Release contents

- Decodium 4.0 source code at tag `v1.0.517`.
- Windows x64 installer executable.
- macOS DMG and ZIP packages for Apple Silicon and Intel, including supported
  macOS compatibility variants.
- Linux x86_64 and ARM64 AppImage packages with SHA-256 checksums.

No local test suite was executed for this release, as requested.
