# Decodium 4 FT2 1.0.285 — FT2 always-async + FT8 QSO-completion fix (field test)

Release 1.0.285 bundles the FT8 QSO-completion fix from 1.0.284 with a robustness
change for FT2: **FT2 is now always async, permanently and not disablable**.

## Changes Since 1.0.284

- **FT2 async TX is now permanent and cannot be turned off.** FT2 has always
  treated async as mandatory, but the flag could in principle be off at startup; it
  is now forced on (default true + setter ignores any disable request). This removes
  the FT2 **sync** TX path entirely.
- **Why this matters:** the FT2 sync path had the same class of QSO-completion bug
  fixed for FT8 in 1.0.284 — it committed the next transmission before decoding the
  partner's reply (its decode grace, 250 ms, was shorter than the 1000 ms decode
  settle). Forcing FT2 to async makes that path unreachable. FT2 async is
  decode-then-decide by construction and is immune.
- Includes the FT8/FT4 QSO-completion fix from 1.0.284 (decode-then-decide, shift
  instead of trim on late starts, defer past the shift window).

## Validation

On-air: FT2 QSO (OZ5BD) completed and logged via the async path (report → RR73 →
73 → logged); the sync grace path is never entered. FT8 (DL3EBJ) and earlier FT2
(EA2AA) QSOs also completed cleanly.

## Notes

- FT2 is async only; there is no user setting to disable it (this matches the
  upstream "Async L2 is mandatory" rule, now made airtight).
- FT8 TX timing change from 1.0.284 still applies (TX starts ~1.3 s into the slot,
  shifted) — report any regressions.

## Platform Assets

- Windows x64 installer attached to this release.
