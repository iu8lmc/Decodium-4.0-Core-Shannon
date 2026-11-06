# Decodium 4 v1.0.656

## English (UK)

FT2 QSOs confirmed by the other station are no longer lost when multi-answer mode (MAM) is on. Built on elisir80's 1.0.655.

### QSOs confirmed but not logged

- With multi-answer mode (MAM) switched on, even with a single stream, every AutoCQ QSO goes through a MAM slot. When the other station had already confirmed our report (R+report) but kept repeating it because it never decoded our RR73, the slot was dropped at the retry limit **without logging the QSO**, and Decodium went back to CQ. The normal sequencer logs the QSO in that case; the MAM path did not. Two real QSOs were lost this way on 18 September.
- Now a MAM slot that has reached the roger stage (the other station has confirmed our report) is logged before it is closed, whether it ends at the retry limit or after silence.
- Reproduced and verified without a radio, with a simulated station that never hears our RR73: before, four RR73 and back to CQ with no log; now the QSO is logged after the fourth RR73. A normal QSO (73 received) is logged once, as before.

### A QSO journal that is not rotated away

- The diagnostic log rotates at 5 MB × 3 and covers only a few hours, so the lost QSOs above could not be explained from the logs of those days.
- Decodium now also writes `%LOCALAPPDATA%\Decodium\Decodium\qso_journal\sequencer_<date>.log`: one small file per day, kept for 30 days, with what was transmitted and every sequencer and MAM decision (start, retries, log or drop). `DECODIUM_QSO_JOURNAL=0` switches it off.

### For testers

- New `--lab-rx-inject <file>` option: each line appended to the file enters the FT2 asynchronous decode path as a decoded row, so the sequencer can be tested against a simulated station without a radio.

### Downloads

Source ZIP and tar.gz archives are available for this tag. The Windows x64 installer EXE is attached to this release; GitHub workflows add Linux x86_64/aarch64 AppImages with checksum files as they finish.

---

## Italiano

I QSO FT2 confermati dal corrispondente non vanno più persi con la modalità multi-risposta (MAM) accesa. Costruita sulla 1.0.655 di elisir80.

### QSO confermati ma non loggati

- Con la modalità multi-risposta (MAM) accesa, anche con un solo flusso, ogni QSO in AutoCQ passa da uno "slot" MAM. Quando il corrispondente aveva già confermato il nostro rapporto (R+rapporto) ma continuava a ripeterlo perché non decodificava il nostro RR73, lo slot veniva chiuso al limite dei tentativi **senza loggare il QSO**, e Decodium tornava in CQ. Il sequencer normale in quel caso logga il QSO; il percorso MAM no. Il 18 settembre due QSO reali sono andati persi così.
- Ora uno slot MAM arrivato allo stadio della conferma (il corrispondente ha confermato il nostro rapporto) viene loggato prima di essere chiuso, sia al limite dei tentativi sia dopo un silenzio.
- Riprodotto e verificato senza radio, con una stazione simulata che non sente mai il nostro RR73: prima quattro RR73 e ritorno in CQ senza log; ora il QSO viene loggato dopo il quarto RR73. Un QSO normale (73 ricevuto) si logga una volta sola, come prima.

### Un registro dei QSO che non si cancella

- Il log diagnostico ruota a 5 MB × 3 e copre poche ore, quindi i QSO persi qui sopra non si sono potuti spiegare con i log di quei giorni.
- Decodium ora scrive anche `%LOCALAPPDATA%\Decodium\Decodium\qso_journal\sequencer_<data>.log`: un file piccolo al giorno, tenuto 30 giorni, con cosa è stato trasmesso e ogni decisione del sequencer e del MAM (avvio, tentativi, log o abbandono). `DECODIUM_QSO_JOURNAL=0` lo spegne.

### Per i tester

- Nuova opzione `--lab-rx-inject <file>`: ogni riga aggiunta al file entra nel decode FT2 asincrono come una riga decodificata, per provare il sequencer contro una stazione simulata senza radio.

### Download

Per questo tag sono disponibili gli archivi sorgente ZIP e tar.gz. L'installer Windows x64 EXE è allegato a questo rilascio; i workflow di GitHub aggiungono le AppImage Linux x86_64/aarch64 con i file di checksum man mano che finiscono.
