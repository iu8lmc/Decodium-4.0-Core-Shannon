# Decodium 4 v1.0.640

## English (UK)

This release absorbs upstream v1.0.638 and v1.0.639, whose safe FT8 CPU dispatch fixes a crash that restarted the program every few minutes on older processors, adds far more informative crash and startup diagnostics, renames the decoder to **superldpc**, teaches FT2 to remember the stations it has already heard, and links Decodium to DecoLog.

### Older processors: the program no longer restarts during decoding

- On processors without AVX2 — Ivy Bridge and earlier — Decodium could die with `STATUS_ILLEGAL_INSTRUCTION` (`0xC000001D`) a minute or two after the traffic started, and the program simply reappeared. The report that led to this fix described it as "it restarts after two or three minutes".
- The cause was the FT8 BICM extrinsic-information path, which entered the AVX2 implementation **directly** instead of going through the CPU dispatcher. The LDPC decoder itself was already guarded, which is why ordinary decoding worked and only this path failed. It is used by one of the four sensitivity levers, so it only came into play once the adaptive governor granted them: in the diagnostic log the crash always follows the first partial decode after `[LEVE] sensibilita' ACCESE`.
- CPU capabilities, operating-system AVX state, decoder enablement and the environment override are now all checked before the accelerated implementation is entered; an unsupported configuration keeps the safe fallback. Regression tests cover nine simulated CPU configurations, Ivy Bridge included.

### Crash and startup diagnostics that say something

- The crash handler used to record only the exception code. It now records the exception address, the module and offset, the thread, the access type for memory faults, and **the first sixteen bytes at the faulting address**: a `C4`/`C5` VEX prefix means an AVX instruction on a processor that lacks it, `0F 0B` a compiler trap, incoherent bytes a jump into data.
- The startup block with Qt, operating system, processor, locale, screen and memory had never been written to the diagnostic log at all: the QML application uses the static logging API and never constructs the logging object, so the call was skipped. It is now always written.
- A new line reports the processor model, the AVX/AVX2/FMA/OSXSAVE and operating-system state flags, and which decoder was actually selected, with the reason for any fallback. This previously went only to standard error, where user logs never saw it.

### The decoder is now called superldpc

- The two-stage LDPC decoder written for this project has been renamed from fastldpc to **superldpc** throughout the code, the documentation and the site, to avoid confusion with unrelated projects of similar name.
- Attribution has been corrected: `ft8_lib` by Kārlis Goba YL3JG is MIT-licensed, the LDPC(174,91) and CRC-14 tables come from the FT8 protocol by Steve Franke K9AN and Joe Taylor K1JT and are used unmodified, the pair search follows the npre1/npre2 idea from the WSJT-X OSD, and "written from scratch" applies to superldpc, not to all of Decodium.

### FT2 remembers the stations it has heard

- FT8 has long used a memory of recent senders to place candidates; FT2 had none. It now keeps the frequencies of the stations decoded in the previous cycles and forces a candidate where one is expected, and it can also try the whole 77-bit message of a station heard recently.
- On benches with known truth the gain runs from 6.1 % under ideal conditions to 18.6 % with 1 Hz Rayleigh fading, with no false decodes on 500 slots of noise and about 14 % more decode time.
- It is off by default pending an on-air trial: `DECODIUM_FT2_STORICO=1` enables it, `DECODIUM_FT2_STORICO_AP=1` adds the whole-message hypothesis, with `DECODIUM_FT2_STORICO_HZ` and `DECODIUM_FT2_STORICO_CICLI` for the search window and the memory depth.

### DecoLog: shared log and cluster spots

- Decodium connects to the local DecoLink channel: contacts logged in DecoLog enter the worked-before sets (B4, new DXCC, zone, locator) and stay there across reloads of the ADIF file, and a confirmation appears in the message bar when DecoLog has stored a contact sent over UDP.
- The spots DecoLog sends — already compared against its log, so NEW DXCC, NEW BAND and so on — enter the DX Cluster list and the waterfall without duplicates, and alert spots also appear in the status bar.
- Double-clicking a spot in DecoLog takes the radio to the calling frequency, switches between FT8, FT4 and FT2, and prepares the DX call and receive frequency. It never transmits.

