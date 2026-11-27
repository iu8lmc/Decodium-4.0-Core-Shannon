# Decodium 4 v1.0.701

## English (British)

This release contains the CW CAT, CW-window, file-dialog and amplifier
compatibility fixes developed after v1.0.700.

### CW function keys and CAT transmission

- F1-F12 are now handled by the CW external window when it is the active
  operational window, so the physical function keys and the on-screen macro
  buttons use the same macro dispatch path.
- Dashboard shortcuts that previously competed for F1, F2, F3, F4 and F11 are
  disabled while the CW window is visible. F1 can no longer toggle monitoring,
  F2 can no longer open the QSY picker, F4 can no longer open manual QSO
  logging, and F11 can no longer switch the main dashboard to fullscreen while
  the operator is pressing a CW macro key.
- Added explicit `Qt.WindowShortcut` scoping to the CW macro shortcuts and
  preserved the existing safety gates for an unavailable transmitter, the CW
  macro editor and the CW setup dialog.
- Fixed literal one-character Morse messages for Yaesu CAT radios. Hamlib's
  Yaesu implementation reserves a one-character `send_morse()` payload for
  memory selection; Decodium now appends a trailing word gap for a literal
  single-character message, including the default F5 question mark and
  user-defined one-character macros.
- Preserved the rigctld fallback introduced for CAT speed control: an
  unavailable or unimplemented `KEYSPD` level does not prevent the actual
  `rig_send_morse()` call, while other speed-setting errors remain reported.
- Improved CAT keyer ownership. Native radio keyers can own break-in/PTT timing
  so inter-character gaps do not become one continuous application-held
  carrier. Explicit PTT remains available for legacy integrations.
- Stop now remains armed for a native CAT keyer after the estimated Morse time,
  calls the native stop command, releases an emergency PTT state when needed,
  and invalidates late asynchronous completions.
- Added `[CW-MACRO]` diagnostics with macro index, label, template length and
  expanded length. Empty expanded macros now produce a visible warning instead
  of silently doing nothing.

### Clickable CW callsigns

- The CW decoder now extracts conservative callsign-shaped tokens from the RX
  transcript and exposes them through a dedicated `decoderCalls` property.
- The CW window displays the recognised stations as clickable chips.
- Clicking a station fills the CW `Call` field and synchronises the application
  DX call, allowing `{CALL}` macros to use it immediately.
- The operation is explicitly receive-side only and does not start TX.
- Portable suffixes such as `/P` and `/MM` are supported, while arbitrary
  partial words remain non-clickable to avoid false callsigns.

### ADIF and file-dialog focus

- Fixed the case where the ADIF import file chooser opened behind Decodium or
  another application and therefore could not be used.
- The bridge now resolves the QML application's active owner window, creates
  the file dialog window handle before showing it, sets the transient parent,
  uses application modality, raises and activates the dialog, and explicitly
  requests native activation before entering `exec()`.
- The same activation helper is shared by the bridge file-dialog path, keeping
  the behaviour consistent for open and save operations.

### SPE Expert / Taurus amplifier support

- Added a manufacturer/protocol selection in the amplifier settings for
  `SPE — Expert / Taurus`.
- Added bridge configuration support for persisting and applying the selected
  amplifier manufacturer.
- Extended the SPE status parser to accept the real Expert 1.5K-FA Taurus
  frame, which contains a leading comma before the `15K` model field and an
  additional comma before `CR LF`.
- Retained support for the documented SPE frame without those framing commas.
- Corrected frame-end detection so checksum bytes, optional Taurus framing and
  `CR LF` are consumed as one complete frame. This prevents the extra comma
  from remaining in the receive buffer and desynchronising every subsequent
  poll.
- Checksum validation remains mandatory, and malformed or incomplete frames
  continue to be rejected safely.
- Documented the Taurus frame variant and added tests for single-frame parsing,
  streaming extraction, checksum handling and mixed standard/Taurus frames.

### Tests and validation

- Added `test_cw_cat_speed_fallback` for optional `KEYSPD` and Yaesu literal
  single-character payload handling.
- Extended `test_cw_controller` for one-character macro delivery, native CAT
  keyer timing and stop behaviour.
- Extended the QML source contract tests for CW function-key precedence.
- Added `test_file_dialog_focus` for owner-window, transient-parent and
  activation requirements.
- Extended amplifier telemetry coverage with the real Taurus 15K frame and
  mixed serial-stream extraction.
- The local release validation includes `git diff --check`, the
  `decodium_qml` build and the focused CW, QML, file-dialog and amplifier test
  targets.

## Italiano

Questa release contiene le correzioni CAT CW, finestra CW, dialoghi file e
compatibilità amplificatori sviluppate dopo la v1.0.700.

### Tasti funzione CW e trasmissione CAT

- F1-F12 vengono ora gestiti dalla finestra CW esterna quando è la finestra
  operativa attiva; i tasti fisici e i pulsanti macro a video usano quindi lo
  stesso percorso di invio.
