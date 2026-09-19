# Decodium 4.0 v1.0.520

Version 1.0.520 makes the manual rotator control and the PSK Reporter Live Map
window introduced in 1.0.519 available in every supported language.

## Changes from 1.0.519 to 1.0.520

### Rotator control and PSK spots localised

- 1.0.519 added 58 translatable strings, 39 of which never reached the
  translation catalogs. All of them are now translated into every supported
  language: 546 catalog entries.
- They cover the manual rotator movement controls (azimuth and elevation
  steps), the separate command and feedback ports for rotctld over TCP or UDP,
  the tracking status messages, and the adjustable look-back window for PSK
  Reporter spots shown on the Live Map.

### Verification

- All fourteen catalogs hold the same number of entries, none unfinished, no
  empty translations and no placeholder mismatches between source and
  translation.
- Compiled catalogs were regenerated and read back in German.
- A full audit of every translatable string currently in the sources reports no
  missing catalog entry.

### Included from upstream

- 1.0.519: satellite and rotator operations, including manual rotator movement,
  configurable command and feedback ports, and the rotator mock tool for
  testing without hardware.
