## Decodium 4 FT2 1.0.421

Questa release contiene un fix mirato rispetto alla 1.0.420.

### LoTW

- Ripristinato il caricamento automatico dei dati LoTW all'avvio quando l'opzione **LotW Enabled** e attiva.
- I marker grafici LoTW tornano disponibili subito dopo il boot dell'app, senza dover usare manualmente **Force Update** o disattivare/riattivare l'opzione.
- Il caricamento resta asincrono: viene schedulato dopo il caricamento settings e usa prima la cache locale valida, evitando impatti sulla pipeline di decode.
- Dopo il caricamento della cache o del download, la lista decode viene ricalcolata e aggiorna i flag `isLotw` sulle righe gia presenti.

### Note tecniche

- Nessuna modifica al decoder FT4/FT8 o ai timing di rilascio decode.
- Nessuna modifica al formato dei dati LoTW: continua a essere usato `lotw-user-activity.csv` con cache locale.
- La modifica riguarda solo il percorso di startup: il comportamento del pulsante **Force Update** resta invariato.

### Asset previsti

- Codice sorgente GitHub per `v1.0.421`.
- Installer Windows x64.
- Pacchetti macOS Apple Silicon.
- Pacchetti macOS Intel.
- AppImage Linux x86_64.
- AppImage Linux aarch64.
