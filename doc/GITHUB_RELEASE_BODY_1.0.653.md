# Decodium 4 v1.0.653

## English (UK)

RTTY transmits again when the DecoPort gateway has a radio of its own, and the DecoPort window opens again.

### RTTY: PTT went to the wrong port

- Local RTTY transmission shares the PTT path used by DecoPort remote clients. When the DecoPort gateway had opened its own radio on another serial port, the PTT command was sent there instead of to the application's CAT connection. On a Yaesu FT-991 that port was the "Standard" COM port, which does not accept CAT commands: the AFSK audio played normally but the radio stayed in receive.
- Local RTTY now keys the radio through the application's CAT connection whenever it is connected; the gateway's own radio is used only by remote clients, or when there is no CAT connection. PTT is released on the same port that raised it.
- Reminder: to transmit AFSK from the sound card, keep the radio in **DIGU** (DATA-U). In the true RTTY modes (RTTY-U/RTTY-L) the FT-991 and many other radios wait for FSK keying and ignore the USB audio.

### DecoPort window would not open

- The window declared `Component.onCompleted` twice on the same object. Qt rejects the whole file in that case ("Property value set multiple times"), so the window could not be loaded and its menu entry and status-bar button appeared to do nothing. The two handlers are now one. The fault had been present since 1.0.601.

### Downloads

Source ZIP and tar.gz archives are available for this tag. The Windows x64 installer EXE is attached to this release; GitHub workflows add Linux x86_64/aarch64 AppImages with checksum files as they finish.

---

## Italiano

L'RTTY torna a trasmettere quando il gateway DecoPort ha una radio propria, e la finestra DecoPort torna ad aprirsi.

### RTTY: il PTT finiva sulla porta sbagliata

- La trasmissione RTTY locale usa lo stesso percorso del PTT dei client remoti di DecoPort. Quando il gateway DecoPort aveva aperto una radio propria su un'altra porta seriale, il comando PTT andava lì invece che alla connessione CAT dell'applicazione. Su una Yaesu FT-991 quella porta era la COM "Standard", che non accetta comandi CAT: l'audio AFSK usciva regolarmente ma la radio restava in ricezione.
- Ora l'RTTY locale manda in trasmissione la radio dalla connessione CAT dell'applicazione ogni volta che è collegata; la radio propria del gateway la usano solo i client remoti, o quando il CAT manca. Il PTT viene rilasciato sulla stessa porta da cui è stato alzato.
- Promemoria: per trasmettere in AFSK dalla scheda audio tenere la radio in **DIGU** (DATA-U). Nei modi RTTY veri (RTTY-U/RTTY-L) la FT-991 e molte altre radio aspettano la manipolazione FSK e ignorano l'audio USB.

### La finestra DecoPort non si apriva

- La finestra dichiarava due volte `Component.onCompleted` sullo stesso oggetto. In questo caso Qt rifiuta l'intero file ("Property value set multiple times"): la finestra non si caricava, e la voce di menu e il pulsante nella barra di stato sembravano non fare niente. I due gestori ora sono uno solo. Il difetto c'era dalla 1.0.601.

### Download

Per questo tag sono disponibili gli archivi sorgente ZIP e tar.gz. L'installer Windows x64 EXE è allegato a questo rilascio; i workflow di GitHub aggiungono le AppImage Linux x86_64/aarch64 con i file di checksum man mano che finiscono.
