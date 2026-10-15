# Decodium 4.0 v1.0.512

Version 1.0.512 makes the Callsign Intelligence and Live Map statistics work
introduced in 1.0.510 and 1.0.511 available in every supported language, and
carries two long-standing interface fixes.

## Changes from 1.0.511 to 1.0.512

### Callsign Intelligence and map statistics localised

- 1.0.510 and 1.0.511 added 160 translatable strings, 139 of which never
  reached the translation catalogs. All of them are now translated into every
  supported language: 1946 catalog entries covering the callsign lookup window,
  the local database and Club Log OQRS settings, the new logbook statistics
  (award progression, period comparison, QSO drill-down) and the Live Map layer
  style, temporal decay and roster scope controls.
- Roughly forty-five of those strings were written with Italian source text.
  Without catalog entries an English interface would have shown them in
  Italian; they now read correctly in English and in the other twelve
  languages.

### Interface fixes

- The RX auto-level "AUTO" label referenced a colour property that does not
  exist in the main window, leaving the colour undefined whenever automatic
  level was off.
- The "ALC target" tooltip was bound to a property that a HoverHandler does not
  provide, so the tooltip never appeared. It now uses the correct property.

### Verification

- All fourteen catalogs hold the same number of entries, none unfinished, no
  empty translations and no placeholder mismatches between source and
  translation.
- Compiled catalogs were read back and spot-checked in German and English.
- The application starts, decodes and renders the Live Map with no QML warnings
  at all.

### Included from upstream

- 1.0.511: safe audio recovery and panadapter guard.
- 1.0.510: operational map intelligence and callsign services.
