# Decodium 4 v1.0.658

## English (UK)

The DX Cluster no longer stalls the interface or costs decodes during pile-ups. Includes elisir80's 1.0.657.

### DX Cluster: no more interface stalls and lost audio

- The DX Cluster list was rebuilt from scratch every time a group of spots arrived: the whole list (up to 200 spots) was copied into the interface and every row recreated. Two copies of the panel exist (docked and detached) and both did this even when the panel was closed. Each rebuild held the interface thread for 100-180 ms; during a busy pile-up, with spots arriving almost every second, the progress bar stuttered.
- On a measured afternoon of heavy cluster traffic this also cost reception. The audio capture lost 0.3-0.7 s per FT8 slot (up to 95% of slots), signals arrived 0.7 s early in Decodium's window, SNR was 6 dB lower than WSJT-X's on the same audio, and Decodium decoded half as many messages as WSJT-X listening to the same input.
- The spots now live in a list model that adds the new row and removes the oldest one, without touching the others. The mode filter hides rows instead of rebuilding the list, and the list keeps scrolling to the newest spot even when it is full. The spot labels on the waterfall are refreshed at most once per second.
- Measured on air with the panel open and 24-27 spot updates per minute: no interface stalls from the panel (previously about 30 per minute at ~105 ms), DT and SNR identical to WSJT-X's on the same messages, and Decodium at 1.13-1.23 times WSJT-X's distinct messages per slot.

### From elisir80 1.0.657

- Caller-retry limits are consistent between the active profile, the controls and the runtime; repeated TX2 reports respect the configured cap.
- AutoCQ/MAM can no longer send RR73 to a new caller before our report has been transmitted to that caller.
- A QSO is logged immediately after our verified final 73.
- macOS Hamlib PTT allows for the one-second CI-V transaction timeout and no longer reconnects CAT straight away after a TX failure.

### Downloads

Source ZIP and tar.gz archives are available for this tag. The Windows x64 installer EXE is attached to this release; GitHub workflows add Linux x86_64/aarch64 AppImages with checksum files as they finish.

---

## Italiano

Il DX Cluster non blocca più l'interfaccia e non fa più perdere decodifiche durante i pile-up. Comprende la 1.0.657 di elisir80.

### DX Cluster: niente più blocchi dell'interfaccia e audio perso

- La lista del DX Cluster veniva ricostruita da capo a ogni gruppo di spot in arrivo: l'intero elenco (fino a 200 spot) veniva ricopiato nell'interfaccia e ogni riga ricreata. Il pannello esiste in due copie (incassata e staccata) e lo facevano entrambe, anche a pannello chiuso. Ogni ricostruzione teneva fermo il thread dell'interfaccia per 100-180 ms; durante un pile-up, con spot quasi ogni secondo, la barra di avanzamento andava a scatti.
- In un pomeriggio di cluster molto attivo questo è costato anche in ricezione. La cattura audio perdeva 0,3-0,7 s per slot FT8 (fino al 95% degli slot), i segnali arrivavano 0,7 s in anticipo nella finestra di Decodium, lo SNR era 6 dB sotto quello di WSJT-X sullo stesso audio, e Decodium decodificava la metà dei messaggi di WSJT-X collegato allo stesso ingresso.
- Ora gli spot stanno in un modello a righe che aggiunge la riga nuova e toglie la più vecchia senza toccare le altre. Il filtro per modo nasconde le righe invece di ricostruire la lista, e la lista continua a scendere sullo spot più recente anche quando è piena. Le etichette degli spot sulla waterfall si aggiornano al massimo una volta al secondo.
- Misurato in aria con il pannello aperto e 24-27 aggiornamenti di spot al minuto: nessun blocco dell'interfaccia dal pannello (prima circa 30 al minuto da ~105 ms), DT e SNR identici a WSJT-X sugli stessi messaggi, e Decodium a 1,13-1,23 volte i messaggi distinti per slot di WSJT-X.

### Dalla 1.0.657 di elisir80

- I limiti di ripetizione verso chi chiama sono coerenti fra profilo attivo, controlli e runtime; i rapporti TX2 ripetuti rispettano il limite impostato.
- AutoCQ/MAM non può più mandare RR73 a un nuovo chiamante prima che gli sia stato trasmesso il nostro rapporto.
- Il QSO viene registrato subito dopo il nostro 73 finale verificato.
- Su macOS il PTT con Hamlib tiene conto del timeout di un secondo delle transazioni CI-V e non riconnette più subito la CAT dopo un errore in TX.

### Download

Per questo tag sono disponibili gli archivi sorgente ZIP e tar.gz. L'installer Windows x64 EXE è allegato a questo rilascio; i workflow di GitHub aggiungono le AppImage Linux x86_64/aarch64 con i file di checksum man mano che finiscono.
