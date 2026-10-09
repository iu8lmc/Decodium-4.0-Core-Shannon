# Decodium 4 FT2 v1.0.548

Release 1.0.548 consolidates the work delivered after 1.0.546: amplifier
telemetry and SPE integration, safer cross-platform updating, more reliable
floating-window geometry, steadier graphics diagnostics, RTL-SDR settings
polish, and the supporting tests and translations.

## English (British)

### Highlights since 1.0.546

#### Amplifier telemetry and the DECØMETER

- The DECØMETER can read output power, SWR, supply voltage and current,
  heatsink temperatures, warnings and alarms directly from a supported SPE
  Expert amplifier.
- Added the amplifier settings and the EXC/AMP source selector, with the
  displayed source always made explicit.
- Added a native C++ telemetry reader with frame validation, checksum
  protection, malformed-frame rejection and tests for fragmented and noisy
  serial input.
- The CAT port is rejected when it is already assigned to the amplifier, so a
  configuration error cannot make the two serial users compete for the same
  port.
- Added `tools/amp-probe/` and the `tools/spe-tci-bridge/` passive-listening
  path for installations where the manufacturer's application must retain
  ownership of the USB connection.
- Documented the SPE Expert protocol and the TCI telemetry route for Expert
  1.3K-FA, 1.5K-FA and 2K-FA families.

#### Safer update delivery

- The updater now selects the release asset matching both operating system and
  CPU architecture, preventing an x86_64 installation from choosing an ARM64
  package (and vice versa).
- Linux AppImage updates are written through `QSaveFile` and committed by an
  atomic rename, preserving the working image if a download is interrupted.
- A writable running AppImage is replaced and restarted automatically after
  explicit user confirmation. If the installation directory is not writable,
  the package is saved in Downloads and the user is told how to finish the
  update manually.
- The update dialog now explains the platform-specific action and reports the
  installer/download status.
- Fixed the first asynchronous opening of the update dialog.

#### Desktop windows and graphics stability

- Floating Decodium windows now clamp their size and position to the monitor
  that contains the dragged window, including multi-monitor moves.
- Added edge and corner resizing for floating windows, with aspect-ratio
  preservation where required.
- Deferred geometry persistence while a move or resize is in progress, and
  reject invalid or obsolete saved dimensions instead of progressively
  shrinking a window on every restart.
- Reset detached windows to a safe visible geometry when the application
  restores its layout.
- Cached the Qt Quick scene-graph API used by the panadapter, invalidating the
  cache when the scene changes, so render-thread diagnostics do not query the
  renderer on every frame.
- Routed high-volume panadapter/main-thread performance records through the
  asynchronous diagnostic writer rather than the synchronous startup log.

#### CAT, RTL-SDR and translations

- CAT telemetry settings now reconnect only when their values actually change,
  and duplicate reconnect requests are coalesced.
- Added a consistent custom checkbox presentation for RTL-SDR settings and
  tidied the settings grid layout.
- Added the new updater messages in English and Italian, while retaining the
  complete translation coverage introduced with the amplifier controls.
- Increased the GitHub coding-agent conversation budget and made the agent
  callable with `@decodium_agent`.

### Validation

- Added `test_decodium_update_support`, covering architecture matching,
  generic-package fallback, Windows installer selection and atomic executable
  AppImage replacement.
- The release workflows validate the repository layout and the declared
  release version before building the Windows installer, macOS DMGs and Linux
  AppImages.

## Italiano

### Novità dalla 1.0.546

#### Telemetria dell’amplificatore e DECØMETER

- Il DECØMETER può leggere direttamente da un amplificatore SPE Expert la
  potenza d’uscita, il ROS, la tensione e la corrente di alimentazione, le
  temperature dei dissipatori, gli avvisi e gli allarmi.
- Aggiunte le impostazioni dell’amplificatore e la selezione della sorgente
  EXC/AMP, con indicazione esplicita della sorgente mostrata.
- Aggiunto un lettore C++ nativo della telemetria, con validazione dei frame,
  controllo del checksum, rifiuto dei frame corrotti e test per dati seriali
  rumorosi o spezzati tra più letture.
- La porta CAT viene rifiutata quando è già assegnata all’amplificatore, così
  un errore di configurazione non fa competere due utilizzatori sulla stessa
  porta seriale.
- Aggiunti `tools/amp-probe/` e il percorso `tools/spe-tci-bridge/` in ascolto
  passivo, per i casi in cui il programma del costruttore debba mantenere la
  proprietà della connessione USB.
- Documentati il protocollo SPE Expert e il percorso della telemetria via TCI
  per le famiglie Expert 1.3K-FA, 1.5K-FA e 2K-FA.

