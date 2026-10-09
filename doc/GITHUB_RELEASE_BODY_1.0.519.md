# Decodium 4.0 v1.0.519

Version 1.0.519 consolidates the station and ADIF localisation delivered after
v1.0.517 and completes a substantial operational update for satellite windows,
rotator control, Live Map PSK data and non-blocking window-state persistence.

## Changes from v1.0.517 to v1.0.519

### ADIF output and station-field localisation

- The ADIF UDP output, HRD Logbook preset and descriptive station fields added
  after v1.0.517 are available in every supported language.
- Translation coverage includes the station label, rig or radio description,
  legacy operator callsign guidance and the Linux PulseAudio/PipeWire monitor
  source note used for browser, WebSDR and KiwiSDR capture.
- Translation catalogues were aligned so the same controls and placeholders
  are available regardless of the selected interface language.

### Astronomical Data and Satellite tracking windows

- Astronomical Data and Satellite tracking now use independent native desktop
  windows instead of being constrained to the main Decodium surface.
- Both windows can be moved across multiple monitors and resized while keeping
  their desktop-global position, dimensions and target monitor between opens
  and application restarts.
- Native system movement is used for stable cross-monitor dragging, with a
  manual fallback when the platform move operation is unavailable.
- Header drag regions no longer intercept the satellite, minimise or close
  controls, and the close action now hides the resident window content instead
  of rebuilding its complete QML tree on every reopen.
- Material colours are explicitly propagated to the hosted windows, preserving
  readable text, controls and contrast outside the main application window.
- The electric-blue frame remains continuous across the top edge and corners,
  and the window layouts provide scrollable content when the available screen
  height is limited.
- Upcoming-pass timestamps display a space between date and time, while the
  pass list exposes AOS, LOS, maximum elevation and azimuth range without
  clipping the lower portion of the window.

### Satellite operation and rotator controls

- The satellite panel presents satellite selection, TLE refresh, tracking,
  automatic rotator control, rotator enablement, automatic Doppler correction
  and asynchronous 24-hour pass prediction in one operational view.
- Live data includes azimuth, elevation, range, horizon visibility, Doppler,
  tracked frequency, observer locator, TLE age and rotator feedback status.
- Manual movement controls are clearly labelled for azimuth left/right and
  elevation up/down, using 10-degree azimuth and 5-degree elevation steps.
- Rotor STOP and PARK commands are available from the same panel, with status
  feedback and explicit guidance when rotator output is disabled.
- Protocol, host, transport and command-port controls have independent space,
  remain readable at compact window sizes and automatically present the
  correct transport terminology.

### PSTRotator, CatRotator and Hamlib rotctld

- PSTRotator uses UDP port 12000 by default and listens for feedback on the
  following port, 12001, while still allowing both endpoints to follow a
  user-selected command port.
- CatRotator uses UDP port 12000 by default and explicitly reports that position
  feedback is unavailable instead of displaying misleading polling controls.
- Hamlib rotctld support uses asynchronous TCP on port 4533 by default, with
  commands and azimuth/elevation feedback sharing the same connection.
- Protocol changes select the recommended default port only while the current
  value still follows the previous protocol default, preserving deliberate
  custom port assignments.
- TCP connection state, reconnect attempts, command coalescing and feedback
  parsing are handled without blocking the graphical event loop.
- Rotator configuration and transport information are shared consistently by
  Satellite tracking and the Live Map operations panel.

### Local rotator simulation tools

- A dependency-free asynchronous rotator simulator is included for PSTRotator,
  CatRotator and Hamlib rotctld.
- The simulator logs movement, stop and park commands, maintains simulated
  azimuth/elevation state and returns feedback for protocols that support it.
- Documentation includes default endpoints, manual-control steps and examples
  for running multiple mock rotators on separate ports.

### Live Map PSK Reporter data

- Live Map PSK spots now use an independent configurable look-back and expiry
  window from 5 to 60 minutes, with a 15-minute default.
- The map request, loading state and refresh action are separated from the
  callsign heard-by query so one consumer no longer changes the behaviour of
  the other.
- Enabled PSK layers refresh periodically with per-consumer rate limiting, and
  manual refresh can explicitly request a new map snapshot.
- The selected PSK map interval is persisted per profile and synchronised with
  the map source-decay configuration and temporal presentation.
- Network requests remain disabled in Offline mode and retain bounded response
  handling before replacing the current map snapshot.

### Panadapter responsiveness and window persistence

- Window geometry and decode-panel layout are captured as one coherent
  snapshot instead of performing a separate synchronous settings flush for
  every floating window.
- The snapshot is written by the existing serial settings worker, keeping INI
  filesystem work away from the panadapter and graphical event loop.
- The settings object is created, used and destroyed entirely on the worker
  thread, preventing deferred settings events from referencing an object that
  has already been released.
- The final queued layout snapshot is flushed during orderly shutdown so the
  last position, size and monitor assignment are not lost.
- Diagnostic names were added to the general settings and window-state timers,
  making future slow-event reports identify the responsible timer directly.

### Linux Wayland and NVIDIA guidance

- The README documents the optional `QSG_NO_VSYNC=1` AppImage launch workaround
  for affected Wayland and NVIDIA systems that experience long Qt Quick
  synchronisation stalls.
- The guidance also records the possible tearing and GPU-usage trade-offs and
  clarifies that the workaround is not required on unaffected systems.

## Release contents

- Decodium 4.0 source code at tag `v1.0.519`.
- Windows x64 installer executable.
- macOS DMG and ZIP packages for Apple Silicon and Intel, including the
  available compatibility variants.
- Linux x86_64 and aarch64 AppImage packages with SHA-256 checksums.

No local or automated test suite was executed for this release, as requested.
