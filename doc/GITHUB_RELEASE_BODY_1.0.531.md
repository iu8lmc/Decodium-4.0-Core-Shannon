# Decodium 4.0 v1.0.531

Version 1.0.531 brings the fork in line with upstream 1.0.530 and repairs two
things that release broke without saying so: a settings button that did nothing,
and the whole Settings panel falling back to English in every language.

## Included from upstream

- **1.0.530: CAT, settings and map reliability.** The settings dialog is split
  from one 8400-line file into fourteen per-tab files, with map and CAT
  reliability work alongside it.

## Repaired after the split

### The "Detect my radio" button did nothing

- The button ended up in `SettingsTab1.qml` while the results panel stayed in
  `SettingsDialog.qml`. In QML an `id` is not visible across component
  boundaries — the tabs only receive a `dialog` property — so the click handler
  referred to something out of scope and silently did nothing.
- The results panel now lives next to the button that opens it.

### The Settings panel appeared in English in all 14 languages

- In Qt the translation context of `qsTr()` in a QML file is the file name.
  Moving the strings into `SettingsTab0..13` changed their context, but the
  catalogs still filed all 757 of them under `SettingsDialog`, so at runtime
  none of them would have been found.
- The counters could not reveal this: they report the entries that are
  *present*, not the ones that are *looked up*, and they read 0 untranslated
  throughout.
- The fourteen contexts have been rebuilt from the existing translations:
  +731 strings per language, 4908 messages per catalog.

## Translations

- The 49 strings that upstream added with the RTL-SDR receiver and the
  eQSL/LoTW confirmation sections are now translated in all 14 languages
  (585 new entries).
- The eQSL and LoTW texts were written in Italian in the source, so English
  users were reading Italian; they now have proper English along with the other
  twelve languages.
- Every catalog holds 4908 messages with none left untranslated.
