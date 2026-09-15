# Decodium 4 FT2 1.0.258

This release focuses on CAT reliability, AutoCQ/QSO sequencing, decode panel stability, and UI polish.

## Fixes

- Ham Radio Deluxe / Icom data mode guard: reasserts DATA mode when HRD reports temporary `Data Off` after QSY and before PTT, preventing USB-D from falling back to plain USB.
- Signal RX auto-clear now counts only real RX decode rows; local TX timeline rows no longer trigger a reset that removes previous received decodes.
- AutoCQ and normal TX sequencing fixes for stale queue entries, final 73 handling, directed report handling, and manual TX transitions.
- Live Map clearing and band-change reset behavior tightened so stale spots and paths do not survive band changes or clear operations.
- Full Spectrum and Signal RX decode windows retain independent 250-row reset behavior.
- macOS GPU indicator removed where the OS process GPU counter is unavailable.
- Compact button labels shortened to fit narrow headers.

## Artifacts

- Windows x64 installer
- macOS Apple Silicon DMG/ZIP
- Linux x86_64 AppImage built with Qt 6.11
