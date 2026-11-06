# Decodium 4.0 v1.0.525

Version 1.0.525 restores the themed **Confirm QSO logging** window. Since the
confirmation was introduced it never managed to open: an exception aborted it
before it became visible, and users only ever saw the plain system dialog that
exists as a last-resort fallback.

## Changes from 1.0.524 to 1.0.525

### The themed confirmation window could not open at all

- `centerOnHostWindow()` read `availableGeometry` from the entries of
  `Qt.application.screens`. Those entries are `QQuickScreenInfo`, which does not
  expose that property — it belongs to the C++ `QScreen`. The read returned
  `undefined` and the following `geometry.x` raised
  `TypeError: Cannot read property 'x' of undefined`.
- The exception propagated out of the window's `open()` **before `show()` and
  before the bridge was told the window had appeared**. With no such notice the
  bridge waited half a second and fell back to the native dialog. This happened
  on every layout, not just unusual ones.
- Screen geometry is now derived from the properties QML actually exposes
  (`desktopAvailableWidth` / `desktopAvailableHeight`, falling back to
  `width` / `height`, positioned with `virtualX` / `virtualY`) — the same
  approach already used in `Main.qml`.
- Centring is additionally wrapped so that a failure can never prevent the
  window from opening. That window is also the one that notifies the bridge, so
  appearing in an imperfect position is always better than not appearing.

### The fallback dialog now follows the active theme

- The native `QDialog` kept the system's default appearance and looked foreign
  next to the rest of the interface. It now takes its background, text, border
  and accent colours from the active theme, so it follows light and dark themes
  on its own.
- The fallback remains reachable by design in layouts that cannot show the QML
  window, so it is worth having it match.
