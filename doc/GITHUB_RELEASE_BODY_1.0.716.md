# Decodium 4 v1.0.716

## English (British)

This release is built on v1.0.715 and fixes a transmitter that could be left repeating the same
message for as long as the operator did not notice.

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

Questa versione è basata sulla v1.0.715 e corregge un trasmettitore che poteva restare a ripetere
lo stesso messaggio finché l'operatore non se ne accorgeva.

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
