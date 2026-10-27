# Decodium 4 v1.0.721

## English (British)

This release is built on v1.0.720 and gives back to FT8 and FT4 operators the patience they set.

### The TX3 wait follows "Caller retries"

- Since 1.0.716 the R-report (TX3) sent to a station that had only sent a report was capped at the
  closing limit (3 transmissions in FT8, about 90 s). A DX station in a pile-up can take many
  more cycles to come back, so such QSOs were closed and logged "anyway" without the station's
  RR73. 1.0.712 had no such cap and did not do this.
- In FT8 and FT4 the cap is now the larger of the closing limit and the operator's own "Caller
  retries" (20 for an operator who works DX, 10 minutes of waiting). With the default settings
  nothing changes, and a station that vanishes is still bounded by the same setting, so the
  endless repeat of 1.0.715 and earlier does not come back. FT2 keeps its own cap.

## Italiano

Questa versione è basata sulla v1.0.720 e restituisce a chi usa FT8 e FT4 la pazienza che ha
impostato.

### L'attesa del TX3 segue "Caller retries"

- Dalla 1.0.716 l'R-report (TX3) mandato a una stazione che aveva inviato solo il report era
  limitato al tetto di chiusura (3 trasmissioni in FT8, circa 90 s). Un DX in pile-up può impiegare
  molti più cicli a tornare, quindi quei QSO venivano chiusi e registrati "comunque" senza l'RR73
  della stazione. La 1.0.712 non aveva quel tetto e non lo faceva.
- In FT8 e FT4 il tetto è ora il maggiore tra il limite di chiusura e il "Caller retries"
  dell'operatore (20 per chi lavora DX, 10 minuti di attesa). Con le impostazioni di default non
  cambia nulla, e una stazione che sparisce resta limitata dalla stessa impostazione, quindi la
  ripetizione senza fine della 1.0.715 e precedenti non torna. FT2 mantiene il suo tetto.
