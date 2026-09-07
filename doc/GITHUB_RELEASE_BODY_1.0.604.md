# Decodium 4 FT2 v1.0.604

Version 1.0.604 consolidates the work completed after v1.0.601: the RTTY
integration is completed and made portable, SSB speech-to-text is integrated
locally, SPE amplifiers can be discovered automatically, negative transverter
offsets are saved reliably, and native WSPR reports are actually delivered to
WSPRnet.

## English (UK)

### RTTY completion and radio control

- RTTY now has its own remembered rig mode, defaulting to **RTTY-U**, rather
  than inheriting the wide data-mode setting used by FT8 and the other digital
  modes.
- **RTTY-U, RTTY-L, DIGU and DIGL** can be selected from the mode row. The
  tooltips explain the important distinction: the radio's native RTTY modes
  expect FSK keying, while DIGU/DIGL accept transmitted audio.
- The selected RTTY mode is reapplied after a QSY, once the radio has completed
  the band change. It does not override a later manual mode change or continue
  acting after the operator leaves RTTY.
- **REV, AFC, AUTO and CENTRE** controls are restored. Band changes made from
  Decodium or directly on the radio now reset the appropriate RTTY state.
- The RTTY band bar, tuning scale, mark/space reference lines and tuning bridge
  are mounted in the window again. A layout defect that placed mode buttons on
  top of one another has also been corrected.
- The RTTY waterfall now uses a 1024-sample analysis window, with adaptive gain,
  so mark and space remain visibly distinct without clipping strong signals.
- Baudot encoding now uses a portable byte vector, and the Viterbi decoder uses
  a finite unreachable-state sentinel that remains valid under fast-math. This
  removes Clang/libc++, GCC/libstdc++ and MSVC build differences.

### Local SSB speech-to-text

- Decodium can transcribe SSB speech into the decode list using the vendored
  **whisper.cpp v1.9.3** engine. Audio remains on the operator's computer and no
  cloud transcription service is used.
- The speech model is downloaded only when first requested and can be unloaded
  when transcription is disabled, avoiding a permanent installer and memory
  cost for stations that do not use phone.
- English and Italian are supported. A radio-aware recomposer restores common
  amateur-radio wording and phonetic callsigns, while uncertain callsign
  characters remain marked instead of being invented and offered for logging.
- The incomplete upstream vendor import has been repaired by restoring the
  matching ggml BLAS and Metal sources required by the default macOS
  configuration. Third-party compiler warnings are isolated from Decodium's
  warnings-as-errors policy. Speech-enabled builds now configure and link on
  Apple Silicon with CPU, Accelerate/BLAS and Metal backends present; Decodium's
  current recognition path remains local and CPU-selected.

### SPE Expert amplifier discovery

- **Settings → Radio → Amplifier → Search** probes free serial ports using the
  documented SPE protocol and fills in the detected port, model, speed and
  active polling settings.
- CAT ports are explicitly excluded so discovery cannot take the radio away
  from the active CAT controller.
- Detection is based on a valid SPE protocol response rather than a fragile USB
  identity list, covering bridge-chip variations between amplifier revisions.
- `doc/SPE_Amplifier_Setup.pdf` documents the unusual RS-232 pinout, shared-port
  arrangements, settings, readings and common failure cases.

### QO-100 and transverter frequency offsets

- Negative station/transverter offsets are now validated against the signed
  `FrequencyDelta` range. Values such as **-2556 MHz** for a QO-100 13 cm uplink
  are no longer silently rejected as though they were unsigned frequencies.
- The editor accepts decimal commas, explicit `Hz` or `MHz` units and common
  typographic minus characters copied from formatted documents.
- Settings are flushed to disk and read back after add, update or delete. The
  interface reports **saved and verified** or gives an explicit error instead
  of closing the operation with a silent failure.
- Automated tests cover the negative offset conversion and its complete
  `QSettings` persistence round trip.

### WSPRnet report delivery

- Fixed the native WSPR path which queued every decoded report but never armed
  the upload worker. Reports received by Decodium can now leave the queue and
  reach WSPRnet when uploading is enabled.
- Queue accounting now handles reports added while another request is active,
  and completion is reported only after both the queue and outstanding network
  requests are empty.
- Network failures expose the useful error text, WSPR upload state is written to
  the Decodium log, and the report identifies the actual Decodium application
  version rather than the old hard-coded `Decodium/3.0` value.
- A deterministic local HTTP regression test verifies that a decoded WSPR row
  starts the uploader and produces the expected form submission without making
  an external network request.

### Downloads

- **Windows x64:** installer executable.
- **macOS Apple Silicon:** DMGs for Sequoia and Tahoe, each with SHA-256.
- **macOS Intel:** DMGs for Ventura, Sonoma and Sequoia, each with SHA-256.
- **Linux x86_64 and aarch64:** Qt 6.11 AppImages, each with SHA-256.
- GitHub also provides the source code archives generated from tag `v1.0.604`.

---

## Italiano

La versione 1.0.604 consolida il lavoro completato dopo la v1.0.601:
l'integrazione RTTY è stata completata e resa portabile, la trascrizione della
fonia SSB funziona localmente, gli amplificatori SPE possono essere individuati
automaticamente, gli offset negativi dei transverter vengono salvati in modo
affidabile e i rapporti WSPR nativi vengono realmente inviati a WSPRnet.

### Completamento RTTY e controllo della radio

- RTTY dispone ora di un modo radio dedicato e memorizzato, con **RTTY-U** come
  valore iniziale, invece di ereditare il modo dati largo usato da FT8 e dagli
  altri modi digitali.
