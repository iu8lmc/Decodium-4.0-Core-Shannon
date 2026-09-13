# Decodium 4 FT2 v1.0.543

Release 1.0.543 is a cumulative release from 1.0.540 to 1.0.543. It retains
the UDP, Windows graphics, TCI and Romanian-language work from the preceding
releases and adds the large-logbook import, CAT/CI-V persistence, DXCC and QSO
timestamp fixes prepared in the local fork.

## English (British)

### UDP reporting and Windows graphics, carried forward from 1.0.540

- Added independent Decode, Status, Logged QSO and WSPR filters for the
  primary, secondary and tertiary UDP destinations. All four remain enabled by
  default for compatibility with existing integrations.
- A Logged-QSO-only destination stays silent during reception and receives one
  ADIF datagram when a QSO is committed.
- Improved the Windows Qt Quick graphics fallback chain through D3D12, D3D11,
  D3D11 WARP and the Qt software renderer, with clearer diagnostics and a
  conservative Slow-PC path.
- Corrected the CAT poll-interval layout and related settings text.

### 1.0.541 language and compatibility work

- Integrated the 1.0.540 upstream work without changing the existing UDP
  defaults or the cross-platform graphics safeguards.
- Completed the Romanian interface catalogue, including the main window,
  FT2-Link, Live Map, settings pages and service messages.
- Preserved the fixes introduced in the 1.0.537–1.0.539 series for TCI receive
  audio, UDP client identity, decode de-duplication, revision diagnostics and
  graphics start-up recovery.

### 1.0.542 TCI transmit improvements

- The negotiated `audio_samplerate` response is now handled and recorded.
- TCI transmit frames report the rate at which the generated samples were
  actually produced, with a warning when the server rate differs.
- Added a measured push fallback for TCI servers that do not send `TxChrono`.
  It is active only during TCI transmission and yields to the first server-paced
  frame.
- Added diagnostics distinguishing server-paced, client-paced and rate-mismatch
  TCI audio frames.

### 1.0.543 large ADIF import and logbook safety

- Removed the old 32 MB ADIF document limit. The parser can now accept large
  ADIF exports such as logbooks containing tens of thousands of QSOs. A
  separate 200,000-record safety ceiling remains to protect against malformed
  or runaway input.
- Moved parsing, de-duplication, merging, backup creation and atomic writing to
  a background Qt worker. The main interface remains responsive during import.
- Added a progress indicator showing the current phase and percentage, together
  with a final result containing imported, skipped and total QSO counts.
- Created an atomic timestamped backup of the active logbook before replacing
  it. If the backup cannot be created, the import is aborted without modifying
  the logbook.
- Added a concurrent-import guard and a change-detection check so an active
  logbook or source file changed during the operation is not silently
  overwritten.
- Updated both the floating and popup log windows to use the asynchronous API,
  disable only the import control while the job is active and refresh the cache
  after completion.

### 1.0.543 CAT, CI-V, DXCC and QSO metadata fixes

- CI-V addresses can be edited directly in the CAT settings as hexadecimal
  values from `0x00` to `0xFF`.
- Saved CI-V addresses now take precedence over model defaults and are persisted
  with the CAT configuration/profile instead of being overwritten on reload.
- Corrected the special KG4 callsign resolution so the length-sensitive US
  callsign forms are not incorrectly classified as Guantanamo Bay exceptions.
- QSO `TIME_ON` now prefers the first valid reply from the other station, while
  retaining the previous sequence start as a safe fallback.

### Validation

- Local `decodium_qml` build completed successfully with Qt 6.11.
- `qmllint` completed successfully for both log-window QML components; the
  existing project-wide layout warnings remain non-fatal.
- The targeted sequencer, map and DXCC tests passed.
- The release workflows build the Windows x64 installer, macOS Apple Silicon
  and Intel DMG/ZIP packages, and Linux x86_64 and aarch64 AppImages with
  checksums.

## Italiano

La versione 1.0.543 è una release cumulativa dalla 1.0.540 alla 1.0.543.
Conserva il lavoro sui filtri UDP, sulla grafica Windows, sul TCI e sulla lingua
rumena delle versioni precedenti e aggiunge i fix locali per importazione dei
logbook grandi, CAT/CI-V, DXCC e timestamp dei QSO.

### Filtri UDP e grafica Windows, dalla 1.0.540

