# Decodium 4 v1.0.727

## English (British)

This release is built on v1.0.726 and changes how many RR73 a QSO sends before it closes, and stops a station worked a few minutes ago from being reopened in a pile-up.

- **Final RR73 in FT2, FT4 and FT8: at least 2.** Before, with the operator's limit at 1, the QSO was logged after one RR73, often before the partner had sent its 73 or had stopped sending its report. The operator's value is used when it is higher than 2.
- **Pile-up (MAM): a station worked or logged within 5 minutes is not reopened by a directed decode.** Before, a station logged at 23:01 could be reopened at 23:03 and logged again.

## Italiano

Questa versione è basata sulla v1.0.726 e cambia quanti RR73 invia un QSO prima di chiudersi, e impedisce che una stazione lavorata pochi minuti fa venga riaperta nel pile-up.

- **RR73 finale in FT2, FT4 e FT8: almeno 2.** Prima, con il limite dell'operatore a 1, il QSO veniva registrato dopo un solo RR73, spesso prima che il partner avesse mandato il suo 73 o smesso di mandare il rapporto. Si usa il valore dell'operatore quando è più alto di 2.
- **Pile-up (MAM): una stazione lavorata o registrata entro 5 minuti non viene riaperta da una decodifica diretta.** Prima una stazione registrata alle 23:01 poteva essere riaperta alle 23:03 e registrata di nuovo.
