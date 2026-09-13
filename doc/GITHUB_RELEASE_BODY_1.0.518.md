# Decodium 4.0 v1.0.518

Version 1.0.518 makes the ADIF UDP output and station-description work
introduced in 1.0.517 available in every supported language.

## Changes from 1.0.517 to 1.0.518

### ADIF output and station fields localised

- 1.0.517 added 19 translatable strings, 16 of which never reached the
  translation catalogs. All of them are now translated into every supported
  language: 224 catalog entries.
- They cover the N1MM-compatible ADIF UDP output and its HRD Logbook preset,
  the descriptive station fields (station label, rig or radio, and the legacy
  operator callsign that never replaces My Call), and the Linux note about
  Pulse/PipeWire monitor sources for capturing WebSDR or KiwiSDR browser audio.

### Verification

- All fourteen catalogs hold the same number of entries, none unfinished, no
  empty translations and no placeholder mismatches between source and
  translation.
- Compiled catalogs were regenerated and read back in German.
- A full audit of every translatable string currently in the sources reports no
  missing catalog entry.

### Included from upstream

- 1.0.517: ADIF zones and station workflow, including the ADIF UDP output for
  N1MM Logger+ and HRD Logbook QSO forwarding.