- Aggiunti filtri indipendenti Decode, Status, QSO registrati e WSPR per le
  destinazioni UDP primaria, secondaria e terziaria. Tutti restano attivi di
  serie per mantenere la compatibilità con le integrazioni esistenti.
- Una destinazione configurata per i soli QSO registrati resta silenziosa
  durante la ricezione e riceve un solo datagramma ADIF quando il QSO viene
  registrato.
- Migliorata la catena di fallback grafico Qt Quick su Windows attraverso
  D3D12, D3D11, D3D11 WARP e renderer software Qt, con diagnostica più chiara e
  percorso conservativo per i PC lenti.
- Corretto l'allineamento del Poll Interval CAT e dei relativi testi.

### Lavoro di compatibilità e lingua della 1.0.541

- Integrato il lavoro upstream della 1.0.540 senza cambiare i default UDP o le
  protezioni grafiche multipiattaforma.
- Completato il catalogo dell'interfaccia rumena, inclusi finestra principale,
  FT2-Link, Live Map, pagine delle impostazioni e messaggi di servizio.
- Conservati i fix della serie 1.0.537–1.0.539 per audio RX TCI, identità dei
  client UDP, deduplicazione dei decode, diagnostica della revisione e recupero
  grafico all'avvio.

### Miglioramenti TCI della 1.0.542

- La risposta negoziata `audio_samplerate` viene ora gestita e memorizzata.
- I frame TCI dichiarano il rate al quale i campioni sono stati realmente
  prodotti, con avviso quando differisce da quello del server.
- Aggiunto un fallback push misurato per i server TCI che non inviano
  `TxChrono`. Si attiva solo durante una trasmissione TCI e lascia il controllo
  al primo frame scandito dal server.
- Aggiunta diagnostica per distinguere audio TCI scandito dal server, dal client
  o con differenza di sample rate.

### Importazione ADIF e sicurezza del logbook nella 1.0.543

- Rimosso il vecchio limite di 32 MB per i documenti ADIF. Ora è possibile
  importare esportazioni grandi, compresi logbook con decine di migliaia di QSO.
  Rimane un limite di sicurezza separato a 200.000 record per proteggere da
  input malformati o senza fine.
- Parsing, deduplicazione, unione, backup e scrittura atomica vengono eseguiti
  in un worker Qt in background. L'interfaccia resta reattiva durante
  l'importazione.
- Aggiunto un indicatore di avanzamento con fase e percentuale, oltre al
  risultato finale con numero di QSO importati, ignorati e totale.
- Prima di sostituire il logbook attivo viene creato un backup atomico con
  timestamp. Se il backup non può essere creato, l'importazione viene annullata
  senza modificare il logbook.
- Aggiunti un blocco contro importazioni concorrenti e un controllo delle
  modifiche: un logbook o file sorgente cambiato durante l'operazione non viene
  sovrascritto in silenzio.
- Le finestre Log flottante e popup usano entrambe la nuova API asincrona,
  disabilitano solo il pulsante di importazione durante il lavoro e aggiornano
  la cache al termine.

### Fix CAT, CI-V, DXCC e metadati QSO nella 1.0.543

- Gli indirizzi CI-V sono modificabili direttamente nelle impostazioni CAT come
  valori esadecimali da `0x00` a `0xFF`.
- Gli indirizzi CI-V salvati hanno ora precedenza sui default del modello e
  vengono mantenuti nel profilo/configurazione CAT anche dopo il riavvio.
- Corretta la risoluzione speciale dei nominativi KG4, così le forme USA con
  lunghezza significativa non vengono classificate erroneamente come eccezioni
  di Guantanamo Bay.
- `TIME_ON` del QSO preferisce ora il primo reply valido dell'altra stazione,
  mantenendo l'inizio della sequenza come fallback sicuro.

### Verifica

- Build locale di `decodium_qml` completata con Qt 6.11.
- `qmllint` completato correttamente per entrambe le finestre QML del log; i
  warning di layout già presenti nel progetto restano non bloccanti.
- Superati i test mirati del sequencer, della mappa e del DXCC.
- I workflow di release generano installer Windows x64, pacchetti DMG/ZIP per
  macOS Apple Silicon e Intel, AppImage Linux x86_64 e aarch64 con checksum.

## Release assets

The release contains the source archive plus the Windows x64 installer, macOS
Apple Silicon and Intel DMG/ZIP packages, and Linux x86_64 and aarch64
AppImages, with checksums where supplied by the workflows.
