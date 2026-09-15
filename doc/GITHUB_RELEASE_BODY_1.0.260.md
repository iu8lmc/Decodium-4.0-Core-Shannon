# Decodium 4 FT2 1.0.260

This release focuses on UI state persistence, FT2 async decode ordering, local TX echo filtering, and platform-specific GPU indicator behavior.

## Fixes

- Restores persistence across restart for key floating/docked UI panels and toolbar visibility states, including Full Spectrum, Signal RX, cluster, time sync, live map, active stations, Astro/EME, DecoSyncTime monitor, and DX Cluster panel placement.
- Fixes FT2 async Signal RX ordering by tying decode timestamps to the decoded UTC slot instead of delayed callback delivery time.
- Suppresses recent local FT2 TX echoes from the RX/UI and AutoSeq paths, preventing non-inverted self-decodes from looking like remote traffic.
- Keeps the GPU indicator hidden on macOS, where the process GPU counter is not reliable, while restoring the Windows/Linux fallback render-activity indicator instead of showing `n/a`.

## Artifacts

- Windows x64 installer
- macOS Apple Silicon DMG/ZIP
- Linux x86_64 AppImage built with Qt 6.11
