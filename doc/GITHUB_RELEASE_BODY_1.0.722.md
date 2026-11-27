# Decodium 4 v1.0.722

## English (British)

This release is built on v1.0.721 and stops the FT8 decode gate from losing transmit cycles.

### FT8 no longer skips cycles on a slow PC

- The gate introduced in 1.0.714 makes automatic FT8 TX wait for the result of the previous RX slot,
  so that a stale message does not go out when that result changes what must be sent. It is useful
  on a PC with a fast decoder. On a PC whose decoder takes about as long as the wait, TX cycles were
  skipped: the operator saw the transmitter miss two cycles after two passes and then resume,
  sometimes more (IT9MRM).
- Every click, HALT or finished QSO made the program forget how slow the decoder was, and it skipped
  two more cycles while learning that again. That memory now survives a click, a HALT and a QSO.
- As soon as one result has taken 1 s or more, the gate waits until 1.4 s and then sends instead of
  skipping the slot. The wait ends at 1.4 s rather than 1.6 s: on a busy PC the timer ending the
  wait can fire 200 ms late, and a start was refused 6 ms past the 1.8 s limit.
- If the gate still gives trouble on a particular PC, `DECODIUM_FT8_TX_GATE_OFF=1` in the environment
  switches it off and brings back the behaviour of 1.0.712.

## Italiano

Questa versione è basata sulla v1.0.721 e impedisce al gate della decodifica FT8 di far perdere
cicli di trasmissione.

### L'FT8 non salta più cicli su un PC lento

- Il gate introdotto nella 1.0.714 fa attendere al TX automatico FT8 il risultato dello slot RX
  precedente, così non parte un messaggio vecchio quando quel risultato cambia cosa va mandato. È
  utile su un PC con decoder veloce. Su un PC il cui decoder impiega più o meno quanto l'attesa, i
  cicli TX venivano saltati: l'operatore vedeva il trasmettitore perdere due cicli dopo due
  passate e poi riprendere, a volte di più (IT9MRM).
- Ogni clic, HALT o QSO concluso faceva dimenticare al programma quanto fosse lento il decoder, e ne
  saltava altri due mentre lo reimparava. Ora quella memoria sopravvive a un clic, a un HALT e a un
  QSO.
- Appena un risultato ha impiegato 1 s o più, il gate attende fino a 1,4 s e poi trasmette invece di
  saltare lo slot. L'attesa finisce a 1,4 s e non più a 1,6 s: su un PC carico il timer che chiude
  l'attesa può scattare con 200 ms di ritardo, e una partenza era stata rifiutata a 6 ms dal limite
  di 1,8 s.
- Se su un PC il gate dà ancora problemi, `DECODIUM_FT8_TX_GATE_OFF=1` nell'ambiente lo spegne e
  riporta il comportamento della 1.0.712.
