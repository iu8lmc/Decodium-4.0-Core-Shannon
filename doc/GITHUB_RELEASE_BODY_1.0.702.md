# Decodium 4 v1.0.702

## English (British)

This release contains the CW transmission and CAT-bridge fixes developed after
v1.0.701. The focus is reliable native CW keying when Decodium is connected
directly to a radio or through the shared CAT path used by DecoDXLog and
`rigctld`.

### CW mode and CAT dispatch

- Decodium now verifies the radio's physical mode immediately before calling
  Hamlib's native Morse keyer. If a CAT bridge still reports DATA, USB, SSB or
  another non-CW mode after a preceding frequency or mode change, the worker
  reasserts CW before sending the Morse text.
- The serialised native-keyer operation now keeps the manager's desired state
  in CW and RX while the message is dispatched. A stale DATA/SSB desired state
  can no longer overwrite the CW correction before the text reaches the radio.
- The same path works through a NET rigctl bridge, including the
  Decodium-to-DecoDXLog-to-rigctld chain. The speed-control query remains
  optional: radios or bridges that do not expose CW speed control can still
  accept and transmit the Morse text.
- The native CAT dispatch watchdog is now bounded at 30 seconds. This gives a
  double CAT bridge enough time to drain its worker queues without leaving an
  unbounded request running.

### Emergency Stop and PTT release

- The CW `Stop` control is now an unconditional emergency release. It sends
  both the native `stop_morse` operation, when the backend provides it, and a
  physical `PTT OFF` request.
- Stop also sends the release when Decodium's cached state already says that
  the controller is idle. This covers radios whose native keyer is transmitting
  while the application cache remains `ptt=false`.
- The desired PTT state is cleared before the emergency operation is queued, so
  a late CAT acknowledgement or a stale queued transaction cannot reassert TX
  after Stop.
- For Yaesu CAT keyers using an external RTS, DTR or other non-CAT PTT route,
  Stop temporarily routes the CAT release through the radio backend as well,
  then restores the configured PTT route. This covers radios where the native
  Morse command can start a CAT keyer memory but the Hamlib backend does not
  expose `stop_morse`.
- The CW window exposes a stable `cwStopButton` object name for deterministic
  QML regression coverage.

### Macros and regression coverage

- Added coverage for the lowercase `{call}` placeholder. It is expanded to the
  uppercase callsign before the CAT transmission is sent, matching the other
  CW macro fields.
- Added controller coverage proving that an idle Stop still requests both
  native Morse stop and `PTT OFF`.
- Added QML coverage proving that the visible CW Stop button invokes the same
  emergency-release path as a real click.
- Added CAT source-order and dispatch checks for CW reassertion, the 30-second
  watchdog, PTT release ordering, the Yaesu external-PTT fallback and desired
  PTT clearing.

### Live verification and release contents

- The corrected shared-CAT path was exercised locally with DecoDXLog owning the
  IC-7300 serial port, `rigctld` serving the radio, and Decodium connected as a
  NET rigctl client on `127.0.0.1:4533`.
- A real CW macro was accepted by the bridge with `send result=0`; pressing
  `Stop` produced `ptt_off_rc=0` and `stop_morse_rc=0`, and the radio returned
  to receive.
- The repository version is aligned to `1.0.702`, the source is tagged
  `v1.0.702`, and the GitHub release contains the source-code archive.
- GitHub Actions builds and publishes the Windows x64 installer executable.
- GitHub Actions builds and publishes Apple Silicon macOS DMGs for the
  supported Tahoe and Sequoia runners.
- GitHub Actions builds and publishes Intel macOS DMGs for the supported
  Ventura, Sonoma and Sequoia runners.
- GitHub Actions builds and publishes Linux x86_64 and Linux aarch64 AppImages
  with their SHA-256 checksum files.

## Italiano

Questa release contiene le correzioni sviluppate dopo la v1.0.701 per la
trasmissione CW e i collegamenti CAT. L'obiettivo è rendere affidabile il
keyer CW nativo sia con la radio collegata direttamente a Decodium sia tramite
il percorso CAT condiviso usato da DecoDXLog e `rigctld`.

### Modo CW e invio CAT

