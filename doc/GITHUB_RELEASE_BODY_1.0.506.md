# Decodium 4 v1.0.506

## English

Release highlights (`v1.0.505 -> v1.0.506`):

### Live Map station attribution

- Corrected station attribution for directed weak-signal messages. In a
  standard directed message, the first callsign is the addressee and the
  second callsign is the transmitting station.
- Associated transmitted Maidenhead locators with the actual sender instead
  of the addressed station.
- Recalculated DXCC entity, continent, CQ zone and ITU zone from the corrected
  transmitting callsign.
- Prevented stale state and geographic metadata from a previously selected or
  addressed station from being applied to the transmitter.
- Preserved explicit source handling for PSK and OAMS data while applying the
  corrected parsing only to local decoder traffic.

### Existing map database repair

- Added a one-time SQLite migration for decoder spots stored by earlier
  versions.
- Repaired sender and target callsigns in persisted directed messages.
- Updated the related spot-event history so roster, grid details and map
  selections use consistent callsign attribution.
- Rebuilt geographic metadata for repaired rows and removed stale state data
  when the transmitting station changed.
- Recorded the migration version in the map database so the repair is
  transactional and runs only once.

### Release assets

- Windows x64 installer built with Qt 6.11.0.
- macOS Apple Silicon DMG and ZIP for Tahoe and Sequoia.
- macOS Intel DMG and ZIP for Ventura, Sonoma and Sequoia.
- Linux x86_64 AppImage and SHA-256 file.
- Linux aarch64 AppImage and SHA-256 file.
- GitHub source archives.

## Italiano

Novita principali (`v1.0.505 -> v1.0.506`):

### Attribuzione delle stazioni nella Live Map

- Corretta l'attribuzione delle stazioni nei messaggi weak-signal diretti. In
  un messaggio standard diretto, il primo nominativo e il destinatario e il
  secondo e la stazione trasmittente.
- Associato il locator Maidenhead trasmesso alla stazione che lo ha realmente
  inviato, invece che alla stazione destinataria.
- Ricalcolati entita DXCC, continente, zona CQ e zona ITU usando il nominativo
  corretto della stazione trasmittente.
- Impedito che dati geografici o dello stato appartenenti a una stazione
  selezionata o destinataria vengano applicati al trasmettitore.
- Conservata la gestione esplicita delle sorgenti PSK e OAMS, applicando il
  nuovo parsing esclusivamente al traffico proveniente dal decoder locale.

### Riparazione del database esistente

- Aggiunta una migrazione SQLite automatica e una tantum per gli spot salvati
  dalle versioni precedenti.
- Corretti nominativo trasmittente e nominativo destinatario nei messaggi
  diretti gia persistiti.
- Aggiornato lo storico degli eventi collegati, mantenendo coerenti roster,
  dettagli locator e selezioni sulla mappa.
- Ricostruiti i metadati geografici delle righe corrette ed eliminati i dati
  dello stato non piu validi quando cambia il nominativo trasmittente.
- Registrata la versione della migrazione nel database, rendendo la procedura
  transazionale ed eseguibile una sola volta.

### Asset della release

- Installer Windows x64 compilato con Qt 6.11.0.
- DMG e ZIP macOS Apple Silicon per Tahoe e Sequoia.
- DMG e ZIP macOS Intel per Ventura, Sonoma e Sequoia.
- AppImage Linux x86_64 e relativo SHA-256.
- AppImage Linux aarch64 e relativo SHA-256.
- Archivi sorgente GitHub.
