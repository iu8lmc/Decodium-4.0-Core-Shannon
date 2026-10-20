# Decodium 4.0 v1.0.507

Version 1.0.507 completes the localisation of the Live Map introduced in
1.0.505 and carries the station-attribution fix from 1.0.506.

## Changes from 1.0.506 to 1.0.507

### Live Map fully localised

- 1.0.505 introduced 272 new translatable strings but shipped without
  translation catalog entries for them. 254 were missing outright, so Qt fell
  back to the source text and the thirteen non-English languages showed the
  whole Live Map, its roster, statistics and layer descriptions in English.
- All 254 strings are now translated into every supported language: 3556
  catalog entries across Catalan, Danish, Dutch, English, French, German,
  Hungarian, Italian, Japanese, Latvian, Russian, Simplified Chinese,
  Traditional Chinese and Spanish.
- Amateur radio abbreviations are deliberately left untouched (ADIF, PSK, QSL,
  QSO, DXCC, IOTA, POTA, WPX, MUF, foF2, SFI, CQ, CALL), and terminology
  follows the wording already used elsewhere in the interface.

### Verification

- Every catalog validates as XML, all fourteen files hold the same number of
  entries and none is left unfinished.
- Placeholder tokens (%1 … %9) are checked to match between source and
  translation in every entry, which is what prevents formatting failures at
  runtime.
- Compiled catalogs were read back and spot-checked in German, Japanese and
  Russian.

### Included from upstream

- 1.0.506: corrected station attribution for directed weak-signal messages,
  where the first callsign is the addressee and the second is the transmitting
  station, plus the one-time map database repair for spots stored by earlier
  versions.
- 1.0.505: Qt Concurrent linkage fix for release packaging.
