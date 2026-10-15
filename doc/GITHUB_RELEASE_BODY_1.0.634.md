# Decodium 4 v1.0.634

## English (UK)

This release corrects the automatic scrolling of the decode lists and the timing of the station telemetry message sent after a QSO, and adds a diagnostic for the brief pause some operators notice when a QSO is logged.

### Full Spectrum and Signal RX stay on the last line

- The lists stopped following new decodes whenever the view happened to be more than a few rows above the bottom, for any reason: a batch of decodes arriving together, old rows being trimmed from the top, or Qt re-estimating row heights. Once detached, the list stayed a few lines above the newest decode.
- The lists now stop following only when you scroll yourself — by dragging, with the mouse wheel or with the scroll bar — and resume as soon as you return to the bottom.
- The bottom position is no longer estimated from the total content height, which with rows and separators of different heights could leave the last line out of view: the list now positions itself on the last row directly.
- This applies to both lists, docked and detached.

### Station telemetry sent in its own TX slot

- With "send station + weather info after each QSO" enabled, the extra message was started immediately after logging. In slot-based modes this could fall in the middle of a period: in one FT2 case it started 2.7 s into a 3.75 s slot, so aligning the audio to the slot left about 70 ms of signal with PTT keyed for around a second. If the 73 was still being transmitted, the message was dropped altogether.
- The message now starts at the beginning of your next TX period, complete. It waits for a transmission in progress to finish, takes precedence over the CQ in that period, and is abandoned if it cannot start within four periods. Asynchronous FT2 transmission is unchanged.

### Diagnosing the pause after logging

- Logging itself takes about 11 ms. In recent FT2 tests the interface paused for 120–200 ms shortly afterwards, only after a QSO was logged and never at other transmissions.
- For four seconds after each logged QSO, Decodium now records the longest interface pauses and the events that caused them in the diagnostic log (`[LOGSTALL]`), so the cause can be identified and corrected in a following release.

### Downloads

Source ZIP and tar.gz archives are available for this tag. The Windows x64 installer EXE is attached to this release; GitHub workflows add Linux x86_64/aarch64 AppImages with checksum files as they finish.

---

## Italiano

Questo rilascio corregge lo scorrimento automatico delle liste delle decodifiche e il momento in cui parte il messaggio di telemetria di stazione dopo un QSO, e aggiunge una diagnostica per la breve pausa che alcuni operatori notano quando un QSO viene messo a log.

### Full Spectrum e Signal RX restano sull'ultima riga

- Le liste smettevano di seguire le nuove decodifiche ogni volta che la vista si trovava qualche riga sopra il fondo, per qualunque motivo: un gruppo di decodifiche arrivato insieme, le righe più vecchie tolte in cima, oppure Qt che ricalcolava le altezze delle righe. Una volta staccata, la lista restava ferma qualche riga sopra l'ultima decodifica.
- Ora le liste smettono di seguire solo quando scorri tu — trascinando, con la rotella o con la barra di scorrimento — e riprendono appena torni in fondo.
- La posizione del fondo non viene più stimata dall'altezza totale del contenuto, che con righe e separatori di altezze diverse poteva lasciare fuori vista l'ultima riga: la lista si porta direttamente sull'ultima riga.
- Vale per entrambe le liste, sia agganciate sia staccate.

### Telemetria di stazione nel suo slot di trasmissione

- Con «invia informazioni di stazione e meteo dopo ogni QSO» attivo, il messaggio aggiuntivo partiva subito dopo la messa a log. Nei modi a slot poteva cadere a metà periodo: in un caso FT2 è partito a 2,7 s dentro uno slot da 3,75 s, e l'allineamento dell'audio allo slot ha lasciato circa 70 ms di segnale con il PTT chiuso per circa un secondo. Se il 73 era ancora in trasmissione, il messaggio veniva scartato del tutto.
- Ora il messaggio parte all'inizio del tuo prossimo periodo di trasmissione, completo. Aspetta che finisca una trasmissione in corso, in quel periodo ha la precedenza sul CQ, e viene abbandonato se non riesce a partire entro quattro periodi. La trasmissione FT2 asincrona non cambia.

### Diagnosi della pausa dopo la messa a log

- La messa a log in sé richiede circa 11 ms. Nelle prove FT2 recenti l'interfaccia si fermava per 120–200 ms subito dopo, solo dopo la messa a log di un QSO e mai nelle altre trasmissioni.
- Per quattro secondi dopo ogni QSO messo a log, Decodium registra ora nel log diagnostico le pause più lunghe dell'interfaccia e gli eventi che le hanno causate (`[LOGSTALL]`), così la causa potrà essere individuata e corretta in un rilascio successivo.

### Download

Per questo tag sono disponibili gli archivi sorgente ZIP e tar.gz. L'installer Windows x64 EXE è allegato a questa release; i workflow di GitHub aggiungono le AppImage Linux x86_64/aarch64 con i file di controllo man mano che terminano.
