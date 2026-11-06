# Decodium 4 FT2 1.0.271

This release consolidates the local fixes developed after 1.0.270.  It focuses on weak-signal decode sensitivity, FT2 ghost filtering, RX audio auto-levelling, CAT/data-mode stability, decode-history persistence, and several UI clean-ups requested during field testing.

## Weak-Signal Decode Improvements

- Improves FT8 deep decode by enabling the supplemental path automatically at deep decode depth.
- Extends the FT8 Stage 4 main pass count and enables the repeated-hint supplemental decode path for weak repeated signals.
- Adjusts FT8 bit metric scaling for better behaviour near the lower SNR edge.
- Adds an LDPC OSD fallback path after the regular BP/OSD attempts fail, so valid weak frames have another recovery path before being discarded.
- Raises the displayed/accepted weak SNR floor for FT2 and FT4 from the previous `-21 dB` clamp to `-26 dB`.
- Removes the old UI-side ghost rule that rejected decodes only because their SNR was below `-22 dB`; valid FT8/FT4/FT2 decodes around `-24/-26 dB` now remain visible.
- Adds a synthetic weak-signal regression test harness for FT8/FT4/FT2, including an FT8 deep-decode test at `-24 dB`.

## FT2 Ghost Decode Handling

- Filters FT2 standalone telemetry/hash payloads before they can appear as fake callsigns in Signal RX, Decode History, or Live Map.
- Keeps structural ghost checks, such as invalid callsign/grid patterns and unresolved AP placeholders, while avoiding blanket rejection of real low-SNR decodes.
- Prevents a single long hexadecimal FT2 payload from being promoted into a visible station or map marker.

## QSO And TX Behaviour

- Fixes FT8/FT4 double-click reply period selection so replying to a second-slot caller does not move Decodium into the same slot as the remote station.
- Adds specific FT4 handling for displayed seconds such as `:07`, `:22`, `:37`, and `:52`, which represent `.5 s` slot boundaries internally.
- Keeps the selected application mode canonical (`FT8`, `FT4`, `FT2`, etc.) and refuses to save radio-only mode names as the application mode.
- Prevents radio/CAT labels such as `DATA`, `DATA-U`, `USB-D`, `DIGU`, `PKT`, `USB`, or `FSK` from replacing the active decode mode.
- Improves QSY preset handling when a preset contains a radio-only mode: Decodium now preserves or remaps the application mode instead of switching into an invalid decode mode.

## CAT, HRD, OmniRig, And Time Sync

- Skips OmniRig `Data/Pkt` reassertion when it would map Kenwood rigs such as TS-890/TS-590 into FSK, preserving the radio's current USB-D/data mode instead.
- Keeps CAT mode synchronisation stricter for application modes and safer for radio-only mode labels.
- Simplifies Time Sync opening so the QML time-sync settings are opened directly, avoiding the legacy path that could reload settings and unexpectedly change band or mode.
- Ensures invalid saved modes are ignored on startup and fall back to `FT8` instead of reopening in a radio transport mode.

## RX Audio Auto-Level

- Adds `Auto RX Input Level`, enabled by default.
- The RX level is automatically reduced when input is clipping or too close to full scale.
- The RX level can slowly rise again when a useful signal is consistently too low and there is headroom.
- Manual movement of the RX slider temporarily holds off automatic changes, so the operator remains in control.
- Adds an `AUTO` toggle near the RX slider in the main header and in Setup.
- Persists the auto-level setting across restarts.

## Decode History

- Fixes Decode History persistence so it is not limited to FT2-only paths.
- Persists non-TX decodes from the smooth-flow path, FT8/FT4 direct paths, FT2 async path, legacy JT/Q65/WSPR path, and legacy mirrored band/RX lists.
- Adds de-duplication keys to avoid flooding the SQLite history with mirrored duplicates from the same session/frequency/mode/message.
- Keeps Decode History export/search working across all persisted modes.

## Live Map And Decode Windows

- Clears the Live Map on mode changes together with the decode windows.
- Removes stale Live Map markers that could survive a mode reset.
- Removes the Signal RX frequency badge and the empty-state text `No messages at ... Hz`; the panel is cleaner when no RX-frequency rows are present.

## UI Refinements

- Removes the large `LAYOUT RESET` button from the top-left header area.
- Improves the Call dialog contrast, field styling, radio buttons, checkboxes, spin boxes, and button states.
- Slightly enlarges the Call dialog default size to reduce cramped controls.
- Improves Decode History contrast and styling for filters, combo boxes, spin boxes, buttons, and table rows.
- Replaces hard-coded missing fonts such as `Consolas` with the shared Decodium monospace font family to avoid Qt font alias warnings.
- Keeps the macOS GPU indicator hidden while preserving the Windows/Linux status-bar path.
- Keeps the Tune/Halt control layout work and other toolbar refinements from the local 1.0.270 -> 1.0.271 cycle.

## Documentation

- Rewrites `README.md` as a full GitHub homepage in Italian and British English.
- Consolidates the active project documentation into the main README.
- Removes obsolete legacy release-note/readme files from the repository root.

## Artifacts

- Windows x64 installer
- macOS Apple Silicon DMG/ZIP
- Linux x86_64 AppImage built with Qt 6.11
