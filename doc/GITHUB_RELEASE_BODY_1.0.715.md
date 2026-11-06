# Decodium 4 v1.0.715

## English (British)

This release is built on v1.0.714 and fixes four faults found in real traffic: a sequencer that
stopped on a slow PC, and three ways a QSO could be logged that had not really happened.

### FT8 auto TX no longer stalls when the decoder is late

- Since 1.0.714 automatic FT8 TX waits at most 1.2 s for the result of the previous RX slot and
  otherwise skips the slot. On a PC where the decode takes 2.3-2.9 s (a system that keeps
  suspending and resuming) every slot was skipped: the final 73 never went out, the QSO was never
  logged and the partner stayed active, so every other caller was ignored.
- The gate now counts consecutive late slots. After one, a closing message (RR73 or 73), which no
  reply can change, is sent without waiting. After two, any message is. A decode in time resets the
  count. The commit deadline and the check that the whole frame fits in the slot are unchanged.
- The log shows `FT8 auto TX: decoder late for N slot(s), sending TXn without ...` when it acts.

### QSOs that did not happen are no longer logged (report from T77RN)

- A low-confidence decode (marked "?") rebuilt by the a-priori memory can keep an exchange moving
  but can never close one: a phantom 73, RR73, RRR or TU used to log a QSO.
- "Log anyway" at the retry cap now logs only if the partner really sent a report. Otherwise
  `QSO NOT logged: retry cap reached but <call> never sent a report` appears in the log. The old
  message said "partner left" even when the partner was still transmitting; it now says "retry cap
  reached".
- The ADIF record says what happened: RST_RCVD is left empty when no report was received (it used
  to be a made-up -10), RST_SENT is the report really transmitted to that station, and the grid
  falls back to the one remembered from its decodes.

## Italiano

Questa versione è basata sulla v1.0.714 e corregge quattro difetti trovati nel traffico reale: un
sequencer che si fermava su un PC lento e tre modi in cui poteva essere registrato un QSO mai
avvenuto.

### Il TX automatico FT8 non si blocca più con il decoder in ritardo

- Dalla 1.0.714 il TX automatico FT8 attende al massimo 1,2 s il risultato dello slot RX
  precedente, altrimenti salta lo slot. Su un PC dove la decodifica impiega 2,3-2,9 s (un sistema
  che continua a sospendersi e riprendersi) ogni slot veniva saltato: il 73 finale non partiva, il
  QSO non veniva registrato e il partner restava attivo, quindi tutti gli altri chiamanti venivano
  ignorati.
- Il gate ora conta gli slot consecutivi in ritardo. Dopo uno, un messaggio di chiusura (RR73 o
  73), che nessuna risposta può cambiare, parte senza attendere. Dopo due, qualsiasi messaggio. Una
  decodifica in tempo azzera il conteggio. Il limite di commit e il controllo che la frame intera
  stia nello slot restano invariati.
- Nel log compare `FT8 auto TX: decoder late for N slot(s), sending TXn without ...` quando
  interviene.

### I QSO mai avvenuti non vengono più registrati (segnalazione di T77RN)

- Una decodifica a bassa confidenza (marcata "?") ricostruita dalla memoria a-priori può far
  proseguire uno scambio ma non può mai chiuderlo: un 73, RR73, RRR o TU fantasma registrava un
  QSO.
- Il "log anyway" al limite dei tentativi registra ora solo se il partner ha davvero mandato un
  report. Altrimenti nel log compare `QSO NOT logged: retry cap reached but <call> never sent a
  report`. Il vecchio messaggio diceva "partner left" anche quando il partner trasmetteva ancora;
  ora dice "retry cap reached".
- Il record ADIF dice ciò che è successo: RST_RCVD resta vuoto se non è arrivato alcun report
  (prima era un -10 inventato), RST_SENT è il report realmente trasmesso a quella stazione e il
  grid ripiega su quello ricordato dalle sue decodifiche.
