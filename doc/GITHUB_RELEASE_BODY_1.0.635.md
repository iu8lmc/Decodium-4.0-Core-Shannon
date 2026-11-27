# Decodium 4 v1.0.635

## English (UK)

Cumulative changes since v1.0.632, including Martino's v1.0.633 and v1.0.634 updates and the following local corrections.

### AutoCQ: personalised calls and listening pauses

- Burst counting now recognises the scheduled AutoCQ operation rather than requiring its text to start with CQ or QRZ. Personalised TX6 calls such as `TEST VY2XT FN86` therefore participate in the configured burst/listening cadence.
- Normal QSO replies, active-partner exchanges and empty messages remain excluded. Counting stays in the transmission-completion path; operator-aborted calls are not counted by a subsequent completion notification.
- The correction covers both modern and legacy backends. It concerns “CQs per burst” and “Listening cycles between bursts”; it does not redesign the separate maximum-call/pause controls.

### EasyLog and UDP ADIF band names

- A shared normaliser now handles outgoing ADIF in both backends, including primary, secondary and tertiary logging destinations, WSJT-X LoggedADIF messages and direct ADIF datagrams.
- BAND and BAND_RX values use lower case (`20m`, `70cm`). Field lengths and formatting are preserved, including typed fields; reports such as `-10` and `-12`, callsigns and other values are not changed.
- Normalisation no longer depends on the obsolete hidden EasyLogLowercaseBand setting. Existing local logbook records are not rewritten.
- The binary QSOLogged message carries frequency rather than a textual band. A logger deriving its own band label from that message may still apply its own capitalisation; actual EasyLog import requires confirmation.

### Dashboard SuperFox control

- In FT8, clicking SuperFox enables the option and selects Hound if neither Hound nor Fox is selected. An existing Fox selection is retained; Fox transmission is never enabled implicitly.
- The indicator turns green when the mode is active. Clicking again disables SuperFox without changing the Hound/Fox selection. Settings are saved for restart.
- Changes are blocked during TX or Tune; the button does not silently switch the operating mode to FT8.

### Resizable dashboard panels

- The lower divider remembers its chosen height instead of returning to 160 pixels after restart. Its minimum area height is reduced from 100 to 40 pixels outside FT2-Link.
- Dragging uses stable coordinates to avoid movement feedback. The upper divider's stored height is no longer overwritten by ordinary automatic geometry changes; a collapsed height is also retained.

### Included from v1.0.633

- Automatic discovery and launch of a configured FT2 Log Bridge, duplicate-process protection and UDP configuration warnings. Settings → Reporting provides automatic-start, location, search and launch controls.
- Windows gains the receive-only `decodium-rx.exe` terminal FT8/FT4/FT2 receiver and a Start-menu shortcut. It supports interactive setup, ALL.TXT output, WAV input, slot recording and filtering, without CAT/PTT transmission. Linux AppImages do not yet include this companion executable.

### Included from v1.0.634

- Full Spectrum and Signal RX follow the latest row reliably, both docked and detached. Manual scrolling suspends following until the operator returns to the bottom.
- Optional post-QSO station telemetry is deferred to a suitable TX slot, waits for ongoing transmission and expires if it cannot start within four periods. This timing change is not a claim of fully validated FT2 telemetry support. Telemetry remains optional and disabled by default, not a required part of an FT2 QSO.
- Additional `[LOGSTALL]` diagnostics record interface pauses during the four seconds after logging; this is diagnostic instrumentation, not a confirmed cure for every pause.

### Validation and downloads

Local macOS builds and targeted AutoCQ classification/ADIF tests passed during development, including receipt of normalised ADIF on three local UDP ports. These tests do not replace Windows UI, real-radio or EasyLog end-to-end verification. Source ZIP/tar.gz archives accompany the tag. GitHub workflows publish the Windows x64 installer, Linux x86_64/aarch64 AppImages and macOS Apple Silicon/Intel DMGs as each build finishes; platform checksum files accompany the AppImages and DMGs.

---

## Italiano

Modifiche cumulative dalla v1.0.632, comprendenti gli aggiornamenti v1.0.633 e v1.0.634 di Martino e le seguenti correzioni locali.

### AutoCQ: chiamate personalizzate e pause di ascolto