- Decodium verifica ora il modo fisico della radio immediatamente prima di
  chiamare il keyer Morse nativo di Hamlib. Se un bridge CAT riferisce ancora
  DATA, USB, SSB o un altro modo non CW dopo un precedente cambio di frequenza
  o modo, il worker reimposta CW prima di inviare il testo Morse.
- L'operazione del keyer nativo mantiene lo stato desiderato del manager in CW
  e RX durante l'invio. Uno stato desiderato vecchio DATA/SSB non può più
  sovrascrivere la correzione CW prima che il testo raggiunga la radio.
- Lo stesso percorso funziona attraverso un bridge NET rigctl, compresa la
  catena Decodium → DecoDXLog → rigctld. La lettura della velocità CW resta
  opzionale: radio o bridge che non espongono il controllo della velocità
  possono comunque accettare e trasmettere il testo Morse.
- Il watchdog dell'invio CAT nativo è ora limitato a 30 secondi. Il doppio
  bridge CAT ha così il tempo di svuotare le code dei worker senza lasciare una
  richiesta senza limite.

### Stop di emergenza e rilascio PTT

- Il comando `Stop` della finestra CW è ora un rilascio di emergenza
  incondizionato. Invia sia `stop_morse`, quando il backend lo supporta, sia
  una richiesta fisica `PTT OFF`.
- `Stop` invia il rilascio anche quando lo stato cache di Decodium indica che
  il controller è già idle. Questo copre le radio il cui keyer nativo sta
  ancora trasmettendo mentre la cache applicativa è rimasta con `ptt=false`.
- Lo stato PTT desiderato viene azzerato prima di accodare l'emergenza, così
  una risposta CAT tardiva o una transazione vecchia non può riattivare la TX
  dopo `Stop`.
- Per i keyer CAT Yaesu che usano PTT esterno su RTS, DTR o altra linea non CAT,
  `Stop` esegue temporaneamente anche il rilascio attraverso il backend CAT e
  poi ripristina il percorso PTT configurato. Questo copre le radio in cui il
  comando Morse nativo può avviare una memoria del keyer CAT ma il backend
  Hamlib non espone `stop_morse`.
- La finestra CW espone ora il nome stabile `cwStopButton` per i test di
  regressione QML deterministici.

### Macro e copertura di regressione

- Aggiunta la copertura per il segnaposto minuscolo `{call}`. Prima dell'invio
  CAT viene espanso nel nominativo maiuscolo, come gli altri campi delle macro
  CW.
- Aggiunto un test del controller che dimostra che `Stop`, anche in idle,
  richiede sia lo stop Morse nativo sia `PTT OFF`.
- Aggiunto un test QML che dimostra che il pulsante Stop visibile della finestra
  CW richiama lo stesso percorso di rilascio di emergenza di un clic reale.
- Aggiunti controlli CAT sull'ordine delle operazioni per il ripristino CW, il
  watchdog di 30 secondi, l'ordine del rilascio PTT, il fallback Yaesu con PTT
  esterno e l'azzeramento del PTT desiderato.

### Verifica reale e contenuto della release

- Il percorso CAT condiviso corretto è stato provato in locale con DecoDXLog
  proprietario della seriale dell'IC-7300, `rigctld` che serve la radio e
  Decodium collegato come client NET rigctl su `127.0.0.1:4533`.
- Una macro CW reale è stata accettata dal bridge con `send result=0`; la
  pressione di `Stop` ha prodotto `ptt_off_rc=0` e `stop_morse_rc=0`, riportando
  la radio in ricezione.
- La versione del repository è allineata a `1.0.702`, il codice è taggato
  `v1.0.702` e la release GitHub contiene l'archivio del codice sorgente.
- I GitHub Actions generano e pubblicano l'eseguibile installer Windows x64.
- I GitHub Actions generano e pubblicano i DMG macOS Apple Silicon per i
  runner Tahoe e Sequoia supportati.
- I GitHub Actions generano e pubblicano i DMG macOS Intel per i runner
  Ventura, Sonoma e Sequoia supportati.
- I GitHub Actions generano e pubblicano le AppImage Linux x86_64 e Linux
  aarch64 con i relativi file checksum SHA-256.
