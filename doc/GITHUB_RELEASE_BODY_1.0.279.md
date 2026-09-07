# Decodium 4 FT2 1.0.279

Release 1.0.279 consolidates the fixes made after 1.0.278, with emphasis on FT8 live decode timing, weak-signal preservation, QSO workflow safety, UI persistence/readability, frequency handling, and cross-platform packaging.

## Main Changes Since 1.0.278

- Added a two-stage FT8 legacy decode path: a bounded fast live pass for timely display, followed by an optional cancellable deep follow-up pass on the same audio buffer.
- Prevented FT8 deep follow-up decoding from blocking the next real slot; the next slot now pre-empts optional deep work.
- Kept weak-signal recovery available while avoiding the late-decode behaviour where FT8 results appeared several seconds into the following slot.
- Tightened FT8 decode scheduling around TX so optional deep work is skipped while transmitting or when there is not enough safe time before the next decode boundary.
- Added clearer FT8 debug telemetry for live/deep decode starts, completions, queued slots, skipped stale audio, and deep follow-up cancellation.
- Improved local audio/RX handling and decode visibility after TX and mode changes.
- Improved automatic RX level/clipping management and UI feedback around the RX input control.
- Restored and refined Windows GPU indicator behaviour while keeping macOS GPU process counter display disabled where Activity Monitor data is not available.
- Improved graphics fallback handling, including persistent D3D11 fallback behaviour for Windows systems that are unstable with D3D12.
- Improved frequency table handling in Setup/Frequencies, including add/update/new/delete behaviour and mode/band frequency selection.
- Fixed SHF/microwave band frequency selection issues affecting FT4/FT8/FT2 operation on higher bands.
- Improved Live Map clearing on band/mode changes and when decode panes are cleared.
- Reduced stale Live Map paths/markers and improved map state consistency after QSO or band changes.
- Improved DX Cluster floating-window behaviour and startup persistence.
- Improved persistence of pop/dock/visibility settings for Full Spectrum, Signal RX, Cluster, Time Sync, Active Stations, Live Map, Astro/EME, Decode Sync Time monitor, and related panels.
- Added and refined decode history support across all digital modes, not only FT2.
- Improved Decode History and Call dialog styling/readability, including brighter controls and better sizing on macOS/Windows.
- Removed redundant frequency badges and empty Signal RX placeholder text where they distracted from the active decode view.
- Improved footer and control-row button sizing so icons/text fit consistently.
- Fixed Call dialog Stop so it behaves like main-screen HALT.
- Improved draggable/movable behaviour for the Call dialog on Windows.
- Added clearer README/home-page documentation for GitHub.
- Improved waterfall/visual UI controls, including TX/RX width visibility and colour/contrast readability work.
- Added guards against FT2/FT8 ghost/implausible decodes and improved DT/placeholder handling in edge cases.
- Improved FT2 async duplicate handling so UI suppression does not hide legitimate repeated-slot activity.
- Improved special-call and QSO state handling, including manual/double-click QSO resets, pending TX ownership checks, and retry-limit cleanup.
- Improved AutoCQ/queue behaviour so manually selected stations and normal TX are less likely to inherit stale queue or partner state.
- Improved Ham Radio Deluxe and OmniRig data-mode handling to better preserve DATA/USB-D operation across TX/RX, mode changes, and band changes.
- Improved Windows/macOS/Linux packaging metadata for 1.0.279.

## Platform Assets

- Windows x64 installer is built by the Windows GitHub Actions runner.
- macOS Apple Silicon DMG/ZIP assets are built by the macOS GitHub Actions runner.
- Linux x86_64 Qt 6.11 AppImage is built by the Linux GitHub Actions runner.

## Notes

- This release is intended as a practical field-fix release after 1.0.278.
- The FT8 timing change deliberately balances two requirements: fast visible decodes at the slot boundary and preservation of deep weak-signal attempts when the CPU has enough spare time.
