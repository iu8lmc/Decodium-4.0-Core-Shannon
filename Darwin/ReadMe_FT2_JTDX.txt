Legacy note: FT2 + JTDX macOS coexistence
=========================================

Status
------
This document is retained only as historical reference for older macOS
installations that used `sysctl` / `LaunchDaemon` tuning for shared memory.

It does not describe the normal installation path for current Decodium
releases.

What changed
------------
Current Decodium macOS builds in this fork use the newer Darwin shared-memory
wrapper and do not require users to install `com.ft2.jtdx.sysctl.plist` as a
normal setup step.

Why keep this file
------------------
Older WSJT-X / JTDX / FT2 installations may still leave behind system files
such as:
- `/Library/LaunchDaemons/com.wsjtx.sysctl.plist`
- `/Library/LaunchDaemons/com.jtdx.sysctl.plist`
- `/Library/LaunchDaemons/com.ft2.jtdx.sysctl.plist`

If a user is diagnosing a legacy machine that already contains these files,
they may still matter for historical troubleshooting.

Guidance for current users
--------------------------
- Do not install or enable the old `sysctl` daemon files on a normal current
  Decodium installation unless you are deliberately investigating a legacy
  environment.
- For current app installation, use the normal macOS release notes and, if
  needed, the quarantine command:

  sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
