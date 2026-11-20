# Decodium 4 1.0.1001

## English

### DXpedition calendar

- Open **DX calendar** from the DX Cluster panel to browse current and upcoming DXpeditions, their dates and the details supplied by the source, including bands, modes and QSL information where available.
- Search by callsign, entity or details, and filter current, upcoming or all entries. Data comes from the DXCAL calendar linked by NG3K ADXO; dates and information can change.
- Download, parsing, filtering and local cache access run asynchronously. The last valid copy remains available offline or if an update fails. The calendar does not change the radio or start transmission.
- Calendar controls follow the application theme, with readable search, filter and buttons. The calendar does not yet plot expedition locations on the map.

### Settings and radio/audio help

- Search the settings to find relevant pages and controls more quickly.
- A guided radio and audio troubleshooting section shows the connection and monitoring state and offers TCI-specific checks for server address, receiver, audio enablement and RX gain. It reads the current state without changing radio or audio settings.

### Stability and build maintenance

- Adjust QML window cleanup on exit so the main window and its calendar worker are destroyed before the QML engine, addressing a macOS shutdown crash.
- Fix Windows GCC and macOS Intel compiler warning gates affecting release builds.
- Add focused A/B and integration tests for settings search, diagnostics, calendar parsing, filters, asynchronous cache, responsive processing and window cleanup.

## Italiano

### Calendario DXpedition

- Apri **DX calendar** dal pannello DX Cluster per consultare le spedizioni attive e future, le date e i dettagli disponibili dalla fonte, comprese bande, modi e informazioni QSL quando presenti.
- Cerca per nominativo, entità o dettagli e filtra le operazioni attive, future o tutte. I dati provengono dal calendario DXCAL collegato da NG3K ADXO; date e informazioni possono cambiare.
- Download, elaborazione, filtri e accesso alla cache locale sono asincroni. L'ultima copia valida rimane disponibile offline o se l'aggiornamento fallisce. Il calendario non modifica la radio e non avvia trasmissioni.
- I controlli seguono il tema dell'applicazione, con ricerca, filtro e pulsanti leggibili. Le posizioni delle spedizioni non sono ancora mostrate sulla mappa.

### Impostazioni e assistenza radio/audio

- La ricerca nelle impostazioni aiuta a trovare più rapidamente pagine e controlli.
- Una sezione guidata per la diagnostica radio e audio mostra lo stato della connessione e del monitoraggio e propone verifiche specifiche TCI per indirizzo del server, ricevitore, abilitazione audio e guadagno RX. Legge lo stato corrente senza modificare impostazioni radio o audio.

### Stabilità e manutenzione della build

- Modificato l'ordine di chiusura delle finestre QML affinché la finestra principale e il worker del calendario vengano distrutti prima del motore QML, correggendo un crash alla chiusura su macOS.
- Corretti avvisi dei compilatori GCC su Windows e macOS Intel che ostacolavano le build di rilascio.
- Aggiunti test A/B e di integrazione per ricerca nelle impostazioni, diagnostica, lettura e filtri del calendario, cache asincrona, reattività e chiusura delle finestre.