### Absorbed from upstream v1.0.638 and v1.0.639

- Safe FT8 CPU dispatch, described above.
- Q65 EME Doppler tracking in the Astro window, with CFOM, full Doppler to the DX grid and own echo; tracking starts off in every session and never enables transmission.
- RTTY and native SSTV can transmit on audio alone, with VOX or manual PTT; RTTY timing-loop feedback is corrected for long receptions and gains a 50 baud / 450 Hz preset.
- AutoCQ counts completed calls rather than retry recoveries, and the generic and burst pauses overlap instead of adding up; the watchdog labels now say clearly that its period counts elapsed periods, not calls.
- The logbook can export selected contacts to ADIF, using the stored field set, in both views.
- Hiding a decode in the display no longer suppresses its alert or PSK Reporter processing, and changing mode resets the Full Spectrum and Signal RX models immediately.

### Downloads

Source ZIP and tar.gz archives are available for this tag. The Windows x64 installer EXE is attached to this release; GitHub workflows add Linux x86_64/aarch64 AppImages with checksum files as they finish.

---

## Italiano

Questo rilascio assorbe le versioni upstream 1.0.638 e 1.0.639, il cui dispatch FT8 sicuro corregge un crash che sulle CPU meno recenti riavviava il programma ogni pochi minuti, porta diagnostiche di crash e di avvio finalmente utili, rinomina il decodificatore in **superldpc**, insegna a FT2 a ricordare le stazioni già sentite e collega Decodium a DecoLog.

### Processori meno recenti: il programma non si riavvia più mentre decodifica

- Sui processori senza AVX2 — Ivy Bridge e precedenti — Decodium poteva morire con `STATUS_ILLEGAL_INSTRUCTION` (`0xC000001D`) uno o due minuti dopo l'arrivo del traffico, e il programma semplicemente ricompariva. La segnalazione che ha portato alla correzione diceva «si riavvia dopo due o tre minuti».
- La causa era il percorso dell'informazione extrinseca del BICM in FT8, che entrava **direttamente** nell'implementazione AVX2 invece di passare dal dispatcher della CPU. Il decodificatore LDPC vero e proprio era già protetto: per questo la decodifica normale funzionava e cedeva solo quel percorso. Lo usa una delle quattro leve di sensibilità, quindi entrava in gioco soltanto quando il governatore adattivo le concedeva: nel log diagnostico il crash segue sempre la prima decodifica parziale dopo `[LEVE] sensibilita' ACCESE`.
- Ora prima di entrare nell'implementazione accelerata si controllano capacità della CPU, stato AVX del sistema operativo, abilitazione del decodificatore e variabile d'ambiente; una configurazione non idonea mantiene la ricaduta sicura. I test coprono nove configurazioni di CPU simulate, Ivy Bridge compresa.

### Diagnostiche di crash e di avvio che dicono qualcosa

- Il gestore dei crash registrava solo il codice dell'eccezione. Ora registra indirizzo, modulo e scostamento, thread, tipo di accesso per gli errori di memoria e **i primi sedici byte all'indirizzo dell'eccezione**: un prefisso VEX `C4`/`C5` significa istruzione AVX su un processore che non la ha, `0F 0B` una trappola del compilatore, byte incoerenti un salto in memoria dati.
- Il blocco di avvio con Qt, sistema operativo, processore, lingua, schermo e memoria non era mai stato scritto nel log diagnostico: l'applicazione QML usa l'API statica del log e non costruisce mai l'oggetto, quindi la chiamata veniva saltata. Ora viene scritto sempre.
- Una riga nuova riporta il modello del processore, le estensioni AVX/AVX2/FMA/OSXSAVE con lo stato del sistema operativo e quale decodificatore è stato davvero scelto, con il motivo dell'eventuale ricaduta. Prima usciva solo sullo standard error, dove nei log degli utenti non arriva.

