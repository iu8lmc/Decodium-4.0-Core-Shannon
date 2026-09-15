# Decodium 4 FT2 1.0.377

Questa release include tutto il percorso da **1.0.375** a **1.0.377**: i fix di sicurezza arrivati in 1.0.376 e le correzioni operative aggiunte in 1.0.377.

## Novita in 1.0.377

- **Log QSO FT2/legacy sincronizzato con il log attivo**: quando il backend legacy registra un QSO ADIF, Decodium ora inoltra quel record al bridge QML e lo importa anche nel logbook attivo, con deduplica. Questo evita il caso in cui il QSO risulta scritto nel log del backend embedded ma non appare nella finestra QSO Log principale.
- **Statistiche e cache log aggiornate subito dopo il mirror ADIF**: dopo l'import automatico vengono ricostruiti worked calls/worked sets, cache QSO, conteggi e segnali UI, cosi il log visibile e le statistiche si aggiornano senza import manuale.
- **Settings dialog usabile su monitor piccoli**: la finestra Setup/Settings non forza piu una larghezza minima di 1360 px. Ora si adatta alla viewport, si ricentra/clampa all'apertura e usa un layout compatto per campi, sidebar e footer.
- **Pulsanti OK/Close sempre raggiungibili**: il footer del setup usa vincoli Layout corretti, testo elidibile e bottoni ridimensionabili, evitando che OK/Annulla escano fuori monitor.
- **Reconnect CAT piu robusto all'avvio**: se Decodium viene chiuso mentre la CAT e connessa, al riavvio ritenta la connessione anche quando il checkbox Auto Connect non era esplicitamente attivo. Se l'utente preme Disconnect prima di uscire, il comportamento resta volontariamente disconnesso.
- **Retry CAT estesi per Windows/HRD/porte COM lente**: i tentativi startup passano da 4 a 8 e arrivano fino a 90 secondi. Questo copre porte COM enumerate lentamente da Windows e il watchdog HRD da 75 secondi.

## Incluso da 1.0.376

Fix di sicurezza e stabilita derivati da audit codice:

- **Crash TX FST4/FST4W/Fox/RTTY**: il buffer `foxcom` viene allocato prima della scrittura in tutti i percorsi TX interessati.
- **WAV malformati**: scansione chunk portata a 64 bit con clamp sul buffer, evitando letture fuori limite con dimensioni corrotte.
- **TX-safety FT2/Digital Morse**: il fast-path FT2 dalla Band Activity rispetta il gate Digital Morse e non arma il TX senza conferma manuale.
- **CAT fail-closed durante TX/tune**: in caso di errore CAT mentre si trasmette o si fa tune, Decodium ferma subito il trasmettitore invece di attendere retry.
- **PSK Reporter**: limite massimo di 1 MiB sulle risposte di ricerca, per evitare uso anomalo di memoria.

## Asset previsti

- Windows x64 installer `.exe`.
- macOS Apple Silicon `.dmg` e `.zip`.
- macOS Intel `.dmg` e `.zip`.
- Linux x86_64 AppImage.
- Linux aarch64 AppImage.

Gli asset sono generati e caricati dai runner GitHub Actions della repository.
