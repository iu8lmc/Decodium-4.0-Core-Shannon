# Decodium 4 v1.0.724

## English (British)

This release is built on v1.0.723 and fixes slots that were reopened right after they were dropped in a pile-up (MAM).

- A dropped exchange is remembered as abandoned for 90 seconds, so the next decode from that station no longer reopens it at once.
- The resume count of a dropped slot is carried over, so the two-resume limit holds.
- A low-confidence grid ('?') no longer opens a slot without a report. Recovery traffic (final 73, RR73, report) from a station just dropped is still accepted.
- A station already in the log (within 5 minutes) is not resumed or promoted again from a recovery.

## Italiano

Questa versione è basata sulla v1.0.723 e corregge gli slot riaperti subito dopo lo scarto nel pile-up (MAM).

- Uno scambio scartato viene ricordato come abbandonato per 90 secondi: la prima decodifica successiva non lo riapre più subito.
- Il contatore delle riprese passa allo slot riaperto, quindi il limite di due riprese vale davvero.
- Un grid "?" a bassa confidenza non apre più uno slot senza report. Il traffico di recupero (73 finale, RR73, rapporto) di una stazione appena scartata viene comunque accettato.
- Una stazione già nel log (entro 5 minuti) non viene ripresa né promossa di nuovo da un recupero.
