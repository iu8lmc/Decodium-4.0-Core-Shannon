## English (British)

### Decodium 1.0.655

This release improves CAT digital-mode handling for Icom IC-7300 installations using CAT4OM and OmniRig.

- Normalised `DATA-U`, `USB-D`, `DIGU` and `PKT-U` as equivalent digital USB modes.
- Prevented CAT4OM from silently falling back to plain `USB` when a digital mode is requested.
- Added a post-PTT digital-mode reassertion for CAT4OM and OmniRig, helping retain `DATA-U/USB-D` during transmission.
- Preserved the separate RTTY mode path, including configured LSB/RTTY handling.
- Updated the release version to 1.0.655.

## Italiano

### Decodium 1.0.655

Questa release migliora la gestione dei modi digitali CAT per Icom IC-7300 con CAT4OM e OmniRig.

- Normalizzati `DATA-U`, `USB-D`, `DIGU` e `PKT-U` come modi digitali USB equivalenti.
- Impedito a CAT4OM di ricadere silenziosamente su `USB` quando viene richiesto un modo digitale.
- Aggiunta una riaffermazione del modo digitale dopo l’attivazione del PTT per CAT4OM e OmniRig.
- Mantenuto separato il percorso RTTY, compresa la gestione LSB/RTTY configurata.
- Aggiornata la versione a 1.0.655.
