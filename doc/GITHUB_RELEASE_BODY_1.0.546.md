# Decodium 4 FT2 v1.0.546

Version 1.0.546 brings the Decodium 1.0.544 and 1.0.545 work into this release
line and adds a substantial reliability and desktop-integration update. The
main changes since 1.0.543 are the DECØMETER RF instrument, shared CAT,
multi-monitor windows, safer audio recovery on Windows and direct QMX power/SWR
telemetry when Hamlib does not expose those meters.

## English (British)

### DECØMETER RF instrument

- Added the **DECØMETER**, available from the main menu, with concentric scales
  for forward power, reflected power, SWR and ALC.
- Added realistic instrument ballistics: immediate attack, exponential release,
  three-second peak hold, running average and PEP.
- Added automatic and manual 5 W, 50 W, 500 W and 5 kW ranges, with smooth range
  transitions and correctly relabelled scales.
- Added power, matching and drive screens. Derived values include reflected and
  delivered power, return loss and mismatch loss. Values which cannot be known
  without a vector sensor are explicitly identified instead of being guessed.
- Opening the instrument now enables ordinary PWR/SWR polling when required. If
  the radio is already transmitting or tuning, the setting change is deferred
  until RF has stopped so CAT is never reconfigured on air.
- TUNE, front-panel PTT and PTT from another CAT client are now recognised from
  the measured RF power as well as from Decodium's own TX state.
- Fixed the macOS presentation: the unwanted native white title strip and the
  empty outer area have been removed. The complete faceplate is the window.
- The instrument can now be moved to any monitor on macOS, Windows and Linux.
  Its position and size are retained.
- Added proportional resizing from **450×210** to **1800×840**, always preserving
  the 15:7 aspect ratio. The complete faceplate, scales, text and controls grow
  together instead of stretching or leaving unused borders.

### QMX power and SWR telemetry

- Added a guarded telemetry fallback for QRP Labs QMX radios. Hamlib 4.7 knows
  the QMX backend but does not advertise its power and SWR level capabilities;
  Decodium can now read the documented `PC;` and `SW;` replies when the native
  Hamlib meters are unavailable.
- Raw queries remain on the existing Hamlib worker and the already-open serial
  connection. No second process or competing serial-port owner is created.
- Replies are strictly validated and converted to Decodium's milliwatt and SWR
  units. Repeated malformed replies disable only the fallback channel, avoiding
  an endless stream of failing CAT commands.
- Other radios and Hamlib backends continue to use the standard level API and
  are not affected by this model-specific fallback.

### Windows audio stability

- The Windows/WASAPI receive endpoint is now kept open during transmission.
  Incoming RX samples are discarded before they can reach level meters,
  buffers, recordings, the waterfall or decoders, then sample flow is restored
  after TX.
- This avoids repeated suspend/resume cycles which can leave some USB Audio
  CODEC devices in `StoppedState`/`IOError` after PTT is released.
- Recoverable Windows input I/O failures now enter the bounded audio-watchdog
  recovery path without repeatedly opening a modal error message.
- Linux and macOS retain their established audio suspend/recovery behaviour;
  the WASAPI-specific path is compiled only on Windows.

### Multi-monitor windows and desktop behaviour

- Setup, Log, Macro, Multi-Answer Mode, Active Stations and DECØMETER are now
  hosted by genuine top-level desktop windows. They can cross monitor boundaries
  on macOS, Windows and Linux instead of being constrained to the main QML
  surface.
- Window position and size are persisted and restored, including after a layout
  reset or application restart.
- Added native system movement with a reliable manual fallback, usable from each
  window's header or faceplate, plus resize handles where appropriate.
- Fixed Setup appearing behind always-on-top pop-outs. Visible pop-outs are
  temporarily hidden while Setup is open and restored without stealing focus
  after Setup closes.
- Removed duplicate reduced Log and Macro hosts while preserving the complete
  existing interfaces in their new desktop windows.

### CAT and shutdown reliability

- Added **Shared CAT** under Settings → CAT. Decodium can expose the radio state
  over Hamlib's rigctld-compatible TCP protocol while retaining ownership of the
  physical serial port.
- Read access is always available. Frequency/mode/split control and PTT have
  separate permissions, with PTT kept disabled unless control has explicitly
  been granted.
