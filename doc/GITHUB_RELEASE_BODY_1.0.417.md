# Decodium 4 FT2 1.0.417

Aggiornamento cumulativo dalla `1.0.415` alla `1.0.417`.

## Novita principali

- Base allineata alla `1.0.416`, inclusi sfondo riga decode per categoria su tutte le liste e invio CQ dalla sezione CW.
- Nuovo indicatore separato per utenti LoTW: il colore LoTW non sovrascrive piu' i colori principali della riga, ma usa un piccolo marker grafico nelle liste decode.
- Nuova visualizzazione opzionale dello Stato USA nei decode quando i dati sono disponibili, con formato compatto `United States · TX` / `CA` / `NY`.
- Pannello impostazioni aggiornato con opzione `US State`, stato caricamento dati e comando manuale `Update`.
- Caricamento dati Stato USA asincrono: download, validazione, cache locale e parsing avvengono senza bloccare UI o decodifica.
- Lookup Stato USA basato su callsign e locator, con fallback sul locator presente nel decode e supporto alle mappe a 4 e 6 caratteri.
- Aggiornamento automatico della cache dati dopo 30 giorni o in caso di file mancanti/corrotti.
- Migliorata la resilienza audio in fase di avvio, cambio device, cambio modo, resume del monitor e risveglio del sistema tramite riapertura forzata dello stream di input quando necessario.
- Rafforzato il filtro contro chiamate fantasma deboli con grid geograficamente incompatibile, mantenendo visibili i messaggi di QSO attivi.
- Full Spectrum, finestra decode e pannello Signal RX aggiornati per mostrare marker LoTW e Stato USA in modo consistente.

## Dettagli tecnici

- Aggiunto `UsStateDataManager` con download tramite `QNetworkAccessManager`, salvataggio atomico con `QSaveFile` e parsing su thread separato.
- Aggiunti nuovi ruoli modello decode per `usState` e aggiornamento dei modelli QML dopo il caricamento dati.
- Aggiunte proprieta' bridge `showUsState`, `usStateDataReady`, `usStateDataUpdating`, `usStateGridCount` e `usStateLocatorCount`.
- Aggiunto restart esplicito dello stream `SoundInput` per evitare stream apparentemente attivi ma non piu' produttivi.
- Rimosso LoTW dalla priorita' colore di sfondo riga: le priorita' New/B4/CQ/DX restano leggibili e LoTW rimane visibile con marker dedicato.

## Asset previsti

- sorgenti GitHub generati dal tag `v1.0.417`;
- installer Windows x64;
- DMG/ZIP macOS Apple Silicon;
- DMG/ZIP macOS Intel;
- AppImage Linux x86_64;
- AppImage Linux aarch64;
- file SHA256 dove prodotti dai runner.
