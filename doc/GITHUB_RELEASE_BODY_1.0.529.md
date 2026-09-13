# Decodium 4.0 v1.0.529

Version 1.0.529 fixes a defect that made CAT settings silently revert at every
start whenever a CAT profile was active — most visibly the CAT backend, which
kept falling back to Hamlib and made the TCI audio path impossible to enable.

## Changes from 1.0.528 to 1.0.529

### An active CAT profile silently undid your settings

- On every start, the snapshot of the active CAT profile was written back over
  the live settings. Everything the snapshot carries — the CAT backend, the
  serial port, the baud rate, the CAT mode and the SWR thresholds — was reset to
  whatever had been frozen when the profile was last saved.
- The result was a trap with no way out: a profile saved while the backend was
  Hamlib made it impossible to keep TCI selected. The choice was accepted, then
  rewritten from the inside at the next start, so even making the settings file
  read-only did not help. Because TCI audio only arms when the backend is
  exactly `tci`, that path could never be used, and the same silent reset
  applied to a manually changed port or baud rate.
- Those keys are all **live settings**: they are written by the normal save
  path and by each CAT backend. The profile is a *copy* of them, taken when the
  profile is saved, so re-applying it at startup could never add anything — it
  could only discard later changes.
- The profile is now applied when it is **loaded**, which is when the operator
  actually asks for it. At startup nothing is overwritten; if the snapshot has
  drifted from the settings in use, the diagnostic log says so instead of
  imposing itself:

  ```
  CAT profile colibri: lo snapshot dice hamlib ma vale il backend in uso tci
                       (il profilo si applica solo quando lo si carica)
  ```

- Existing profiles keep working and are still restored by loading them; no
  saved profile is modified by this change.