- Shared CAT listens on `127.0.0.1`; the default port is 4533 to avoid conflict
  with the usual external rigctld port 4532. Reads use Decodium's cached state
  and add no serial polling load.
- Hardened transceiver shutdown with self-invalidating Qt pointers and ordered
  worker/thread disposal, preventing callbacks or virtual calls through an
  object already deleted during application exit or rapid CAT teardown.
- Corrected the CAT settings grid so **Poll Interval (s)** and its spin box no
  longer collide with the first row when CAT4OM-specific controls change
  visibility.

### Localisation and project tooling

- Completed all fifteen interface catalogues for the DECØMETER, large ADIF
  import and CI-V address guidance, with no unfinished translations.
- Added the shared-CAT protocol notes and field-tested compatibility tools.
- Added the `decodium-agent` GitHub App manifest, receiver/analysis service,
  deployment example and the repository's Claude Code workflow integration.

### Validation

- Added focused regression coverage for the Windows RX sample gate and QMX
  telemetry reply parsing, including malformed and overflowing replies.
- Release builds remain platform-specific: Windows x64 installer, macOS Apple
  Silicon and Intel packages, and Linux x86_64 and aarch64 AppImages.

---

## Italiano

### Strumento RF DECØMETER

- Aggiunto il **DECØMETER**, accessibile dal menu principale, con scale
  concentriche per potenza diretta, potenza riflessa, ROS e ALC.
- Aggiunta una dinamica realistica dello strumento: attacco immediato, rilascio
  esponenziale, picco trattenuto per tre secondi, media mobile e PEP.
- Aggiunte portate automatiche e manuali da 5 W, 50 W, 500 W e 5 kW, con
  transizioni morbide e scale rietichettate correttamente.
- Aggiunte le schermate potenza, adattamento e pilotaggio. Le grandezze derivate
  comprendono potenza riflessa ed erogata, return loss e mismatch loss. I valori
  non determinabili senza un sensore vettoriale vengono dichiarati tali e non
  stimati arbitrariamente.
- Aprendo lo strumento viene ora abilitato, quando necessario, il normale
  polling PWR/SWR. Se la radio sta già trasmettendo o è in TUNE, la modifica
  viene rimandata alla fine dell'emissione, senza riconfigurare il CAT in aria.
- TUNE, PTT dal pannello della radio e PTT da un altro client CAT vengono ora
  riconosciuti anche dalla potenza RF misurata, oltre che dallo stato TX interno
  di Decodium.
- Corretta la presentazione su macOS: eliminati la fascia bianca nativa con il
  titolo e lo spazio esterno vuoto. Il frontalino completo coincide con la
  finestra.
- Lo strumento può essere spostato su qualsiasi monitor in macOS, Windows e
  Linux; posizione e dimensioni vengono memorizzate.
- Aggiunto il ridimensionamento proporzionale da **450×210** a **1800×840**, con
  rapporto 15:7 sempre preservato. Frontalino, scale, testi e controlli si
  ingrandiscono insieme senza deformazioni o bordi inutilizzati.

### Telemetria di potenza e ROS per QMX

- Aggiunto un fallback controllato per le radio QRP Labs QMX. Hamlib 4.7 conosce
  il backend QMX ma non dichiara le capacità dei misuratori di potenza e ROS;
  Decodium può ora leggere le risposte documentate `PC;` e `SW;` quando i meter
  Hamlib standard non sono disponibili.
- Le interrogazioni raw restano nel worker Hamlib esistente e usano la porta
  seriale già aperta. Non viene creato un secondo processo né un concorrente
  sulla porta CAT.
- Le risposte vengono validate rigorosamente e convertite nelle unità interne
  di Decodium. Dopo errori ripetuti viene disabilitato soltanto il canale di
  fallback interessato, evitando una sequenza infinita di comandi falliti.
- Tutte le altre radio e i backend Hamlib continuano a usare l'API standard e
  non vengono modificati da questo percorso specifico per QMX.

### Stabilità audio su Windows

- Durante la trasmissione l'ingresso Windows/WASAPI resta ora aperto. I campioni
  RX vengono scartati prima di raggiungere meter, buffer, registrazioni,
  waterfall o decoder e il flusso viene ripristinato al termine del TX.
