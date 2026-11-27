# Decodium 4.0 v1.0.524

Version 1.0.524 fixes a defect that could silently lose a QSO when **Prompt to
Log** is enabled: no confirmation window appeared and the contact was never
written to the log.

## Changes from 1.0.522 to 1.0.524

### Log confirmation could vanish and take the QSO with it

- When the bridge asked for the log confirmation, the TX panel declared it
  would open the window *before* checking whether its host window was visible.
  A panel that then bailed out at that check had already claimed ownership, so
  the native fallback dialog stayed disarmed and nothing at all appeared. The
  QSO was neither logged nor rejected: it was simply lost.
- The claim is now made only by the confirmation window itself, after it has
  actually been shown. A panel that gives up no longer claims anything, so the
  fallback dialog opens as intended and the contact is never dropped.
- The duplicate-confirmation problem that the early claim was meant to prevent
  does not return: QML `Connections` handlers run synchronously, so the claim
  still reaches the bridge well within its half-second grace period.

### Included from upstream

- 1.0.523: repository cleanup and version alignment.
