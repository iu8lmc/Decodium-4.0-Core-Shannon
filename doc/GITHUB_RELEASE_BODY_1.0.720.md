# Decodium 4 v1.0.720

## English (British)

This release is built on v1.0.719 and answers a station that goes on calling after its QSO is logged.

### A one-shot courtesy 73

- A station that did not hear our closing message keeps calling for minutes after the QSO has been
  logged. Since 1.0.719 AutoCQ rightly ignores it for five minutes, but the station was left
  without any answer (seen on the air with EA4HUK and RW0AR, and at T77RN's station with Z32AW and
  RG0S).
- When AutoCQ is idle in CQ and a grid, a report, an R-report or an RR73 addressed to you arrives
  from a partner whose QSO was completed or logged less than five minutes ago, Decodium now sends
  that partner a single 73 and goes back to CQ.
- It never answers a low-confidence decode, never when the partner has already sent its own 73,
  never over a QSO in progress or with a caller waiting, and at most twice per partner in five
  minutes. The QSO is not logged a second time.
- This is new and has been tested on rules and code only. If you see a 73 going out that you did
  not expect, the log line to look for is `courtesy 73:`.

## Italiano

Questa versione è basata sulla v1.0.719 e risponde a una stazione che continua a chiamare dopo che
il suo QSO è stato registrato.

### Un 73 di cortesia, una sola volta

- Una stazione che non ha sentito il nostro messaggio di chiusura continua a chiamare per minuti
  dopo la registrazione del QSO. Dalla 1.0.719 l'AutoCQ la ignora giustamente per cinque minuti, ma
  la stazione restava senza alcuna risposta (visto in aria con EA4HUK e RW0AR, e alla stazione di
  T77RN con Z32AW e RG0S).
- Quando l'AutoCQ è fermo in CQ e arriva un grid, un report, un R-report o un RR73 indirizzato a te
  da un partner il cui QSO è stato completato o registrato da meno di cinque minuti, Decodium gli
  manda ora un solo 73 e torna a CQ.
- Non risponde mai a una decodifica a bassa confidenza, mai quando il partner ha già mandato il suo
  73, mai sopra un QSO in corso o con un chiamante in coda, e al massimo due volte per partner in
  cinque minuti. Il QSO non viene registrato una seconda volta.
- È una novità provata solo sulle regole e sul codice. Se vedi partire un 73 che non ti aspetti, la
  riga di log da cercare è `courtesy 73:`.