- **RTTY-U, RTTY-L, DIGU e DIGL** sono selezionabili dalla riga dei modi. I
  suggerimenti spiegano la differenza importante: i modi RTTY nativi della radio
  attendono la manipolazione FSK, mentre DIGU/DIGL accettano l'audio trasmesso.
- Il modo RTTY selezionato viene riapplicato dopo un QSY, quando la radio ha
  terminato il cambio banda. Non annulla una successiva scelta manuale e non
  interviene dopo l'uscita da RTTY.
- Sono stati ripristinati i controlli **REV, AFC, AUTO e CENTRE**. I cambi banda
  eseguiti da Decodium o dalla manopola della radio azzerano correttamente lo
  stato RTTY interessato.
- La barra delle bande, la scala di sintonia, i riferimenti mark/space e il ponte
  di sintonia sono nuovamente presenti. È stato inoltre corretto il difetto di
  layout che sovrapponeva i pulsanti dei modi.
- Il waterfall RTTY usa ora una finestra di analisi da 1024 campioni con guadagno
  adattivo: mark e space restano separati senza saturare i segnali forti.
- La codifica Baudot usa un vettore di byte portabile e il decoder Viterbi usa un
  valore finito per gli stati irraggiungibili, valido anche con fast-math. Sono
  così eliminate le differenze di compilazione fra Clang/libc++, GCC/libstdc++ e
  MSVC.

### Trascrizione locale della fonia SSB

- Decodium può trascrivere la fonia SSB nella lista dei decodificati usando il
  motore **whisper.cpp v1.9.3** incluso nei sorgenti. L'audio resta sul computer
  dell'operatore e non viene usato alcun servizio cloud.
- Il modello viene scaricato soltanto al primo utilizzo e può essere scaricato
  dalla memoria disattivando la funzione, senza appesantire stabilmente
  l'installer e le stazioni che non usano la fonia.
- Sono supportati inglese e italiano. Un ricompositore radiantistico ripristina
  il gergo e i nominativi pronunciati con l'alfabeto fonetico; i caratteri
  incerti restano segnalati invece di essere inventati e proposti per il log.
- È stata corretta l'importazione upstream incompleta reintegrando i sorgenti
  ggml BLAS e Metal della stessa versione, richiesti dalla configurazione macOS
  predefinita. Gli avvisi del codice di terze parti sono isolati dalla regola che
  rende fatali gli avvisi di Decodium. Le build con Speech attivo ora si
  configurano e collegano su Apple Silicon con i backend CPU, Accelerate/BLAS e
  Metal presenti; il percorso di riconoscimento attuale di Decodium resta locale
  e selezionato sulla CPU.

### Ricerca degli amplificatori SPE Expert

- **Impostazioni → Radio → Amplificatore → Cerca** interroga le porte seriali
  libere con il protocollo SPE e compila porta, modello, velocità e polling
  attivo quando trova l'amplificatore.
- Le porte CAT sono escluse esplicitamente, così la ricerca non può sottrarre la
  radio al controllore CAT in uso.
- L'identificazione avviene tramite una risposta SPE valida e non tramite una
  fragile lista di identità USB, coprendo le differenze fra i convertitori usati
  nelle varie revisioni.
- `doc/SPE_Amplifier_Setup.pdf` documenta il particolare cablaggio RS-232, la
  condivisione della porta, le impostazioni, le letture e i problemi comuni.

### Offset di frequenza QO-100 e transverter

- Gli offset negativi di stazione/transverter vengono ora controllati usando
  l'intervallo con segno di `FrequencyDelta`. Un valore come **-2556 MHz** per
  l'uplink QO-100 sui 13 cm non viene più respinto silenziosamente come se fosse
  una frequenza senza segno.
- L'editor accetta la virgola decimale, le unità esplicite `Hz` o `MHz` e i
  comuni segni meno tipografici copiati da documenti formattati.
- Dopo aggiunta, modifica o eliminazione le impostazioni vengono scritte su disco
  e rilette. L'interfaccia conferma **offset salvato e verificato** oppure mostra
  un errore esplicito, eliminando il precedente fallimento silenzioso.
- I test automatici coprono la conversione dell'offset negativo e l'intero
  salvataggio e caricamento tramite `QSettings`.

### Invio dei rapporti a WSPRnet

- È stato corretto il percorso WSPR nativo che accodava ogni rapporto decodificato
  senza mai avviare il worker di caricamento. Quando l'invio è abilitato, i
  rapporti ricevuti da Decodium possono ora uscire dalla coda e raggiungere
  WSPRnet.
- Il conteggio della coda gestisce i rapporti aggiunti mentre una richiesta è in
  corso e segnala il completamento soltanto quando sia la coda sia le richieste
  di rete sono vuote.
- Gli errori di rete mostrano il testo utile, lo stato dell'invio WSPR viene
  scritto nel log di Decodium e il rapporto indica la versione reale
  dell'applicazione invece del vecchio valore fisso `Decodium/3.0`.
- Un test HTTP locale e deterministico verifica che una riga WSPR decodificata
  avvii l'uploader e produca il modulo previsto senza collegarsi a servizi
  esterni.

### Download

- **Windows x64:** installer eseguibile.
- **macOS Apple Silicon:** DMG per Sequoia e Tahoe, ciascuno con SHA-256.
- **macOS Intel:** DMG per Ventura, Sonoma e Sequoia, ciascuno con SHA-256.
- **Linux x86_64 e aarch64:** AppImage Qt 6.11, ciascuna con SHA-256.
- GitHub fornisce inoltre gli archivi del codice sorgente generati dal tag
  `v1.0.604`.
