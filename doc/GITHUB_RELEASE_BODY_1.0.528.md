# Decodium 4.0 v1.0.528

Version 1.0.528 brings the fork in line with upstream 1.0.527 and ships the
RTL-SDR receiver support that release introduces, on top of the radio detection
and light theme added in 1.0.526.

## Included from upstream

- **1.0.527: RTL-SDR integration and UI resilience.** RTL-SDR dongles can be
  used as a receive input, with their own DSP, tuning plan, RF spectrum and
  audio output paths.
- FT2-Link accepts an explicit bounded diagnostic decode budget, and the
  ultra-low-SNR search is reworked so it stays within it.
- Assorted CI fixes for the RTL-SDR helper build on MinGW and macOS.

## Notes on this build

- This installer is built **with RTL-SDR support enabled**, against librtlsdr
  2.0.2 — the same version the upstream release jobs use.
- Building that helper with the current MSYS2 toolchain (GCC 15.2) needs
  `CFLAGS=-std=gnu17`: librtlsdr's bundled `getopt` declares functions as `()`,
  which under C23 means "no arguments", so `rtl_sdr`, `rtl_test`, `rtl_biast`
  and `rtl_adsb` fail to compile without it. The library itself is unaffected.

## Carried over from 1.0.526

- **Detect my radio**: passive detection of the connected transceiver that
  proposes model, CAT port, baud rate and audio devices, tells apart the two
  ports of a dual-port USB bridge, and offers itself once on a first run.
- Light theme reworked around white cards, hairline borders and slate text,
  keeping a teal-green accent.
- Light/dark switch next to the font size buttons.

## Translations

- All 14 catalogs hold 4177 messages with none left untranslated.