- Si evitano così i ripetuti cicli suspend/resume che possono lasciare alcune
  periferiche USB Audio CODEC in `StoppedState`/`IOError` dopo il rilascio del
  PTT.
- Gli errori I/O recuperabili dell'ingresso Windows entrano ora nel percorso
  controllato dell'audio watchdog senza mostrare continuamente finestre modali.
- Linux e macOS mantengono il precedente comportamento audio; il nuovo percorso
  WASAPI viene compilato esclusivamente su Windows.

### Finestre multi-monitor e comportamento desktop

- Setup, Log, Macro, Multi-Answer Mode, Active Stations e DECØMETER sono ora
  ospitati da vere finestre desktop di primo livello. Possono attraversare i
  confini fra monitor in macOS, Windows e Linux e non sono più vincolati alla
  superficie QML principale.
- Posizione e dimensioni vengono salvate e ripristinate, anche dopo il reset del
  layout o il riavvio dell'applicazione.
- Aggiunto lo spostamento nativo di sistema con fallback manuale affidabile,
  utilizzabile dall'intestazione o dal frontalino, e maniglie di ridimensionamento
  dove previste.
- Corretto Setup che appariva dietro alle finestre pop-out sempre in primo piano.
  I pop-out visibili vengono nascosti temporaneamente e ripristinati alla
  chiusura di Setup senza sottrarre il focus.
- Eliminati gli host ridotti duplicati di Log e Macro, mantenendo le interfacce
  complete nelle nuove finestre desktop.

### CAT e affidabilità in chiusura

- Aggiunta la **CAT condivisa** in Impostazioni → CAT. Decodium può esporre lo
  stato della radio via TCP con protocollo compatibile rigctld, conservando il
  possesso della porta seriale fisica.
- La lettura è sempre disponibile. Controllo di frequenza/modo/split e PTT hanno
  autorizzazioni separate; il PTT resta disabilitato finché il controllo non è
  stato concesso esplicitamente.
- La CAT condivisa ascolta su `127.0.0.1`; la porta predefinita è 4533 per non
  entrare in conflitto con la consueta porta 4532 di un rigctld esterno. Le
  letture usano lo stato in cache e non aggiungono traffico di polling seriale.
- Rafforzata la chiusura del ricetrasmettitore con puntatori Qt auto-invalidanti
  e distruzione ordinata di worker e thread, impedendo callback o chiamate
  virtuali su oggetti già eliminati durante l'uscita o una rapida disconnessione
  CAT.
- Corretta la griglia delle impostazioni CAT: **Intervallo di polling (s)** e il
  relativo selettore non si sovrappongono più alla prima riga quando cambiano i
  controlli specifici CAT4OM.

### Localizzazione e strumenti di progetto

- Completati tutti e quindici i cataloghi dell'interfaccia per DECØMETER, import
  ADIF di grandi dimensioni e guida all'indirizzo CI-V, senza traduzioni
  incomplete.
- Aggiunte le note di protocollo e gli strumenti di compatibilità verificati sul
  campo per la CAT condivisa.
- Aggiunti il manifesto della GitHub App `decodium-agent`, il servizio di
  ricezione/analisi, un esempio di distribuzione e l'integrazione del workflow
  Claude Code nel repository.

### Verifica

- Aggiunti test di regressione mirati per il gate dei campioni RX su Windows e
  per il parsing della telemetria QMX, comprese risposte malformate e overflow.
- Restano disponibili pacchetti specifici per piattaforma: installer Windows
  x64, pacchetti macOS Apple Silicon e Intel, AppImage Linux x86_64 e aarch64.

---

## Localisation (iu8lmc build)

The pop-out windows introduced in this release — active stations, multi-answer,
settings and the DECØMETER — carried translatable titles that had not yet been
translated. They are now available in all fifteen interface languages, reusing
the wording already established for the RF meter. All catalogues report zero
unfinished messages.

## Localizzazione (build iu8lmc)

Le finestre staccabili introdotte in questa versione — stazioni attive,
multi-risposta, impostazioni e DECØMETER — avevano titoli traducibili ma non
ancora tradotti. Ora sono disponibili in tutte e quindici le lingue, riusando
le rese gia' stabilite per il misuratore RF. Tutti i cataloghi riportano zero
messaggi non finiti.