### Il decodificatore si chiama superldpc

- Il decodificatore LDPC a due stadi scritto per questo progetto passa da fastldpc a **superldpc** nel codice, nella documentazione e sul sito, per non confondersi con progetti estranei dal nome simile.
- Corrette le attribuzioni: `ft8_lib` di Kārlis Goba YL3JG è sotto licenza MIT, le tabelle LDPC(174,91) e CRC-14 vengono dal protocollo FT8 di Steve Franke K9AN e Joe Taylor K1JT e sono usate senza modifiche, la ricerca a coppie segue l'idea npre1/npre2 dell'OSD di WSJT-X, e «scritto da zero» vale per superldpc, non per tutto Decodium.

### FT2 ricorda le stazioni sentite

- FT8 usa da tempo la memoria dei mittenti recenti per collocare i candidati; FT2 non l'aveva. Ora conserva le frequenze delle stazioni decodificate nei cicli precedenti e forza un candidato dove una stazione è attesa, e può anche provare l'intero messaggio a 77 bit di una stazione sentita da poco.
- Sui banchi con verità nota il guadagno va dal 6,1 % in condizioni ideali al 18,6 % con evanescenza di Rayleigh a 1 Hz, senza decodifiche false su 500 slot di solo rumore e con circa il 14 % di tempo di decodifica in più.
- Resta spento in attesa della prova in aria: `DECODIUM_FT2_STORICO=1` lo accende, `DECODIUM_FT2_STORICO_AP=1` aggiunge l'ipotesi sul messaggio intero, con `DECODIUM_FT2_STORICO_HZ` e `DECODIUM_FT2_STORICO_CICLI` per la finestra di ricerca e la profondità della memoria.

### DecoLog: log condiviso e spot del cluster

- Decodium si collega al canale locale DecoLink: i QSO registrati in DecoLog entrano negli insiemi worked-before (B4, nuovo DXCC, zona, locatore) e ci restano a ogni ricarica del file ADIF, e nella barra dei messaggi compare la conferma quando DecoLog ha memorizzato un contatto inviato via UDP.
- Gli spot che DecoLog manda — già confrontati col suo log, quindi NEW DXCC, NEW BAND e simili — entrano nella lista del DX Cluster e nella cascata senza doppioni, e quelli d'avviso compaiono anche nella barra di stato.
- Un doppio clic su uno spot in DecoLog porta la radio sulla frequenza di chiamata, cambia modo fra FT8, FT4 e FT2 e prepara il nominativo DX e la frequenza di ricezione. Non trasmette mai.

### Assorbito dalle versioni upstream 1.0.638 e 1.0.639

- Il dispatch FT8 sicuro descritto sopra.
- Tracciamento Doppler EME per Q65 nella finestra Astro, con CFOM, Doppler completo verso il locatore DX ed eco propria; il tracciamento parte spento a ogni sessione e non abilita mai la trasmissione.
- RTTY e SSTV nativa possono trasmettere in solo audio, con VOX o PTT manuale; l'anello di temporizzazione RTTY è corretto per le ricezioni lunghe e guadagna il preset 50 baud / 450 Hz.
- AutoCQ conta le chiamate completate invece dei recuperi dei retry, e la pausa generica e quella a raffica si sovrappongono invece di sommarsi; le etichette del watchdog dicono ora chiaramente che il suo periodo conta i periodi trascorsi, non le chiamate.
- Il logbook può esportare in ADIF i soli contatti selezionati, usando i campi memorizzati, in entrambe le viste.
- Nascondere una decodifica nella lista non ne sopprime più l'avviso né l'invio a PSK Reporter, e il cambio di modo azzera subito i modelli di Full Spectrum e Signal RX.

### Download

Per questo tag sono disponibili gli archivi sorgente ZIP e tar.gz. L'installer Windows x64 EXE è allegato a questo rilascio; i workflow di GitHub aggiungono le AppImage Linux x86_64/aarch64 con i file di checksum man mano che finiscono.