#### Aggiornamenti più sicuri

- L’aggiornamento seleziona ora il pacchetto compatibile con sistema operativo
  e architettura CPU, evitando di scegliere un pacchetto ARM64 su x86_64 o
  viceversa.
- Gli aggiornamenti Linux AppImage vengono scritti tramite `QSaveFile` e
  applicati con un rename atomico: un download interrotto non distrugge
  l’AppImage funzionante.
- Quando l’AppImage corrente è scrivibile, viene sostituita e Decodium viene
  riavviato dopo la conferma esplicita dell’utente. Se la cartella non è
  scrivibile, il pacchetto viene salvato in Download e l’utente riceve le
  istruzioni per completare manualmente l’aggiornamento.
- La finestra di aggiornamento descrive ora l’azione specifica per la
  piattaforma e mostra lo stato del download/installazione.
- Corretto il primo tentativo di apertura asincrona della finestra di
  aggiornamento.

#### Finestre desktop e stabilità grafica

- Le finestre flottanti limitano ora dimensioni e posizione al monitor che
  contiene la finestra trascinata, anche durante gli spostamenti tra più
  monitor.
- Aggiunto il ridimensionamento dai bordi e dagli angoli, mantenendo il
  rapporto d’aspetto dove necessario.
- Il salvataggio della geometria viene rinviato durante spostamento e
  ridimensionamento; dimensioni salvate non valide o obsolete vengono rifiutate
  invece di restringere progressivamente la finestra a ogni riavvio.
- Le finestre staccate vengono riportate a una geometria visibile e sicura
  quando l’applicazione ripristina il layout.
- L’API dello scene graph Qt Quick usata dal panadapter viene messa in cache e
  invalidata quando cambia la scena, evitando interrogazioni del renderer a
  ogni frame durante i diagnostici del thread grafico.
- I messaggi ad alta frequenza sulle prestazioni del panadapter e del main
  thread passano ora dal writer diagnostico asincrono, invece del log sincrono
  di avvio.

#### CAT, RTL-SDR e traduzioni

- Le impostazioni della telemetria CAT provocano una riconnessione solo quando
  il valore cambia davvero; le richieste duplicate vengono accorpate.
- Aggiunta una resa coerente dei checkbox personalizzati nelle impostazioni
  RTL-SDR e riordinata la griglia delle impostazioni.
- Aggiunti i nuovi messaggi dell’updater in inglese e italiano, mantenendo la
  copertura completa delle traduzioni introdotta con i controlli
  dell’amplificatore.
- Aumentato il limite di conversazione dell’agente di coding su GitHub e resa
  disponibile la chiamata tramite `@decodium_agent`.

### Verifica

- Aggiunto `test_decodium_update_support`, con test per la selezione in base
  all’architettura, il ripiego su pacchetto generico, la scelta dell’installer
  Windows e la sostituzione atomica dell’AppImage eseguibile.
- I workflow di rilascio verificano layout del repository e versione
  dichiarata prima di costruire installer Windows, DMG macOS e AppImage Linux.

---

## In this fork (iu8lmc)

- The eleven new updater messages are available in **all fifteen languages**,
  not only English and Italian. A translation catalogue that never received a
  string reports nothing unfinished, so the gap is silent: it shows up only as
  an update dialog half in English, at the moment the program asks permission
  to replace itself.
- `appImageReplacementIsAtomicAndExecutable` no longer fails on Windows. The
  atomic replacement and the file contents are still verified everywhere; the
  executable-bit assertion is confined to Unix, where that permission exists.
  On Windows `QFileInfo` infers it from the file extension, and `.AppImage`
  is not an executable one — so the test was failing by construction rather
  than reporting a defect.

## In questo fork (iu8lmc)

- Gli undici messaggi nuovi dell’aggiornamento sono disponibili in **tutte e
  quindici le lingue**, non solo in inglese e italiano. Un catalogo che una
  stringa non l’ha mai ricevuta non ha nulla da segnalare come non finito:
  la lacuna è silenziosa, e si vede soltanto come una finestra di
  aggiornamento a metà in inglese, proprio quando il programma chiede il
  permesso di sostituirsi.
- `appImageReplacementIsAtomicAndExecutable` non fallisce più su Windows. La
  sostituzione atomica e il contenuto del file si continuano a verificare
  ovunque; l’asserzione sul bit di esecuzione resta a Unix, dove quel permesso
  esiste. Su Windows `QFileInfo` lo deduce dall’estensione, e `.AppImage` non
  è eseguibile: la prova falliva per costruzione, non per un difetto.

