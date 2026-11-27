# Decodium 4 v1.0.723

## English (British)

This release is built on v1.0.722 and fixes the pile-up (MAM) sequencer, found in a real pile-up.

### MAM: a partner's clean 73 closes its slot again

- Since 1.0.715 the MAM sequencer threw away every closing message from a partner (73, RR73, RRR,
  TU), not only the doubtful ones, with the log line "low-confidence signoff ignored". No slot
  could be closed by its partner's 73: it kept repeating RR73 up to the retry limit and was then
  logged as "partner left". In a pile-up of 20 QSOs this was the case for 11, and callers waited
  two to three minutes in the queue while three slots were busy.
- Only a decode marked "?" is now refused as a closing message. A clean 73 or RR73 closes the slot
  and logs the QSO at once.

### MAM: a doubtful 73 after our RR73 frees the slot

- A low-confidence 73 still cannot close a QSO. But when the station has already acknowledged our
  report and we have sent RR73 at least once, it means the station has stopped waiting: the slot is
  freed instead of repeating RR73 up to the limit, and the QSO is logged by the usual "log RR73 when
  the partner leaves" rule.

### Clearer log

- `MAM slot X closed, QSO logged (reason)` is written when the QSO was logged. "dropped without log"
  is kept for the ones that really were not. The wait for the final 73 is 120 s, or 60 s when callers
  are queued. The line "TX guard: blocked stale TX6 because AutoCQ is off" is written at most once a
  minute.

## Italiano

Questa versione è basata sulla v1.0.722 e corregge il sequencer del pile-up (MAM), trovato in un
pile-up reale.

### MAM: il 73 pulito del partner chiude di nuovo il suo slot

- Dalla 1.0.715 il sequencer MAM scartava ogni messaggio di chiusura del partner (73, RR73, RRR,
  TU), non solo quelli dubbi, con la riga di log "low-confidence signoff ignored". Nessuno slot
  poteva essere chiuso dal 73 del partner: continuava a ripetere RR73 fino al limite e poi veniva
  registrato come "partner left". In un pile-up di 20 QSO questo valeva per 11, e i chiamanti in
  coda aspettavano due o tre minuti con tre slot occupati.
- Ora viene rifiutata come chiusura solo una decodifica marcata "?". Un 73 o un RR73 pulito chiude
  subito lo slot e registra il QSO.

### MAM: un 73 dubbio dopo il nostro RR73 libera lo slot

- Un 73 a bassa confidenza continua a non poter chiudere un QSO. Ma quando la stazione ha già
  confermato il nostro report e abbiamo mandato almeno un RR73, vuol dire che ha smesso di aspettare:
  lo slot viene liberato invece di ripetere RR73 fino al limite, e il QSO viene registrato con la
  regola solita "registra RR73 quando il partner se ne va".

### Log più chiaro

- Quando il QSO è stato registrato compare `MAM slot X closed, QSO logged (motivo)`;
  "dropped without log" resta per quelli che davvero non lo sono. L'attesa del 73 finale è di 120 s,
  o di 60 s se ci sono chiamanti in coda. La riga "TX guard: blocked stale TX6 because AutoCQ is
  off" esce al massimo una volta al minuto.
