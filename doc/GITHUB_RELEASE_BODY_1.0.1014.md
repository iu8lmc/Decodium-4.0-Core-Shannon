# Decodium 4 FT2 1.0.1014

## English (UK)

Release 1.0.1014 is the iu8lmc fork build of 1.0.1013 plus mouse-wheel tuning
on the frequency display.

- **Mouse-wheel tuning:** with the pointer over a digit of the frequency
  display at the top, each wheel notch changes that digit by one (wheel up
  raises the frequency, wheel down lowers it). It uses the same step as
  clicking the upper or lower half of the digit, which still works.
- Fast scrolling is added up and sent to the radio as a single frequency
  change 120 ms after the last notch, so the CAT link is not flooded and no
  notch is lost while the radio has not yet reported the new frequency.
  Touchpad scrolling is accumulated the same way.
- The wheel is ignored while transmitting or tuning, like the clicks.
- Validation: Qt Quick tests with real wheel events on the display digits
  (one combined frequency change for two notches, direction, clicks unchanged)
  and an isolated start-up check, plus a test on the operator's own station
  before publication.
- Everything in 1.0.1013 is included unchanged. The previously reported
  Hound/SuperFox RR73 issue remains open.

## Italiano

La release 1.0.1014 è la build del fork iu8lmc della 1.0.1013 con la
sintonia tramite rotella del mouse sul display della frequenza.

- **Sintonia con la rotella:** con il puntatore sopra una cifra del display
  della frequenza in alto, ogni scatto della rotella cambia quella cifra di
  uno (rotella su alza la frequenza, rotella giù la abbassa). Usa lo stesso
  passo del clic sulla metà alta o bassa della cifra, che continua a
  funzionare.
- Se si gira in fretta, gli scatti vengono sommati e mandati alla radio come
  un solo cambio di frequenza 120 ms dopo l'ultimo scatto: la CAT non viene
  sommersa e nessuno scatto va perso mentre la radio non ha ancora comunicato
  la nuova frequenza. Lo scorrimento del touchpad è sommato allo stesso modo.
- Durante la trasmissione o il TUNE la rotella è ignorata, come i clic.
- Verifica: test Qt Quick con eventi rotella veri sulle cifre del display (un
  solo cambio di frequenza per due scatti, verso, clic invariati) e un
  controllo di avvio isolato, più una prova sulla stazione dell'operatore
  prima della pubblicazione.
- Tutto ciò che c'è nella 1.0.1013 è incluso senza modifiche. Il problema
  Hound/SuperFox RR73 precedentemente segnalato rimane aperto.
