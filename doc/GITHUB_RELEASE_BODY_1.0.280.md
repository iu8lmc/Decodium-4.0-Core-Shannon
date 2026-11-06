# Decodium 4 FT2 1.0.280

Release 1.0.280 is a focused field-fix release after 1.0.279.  It concentrates on FT8 live decode timing and on making the lower status bar usable on smaller monitors.

## Main Changes Since 1.0.279

- Reworked the FT8 early visible decode path so it no longer runs the second partial `nzhsym=47` pass when that pass can steal time from the final live decode.
- Capped FT8 early visible decode depth to a shallow live pass, leaving deeper recovery to the controlled follow-up path instead of blocking slot-end results.
- Disabled AP/deep options on the early visible FT8 pass so it stays fast and predictable.
- Added pre-emption of stale in-flight FT8 early/deep work before dispatching the prioritized final decode for the current slot.
- Added pre-emption of stale FT8 work before the first live early pass when needed, preventing older decode work from delaying current-slot display.
- Improved FT8 decode debug logging with explicit live-depth cap and pre-emption messages.
- Improved the bottom status bar layout for small and medium monitors.
- Added responsive footer breakpoints that reduce margins, spacing, separator height, and CPU/GPU bar widths before the right side is clipped.
- Hid low-priority footer elements on narrow layouts: version text, FT thread badge, S-meter dB text, and DX Cluster label collapse progressively as space shrinks.
- Kept the GPU/CPU indicators visible by preventing the flexible spacer from pushing them off the right edge on compact layouts.
- Updated Windows installer metadata and package version to 1.0.280.

## Platform Assets

- Windows x64 installer is built by the Windows GitHub Actions runner.
- macOS Apple Silicon DMG/ZIP assets are built by the macOS GitHub Actions runner.
- Linux x86_64 Qt 6.11 AppImage is built by the Linux GitHub Actions runner.

## Notes

- The FT8 timing change is intentionally conservative: visible live decode should arrive earlier, while weak-signal/deep recovery remains available only when it does not interfere with the next live slot.
- The footer now favours operational indicators over decorative text on smaller displays.
