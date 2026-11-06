# Decodium 4 — 1.0.639

Changes since **v1.0.638** / Modifiche dalla **v1.0.638**.

## English (UK)

### AutoCQ: completed-call limits and listening pauses

- Corrected the Generic AutoCQ controls: the maximum counts completed CQ transmissions rather than entries into the caller-retry recovery path. Custom TX6 calls such as `TEST VY2XT FN86` are included; replies during a QSO are excluded.
- The generic pause starts after CQ playback completes and gates the next CQ, without preventing replies to valid callers. Aborted/error playback is excluded from the app-owned completion accounting.
- Burst/listening cadence and the total CQ budget remain separate. Generic and burst pauses overlap rather than being added together; transmission resumes only in an eligible TX slot after both have expired. A one-second pause may therefore leave the observed slot cadence unchanged.
- Removed the incorrect generic counter/pause handling from caller-retry recovery and the counter reset when processing a queued caller. A new AutoCQ session starts a fresh completed-call budget.
- Connected the generic controls to both the dashboard transmission path and the legacy backend cadence path. Maximum and pause settings are saved and restored.
- Clarified the watchdog labels and help: its period setting counts elapsed periods, including listening time, **not CQ calls**. The safety watchdog remains active and can stop a session before its CQ budget is exhausted. In FT2, ten 3.75-second periods are approximately 37.5 seconds, not ten transmissions.
- Added regression cases for custom calls, excluded QSO replies and failed playback, finite/unlimited budgets and overlapping pauses.

### Logbook: export selected contacts

- Added an independent multi-QSO export selection using the checkboxes beside log rows, with selected-row highlighting. Available in both logbook views, including the detached window.
- **Export ADIF** now offers **Export selected QSOs**, with a selection count, or **Export entire logbook**. The row context menu also offers selection and selected export; export selection can be cleared separately.
- Selected export uses the full stored ADIF field set rather than rebuilding contacts from the visible columns, preserving reports such as `-10` and fields needed by external logbooks. Existing export sanitisation continues to omit invalid unset `MY_IOTA` values.
- Export identities include the logbook path, original record position and a field fingerprint. If a selected record no longer matches the current log, export stops and asks for a fresh selection rather than silently exporting a different contact. Duplicate selection of the same record is ignored.
- Partial export cannot overwrite the active logbook, including through a symbolic-link alias. Output is written atomically. Actual import into Log4OM2 still requires user-side validation.

### RTTY: long-reception clock tracking and clearer baud settings

- Corrected the sign of the timing-loop integral adjustment. Late sample timing now shortens the estimated bit period, instead of reinforcing timing drift towards the estimator clamp during long receptions.
- Added long synthetic reception tests spanning 45.45, 50 and 75 baud; 170, 450 and 850 Hz shifts; and transmitter clock offsets of -2%, 0% and +2%.
- Added the **50 baud / 450 Hz** preset. The decoder panel explicitly shows the configured baud rate, while the status bar labels the measured receive rate **BAUD RX**.

### Distribution and validation

- Source archives are available through GitHub's source ZIP and tar.gz downloads for this tag. Packaging workflows produce the Windows x64 installer EXE, Linux x86_64 and aarch64 AppImages, and macOS Apple Silicon and Intel DMGs. Assets appear as their workflows finish.
- Local compilation and targeted automated checks do not constitute a live-radio test or a Windows/Linux hardware compatibility certification. Please retest the affected AutoCQ and logging scenarios with the new build.
- This release does not claim to fix the previously documented native SSTV back-to-back Robot-frame limitation or unrelated CI failures. The existing macOS Intel packaging compatibility-check bypass is unchanged; OS-specific filenames are not a new runtime certification.

---

## Italiano

### AutoCQ: limite sui CQ completati e pause di ascolto

