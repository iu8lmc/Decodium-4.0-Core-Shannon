# Decodium 4.0 v1.0.522

Version 1.0.522 makes the network CAT (CAT4OM) support and the FT2-Link
received-files panel introduced in 1.0.521 available in every supported
language.

## Changes from 1.0.521 to 1.0.522

### All fourteen languages realigned

Two separate gaps were closed:

- Upstream 1.0.521 added 83 entries for the FT2-Link received-files panel and
  the network CAT settings to the English and Italian catalogs only, leaving
  the other twelve languages behind. Those entries are now present in every
  language, including the five plural forms with the correct number of variants
  per language.
- A further 45 strings from the network CAT manager were missing from every
  catalog: the connection handshakes, exclusive-control ownership, reconnection
  and error messages. They are now translated as well.

1626 catalog entries were added in total, and all fourteen catalogs again hold
the same number of entries.

### Verification

- No unfinished entries, no duplicate entries within a context, no empty
  translations added and no placeholder mismatches between source and
  translation.
- Compiled catalogs were regenerated and read back in German, plural forms
  included.
- A full audit of every translatable string currently in the sources reports no
  missing catalog entry.

### Included from upstream

- 1.0.521: network CAT and FT2-Link reliability, including the CAT4OM manager
  with radio groups and control ownership, and the FT2-Link received-files
  panel with automatic saving and read/unread handling.
