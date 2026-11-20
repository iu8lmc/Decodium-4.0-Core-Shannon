# Decodium 4 v1.0.721

## English (British)

This release is built on v1.0.719 and gives back to FT8 and FT4 operators the patience they set. (1.0.720 was an intermediate build and was not published; its change is included.)

### The TX3 wait follows "Caller retries"

- Since 1.0.716 the R-report (TX3) sent to a station that had only sent a report was capped at the
  closing limit (3 transmissions in FT8, about 90 s). A DX station in a pile-up can take many
  more cycles to come back, so such QSOs were closed and logged "anyway" without the station's
  RR73. 1.0.712 had no such cap and did not do this.
- In FT8 and FT4 the cap is now the larger of the closing limit and the operator's own "Caller
  retries" (20 for an operator who works DX, 10 minutes of waiting). With the default settings
  nothing changes, and a station that vanishes is still bounded by the same setting, so the
  endless repeat of 1.0.715 and earlier does not come back. FT2 keeps its own cap.

### A one-shot courtesy 73 (new, tested on rules and code only)

- A station that did not hear our closing message keeps calling for minutes after the QSO has been
  logged. AutoCQ ignores it for five minutes (since 1.0.719), but it was left without an answer.
- When AutoCQ is idle in CQ and a grid, a report, an R-report or an RR73 addressed to you arrives
  from a partner whose QSO was completed or logged less than five minutes ago, Decodium now sends
  that partner a single 73 and goes back to CQ. Never for a low-confidence decode, never when the
  partner has already sent its own 73, never over a QSO in progress or with a caller waiting, and
  at most twice per partner in five minutes. The QSO is not logged a second time.
- It has not yet been run on the air. If a 73 goes out that you did not expect, look for
  `courtesy 73:` in the log.

## Italiano

Questa versione è basata sulla v1.0.719 e restituisce a chi usa FT8 e FT4 la pazienza che ha
impostato. (La 1.0.720 era una build intermedia e non è stata pubblicata: la sua modifica è inclusa.)

### L'attesa del TX3 segue "Caller retries"

- Dalla 1.0.716 l'R-report (TX3) mandato a una stazione che aveva inviato solo il report era
  limitato al tetto di chiusura (3 trasmissioni in FT8, circa 90 s). Un DX in pile-up può impiegare
  molti più cicli a tornare, quindi quei QSO venivano chiusi e registrati "comunque" senza l'RR73
  della stazione. La 1.0.712 non aveva quel tetto e non lo faceva.
- In FT8 e FT4 il tetto è ora il maggiore tra il limite di chiusura e il "Caller retries"
  dell'operatore (20 per chi lavora DX, 10 minuti di attesa). Con le impostazioni di default non
  cambia nulla, e una stazione che sparisce resta limitata dalla stessa impostazione, quindi la
  ripetizione senza fine della 1.0.715 e precedenti non torna. FT2 mantiene il suo tetto.

### Un 73 di cortesia, una sola volta (novità, provata solo sulle regole e sul codice)

- Una stazione che non ha sentito il nostro messaggio di chiusura continua a chiamare per minuti
  dopo la registrazione del QSO. L'AutoCQ la ignora per cinque minuti (dalla 1.0.719), ma restava
  senza risposta.
- Quando l'AutoCQ è fermo in CQ e arriva un grid, un report, un R-report o un RR73 indirizzato a te
  da un partner il cui QSO è stato completato o registrato da meno di cinque minuti, Decodium gli
  manda ora un solo 73 e torna a CQ. Mai per una decodifica a bassa confidenza, mai quando il
  partner ha già mandato il suo 73, mai sopra un QSO in corso o con un chiamante in coda, e al
  massimo due volte per partner in cinque minuti. Il QSO non viene registrato una seconda volta.
- Non è ancora stato provato in aria. Se parte un 73 che non ti aspetti, cerca `courtesy 73:` nel
  log.
