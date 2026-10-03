# Decodium 4.0 v1.0.509

Version 1.0.509 finishes the localisation work for the Live Map band activity
panel introduced in 1.0.508 and fixes a malformed message string.

## Changes from 1.0.508 to 1.0.509

### Band activity panel localised

- 1.0.508 added the Live Map band activity panel with 23 translatable strings,
  18 of which never reached the translation catalogs. Without them Qt falls
  back to the source text, so the thirteen non-English languages showed the new
  panel in English.
- All 18 strings are now translated into every supported language (252 catalog
  entries): band activity and ranking headings, best band, local and PSK
  receive/transmit counters, the analysis window and the empty-state message.

### Malformed error message

- The "failed to create save directory" message used `path: "%1\%` instead of
  `path: "%1"`. The stray escape both truncated the closing quote in the
  message and prevented the string from ever matching its catalog entry, so it
  stayed in English in every language. The identical message twelve lines below
  was already correct.

### Translation audit

The full catalog set was audited against every translatable string currently in
the sources, not just the strings added by the last release:

- 1497 translatable strings in the sources, all present in the catalogs.
- All fourteen catalogs hold the same number of entries, none unfinished, no
  empty translations and no placeholder mismatches between source and
  translation.
