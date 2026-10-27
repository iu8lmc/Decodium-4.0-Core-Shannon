# Decodium 4 v1.0.719

## English (British)

This release is built on v1.0.718 and stops AutoCQ from reopening a station it has just worked.

### A worked partner stays out of AutoCQ for five minutes

- After a QSO, the "already worked" filter kept the partner out of AutoCQ and the MAM queue for
  only 90 seconds. A station that did not hear our RR73 goes on calling for minutes, so when the
  90 s expired it was taken as a new caller and answered again. On the air this meant four more
  RR73 sent to a station already in the log (seen at T77RN's station), four more reports sent to
  a station logged two minutes before, and in one case a second QSO with the same station.
- The window is now 300 seconds for a partner whose QSO was completed or logged, and it is
  renewed every time that partner calls again, so a station that keeps calling is not reopened.
- A partner that was abandoned (it never answered) keeps the old 90 s window, so a station that
  calls again after a failed round is still served. The window used to suppress duplicate log
  entries is unchanged at 90 s.

## Italiano

Questa versione è basata sulla v1.0.718 e impedisce all'AutoCQ di riaprire una stazione appena
lavorata.

### Un partner già lavorato resta fuori dall'AutoCQ per cinque minuti

- Dopo un QSO, il filtro dei "già lavorati" teneva il partner fuori dall'AutoCQ e dalla coda MAM
  per soli 90 secondi. Una stazione che non ha sentito il nostro RR73 continua a chiamare per
  minuti, quindi alla scadenza dei 90 s veniva presa per un chiamante nuovo e riceveva di nuovo
  risposta. In aria questo significava altri quattro RR73 a una stazione già nel log (visto alla
  stazione di T77RN), altri quattro report a una stazione registrata due minuti prima e, in un
  caso, un secondo QSO con la stessa stazione.
- La finestra è ora di 300 secondi per un partner il cui QSO è stato completato o registrato, e si
  rinnova ogni volta che quel partner chiama ancora, così una stazione che insiste non viene
  riaperta.
- Un partner abbandonato (non ha mai risposto) mantiene la vecchia finestra di 90 s, quindi una
  stazione che richiama dopo un giro fallito viene comunque servita. La finestra usata per
  sopprimere i log duplicati resta di 90 s.
