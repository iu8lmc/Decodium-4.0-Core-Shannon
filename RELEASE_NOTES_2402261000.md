# Decodium v3.0 FT2 - Build 2402261000

**Release Date:** 2026-02-24
**Maintainer:** IU8LMC
**Platform:** Windows x64 / x86
**Signed:** SHA256 + DigiCert timestamp

---

## Download

| File | Arch | Size |
|------|------|------|
| `Decodium_3.0_2402261000_FT2_x64_Setup.exe` | 64-bit | 36.8 MB |
| `Decodium_3.0_2402261000_FT2_x86_Setup.exe` | 32-bit | 34.8 MB |

Both installers are digitally signed (SHA256 + DigiCert RFC3161 timestamp).

---

## New Features

### Time Synchronization Panel (Logger32-style)
- New dedicated dialog: **View > Time Synchronization...**
- Live NTP-corrected UTC clock updated every 100ms with sub-millisecond precision
- NTP status display: sync state, offset, last sync time
- Soundcard drift monitoring: PPM and ms/period with color-coded thresholds
- Bidirectional NTP controls: enable/disable, custom server, Sync Now button
- Color-coded offset history log (green <20ms, yellow <100ms, red >=100ms)
- Persistent window geometry saved in settings
- Fully synchronized with existing status bar NTP controls

### Soundcard Clock Drift Detection
- Real-time measurement of audio device clock drift vs system clock
- Frame counting with elapsed time tracking and outlier filtering
- Drift displayed in PPM and ms/period (emitted every 30s after 10s warm-up)
- Color thresholds: green <10ppm, yellow <50ppm, red >=50ppm
- Visible in status bar (SC label) and Time Sync panel

### NTP Client Improvements
- Round-trip time (RTT) filtering: discards high-latency responses (>100ms)
- IQR (Interquartile Range) outlier removal for robust median calculation
- EMA smoothing (50%) to prevent jumps between sync cycles
- Custom NTP server field in status bar (persisted in settings)
- Refresh interval reduced from 5min to 1min (10s retry when not synced)

### Decoder Watchdog Timer
- Automatic timeout for hung decoders: max(5s, 2x TR period)
- Prevents UI freeze especially in FT2 deep decode mode

### OmniRig Auto-Launch
- Automatic detection and launch of OmniRig.exe if not already running
- 5-second wait loop for COM server availability before connecting

### Multi-Slot Waterfall Markers
- Orange dashed goal posts and bars on waterfall for multi-slot visualization
- Mode-dependent spacing: FT2=200Hz, other modes=60Hz
- Configurable number of slots via existing controls

---

## Bug Fixes

- **PSKReporter:** Fixed `setLocalStation()` missing 5th argument (program_info)
- **Audio output:** Implemented underrun recovery (resume instead of fatal error), reduced notify interval from 1000ms to 20ms, added default buffer sizing (16384 frames) for Windows
- **Multi-slot TX outside Fox mode:** Removed stale multiSlotSequencer/multiSlotRxProcess code and DX Queue guards that were not applicable outside Fox mode
- **FT2 frequency allocation:** Changed 40m FT2 allocation from 7052 kHz to 7062 kHz

---

## Decoder Tuning (Fortran)

- `ft8b.f90`: Increased LLR scaling factor (scalefac 2.83 -> 3.2) for improved soft-decision decoding
- `subtractft8.f90`: Increased NFILT from 4000 to 6000 for better frequency filtering
- `sync8.f90`: Extended sync correlation search window from 2.5s to 3.2s (JZ 62 -> 80)
- `ft8_decode.f90`: More conservative early decode threshold (0.396 -> 0.500), more sensitive sync detection (syncmin 1.3 -> 1.0 pass 1, 2.1 -> 1.8 pass 2)

---

## Branding

- Rebranded from "Decodium 3 FT2" to "Decodium v3.0 FT2" across all dialogs, tooltips, User-Agent strings, ADIF program IDs, splash screen, and About dialog

---

## Build System

- Windows manifest resource integration in VersionResource.rc
- GitHub Actions: macOS build workflow, Linux build with libgfortran5 dependency
- CI: test runner diagnostics workflow

---

## Build Info

- **Compiler:** GCC 15.2.0 (MSYS2 MinGW-w64)
- **Qt:** 5.15.16
- **Boost:** dynamic linking
- **Fortran:** gfortran 15.2.0
- **Inno Setup:** 6
- **Code signing:** signtool SHA256 + DigiCert RFC3161 timestamp
- **Build tag:** 2402261000
