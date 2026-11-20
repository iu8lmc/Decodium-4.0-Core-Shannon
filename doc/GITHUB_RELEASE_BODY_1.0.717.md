# Decodium 4 v1.0.717

## English (British)

This release is built on v1.0.715 and fixes two faults in automatic FT8 transmission found in
real traffic. (1.0.716 was an intermediate build and was not published; its fix is included.)

### FT8 auto TX no longer loses cycles on a slow PC

- Since 1.0.714 automatic FT8 TX waits at most 1.2 s for the result of the previous RX slot and
  otherwise skips the whole TX slot. On a PC whose decoder answers in 1.1-1.7 s this lost roughly
  every other cycle by a few milliseconds: a call was repeated only after 4 turns.
- The gate now remembers the last 6 results. When 2 of them were late it waits until 1.6 s (and
  commits until 1.8 s), and if the result still has not arrived it sends the pending message
  instead of skipping the slot. A PC whose decoder is fast keeps the original 1.2 s / 1.4 s
  deadlines, and a late result is still used when it arrives inside the extended window.
- Replaying a logged sequence from such a PC: 9 of 13 TX slots were used before, 12 of 13 now.
- The log shows `(slow decoder)` on the wait line and `FT8 auto TX: slow decoder, sending TXn
  without the RX result` when it sends without the result.

### FT8 and FT4: the TX3 closing message no longer repeats for ever

- With QuickQSO a caller who answers is sent `R-16 TU` (TX3) as the closing message, and this step
  is repeated until the partner acknowledges it. The cap on those repeats existed for FT2 only. In
  FT8 and FT4 the "Caller retries" limit does not apply to closing steps, so a partner who
  disappeared after one reply left the message repeating until the operator pressed HALT: 41
  identical transmissions in 20 minutes were seen on the air, with no log entry and no watchdog
  stop.
- The per-mode closing cap (FT2, FT4 and FT8 settings) now applies to TX3 as well. When it is
  reached, the QSO is logged only if the partner really sent a report; otherwise it is dropped and
  `QSO NOT logged: retry cap reached but <call> never sent a report` appears in the log.

## Italiano

Questa versione è basata sulla v1.0.715 e corregge due difetti della trasmissione automatica FT8
trovati nel traffico reale. (La 1.0.716 era una build intermedia e non è stata pubblicata: la sua
correzione è inclusa.)

### Il TX automatico FT8 non perde più cicli su un PC lento

- Dalla 1.0.714 il TX automatico FT8 attende al massimo 1,2 s il risultato dello slot RX
  precedente, altrimenti salta l'intero slot TX. Su un PC il cui decoder risponde in 1,1-1,7 s
  questo faceva perdere circa un ciclo su due per pochi millisecondi: una chiamata veniva ripetuta
  solo dopo 4 turni.
- Il gate ora ricorda gli ultimi 6 risultati. Quando 2 di questi sono stati in ritardo attende fino
  a 1,6 s (e conferma fino a 1,8 s) e, se il risultato non è ancora arrivato, invia il messaggio
  pendente invece di saltare lo slot. Un PC con decoder veloce mantiene i limiti originali di
  1,2 s / 1,4 s, e un risultato in ritardo viene comunque usato se arriva nella finestra estesa.
- Rigiocando una sequenza registrata da un PC di questo tipo: prima erano usati 9 slot TX su 13,
  ora 12 su 13.
- Nel log compare `(slow decoder)` sulla riga di attesa e `FT8 auto TX: slow decoder, sending TXn
  without the RX result` quando invia senza il risultato.

### FT8 e FT4: il messaggio di chiusura TX3 non si ripete più all'infinito

- Con QuickQSO a chi risponde viene mandato `R-16 TU` (TX3) come messaggio di chiusura, e questo
  passo viene ripetuto finché il partner non lo conferma. Il limite alle ripetizioni esisteva solo
  per FT2. In FT8 e FT4 il limite "Caller retries" non vale per i passi di chiusura, quindi un
  partner sparito dopo una sola risposta lasciava il messaggio in ripetizione finché l'operatore
  non premeva HALT: sono state viste 41 trasmissioni identiche in 20 minuti, senza registrazione e
  senza che il watchdog fermasse nulla.
- Il limite di chiusura per modo (impostazioni FT2, FT4 e FT8) vale ora anche per il TX3. Quando
  viene raggiunto, il QSO è registrato solo se il partner ha davvero mandato un report; altrimenti
  viene scartato e nel log compare `QSO NOT logged: retry cap reached but <call> never sent a
  report`.
