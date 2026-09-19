# Decodium 4 FT2 1.0.405

Release 1.0.405 is based on upstream 1.0.404 and keeps the FT8 decoder improvements already present in Martino's latest core. This release adds a conservative compatibility fix for PCs that were auto-configured with too few FT decoder threads and, from 1.0.396 onward, could decode little or nothing in FT8.

## Changes from 1.0.404

- FT8 low-thread fallback:
  - PCs with fewer than 3 active FT decoder threads no longer remain on the `depth=2` fast-only FT8 pass when Deep/AP is enabled.
  - In that low-thread case Decodium now runs one capped final full FT8 pass (`depth=4` with AP when enabled) instead of the split fast + deep follow-up pipeline.
  - The conservative pass is capped at 6.5 seconds and is only used when there is no TX audio, no pending TX start, and no active CPU pressure.

- Fast PC behavior preserved:
  - PCs with 3 or more active FT decoder threads keep the existing high-yield path: fast pass plus deep/AP follow-up.
  - No decoder parameters, JTDX-gap recovery paths, hash-cache size, FT8 filters, deduplication, or semantic validation were reduced for fast systems.

- Diagnostics:
  - Added a dedicated log marker for the low-thread path:
    `FT8 final conservative full pass`.
  - Affected users should no longer see repeated FT8 lines like:
    `FT8 final deep followup skipped ... threads=1 threadsOk=0`
    followed by no useful FT8 decode output.

## Expected behavior

- Fast CPUs should keep benefiting from the full Decodium4 FT8 pipeline and the existing JTDX-gap recovery work.
- Slow or constrained CPUs may decode fewer FT8 signals than fast machines, but they should not be forced into the previous fast-only zero-decode behavior.
- Timing remains bounded: the fallback pass uses the same late-decode budget family and caps the low-thread full pass at about 6.5 seconds.

## Validation

- Built locally on macOS with:
  `cmake --build /Users/salvo/Desktop/Decodium4-build --target decodium_app --parallel 6`
- The fix is limited to `DecodiumBridge.cpp` FT8 dispatch selection and does not alter the FT8 decoder core.
