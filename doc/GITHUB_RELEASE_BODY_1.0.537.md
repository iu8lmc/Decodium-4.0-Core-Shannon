# Decodium 4.0 v1.0.537

Version 1.0.537 repairs the TCI receive-audio path. A station running an SDR
through a TCI server could connect the CAT link, see the frequency track
correctly, and still receive no audio at all. Where audio did arrive it came in
at full scale, which flattened the waterfall and left only the strongest
signals decodable.

## English (British)

### Two gates silently blocked the TCI audio stream

- `startAudioCapture` evaluated the RTL-SDR branch before the TCI branch and
  returned from it unconditionally, even when the device could not be opened.
  RTL-SDR mode drives RTL2832U dongles; an SDR hosted by external software is
  not one of them, so the receiver was left with no audio source while the log
  repeated `RTL-SDR: opening device 0 failed (-1)` every twelve seconds. When
  the USB scan has completed without finding a receiver and TCI audio is
  configured, capture now continues instead of falling silent.
- `usingTciAudioInput()` required the CAT backend label to read `tci`, while a
  rig such as "TCI Client RX1" connects with `portType=tci` regardless of which
  selector the operator picked. The transceiver manager already used `portType`
  for the same purpose, so the two conditions contradicted each other. The
  audio gate now follows the transport actually in use.

### Receive level

- Added a **TCI RX gain** control to the CAT settings, next to the TCI audio
  option: a slider from -40 to +6 dB, shown in decibels, applied without
  reconnecting and stored with the other CAT settings.
- The default is -20 dB. TCI servers deliver the stream at full scale:
  measurements on the affected station showed a peak of 0.999 and an RMS of
  0.45, about -7 dBFS, while the decoder works well around -27 dBFS. At full
  scale the waterfall loses all contrast and weak signals disappear into the
  saturation.
- The decibel infrastructure already existed in `writeAudioData`; nothing set
  it, because the QML path never wrote a volume into the desired transceiver
  state, leaving unity gain.

### Stream robustness

- `stream_audio()` compared only against the state confirmed by the server, so
  an `audio_start` issued a few milliseconds after an `audio_stop` was
  discarded because the echo had not yet returned. The stream then stayed off.
  The requested state is now taken into account as well.
- The audio watchdog restarted the TCI stream every few seconds, which does not
  recover it but shuts it down. Restarts are now rate-limited when TCI audio is
  the source, without disabling recovery: the restart is skipped only when
  samples arrived recently or a restart already happened recently.
- `rxAtten` was read in `writeAudioData` before any transceiver state had been
  applied. It now carries an explicit initial value.

### Device recognition

- The device name reported by the TCI server is recorded in the diagnostic log
  as `[TCI] device: …`, which identifies ColibriNANO and other externally
  hosted receivers alongside the transceivers already handled.

### Validation

- Local `decodium_qml` build completed successfully.
- `qmllint` reported no errors on the modified QML file.
- Confirmed on a live station: the audio stream now runs without interruption,
  the watchdog no longer intervenes, and the waterfall populates.

## Italiano

La versione 1.0.537 ripara il percorso dell'audio di ricezione via TCI. Una
stazione con un SDR pilotato da un server TCI poteva collegare il CAT, vedere
la frequenza seguire correttamente e non ricevere alcun audio. Dove l'audio
arrivava, entrava a fondo scala: la cascata perdeva ogni contrasto e restavano
decodificabili soltanto i segnali piu' forti.

### Due barriere spegnevano il flusso audio in silenzio

- `startAudioCapture` valutava il ramo RTL-SDR prima di quello TCI e ne usciva
  comunque, anche quando il dispositivo non si apriva. La modalita' RTL-SDR
  pilota le chiavette basate su RTL2832U; un SDR gestito da software esterno
  non e' fra queste, quindi il ricevitore restava senza alcuna sorgente audio
  mentre il registro ripeteva `RTL-SDR: opening device 0 failed (-1)` ogni
  dodici secondi. Se la ricerca USB e' terminata senza trovare nulla e l'audio
  TCI e' configurato, ora la cattura prosegue invece di restare muta.
- `usingTciAudioInput()` pretendeva che l'etichetta del backend CAT fosse
  `tci`, mentre un rig come "TCI Client RX1" si collega con `portType=tci`
  qualunque sia il selettore scelto dall'operatore. Il gestore del transceiver
  usava gia' `portType` per lo stesso scopo: le due condizioni si
  contraddicevano. Ora conta il trasporto realmente in uso.

### Livello di ricezione

- Aggiunto il controllo **TCI RX gain** nelle impostazioni CAT, accanto
  all'opzione dell'audio TCI: cursore da -40 a +6 dB, valore mostrato in
  decibel, applicato senza riconnettere e salvato con le altre impostazioni.
- Il valore predefinito e' -20 dB. I server TCI consegnano il flusso a fondo
  scala: sulla stazione interessata si misuravano picco 0,999 e rms 0,45,
  circa -7 dBFS, mentre il decodificatore lavora bene intorno a -27 dBFS. A
  fondo scala la cascata perde ogni contrasto e i segnali deboli spariscono
  nella saturazione.
- L'infrastruttura in decibel esisteva gia' in `writeAudioData`: mancava chi la
  valorizzasse, perche' il percorso QML non scriveva mai un volume nello stato
  desiderato del transceiver e restava guadagno unitario.

### Robustezza del flusso

- `stream_audio()` confrontava solo con lo stato confermato dal server: un
  `audio_start` inviato pochi millisecondi dopo un `audio_stop` veniva scartato
  perche' l'eco non era ancora tornata, e il flusso restava spento. Ora si
  tiene conto anche dello stato richiesto.
- Il watchdog audio riapriva il flusso TCI ogni pochi secondi, cosa che non lo
  recupera ma lo spegne. I riavvii sono ora limitati in frequenza quando la
  sorgente e' l'audio TCI, senza disabilitare il recupero: si salta solo se i
  campioni sono arrivati da poco o se ci si e' gia' riavviati da poco.
- `rxAtten` veniva letto in `writeAudioData` prima che fosse applicato un
  qualsiasi stato del transceiver. Ora ha un valore iniziale esplicito.

### Riconoscimento del dispositivo

- Il nome riportato dal server TCI finisce nel registro diagnostico come
  `[TCI] device: …` e identifica ColibriNANO e gli altri ricevitori gestiti da
  software esterno accanto ai transceiver gia' trattati.

### Verifica

- Build locale di `decodium_qml` completata correttamente.
- `qmllint` non ha segnalato errori sul file QML modificato.
- Confermato su stazione reale: il flusso audio scorre senza interruzioni, il
  watchdog non interviene piu' e la cascata si popola.

## Release assets

The release workflows publish the Windows x64 executable, macOS Intel and
Apple Silicon DMG packages, and Linux x86_64 and aarch64 AppImages together
with their checksums where provided by the workflow.
