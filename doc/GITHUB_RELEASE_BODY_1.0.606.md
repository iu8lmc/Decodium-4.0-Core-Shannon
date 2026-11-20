# Decodium 4 FT2 v1.0.606

Version 1.0.606 is the cumulative fork release after v1.0.604. It includes
the v1.0.605 speech-to-text removal, the latest upstream fast-LDPC and FT2/FT4
decode improvements, and the native SSTV/RTTY integration work completed in
this fork.

## English (UK)

### Native SSTV workspace isolation

- Opening the native SSTV window is now an exclusive receive-workspace
  transition. The previously selected FT/JT/WSPR dashboard decoder is stopped
  and its pending decode generation is invalidated.
- The panadapter and shared PCM path remain alive when the user starts the SSTV
  monitor, so the spectrum does not disappear or need a manual restart.
- Results already in flight, legacy backend rows and external decode-selection
  events are discarded while SSTV is open. They cannot enter the dashboard,
  history, automatic sequencing, UDP/PSK reporting or QSO actions.
- Closing SSTV restores the monitoring state that was active before entry and
  drops stale results from the suspended interval.

### RTTY identity and mode behaviour

- RTTY station macros now use Decodium's global callsign, station name and QTH
  profile as their single source of truth and refresh immediately when those
  settings change.
- RTTY starts with the application's global language instead of maintaining a
  competing private language setting.
- Opening the RTTY window switches the application to RTTY and suspends the
  previously selected digital-mode receive path.

### Fast-LDPC and FT2/FT4 decoding updates from upstream

- The fast-LDPC candidate counter is 64-bit and the measured candidate budget
  is used instead of an estimate.
- FT2 deep decoding uses a tuned candidate set and pair span, reducing wasted
  CRC work while retaining the measured candidate range.
- The LDPC alpha parameter is tuned for the current decoder path, reducing
  candidate processing cost.
- Narrow decode bands now use the correct noise-floor and signal-peak region.
- CQ a-priori handling is also applied while in a QSO, and the second decode
  span is expanded to 64 where required.
- The candidate-limit correction is scoped to FT2 and FT4, leaving FT8
  behaviour unchanged.

### Earlier cumulative changes retained from v1.0.604 and v1.0.605

The release retains the completed native RTTY integration and radio control,
Baudot/Viterbi improvements, SPE Expert amplifier discovery, QO-100 and
transverter frequency offsets, WSPRnet report delivery, and the frequency
selector work. As in v1.0.605, the experimental SSB speech-to-text subsystem
is removed because on-air testing did not produce reliable results.

### Packaging and verification

The source tag is `v1.0.606`. GitHub Actions builds the Windows x64 installer,
macOS Apple Silicon and Intel DMG variants, and Linux x86_64 and aarch64
AppImages. Each published binary is accompanied by its workflow-generated
SHA-256 file where provided by the packaging workflow.

## Italiano

### Isolamento della finestra SSTV nativa

- L'apertura della finestra SSTV nativa è ora una transizione esclusiva dello
  spazio di ricezione. Il decoder FT/JT/WSPR selezionato nella dashboard viene
  fermato e la generazione di decodifica pendente viene invalidata.
- Il panadapter e il percorso PCM condiviso rimangono attivi quando si avvia
  il monitor SSTV, quindi lo spettro non scompare e non richiede un riavvio
  manuale.
- I risultati già in elaborazione, le righe del backend legacy e gli eventi
  esterni di selezione decode vengono scartati mentre SSTV è aperta. Non possono
  entrare in dashboard, cronologia, sequenza automatica, UDP/PSK Reporter o
  azioni QSO.
- Alla chiusura di SSTV viene ripristinato lo stato di monitoraggio precedente e
  vengono eliminati i risultati obsoleti dell'intervallo sospeso.

### Identità e comportamento del modo RTTY

- Le macro RTTY usano ora il nominativo, il nome della stazione e il QTH del
  profilo globale di Decodium come unica sorgente, aggiornandosi subito quando
  cambiano queste impostazioni.
- RTTY parte con la lingua globale dell'applicazione e non mantiene più una
  seconda impostazione linguistica privata.
- L'apertura della finestra RTTY commuta l'applicazione in RTTY e sospende il
  percorso di ricezione del modo digitale selezionato in precedenza.

### Aggiornamenti fast-LDPC e decodifica FT2/FT4 da upstream

- Il contatore dei candidati fast-LDPC è a 64 bit e viene usato il budget
  misurato dei candidati invece di una stima.
- La decodifica profonda FT2 usa un insieme di candidati e un pair span tarati,
  riducendo il lavoro CRC inutile e mantenendo l'intervallo misurato.
- Il parametro alpha LDPC è stato tarato per il percorso decoder corrente,
  riducendo il costo di elaborazione dei candidati.
- Le bande di decodifica strette usano ora la regione corretta per fondo e picco
  del segnale.
- La gestione a-priori del CQ viene applicata anche durante un QSO e il secondo
  span viene esteso a 64 dove necessario.
- La correzione del limite candidati è applicata a FT2 e FT4, lasciando
  invariato il comportamento FT8.

### Modifiche cumulative mantenute dalla v1.0.604 e v1.0.605

La release mantiene l'integrazione nativa RTTY e il controllo radio completati,
le migliorie Baudot/Viterbi, il rilevamento dell'amplificatore SPE Expert, gli
offset di frequenza QO-100 e transverter, l'invio dei report WSPRnet e il lavoro
sul selettore di frequenza. Come nella v1.0.605, il sottosistema sperimentale di
trascrizione SSB è stato rimosso perché i test in aria non fornivano risultati
affidabili.

### Packaging e verifica

Il tag sorgente è `v1.0.606`. GitHub Actions genera l'installer Windows x64,
le varianti DMG per macOS Apple Silicon e Intel e le AppImage Linux x86_64 e
aarch64. Ogni binario pubblicato è accompagnato dal relativo file SHA-256
generato dal workflow quando previsto dal percorso di packaging.
