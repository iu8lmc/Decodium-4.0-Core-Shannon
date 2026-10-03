# Decodium 4 FT2 1.0.261

This release focuses on Ham Radio Deluxe/Icom data-mode stability, CAT startup reconnect reliability, FT2/manual QSO state hygiene, decode-window retention, and UI refinements.

## Fixes

- Keeps HRD/Icom data mode stable across TX/RX transitions by preserving pending DATA-U/D1 while HRD temporarily reports USB/Data Off during PTT settling.
- Reasserts configured CAT mode on TX start and band/QSY changes, including HRD, so band changes and fake-it split do not fall back to plain USB.
- Restores startup reconnect for Ham Radio Deluxe through the last-successful CAT retry path.
- Clears pending/deferred AutoSeq state on manual/new QSO and enforces PartnerMemory=OFF without automatic resume from stale partner state.
- Keeps pending TX valid only for the current DX call and fully clears QSO state after retry-limit.
- Includes UI/decode-window persistence, RF Signal retention, compact-control, and Windows GPU indicator maintenance from the recent local work.

## Artifacts

- Windows x64 installer
- macOS Apple Silicon DMG/ZIP
- Linux x86_64 AppImage built with Qt 6.11