- Le scorciatoie globali della dashboard che entravano in conflitto con F1,
  F2, F3, F4 e F11 vengono disabilitate mentre la finestra CW è visibile. F1
  non può più attivare/disattivare il monitor, F2 non può più aprire il QSY
  picker, F4 non può più aprire il log manuale QSO e F11 non può più attivare il
  fullscreen della dashboard mentre si preme una macro CW.
- Aggiunto lo scope esplicito `Qt.WindowShortcut` per le macro CW, mantenendo i
  blocchi esistenti quando il trasmettitore non è pronto o quando sono aperti
  l'editor macro o il setup CW.
- Corrette le trasmissioni Morse letterali di un solo carattere sulle radio
  Yaesu via CAT. L'implementazione Yaesu di Hamlib riserva il payload singolo
  di `send_morse()` alla selezione di una memoria; Decodium aggiunge ora una
  pausa finale ai messaggi letterali di un carattere, compreso il punto
  interrogativo predefinito F5 e le macro personalizzate di un carattere.
- Conservato il fallback per la velocità CAT introdotto per rigctld: se il
  livello `KEYSPD` non è disponibile o implementato, il comando
  `rig_send_morse()` viene comunque eseguito; gli altri errori di velocità
  continuano a essere segnalati.
- Migliorata la gestione del keyer CAT. I keyer nativi della radio possono
  controllare break-in e PTT, evitando che le pause tra caratteri diventino una
  portante continua mantenuta dall'applicazione. Il PTT esplicito resta
  disponibile per le integrazioni legacy.
- Il comando Stop resta armato per il keyer CAT nativo anche dopo il tempo Morse
  stimato, invia il comando nativo di stop, rilascia il PTT di emergenza quando
  necessario e invalida le completion asincrone arrivate in ritardo.
- Aggiunta la diagnostica `[CW-MACRO]` con indice, etichetta, lunghezza del
  modello e lunghezza espansa. Le macro vuote dopo l'espansione mostrano ora un
  avviso invece di non fare nulla silenziosamente.

### Callsign CW cliccabili

- Il decoder CW estrae ora dal transcript RX token con forma prudenziale da
  callsign e li espone tramite la proprietà separata `decoderCalls`.
- La finestra CW mostra le stazioni riconosciute come pulsanti cliccabili.
- Cliccando una stazione vengono riempiti il campo `Call` CW e il DX call
  dell'applicazione, così le macro `{CALL}` possono usarlo subito.
- L'operazione è solo lato ricezione e non avvia alcuna trasmissione.
- Sono supportati anche suffissi portatili come `/P` e `/MM`; le parole
  parziali arbitrarie restano non cliccabili per evitare falsi callsign.

### Focus ADIF e dialoghi file

- Corretto il caso in cui il selettore file per l'import ADIF si apriva dietro
  Decodium o dietro un'altra applicazione e risultava inutilizzabile.
- Il bridge identifica ora la finestra proprietaria QML attiva, crea l'handle
  della finestra prima della visualizzazione, imposta il transient parent, usa
  la modalità modale per l'applicazione, porta il dialogo in primo piano,
  attiva la finestra e richiede esplicitamente l'attivazione nativa prima di
  entrare in `exec()`.
- Lo stesso helper di attivazione viene usato dal percorso file del bridge per
  mantenere coerenti apertura e salvataggio.

### Supporto amplificatore SPE Expert / Taurus

- Aggiunta nelle impostazioni dell'amplificatore la selezione produttore/
  protocollo `SPE — Expert / Taurus`.
- Aggiunto il supporto bridge per salvare e applicare il produttore selezionato.
- Esteso il parser dello stato SPE per accettare la trama reale dell'Expert
  1.5K-FA Taurus, che contiene una virgola iniziale prima del modello `15K` e
  una virgola aggiuntiva prima di `CR LF`.
- Mantenuto il supporto alla trama SPE documentata senza le virgole di framing.
- Corretta l'identificazione della fine trama: byte checksum, framing Taurus
  opzionale e `CR LF` vengono consumati come un unico frame completo. La
  virgola aggiuntiva non resta più nel buffer, evitando il disallineamento dei
  polling successivi.
- Il checksum resta obbligatorio e le trame incomplete o corrotte continuano a
  essere rifiutate in sicurezza.
- Documentata la variante Taurus e aggiunti test per parsing singolo,
  estrazione da stream, checksum e flussi misti standard/Taurus.

### Test e validazione

- Aggiunto `test_cw_cat_speed_fallback` per `KEYSPD` opzionale e payload Yaesu
  letterali di un carattere.
- Esteso `test_cw_controller` per invio macro di un carattere, keyer CAT nativo
  e comportamento di Stop.
- Estesi i test di contratto QML per la priorità dei tasti funzione CW.
- Aggiunto `test_file_dialog_focus` per owner window, transient parent e
  attivazione del dialogo.
- Estesa la copertura telemetria amplificatore con la trama Taurus 15K reale e
  l'estrazione da stream seriale misto.
- La validazione locale della release comprende `git diff --check`, la build
  `decodium_qml` e i target mirati CW, QML, dialoghi file e amplificatore.
