# Decodium 4 FT2 v1.0.589

This fork release carries the changes from v1.0.587 through v1.0.589. It
combines the repeatable native SSTV transmitter, clearer TX diagnostics,
network-port diagnostics, Qt 6.11 Linux CI and a safer ADIF export path.

## English (British)

### v1.0.588: repeatable SSTV TX and clearer diagnostics

- Fixed the native SSTV transmission lifecycle so image, calibration-tone and
  HAMDRM transmissions can be started repeatedly in one Decodium session. A
  terminal notification from the previous session can no longer release the
  audio route while the next transmission is being armed; rejected starts still
  release their route cleanly.
- Improved SSTV TX preflight errors. The diagnostic now identifies the blocking
  condition (for example, TX being armed by another mode) instead of returning
  one generic audio/CAT/VOX message.
- Improved TX audio errors by distinguishing an output that was never pinned,
  one that disappeared, and one with an unusable channel count.
- Added UDP message-client bind diagnostics. Logs now show the requested and
  effective port, interface and whether a fixed shared port is being used; a
  listen-port collision with the decode server is explicitly warned about.
- Added the missing `QTimeZone` include in the SSTV share queue manager and
  marked optional Hamlib telemetry parameters correctly for builds without
  `rig_send_raw`.
- Updated Native SSTV Linux CI to install and verify Qt 6.11, including the
  correct x86_64 and ARM64 aqt architectures, instead of relying on Ubuntu's
  older Qt packages.

### v1.0.589: ADIF export compatibility fix (issue #67)

- ADIF exports now omit the legacy unset sentinel `<MY_IOTA:4>NONE`. TQSL and
  other ADIF validators expect an unset optional field to be absent, not to be
  encoded as the literal value `NONE`.
- The sanitisation is applied only to the exported copy through an atomic save;
  the active Decodium logbook is never modified. Valid `MY_IOTA` values and
  `NONE` values in unrelated fields are preserved.
- Added a focused Qt test covering the legacy sentinel, valid IOTA values and
  unrelated fields, and included the sanitizer in both the QML application and
  the test build.

### Packaging and compatibility

- GitHub's generated source archives for tag `v1.0.589` are the codebase
  downloads for this release.
- Release workflows publish the unsigned Windows x64 installer, Linux Qt 6.11
  AppImages for x86_64 and aarch64, and macOS DMGs for the supported Apple
  Silicon and Intel runner targets, each with a SHA-256 checksum where supplied
  by the workflow.
- Native SSTV/HAMDRM remains the documented in-tree subsystem. This release
  does not claim full on-air RF interoperability with every QSSTV/EasyPal mode,
  nor does it replace live radio/audio validation.

## Italiano

### v1.0.588: TX SSTV ripetibile e diagnostica più chiara

- Corretto il ciclo di vita della trasmissione SSTV nativa: immagini, tono di
  calibrazione e HAMDRM possono essere avviati più volte nella stessa sessione
  di Decodium. Una notifica terminale della sessione precedente non può più
  rilasciare la rotta audio mentre si prepara quella successiva; gli avvii
  rifiutati continuano a rilasciare correttamente la rotta.
- Migliorati gli errori del preflight TX SSTV: ora viene indicata la condizione
  che blocca (per esempio TX armato da un altro modo), invece del messaggio
  generico su audio/CAT/VOX.
- Migliorati gli errori dell'uscita audio TX distinguendo dispositivo mai
  fissato, dispositivo scomparso e numero di canali non utilizzabile.
- Aggiunta la diagnostica del bind UDP del message client: il log mostra porta
  richiesta ed effettiva, interfaccia e uso di una porta fissa condivisa; una
  collisione fra porta di ascolto e server decode viene segnalata esplicitamente.
- Aggiunto l'include mancante di `QTimeZone` nel gestore della coda SSTV e
  marcati correttamente i parametri opzionali della telemetria Hamlib nei build
  privi di `rig_send_raw`.
- Aggiornata la CI Linux Native SSTV per installare e verificare Qt 6.11,
  usando le architetture aqt corrette per x86_64 e ARM64 invece dei pacchetti Qt
  più vecchi di Ubuntu.

### v1.0.589: compatibilità dell'esportazione ADIF (issue #67)

- Le esportazioni ADIF ora omettono il sentinel legacy non impostato
  `<MY_IOTA:4>NONE`. TQSL e gli altri validatori ADIF si aspettano che un campo
  opzionale non impostato sia assente, non codificato con il valore letterale
  `NONE`.
- La pulizia viene applicata solo alla copia esportata tramite salvataggio
  atomico; il logbook attivo di Decodium non viene mai modificato. I valori
  `MY_IOTA` validi e i valori `NONE` degli altri campi vengono conservati.
- Aggiunto un test Qt mirato per il sentinel legacy, i valori IOTA validi e i
  campi non correlati; il sanitizer è incluso sia nell'applicazione QML sia nel
  build dei test.

### Packaging e compatibilità

- Gli archivi sorgente generati da GitHub per il tag `v1.0.589` costituiscono i
  download del codebase di questa release.
- I workflow pubblicano l'installer Windows x64 non firmato, le AppImage Linux
  Qt 6.11 x86_64 e aarch64 e i DMG macOS per i runner Apple Silicon e Intel
  supportati, con checksum SHA-256 dove previsto dal workflow.
- SSTV/HAMDRM nativi restano il sottosistema documentato nel tree. Questa
  release non dichiara interoperabilità RF completa con ogni modo QSSTV/EasyPal
  e non sostituisce la validazione live con radio e audio reali.
