# Decodium 4 FT2 1.0.286 — FT4 grace consistency fix (field test)

Release 1.0.286 adds a small FT4 robustness fix on top of the QSO-completion work in
1.0.284/1.0.285.

## Changes Since 1.0.285

- **FT4 (and FT8) grace consistency under CPU pressure (fix P1-A).** The gate that
  defers an auto-sequence response to the next slot used the *base* decode grace
  (700 ms for FT4), while the fallback timer used the value *clamped for CPU
  pressure* (900 ms). On slow machines this asymmetry could defer a reply decoded
  between 700–900 ms while the fallback would have accepted it — losing a cycle on
  FT4's short 7.5 s slot. Both now use a single shared helper
  (`effectiveAutoTxDecodeGraceMs`), so the threshold is identical.

## Validation

On-air under **Low CPU mode** (the slow-machine condition, early-visible decode
skipped): FT4 QSO (EA5OL) completed and logged with a consistent 900 ms grace, zero
spurious next-slot deferrals, zero front-trim. Earlier FT4 (F4BAL) completed on a
normal-CPU run.

## Notes

- Includes the FT8/FT4 QSO-completion fix (1.0.284) and FT2 always-async (1.0.285).
- The fix only changes behaviour under CPU pressure; normal-CPU timing is unchanged.

## Platform Assets

- Windows x64 installer attached to this release.
