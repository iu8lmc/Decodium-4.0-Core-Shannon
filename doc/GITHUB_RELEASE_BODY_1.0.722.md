# Decodium 4 v1.0.722

## English (British)

This release is built on v1.0.721 and switches off the FT8 decode gate.

### FT8 transmits at the normal time again

- Since 1.0.714 automatic FT8 TX waited for the result of the previous RX slot before keying. On a
  PC whose decoder takes about as long as the wait, TX cycles were skipped: the operator saw the
  transmitter miss two cycles after two passes and then resume, sometimes more (IT9MRM). Every
  click, HALT or finished QSO also made the program forget how slow the decoder was and skip two
  more cycles while it learned that again, and on a busy PC the timer ending the wait could fire so
  late that the start was refused 6 ms past its limit. Version 1.0.712, which sends at the normal
  time, was stable on the same PC.
- The gate is now off. Automatic FT8 TX goes back to the behaviour of 1.0.712: a short decode grace
  at the start of the slot, then the transmission starts anyway. `DECODIUM_FT8_TX_GATE=1` in the
  environment turns the gate back on for testing.
- The gate code has been kept and improved for that purpose: it now remembers the decoder's speed
  across a QSO, a click or a HALT, reacts from results of 1 s upwards, and ends its extended wait at
  1.4 s.

## Italiano

Questa versione è basata sulla v1.0.721 e spegne il gate della decodifica FT8.

### L'FT8 trasmette di nuovo all'orario normale

- Dalla 1.0.714 il TX automatico FT8 attendeva il risultato dello slot RX precedente prima di
  trasmettere. Su un PC il cui decoder impiega più o meno quanto l'attesa, i cicli TX venivano
  saltati: l'operatore vedeva il trasmettitore perdere due cicli dopo due passate e poi riprendere,
  a volte di più (IT9MRM). Inoltre ogni clic, HALT o QSO concluso faceva dimenticare al programma
  quanto fosse lento il decoder e ne saltava altri due mentre lo reimparava, e su un PC carico il
  timer che chiude l'attesa poteva scattare così tardi che la partenza veniva rifiutata a 6 ms dal
  limite. La 1.0.712, che trasmette all'orario normale, era stabile sullo stesso PC.
- Il gate ora è spento. Il TX automatico FT8 torna al comportamento della 1.0.712: una breve
  attesa della decodifica a inizio slot, poi la trasmissione parte comunque.
  `DECODIUM_FT8_TX_GATE=1` nell'ambiente lo riaccende per le prove.
- Il codice del gate è stato tenuto e migliorato a questo scopo: ora ricorda la velocità del decoder
  attraverso un QSO, un clic o un HALT, reagisce già da risultati di 1 s e chiude l'attesa estesa a
  1,4 s.
