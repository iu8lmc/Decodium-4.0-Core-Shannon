# Decodium 4 FT2 1.0.467

Release 1.0.467 continues the FT2-Link integration work and folds in the latest build/release fixes from the upstream main branch.

## FT2-Link UI and workflow

- Fixed the FT2-Link tool stack so the INFO page no longer draws other tool pages on top of itself.
- Increased the INFO panel height and replaced oversized native checkboxes with compact controls in dense FT2-Link rows.
- Hid the classic FT2 TX macro button row while FT2-Link is active, freeing vertical space for the FT2-Link workspace.
- Kept the upstream adaptive FT2-Link panel height fixes so the lower composer area remains in frame on shorter displays.

## BBS and received content handling

- Added a real BBS bulletin list with incoming/outgoing rows, copy actions, timestamps, and status feedback.
- Added read/unread state for incoming BBS bulletins, including per-row toggles and MARK ALL READ.
- Persisted BBS read state in the local FT2-Link store so unread markers survive refresh/restart.
- Added BBS group presets, custom group saving, default group persistence, and group filtering.
- Clarified the BBS composer labels as GROUP, TITLE, and MESSAGE.

## Broadcast, path, and file UX

- Widened the broadcast composer and separated path relay controls into their own row.
- Fixed alert-tag editing so custom tags are no longer overwritten while the user is typing.
- Preserved the received-file inbox read/unread workflow and RXF list behavior from the previous release.
- Improved FT2-Link tab layout resilience so FILE, BBS, BCAST, MAIL, RXF, PATH, PRE, FREQ, BLK, and CLST stay isolated.

## Profile-aware settings

- Added a shared profile settings helper for active Decodium configurations.
- Scoped multiple runtime settings to the active profile, including audio device identity, CAT native settings, OmniRig settings, transceiver settings, split mode, process priority, TX watchdog, PSK Reporter, low-CPU mode, FT decoder threads, NTP settings, and transient QSO state.
- Prioritized the rig/profile name during QML startup so test instances and profile-specific launches keep separate settings.

## Build and release infrastructure

- Integrated upstream CI fixes for Windows packaging paths, release permissions, and FT2-Link gate hashing on MSYS2.
- Kept the known FT2-Link adapter test gate non-blocking in CI until the upstream test gap is resolved.
- Release runners are expected to publish Windows x64 setup, macOS Apple Silicon DMG/ZIP, macOS Intel DMG/ZIP, Linux x86_64 AppImage, and Linux aarch64 AppImage assets for this tag.

## Validation

- Local QML/C++ target build: `cmake --build "/Users/salvo/Desktop/Decodium4-build" --parallel 4 --target decodium_qml`.
- Structural QML stack check confirmed all FT2-Link tool pages are direct children of the same stack after the INFO page fix.