- Il conteggio delle raffiche riconosce ora l'operazione AutoCQ programmata, senza richiedere che il testo inizi con CQ o QRZ. Le chiamate TX6 personalizzate, come `TEST VY2XT FN86`, partecipano quindi alla cadenza chiamate/ascolto configurata.
- Restano escluse le risposte durante un QSO, gli scambi con un interlocutore attivo e i messaggi vuoti. Il conteggio resta nel percorso di completamento della trasmissione; una chiamata interrotta dall'operatore non viene conteggiata dal successivo evento di fine TX.
- La correzione copre backend moderno e legacy. Riguarda “CQs per burst” e “Listening cycles between bursts”; non ridisegna i controlli separati di massimo chiamate/pausa.

### EasyLog e banda nell'ADIF via UDP

- Una normalizzazione comune gestisce l'ADIF in uscita da entrambi i backend: destinazioni di logging primaria, secondaria e terziaria, messaggi WSJT-X LoggedADIF e datagrammi ADIF diretti.
- BAND e BAND_RX vengono inviati in minuscolo (`20m`, `70cm`). Lunghezze e formattazione dei campi sono conservate, compresi i campi tipizzati; rapporti come `-10` e `-12`, nominativi e altri valori non vengono modificati.
- La normalizzazione non dipende più dalla vecchia opzione nascosta EasyLogLowercaseBand. I QSO già presenti nel log locale non vengono riscritti.
- Il messaggio binario QSOLogged contiene la frequenza, non la banda testuale: un logger che ricava autonomamente la banda può ancora applicare la propria capitalizzazione. L'importazione reale in EasyLog richiede conferma.

### Pulsante SuperFox nella dashboard

- In FT8, un clic abilita SuperFox e seleziona Hound se non è già selezionato Hound o Fox. Un'eventuale selezione Fox viene conservata; la trasmissione Fox non viene mai attivata implicitamente.
- Il pulsante diventa verde quando la modalità è attiva. Un secondo clic disabilita SuperFox senza cambiare la selezione Hound/Fox. Le impostazioni vengono salvate per il riavvio.
- Modifiche bloccate durante TX o Tune; il pulsante non cambia automaticamente il modo operativo in FT8.

### Pannelli ridimensionabili della dashboard

- Il divisore inferiore ricorda l'altezza scelta invece di ripartire da 160 pixel al riavvio. Fuori da FT2-Link, l'altezza minima dell'area scende da 100 a 40 pixel.
- Il trascinamento usa coordinate stabili per evitare scatti. L'altezza salvata del divisore superiore non viene più sovrascritta dai normali cambiamenti automatici della geometria; viene conservata anche l'altezza collassata.

### Novità incluse dalla v1.0.633

- Ricerca e avvio automatici di FT2 Log Bridge già configurato, protezione dai processi duplicati e avvisi sulla configurazione UDP. Impostazioni → Reporting offre controlli per avvio automatico, percorso, ricerca e avvio immediato.
- Windows include il ricevitore da terminale `decodium-rx.exe`, solo RX FT8/FT4/FT2, e il collegamento nel menu Start. Supporta configurazione interattiva, ALL.TXT, ingresso WAV, salvataggio slot e filtri, senza trasmissione CAT/PTT. Le AppImage Linux non includono ancora questo eseguibile aggiuntivo.

### Novità incluse dalla v1.0.634

- Full Spectrum e Signal RX seguono correttamente l'ultima riga, sia agganciati sia staccati. Lo scorrimento manuale sospende il seguito automatico fino al ritorno in fondo.
- La telemetria opzionale dopo il QSO viene rinviata a uno slot TX adatto, attende la fine di una trasmissione in corso e scade se non riesce a partire entro quattro periodi. Questa correzione temporale non costituisce conferma del supporto completo e verificato della telemetria FT2. La telemetria resta opzionale, disattivata di default e non necessaria per un QSO FT2.
- Nuova diagnostica `[LOGSTALL]` delle pause dell'interfaccia nei quattro secondi successivi alla messa a log: è strumentazione diagnostica, non una soluzione confermata per tutte le pause.

### Verifiche e download

Durante lo sviluppo sono riuscite le build locali macOS e le prove mirate di classificazione AutoCQ/ADIF, inclusa la ricezione dell'ADIF normalizzato su tre porte UDP locali. Non sostituiscono le verifiche dell'interfaccia Windows, sulla radio e nell'importazione reale EasyLog. Il tag comprende gli archivi sorgente ZIP/tar.gz. I workflow GitHub pubblicano installer Windows x64, AppImage Linux x86_64/aarch64 e DMG macOS Apple Silicon/Intel al completamento dei rispettivi build, con file di controllo per AppImage e DMG.
