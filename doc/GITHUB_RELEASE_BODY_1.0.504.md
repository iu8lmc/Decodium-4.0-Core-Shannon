# Decodium 4.0 v1.0.504

Version 1.0.504 fixes the installer language handling reported by an English
speaking user ("the install gui is mostly in italian") and removes the last
Italian strings that could still surface in an English interface.

## Changes from 1.0.503 to 1.0.504

### Installer follows the Windows UI language

- The setup type and component descriptions (Full / Light / Custom, alert
  sounds, additional language packs) were hardcoded Italian: the component
  selection page stayed in Italian even when the wizard itself ran in English.
  They are now regular custom messages, translated into all 14 installer
  languages.
- Italian was the first entry in the language list, which Inno Setup uses as
  the fallback whenever Windows UI language detection finds no match. English
  is now first, so any unmatched system language falls back to English.
- `LanguageDetectionMethod=uilanguage` is set explicitly instead of relying on
  the implicit default.

### Remaining Italian strings removed from the interface

- Rig control dialog: "Avanzate" and the truncated-error line counter were
  plain literals and could not be translated at all. Both are now translatable
  and shipped in 14 languages.
- Seven translatable strings still used Italian as their source text (TX power,
  operating bands, direct call, band selector, web server toggle, QSO logging
  confirmation). English translations existed, but the source text is also the
  fallback: had the translation catalog failed to load, an English user would
  have seen Italian. Sources are now English and Italian is a regular
  translation.

### Notes for users on older builds

Interfaces with large amounts of Italian text predate 1.0.492: builds up to
1.0.491 contained roughly seventy visible Italian strings, reduced to a handful
in 1.0.492 and none after this release. Updating resolves it.

All 14 languages ship with no unfinished strings.
