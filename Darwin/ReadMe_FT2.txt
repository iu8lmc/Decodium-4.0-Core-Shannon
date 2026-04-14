Decodium macOS notes
===================

Current release flow
--------------------
- Current Decodium macOS releases are distributed as `.dmg` and `.zip`.
- No `.pkg` installer is produced in the current release flow.
- Normal installation is simply:
  1. drag `ft2.app` to `/Applications`
  2. if Gatekeeper blocks startup, run:

     sudo xattr -r -d com.apple.quarantine /Applications/ft2.app

Shared-memory note
------------------
Current macOS builds in this fork do not rely on the older System V
`sysctl` / `LaunchDaemon` setup for decoder IPC.

On Darwin, the runtime IPC path uses the newer file-backed shared-memory
wrapper introduced for this fork, so the old `com.ft2.jtdx.sysctl.plist`
workflow is not part of the normal installation path.

Legacy files in this folder
---------------------------
This `Darwin/` folder still contains historical files such as:
- `com.ft2.jtdx.sysctl.plist`
- `com.wsjtx.sysctl.plist`
- legacy coexistence notes for older WSJT-X / JTDX setups

These files are kept only for historical reference and troubleshooting of
older installations. They should not be applied blindly on a normal current
Decodium installation.

If you are specifically investigating an old coexistence issue involving
legacy WSJT-X or JTDX daemons, read:
- `ReadMe_FT2_JTDX.txt`
