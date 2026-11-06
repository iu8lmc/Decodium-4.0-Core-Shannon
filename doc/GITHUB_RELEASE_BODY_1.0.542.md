# Decodium 4.0 v1.0.542

Version 1.0.542 repairs two gaps in the TCI transmit-audio path, reported by an
operator whose transmissions over TCI did not work with AetherSDR.

## English (British)

### The negotiated sample rate was ignored

- `Cmd_AudioSR` was present in the command table but had no case in the
  dispatch switch, so the server's answer to `audio_samplerate` fell through to
  the default and was discarded.
- Decodium always generates transmit audio at 48000 samples per second — that
  rate is wired into the modulator — yet the reply to a TxChrono frame stamped
  the header with whatever rate the server had declared. When the two differed,
  the server replayed 48000 samples believing them to be at another rate: wrong
  tone frequency and symbol timing outside the slot.
- The server's answer is now recorded, a mismatch raises a warning, and the
  frame declares the rate at which the samples were actually produced.

### Transmitting to servers that do not send TxChrono

- Transmit audio was only ever produced in reply to the server's TxChrono
  frames, which is how ExpertSDR drives the exchange. `txAudioData()`, the
  function meant to push audio on our own initiative, was written but never
  called from anywhere. Against a server that does not emit TxChrono, Decodium
  transmitted nothing at all.
- The body of the TxChrono branch is now `send_tx_audio_frame()`, shared with a
  push fallback: a 40 ms timer that engages only while transmitting and only if
  no TxChrono has arrived for half a second. If none has arrived in the whole
  session, pushing starts on the first tick, so the beginning of the
  transmission is not lost. The first TxChrono disables the fallback again, so
  with ExpertSDR it never engages.


### Romanian, the fifteenth language

- Decodium now speaks Romanian. The interface is fully translated: the 26
  operational panels, the main window, FT2-Link, the Live Map, every settings
  page, the service and error messages, and the remaining dialogues.
- 4035 of 4038 visible messages. The three left out carry an empty source
  string and contain no text, so the practical coverage is complete.
- Romanian diacritics are written with the comma below, `ș` and `ț`, not the
  cedilla. International amateur-radio abbreviations are left unchanged, as in
  the other fourteen languages.

### Diagnostics

- The transmit log line now reads `TCI TX audio frame: … push= … serverRate= …`,
  which distinguishes a server-paced exchange, a client-paced one and a rate
  mismatch without guesswork.

### Validation

- Local `decodium_qml` build completed successfully.
- Runtime check of the built application: no QML warnings, decoding and
  waterfall normal over a sustained run.
- The behaviour against a server that omits TxChrono is verified by
  construction, not on the air: no such server was available here. The fallback
  is inert unless TCI audio is enabled and a transmission is in progress.

## Italiano

La versione 1.0.542 ripara due lacune nel percorso dell'audio di trasmissione
via TCI, emerse dalla segnalazione di un operatore le cui trasmissioni via TCI
non funzionavano con AetherSDR.

### Il rate negoziato veniva ignorato

- `Cmd_AudioSR` era presente nella tabella dei comandi ma non aveva un caso
  nello switch di smistamento, quindi la risposta del server ad
  `audio_samplerate` cadeva nel ramo predefinito e veniva scartata.
- Decodium genera sempre l'audio di trasmissione a 48000 campioni al secondo —
  quel valore è cablato nel modulatore — eppure la risposta a un frame TxChrono
  timbrava nell'intestazione il rate dichiarato dal server. Quando i due
  differivano, il server riproduceva campioni a 48000 credendoli di un'altra
  frequenza: tono sbagliato e simboli fuori dallo slot.
- La risposta del server viene ora memorizzata, uno scostamento genera un
  avviso, e il frame dichiara il rate a cui i campioni sono stati davvero
  prodotti.

### Trasmettere verso server che non inviano TxChrono

- L'audio di trasmissione nasceva soltanto in risposta ai frame TxChrono del
  server, il modo in cui ExpertSDR scandisce lo scambio. `txAudioData()`, la
  funzione destinata a spingere l'audio di nostra iniziativa, era scritta e non
  veniva chiamata da nessuna parte. Con un server che non emette TxChrono,
  Decodium non trasmetteva assolutamente nulla.
- Il corpo del ramo TxChrono è ora `send_tx_audio_frame()`, condiviso con un
  ripiego push: un timer da 40 ms che entra in funzione solo mentre si
  trasmette e solo se non arriva un TxChrono da mezzo secondo. Se in tutta la
  sessione non ne è mai arrivato uno, la spinta parte al primo tick, così non si
  perde l'inizio della trasmissione. Il primo TxChrono disattiva di nuovo il
  ripiego: con ExpertSDR non entra mai in funzione.


### Il rumeno, quindicesima lingua

- Decodium parla ora anche rumeno. L'interfaccia e' tradotta per intero: i 26
  pannelli operativi, la finestra principale, FT2-Link, la Live Map, tutte le
  pagine delle impostazioni, i messaggi di servizio e di errore e i dialoghi
  rimanenti.
- 4035 messaggi visibili su 4038. I tre esclusi hanno sorgente vuota e non
  contengono testo, quindi la copertura reale e' completa.
- I diacritici rumeni sono scritti con la virgola sotto, `ș` e `ț`, non con la
  cediglia. Le sigle internazionali del radiantismo restano invariate, come
  nelle altre quattordici lingue.

### Diagnostica

- La riga di registro della trasmissione riporta ora
  `TCI TX audio frame: … push= … serverRate= …`, che distingue senza indovinare
  uno scambio scandito dal server, uno scandito dal client e uno scostamento di
  frequenza di campionamento.

### Verifica

- Build locale di `decodium_qml` completata correttamente.
- Prova a runtime dell'applicazione compilata: nessun avviso QML, decodifica e
  cascata regolari per l'intera esecuzione.
- Il comportamento verso un server che omette TxChrono è verificato per
  costruzione e non in aria: qui non era disponibile un server simile. Il
  ripiego resta inerte se l'audio TCI non è attivo e non c'è una trasmissione in
  corso.

## Release assets

The release workflows publish the Windows x64 executable, macOS Intel and
Apple Silicon DMG packages, and Linux x86_64 and aarch64 AppImages together
with their checksums where provided by the workflow.
