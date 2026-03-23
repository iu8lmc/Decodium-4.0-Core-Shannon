Legacy upstream WSJT-X macOS note
=================================

This file is kept only as archived historical material from older WSJT-X
macOS installation guidance.

It does not describe the current Decodium macOS packaging or installation
flow.

For current Decodium macOS releases:
- use the release README / release notes
- install the app by dragging it to `/Applications`
- if Gatekeeper blocks startup, use:

  sudo xattr -r -d com.apple.quarantine /Applications/ft2.app

Important
---------
The old `com.wsjtx.sysctl.plist` / `LaunchDaemons` instructions in the
historical WSJT-X notes are not part of the normal current Decodium release
path and should not be treated as current installation guidance.
