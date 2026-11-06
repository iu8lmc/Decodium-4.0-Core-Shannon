# Decodium 4 v1.0.732

## English (British)

This release is built on the upstream main branch (v1.0.728) and adds our pile-up and FT2 fixes on top of it.

- **Per-call QSO limit** (MSHV "Maximum QSOs per Call"). In the PILEUP dialog, 0 = no limit (default), 1, 2 or 3. A station that already has that many QSOs today on the same band and mode does not get a new slot. The count is read from today's log at startup.
- **FT2: the closing wait lasts at least two periods** before a QSO is logged after one RR73.
- **AutoCQ and pile-up: a station worked or logged within 10 minutes** is not reopened by a directed decode or a late report.
- **Stale CQ**: after AutoCQ is stopped, a CQ left in the transmit slot is no longer retried every period.
- Full Spectrum rows are no longer reused, which removes the overlapping text on fast updates.

## Italiano

Questa versione è basata sul ramo principale di upstream (v1.0.728) e aggiunge le nostre correzioni per pile-up e FT2.

- **Limite di QSO per nominativo** (MSHV "Maximum QSOs per Call"). Nel dialogo PILEUP, 0 = nessun limite (predefinito), 1, 2 o 3. Una stazione che ha già quel numero di QSO oggi, sulla stessa banda e modo, non ottiene un nuovo slot. Il conteggio viene letto dal log di oggi all'avvio.
- **FT2: l'attesa di chiusura dura almeno due periodi** prima che un QSO venga registrato dopo un solo RR73.
- **AutoCQ e pile-up: una stazione lavorata o registrata entro 10 minuti** non viene riaperta da una decodifica diretta o da un rapporto tardivo.
- **CQ residuo**: dopo lo stop dell'AutoCQ, un CQ rimasto nello slot di trasmissione non viene più ritentato a ogni periodo.
- Le righe della finestra Full Spectrum non vengono più riciclate: sparisce il testo sovrapposto durante gli aggiornamenti rapidi.
