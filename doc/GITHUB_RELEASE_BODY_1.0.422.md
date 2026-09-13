## Decodium 4 FT2 1.0.422

Questa release contiene un fix mirato rispetto alla 1.0.421 per la stabilita FT4 su computer che non riescono a completare i decode profondi entro la finestra utile.

### FT4

- Aggiunto un guard adattivo per i callback FT4 tardivi: quando un decode FT4 arriva oltre la finestra di consegna, Decodium registra lo streak e attiva temporaneamente un profilo FT4 alleggerito.
- Aggiunto rilevamento del backlog FT4 sui serial ancora pendenti: se il worker accumula piu decode o un decode resta troppo vecchio, la coda FT4 viene drenata prima del nuovo final pass.
- L'early decode FT4 viene saltato quando il profilo adattivo e attivo o quando e gia presente backlog, evitando che i PC piu lenti accodino lavoro che poi verrebbe scartato come stale.
- Il final pass FT4 resta sempre il percorso principale: durante il cooldown adattivo viene limitato a `depth <= 3` e `threads <= 4`, cosi i risultati arrivano in tempo invece di essere scartati.
- Il comportamento aggressivo resta invariato sui PC veloci: il profilo adattivo scatta solo dopo late callback o backlog FT4 reale.

### Stabilita e diagnostica

- I log FT4 ora indicano quando il profilo adattivo entra in funzione, con motivo, seriale, elapsed time e streak.
- Aggiunti log di dispatch FT4 con depth effettiva, depth richiesta, thread effettivi e flag `adaptive=1`.
- Aggiunto log di drenaggio backlog FT4 con seriale mantenuto e numero di metadati rimossi.
- La protezione e confinata a FT4: non modifica i percorsi FT8, FT2, Q65 o il decoder DSP interno.

### Timing

- Il fix non allarga la finestra di consegna dei decode.
- Nei casi di CPU limitata preferisce ridurre temporaneamente il carico FT4 invece di accettare risultati fuori tempo.
- Il profilo normale resta disponibile automaticamente appena il cooldown scade.

### Versione e packaging

- Versione locale aggiornata a `1.0.422` tramite `fork_release_version.txt`.
- Aggiornati i riferimenti installer Windows Inno Setup e NSIS a `1.0.422`.

### Asset previsti

- Codice sorgente GitHub per `v1.0.422`.
- Installer Windows x64.
- Pacchetti macOS Apple Silicon.
- Pacchetti macOS Intel.
- AppImage Linux x86_64.
- AppImage Linux aarch64.