- Corretti i controlli Generic AutoCQ: il limite conta le trasmissioni CQ completate, non i passaggi nel recupero del limite tentativi verso un interlocutore. Sono incluse le chiamate TX6 personalizzate, ad esempio `TEST VY2XT FN86`; le risposte durante un QSO sono escluse.
- La pausa generica parte al completamento dell'audio CQ e impedisce la successiva chiamata fino alla scadenza, senza impedire le risposte ai chiamanti validi. Nel percorso audio gestito dall'app, trasmissioni interrotte o terminate con errore non consumano il conteggio.
- Cadenza burst/ascolto e limite complessivo dei CQ rimangono separati. La pausa generica e quella del burst si sovrappongono, non si sommano: si riparte solo nel successivo slot TX consentito dopo la scadenza di entrambe. Una pausa di un secondo può quindi non cambiare la cadenza degli slot osservata.
- Rimossi conteggio e pausa dal percorso errato di recupero dei tentativi e l'azzeramento del conteggio alla gestione di un chiamante in coda. Una nuova sessione AutoCQ avvia un nuovo conteggio.
- Collegati i controlli generici sia al percorso TX della dashboard sia alla cadenza del backend legacy. Limite e pausa vengono salvati e ripristinati.
- Chiariti etichette e aiuto del watchdog: il limite a periodi misura i periodi trascorsi, compreso l'ascolto, **non il numero di CQ**. La protezione resta attiva e può fermare la sessione prima del limite CQ. In FT2 dieci periodi da 3,75 secondi equivalgono a circa 37,5 secondi, non a dieci trasmissioni.
- Aggiunti test per chiamate personalizzate, esclusione delle risposte QSO e degli errori audio, limiti finiti/illimitati e sovrapposizione delle pause.

### Log: esportazione dei contatti selezionati

- Aggiunta una selezione multipla dedicata all'esportazione tramite caselle accanto alle righe, con evidenziazione dei contatti selezionati. Disponibile in entrambe le viste del log, inclusa la finestra staccata.
- **Export ADIF** permette di scegliere **Export selected QSOs**, mostrando il numero selezionato, oppure **Export entire logbook**. Anche il menu contestuale delle righe offre selezione ed esportazione parziale; è possibile cancellare separatamente la selezione di esportazione.
- L'esportazione parziale usa tutti i campi ADIF memorizzati, senza ricostruire i QSO dalle sole colonne visibili: vengono conservati rapporti come `-10` e i campi destinati ai log esterni. Resta attiva la sanitizzazione che omette valori non validi di `MY_IOTA` non impostato.
- L'identità di esportazione comprende percorso del log, posizione originale del record e impronta dei campi. Se un record non corrisponde più a quello selezionato, l'esportazione si ferma e richiede una nuova selezione, invece di esportare silenziosamente un contatto diverso. Lo stesso record selezionato più volte non viene duplicato.
- L'esportazione parziale non può sovrascrivere il log attivo, neppure attraverso un collegamento simbolico. Scrittura del file atomica. L'importazione reale in Log4OM2 rimane da validare con gli utenti.

### RTTY: inseguimento del clock nelle ricezioni lunghe e baud più chiari

- Corretto il segno della componente integrale del recupero temporale. Un campionamento in ritardo riduce ora il periodo di bit stimato, invece di accentuare la deriva verso il limite dello stimatore durante ricezioni prolungate.
- Aggiunti test sintetici di ricezione lunga a 45,45, 50 e 75 baud, con shift di 170, 450 e 850 Hz e scostamenti del clock trasmittente di -2%, 0% e +2%.
- Aggiunto il preset **50 baud / 450 Hz**. Il pannello decoder mostra esplicitamente i baud configurati; la barra di stato identifica la velocità misurata in ricezione come **BAUD RX**.

### Distribuzione e verifiche

- Codice sorgente disponibile negli archivi ZIP e tar.gz del tag GitHub. I workflow producono installer EXE Windows x64, AppImage Linux x86_64 e aarch64 e DMG macOS Apple Silicon e Intel. I pacchetti compaiono al completamento delle rispettive build.
- Compilazione locale e controlli automatici mirati non sostituiscono la prova con radio reale né certificano l'hardware Windows/Linux. Ripetere con la nuova build le prove AutoCQ e di esportazione interessate.
- Questa versione non dichiara risolti il limite SSTV già documentato sui frame Robot consecutivi né i problemi CI non pertinenti. Rimane invariato il bypass del controllo di compatibilità del packaging macOS Intel: i nomi dei file per versione macOS non costituiscono una nuova certificazione di esecuzione.
