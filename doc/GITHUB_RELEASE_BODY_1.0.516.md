# Decodium 4.0 v1.0.516

Version 1.0.516 makes the satellite tracking, rotator control and offline-mode
work introduced in 1.0.513, 1.0.514 and 1.0.515 available in every supported
language.

## Changes from 1.0.515 to 1.0.516

### Satellite, rotator and offline strings localised

- The three upstream releases added 114 translatable strings, 109 of which
  never reached the translation catalogs. All of them are now translated into
  every supported language: 1526 catalog entries.
- The bulk covers the new satellite window (pass prediction, azimuth and
  elevation, Doppler, TLE age, rotator host, park and stop commands, rotor
  feedback) and the Live Map propagation forecast layers with their temporal
  legend, plus the offline-mode messages for the DX cluster, the callsign
  services, the updater and the propagation cache.
- As in earlier releases, some of those strings were written with Italian
  source text. They now read correctly in English and in the other twelve
  languages.

### Verification

- All fourteen catalogs hold the same number of entries, none unfinished, no
  empty translations and no placeholder mismatches between source and
  translation.
- Compiled catalogs were read back and spot-checked in German.
- The application starts, receives and renders the Live Map with no QML
  warnings.

### Included from upstream

- 1.0.515: PipeWire recovery and PSK history.
- 1.0.514: asynchronous runtime and satellite operations, including the SGP4
  propagator and rotator service.
- 1.0.513: map performance and diagnostics.
