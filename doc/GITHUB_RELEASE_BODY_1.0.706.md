# Decodium 4 v1.0.706

## English (British)

This release is built on v1.0.705 and adds an optional score that decides which
caller to answer first when several stations call you.

### Best caller first (new, off by default)

- Until now AutoCQ and multi-answer took the first caller decoded, or the first one
  in the queue. With the new **Best caller first** switch (Settings, next to the
  other FT2/AutoCQ options) the next station is chosen by a score.
- The score favours a strong signal, a new DXCC entity (more if it is new on any
  band, less if it is only new on the current band) and a station that has waited
  longer in the queue, so nobody is left out for ever. A station already worked on
  the current band is moved down; one worked only on another band is moved down
  less.
- It works in FT8, FT4 and FT2:
  - stations waiting in the caller queue (a QSO is in progress, or the
    multi-answer slots are full) are kept in score order;
  - with AutoCQ idle and several stations answering in the same decode batch, the
    best one is fed to the sequencer first and starts the QSO. The decode list on
    screen is not reordered.
- Your own choices always win. A queue order you set by hand is never changed, and
  stations you queue with a double click stay ahead of the automatic callers, in the
  order you clicked them.
- Every reordering is written to the diagnostic log (`CallerScoring ...`) with the
  scores, so the choices can be checked afterwards.
- The switch is off by default: with it off, behaviour is identical to v1.0.705.

### Limits

- In FT2 asynchronous mode decodes can arrive a few at a time. Two stations that
  call in the same period but are decoded at different moments are not compared; the
  first one starts the QSO.
- The weights were chosen by hand and have only been checked in multi-answer use,
  not yet compared over many AutoCQ sessions.
- The legacy macOS path and the multi-answer slot assignment itself are unchanged.
- The text of the new switch is English only for now.

### Release contents

- Based on elisir80/Decodium-4.0-Core-Shannon v1.0.705 (legacy FT2 and CW signal
  selection).
- The Windows installer is built from this repository with the fork packaging.

## Italiano

Questa release è costruita sulla v1.0.705 e aggiunge un punteggio opzionale che
decide a quale stazione rispondere per prima quando più stazioni ti chiamano.

### Best caller first, cioè la migliore per prima (nuovo, spento di default)

- Finora AutoCQ e risposta multipla prendevano il primo chiamante decodificato o il
  primo in coda. Con il nuovo interruttore **Best caller first** (Impostazioni,
  vicino alle altre opzioni FT2/AutoCQ) la stazione successiva viene scelta con un
  punteggio.
- Il punteggio premia un segnale forte, un nuovo DXCC (di più se è nuovo su qualsiasi
  banda, di meno se è nuovo solo sulla banda corrente) e una stazione che aspetta in
  coda da più tempo, così nessuno resta escluso per sempre. Una stazione già lavorata
  sulla banda corrente scende in graduatoria; una lavorata solo su un'altra banda
  scende meno.
- Funziona in FT8, FT4 e FT2:
  - le stazioni in attesa nella coda (un QSO è in corso, oppure gli slot della
    risposta multipla sono pieni) restano in ordine di punteggio;
  - con AutoCQ a riposo e più stazioni che rispondono nello stesso gruppo di
    decodifiche, la migliore viene data per prima al sequencer e avvia il QSO. La
    lista delle decodifiche a schermo non viene riordinata.
- Le tue scelte vincono sempre. Un ordine della coda impostato a mano non viene mai
  cambiato, e le stazioni che accodi con il doppio clic restano davanti a quelle
  automatiche, nell'ordine in cui le hai cliccate.
- Ogni riordino viene scritto nel log diagnostico (`CallerScoring ...`) con i
  punteggi, così le scelte si possono controllare dopo.
- L'interruttore è spento di default: da spento il comportamento è identico alla
  v1.0.705.

### Limiti

- In FT2 asincrono le decodifiche possono arrivare poche alla volta. Due stazioni che
  chiamano nello stesso periodo ma vengono decodificate in momenti diversi non
  vengono confrontate: la prima avvia il QSO.
- I pesi sono stati scelti a mano e sono stati verificati solo in uso con risposta
  multipla, non ancora confrontati su molte sessioni di AutoCQ.
- Il percorso legacy di macOS e l'assegnazione degli slot della risposta multipla non
  cambiano.
- Il testo del nuovo interruttore per ora è solo in inglese.

### Contenuto della release

- Basata su elisir80/Decodium-4.0-Core-Shannon v1.0.705 (FT2 legacy e selezione del
  segnale CW).
- L'installer Windows è costruito da questo repository con il pacchetto del fork.
