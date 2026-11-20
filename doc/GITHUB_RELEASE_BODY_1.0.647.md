# Decodium 4 v1.0.647

## English (UK)

A compatibility fix for the DecoLink channel, on top of v1.0.646.

### DecoLink: the handshake also accepts the log's new name

- The station log has been renamed from **DecoLog** to **DecoDXLog**. Decodium's handshake check required the `app` field to be exactly `DecoLog`, so with the renamed program Decodium connected, read `DecoDXLog`, closed the connection and retried five seconds later — indefinitely, and without saying why.
- `app` is the name of the **protocol**, not of the program. Decodium now accepts both `DecoLog` and `DecoDXLog`, so a further rename cannot stall the link again.
- The handshake's new `product` field, when present, is the name Decodium displays: a station running DecoDXLog reads *DecoDXLog* where it used to read *DecoLog*. Older logs that do not send the field are unaffected.
- This is not urgent for anyone: DecoDXLog 0.9.1 already sends `app: "DecoLog"` again, so installed copies of Decodium keep working as they are. The change matters for what comes next, and for showing the right name.

### Connection status lines are translatable

- The two DecoLink status lines were written in Italian and could not be translated, so they appeared in Italian in every language. They are now English at source and translated into all fifteen supplied languages. They had escaped the previous pass because they are written straight into the status property rather than through the status-message call.

### Downloads

Source ZIP and tar.gz archives are available for this tag. The Windows x64 installer EXE is attached to this release; GitHub workflows add Linux x86_64/aarch64 AppImages with checksum files as they finish.

---

## Italiano

Una correzione di compatibilità per il canale DecoLink, sopra la 1.0.646.

### DecoLink: il saluto accetta anche il nome nuovo del log

- Il log di stazione è stato rinominato da **DecoLog** a **DecoDXLog**. Il controllo del saluto in Decodium pretendeva che il campo `app` fosse esattamente `DecoLog`, quindi con il programma rinominato Decodium si collegava, leggeva `DecoDXLog`, chiudeva la connessione e riprovava dopo cinque secondi — all'infinito, e senza dire perché.
- `app` è il nome del **protocollo**, non del programma. Decodium ora accetta sia `DecoLog` sia `DecoDXLog`, così un'altra rinomina non può più bloccare il collegamento.
- Il campo nuovo `product` del saluto, quando c'è, è il nome che Decodium mostra: una stazione con DecoDXLog legge *DecoDXLog* dove prima leggeva *DecoLog*. I log più vecchi, che quel campo non lo mandano, non cambiano comportamento.
- Non è urgente per nessuno: DecoDXLog 0.9.1 manda di nuovo `app: "DecoLog"`, quindi le copie di Decodium già installate continuano a funzionare così come sono. La modifica serve per il seguito, e per mostrare il nome giusto.

### Le righe di stato del collegamento sono traducibili

- Le due righe di stato di DecoLink erano scritte in italiano e non si potevano tradurre, quindi comparivano in italiano in tutte le lingue. Ora sono inglesi all'origine e tradotte in tutte e quindici le lingue fornite. Erano sfuggite al giro precedente perché vengono scritte direttamente nella proprietà di stato e non passano dalla chiamata dei messaggi di stato.

### Download

Per questo tag sono disponibili gli archivi sorgente ZIP e tar.gz. L'installer Windows x64 EXE è allegato a questo rilascio; i workflow di GitHub aggiungono le AppImage Linux x86_64/aarch64 con i file di checksum man mano che finiscono.
