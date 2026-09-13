# Decodium 4 FT2 1.0.287 — Waterfall/Full Spectrum drag fix

Release 1.0.287 fixes a window-drag bug on the Waterfall and Full Spectrum pop-out
windows (reported by a tester).

## The bug

With Frameless popouts enabled, the **Waterfall** pop-out window could be dragged
from **anywhere in its body**, not just the title bar — in particular, missing the
thumb of a slider would start moving the whole window. It was the only window that
moved on a click anywhere, while the others move only from the header.

## Cause and fix

Both the Waterfall and Full Spectrum windows had a `DragHandler` attached to the
**Window root** (calling `startSystemMove()`), so its reach covered the entire body
and conflicted with the sliders' grab. The header already has a dedicated move
handler (`MouseArea`) like every other pop-out.

- Removed the root `DragHandler` from the Waterfall window.
- Removed the same root `DragHandler` from the Full Spectrum window (same latent
  issue — it has no sliders, so it was less visible).
- Window move now happens **only from the header**, consistent with all other
  pop-outs, and the magnetic-dock snap is restored (the previous `startSystemMove`
  bypassed it).

## Notes

- Includes everything from 1.0.286 (FT8/FT4 QSO-completion fixes, FT2 always-async).
- Only the move handler changed; resize and all controls are unaffected.

## Platform Assets

- Windows x64 installer attached to this release.
