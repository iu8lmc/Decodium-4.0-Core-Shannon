# Decodium 4 v1.0.718

## English (British)

This release is built on v1.0.717 and fixes a QSO that was never logged when the partner answered
our RR73 with an RR73 of its own.

### A partner who answers RR73 with RR73 is now logged

- Some stations, after receiving our RR73, send RR73 back instead of 73. The sequencer has a safety
  barrier that refuses to send a closing message unless the partner has received a numeric report
  from us. It only looked for a numeric report in the last message we had transmitted, and our last
  message was the RR73 itself, so the barrier took the exchange for a stale one and restarted the
  QSO at TX2. The report was sent again, the partner repeated its answer, and the cycle ran three
  times in four minutes without the QSO ever being logged (LY5A, 27/10/2026).
- Our own RR73, RRR or 73 now counts as proof that the partner has been answered. The barrier
  still blocks a closing message when the last thing sent to that station was only a grid, a CQ
  or a message for somebody else. The 73 is sent and the QSO is logged as usual.

## Italiano

Questa versione è basata sulla v1.0.717 e corregge un QSO che non veniva mai registrato quando il
partner rispondeva al nostro RR73 con un altro RR73.

### Un partner che risponde RR73 a RR73 viene ora registrato

- Alcune stazioni, dopo aver ricevuto il nostro RR73, rispondono con RR73 invece di 73. Il
  sequencer ha una barriera di sicurezza che rifiuta di mandare un messaggio di chiusura se il
  partner non ha ricevuto da noi un report numerico. La barriera cercava il report numerico solo
  nell'ultimo messaggio trasmesso, e l'ultimo messaggio era proprio l'RR73, quindi considerava lo
  scambio obsoleto e riavviava il QSO da TX2. Il report veniva rimandato, il partner ripeteva la
  risposta e il ciclo si è ripetuto tre volte in quattro minuti senza che il QSO fosse mai
  registrato (LY5A, 27/10/2026).
- Il nostro stesso RR73, RRR o 73 vale ora come prova che il partner è stato risposto. La barriera
  continua a bloccare un messaggio di chiusura quando l'ultima cosa mandata a quella stazione era
  solo un grid, un CQ o un messaggio per un altro. Il 73 viene inviato e il QSO registrato come al
  solito.
