# Decodium 4 FT2 1.0.284 — FT8 QSO-completion fix (field test)

Release 1.0.284 is a **field-test build** of a targeted fix for FT8 QSOs that fail to
complete on some machines — the remote station answers but the QSO never closes
(D4 keeps repeating / the partner does not get a timely reply), while Decodium 3
and JTDX complete the same QSOs on the same computers.

## Root cause (diagnosed from a live capture)

The FT8 auto-sequence committed its next transmission (e.g. a CQ) **at the slot
boundary, before decoding the slot that just ended**. A weak partner's reply —
missed by the fast intra-slot "early visible" pass — was only decoded ~0.8–1.0 s
later, by which time D4 had already started transmitting. The proper reply was then
**deferred by a full cycle**, so the partner never received a timely answer and the
QSO desynchronised into a loop. On slower PCs the early-visible pass is skipped
entirely, so essentially every QSO failed this way. TX timing/PTT itself was fine.

## Changes Since 1.0.283

- FT8/FT4 auto-sequence now **waits for the just-ended slot's decode before
  committing the transmission** (decode-then-decide), so a partner's reply redirects
  the TX from CQ to the correct response instead of being deferred a cycle.
- Increased the FT8 decode grace to 1200 ms (FT4 700 ms) so the wait covers the
  decode + state-advance window, and kept a useful grace (≥900 ms) even under CPU
  pressure — the slow machines that need to wait the most no longer wait the least.
- Late transmit start now **shifts the whole waveform** (up to +2.0 s into the slot)
  instead of trimming its front. Trimming destroyed the leading Costas sync array, so
  the partner could not decode it; shifting keeps the whole signal at a decodable DT.
- Transmissions that would start later than +2.0 s now **defer to the next slot
  boundary** (clean) instead of transmitting a trimmed signal.
- Updated Windows installer/package metadata to 1.0.284.

## Validation

On-air on a fast machine: FT8 (DL3EBJ) and FT2 (EA2AA) QSOs completed and were
logged; CQ/response transmissions are sent whole (shifted, no trim); late retries
defer instead of trimming. **Please field-test on the affected (slower) machines** —
that is where the fix matters most.

## Notes

- This changes FT8 TX timing: transmissions now start ~1.3 s into the slot (shifted)
  instead of ~0.5 s. This is well within decoder tolerance but is a real change —
  report any regressions.
- FT2 async TX behaviour is unchanged.

## Platform Assets

- Windows x64 installer attached to this release.
