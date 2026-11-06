# Decodium 4 FT2 v1.0.657

## English (British)

Release 1.0.657 consolidates the post-1.0.656 TX sequencing, logging and CAT
stability work.

### Fixed

- Profile-aware loading now makes MaxCallerRetries and CallerRetriesAlwaysHard
  consistent between the active profile, the QML control and the bridge runtime.
- Repeated TX2 reports now obey the configured caller-retry cap, including
  when the QSO has already entered a closing state.
- AutoCQ/MAM stale state can no longer send RR73 to a new caller before our
  numeric report has actually been transmitted to that caller.
- A QSO is committed to the log immediately after a verified local final 73;
  the commit no longer depends on a subsequent decode or CAT state update.
- macOS Hamlib PTT handling now allows for Hamlib's one-second CI-V transaction
  timeout and avoids immediate CAT reconnection from the TX failure path.

### Verification

- git diff --check
- macOS decodium_qml target build

## Italiano

La release 1.0.657 consolida il lavoro successivo alla 1.0.656 sulla
sequenza TX, sul logging automatico e sulla stabilità CAT.

### Correzioni

- Il caricamento profile-aware rende coerenti MaxCallerRetries e
  CallerRetriesAlwaysHard tra profilo attivo, controllo QML e runtime bridge.
- I report TX2 ripetuti rispettano ora il limite caller-retry configurato anche
  quando il QSO è già entrato nella fase di chiusura.
- Lo stato obsoleto AutoCQ/MAM non può più inviare RR73 a un nuovo caller
  prima che il nostro report numerico sia stato realmente trasmesso.
- Il QSO viene scritto nel log subito dopo il nostro 73 finale verificato;
  non dipende più da un decode successivo o da un aggiornamento CAT.
- Il PTT Hamlib su macOS tiene conto del timeout CI-V di un secondo ed evita
  la riconnessione CAT immediata dal percorso di errore TX.

### Verifica

- git diff --check
- build macOS del target decodium_qml
