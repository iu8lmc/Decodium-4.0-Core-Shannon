## Decodium 4 FT2 1.0.426

Questa release contiene un fix mirato rispetto alla 1.0.425 per migliorare la puntualita dei decode FT4 sui sistemi dove il decoder entrava in protezione adattiva dopo un backlog temporaneo.

### FT4 live decode

- Ridotta la finestra adattiva applicata quando il pre-decode FT4 incontra un backlog temporaneo.
- Il cooldown lungo resta riservato alle callback FT4 realmente tardive, dove serve ancora a proteggere i sistemi piu lenti.
- Gli early decode FT4 non vengono piu soppressi per 60 secondi dopo un singolo `early-skip`.
- Il drain del backlog FT4 continua a ripulire il lavoro pendente, ma applica solo una protezione breve per evitare ritardi a catena.

### Impatto operativo

- I decode FT4 dovrebbero tornare a comparire piu vicini alla finestra prevista anche su PC medi, senza togliere la protezione per macchine lente o sotto pressione CPU.
- La logica non aumenta il carico sui single-core: le protezioni Low CPU e CPU pressure restano attive.
- La riduzione adattiva di profondita/thread rimane disponibile quando il decoder rileva callback FT4 veramente oltre soglia.

### Diagnostica

- I log `FT4 adaptive CPU limit active` continuano a mostrare il motivo e la durata residua della protezione.
- I casi `early-skip` e `backlog drain` restano visibili nei log, ma non bloccano piu gli early decode per molti slot consecutivi.

### Versione e packaging

- Versione locale aggiornata a `1.0.426` tramite `fork_release_version.txt`.
- Aggiornati i riferimenti installer Windows Inno Setup e NSIS a `1.0.426`.

### Asset previsti

- Codice sorgente GitHub per `v1.0.426`.
- Installer Windows x64.
- Pacchetti macOS Apple Silicon.
- Pacchetti macOS Intel.
- AppImage Linux x86_64.
- AppImage Linux aarch64.
