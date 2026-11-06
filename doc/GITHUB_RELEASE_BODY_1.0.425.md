## Decodium 4 FT2 1.0.425

Questa release contiene un fix mirato rispetto alla 1.0.424 per separare correttamente il TX Watchdog dal limite del pannello `CALL`.

### CALL e DX-watch

- Il campo `Max chiamate a vuoto` del pannello `CALL` ora conta solo i TX effettuati senza rivedere il target nei decode RX.
- Se il target continua a essere ricevuto, il contatore viene azzerato e la chiamata puo proseguire normalmente.
- Se il target sparisce, Decodium ferma la chiamata dopo il numero configurato di TX a vuoto.
- Con `Re-arm` attivo, al raggiungimento del limite Decodium torna in ascolto sul target invece di continuare a chiamare.
- Il rilevamento del target riusa lo stesso criterio del DX-watch: il target deve comparire come mittente del decode, inclusi i casi `CQ <call>` e i messaggi diretti.

### TX Watchdog

- Il TX Watchdog resta il limite superiore assoluto di sicurezza quando e configurato.
- Il limite `CALL` non duplica piu il watchdog: ora serve a decidere quante chiamate fare dopo che il target non viene piu ricevuto.
- Il timeout totale del pannello `CALL` mantiene il comportamento precedente e resta subordinato alla priorita del watchdog.

### Interfaccia e diagnostica

- Rinominata l'etichetta del pannello da `Tentativi max` a `Max chiamate a vuoto`.
- La telemetria runtime mostra `A vuoto X / N` per chiarire cosa viene contato.
- Aggiornati i tooltip del pulsante `CALL` nella barra TX.
- Aggiunti log espliciti `missedTargetTx` e `target seen again` per diagnosticare quando il target viene rivisto e quando il contatore viene azzerato.

### Versione e packaging

- Versione locale aggiornata a `1.0.425` tramite `fork_release_version.txt`.
- Aggiornati i riferimenti installer Windows Inno Setup e NSIS a `1.0.425`.

### Asset previsti

- Codice sorgente GitHub per `v1.0.425`.
- Installer Windows x64.
- Pacchetti macOS Apple Silicon.
- Pacchetti macOS Intel.
- AppImage Linux x86_64.
- AppImage Linux aarch64.
