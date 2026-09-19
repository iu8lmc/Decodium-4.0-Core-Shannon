# Decodium 4.0 v1.0.532

Version 1.0.532 adds a 3D stacked-trace spectrum to the waterfall panel: the
history of the band drawn as a surface receding into the distance, with the
waterfall and every overlay still rendering as before.

## Stacked-trace 3D spectrum

- A single **3D** button in the waterfall toolbar switches the spectrum between
  the familiar 2D trace and a perspective surface built from the band's recent
  history. The waterfall underneath, the frequency labels, the RX and TX markers
  and the decoded callsigns keep rendering as usual.
- The surface is drawn back to front and filled, so nearer traces hide the ones
  behind them and only the ridges that rise above the closer profile stay
  visible. Depth spacing is non-linear: near traces spread apart while distant
  ones bunch toward the horizon, and a perspective frequency grid converges on
  the vanishing point.
- Ridge height is measured above a floor you choose rather than above the
  absolute minimum, so the noise stays flat and the signals stand out. A short
  contrast curve reinforces that.
- Single-frame impulses are rejected with a median across three consecutive
  history rows, which keeps the surface from being littered with spikes.
- The most recent trace is drawn thicker, so the live sweep separates from the
  history behind it.

### Controls

- **3D** — one button, lit when active.
- **Traces** (8–96) — fewer traces separate the ridges, more of them show a
  longer history.
- **Floor** (0–30 dB) — how far above the minimum the ridges start.

Both sliders appear only while 3D is on, and every setting is remembered.

### On cost

The feature is **off by default** and reuses the raw dB history the waterfall
already keeps, so no extra data is collected and no new shader is involved.
Points per trace are decimated to a fixed ceiling; on this machine building the
whole surface measured about 0.3 ms per frame with the render thread idle for
15 ms out of every frame. It should still be left off on modest hardware.

## Translations

- The six new interface strings are translated in all 14 languages; every
  catalog holds 4914 messages with none left untranslated.
