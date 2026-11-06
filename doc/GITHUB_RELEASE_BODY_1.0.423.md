## Decodium 4 FT2 1.0.423

Questa release contiene fix mirati rispetto alla 1.0.422 per stabilita FT4, gestione colori decode e persistenza del TX watchdog.

### FT4

- Corretto un caso di perdita decode FT4 su PC al limite: durante il drenaggio backlog venivano rimossi anche i metadati di un decode gia in esecuzione.
- Il decode FT4 gia partito viene ora preservato fino alla finestra utile, evitando che risultati validi vengano scartati come callback stale.
- La coda FT4 continua a essere alleggerita quando serve: vengono rimossi i serial accodati vecchi, ma non il decode che il worker sta probabilmente completando.
- La correzione non allarga i tempi di rilascio dei decode e resta confinata al percorso FT4 live.

### Colori decode

- Il colore "CQ nel Messaggio" ora ha precedenza corretta sulle categorie DX Entity e nuove entita quando il relativo colore utente e attivo.
- Full Spectrum, Signal RX e Decode Window usano lo stesso colore configurato per i messaggi CQ.
- La resa dei CQ e ora coerente tra backend e QML, evitando che alcuni CQ restino bianchi o vengano colorati da un'altra categoria.

### TX Watchdog

- Corretto il pannello Setup avanzate: ora mostra esplicitamente modalita TX Watchdog, tempo in minuti e conteggio.
- La modalita Count non viene piu interpretata come `0 minuti` e quindi non viene piu salvata accidentalmente come OFF.
- Le impostazioni `Off`, `Time` e `Count` restano coerenti tra menu principale e Setup avanzate.

### Stabilita e diagnostica

- Migliorati i log del drenaggio backlog FT4 includendo seriale preservato e eta del decode in esecuzione.
- Conservata la protezione adattiva introdotta nella 1.0.422 per i computer piu lenti.

### Versione e packaging

- Versione locale aggiornata a `1.0.423` tramite `fork_release_version.txt`.
- Aggiornati i riferimenti installer Windows Inno Setup e NSIS a `1.0.423`.

### Asset previsti

- Codice sorgente GitHub per `v1.0.423`.
- Installer Windows x64.
- Pacchetti macOS Apple Silicon.
- Pacchetti macOS Intel.
- AppImage Linux x86_64.
- AppImage Linux aarch64.
