## Decodium 4 FT2 1.0.424

Questa release contiene un fix mirato rispetto alla 1.0.423 per rendere il TX Watchdog il limite superiore effettivo quando e configurato dall'utente.

### TX Watchdog

- Il TX Watchdog ora ha priorita sui limiti automatici di retry quando e impostato con un limite valido.
- Il limite `Caller retries` continua a essere contato e registrato nei log, ma non puo piu fermare un QSO prima del TX Watchdog.
- I limiti del pannello `CALL` (`Max retries` e `Timeout`) restano attivi quando il TX Watchdog e disattivato, ma non interrompono piu la chiamata prima del watchdog quando questo e configurato.
- Il watchdog viene considerato configurato solo se la modalita ha un limite reale:
  - `Time` richiede minuti maggiori di zero;
  - `Count` richiede un conteggio valido.
- Gli stop manuali, la chiusura dell'applicazione e la conclusione reale del QSO restano prioritari e continuano a fermare il TX immediatamente.

### Diagnostica

- Aggiunti log espliciti quando un limite retry/timeout viene raggiunto ma ignorato perche il TX Watchdog e prioritario.
- I log distinguono meglio il caso in cui un limite retry viene superato dal caso in cui il watchdog scatta davvero.

### Versione e packaging

- Versione locale aggiornata a `1.0.424` tramite `fork_release_version.txt`.
- Aggiornati i riferimenti installer Windows Inno Setup e NSIS a `1.0.424`.

### Asset previsti

- Codice sorgente GitHub per `v1.0.424`.
- Installer Windows x64.
- Pacchetti macOS Apple Silicon.
- Pacchetti macOS Intel.
- AppImage Linux x86_64.
- AppImage Linux aarch64.
