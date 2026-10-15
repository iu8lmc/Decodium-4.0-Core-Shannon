# Decodium 4 v1.0.650

## English (UK)

A new keyboard mode, **JTTY** from WSJT-X 3.2, rewritten in native C++, and a cleaner FT2 asynchronous decode list.

### JTTY — the keyboard mode of WSJT-X 3.2

- **JTTY** is the new mode introduced by WSJT-X 3.2.0-rc1 for fast RTTY-style contest exchanges and keyboard-to-keyboard QSOs: it feels like RTTY, with far better weak-signal performance and a very low error rate. There are no T/R periods: you transmit whenever you like, in 1.888-second 4-GFSK frames about 127 Hz wide.
- The whole modem has been **rewritten in C++** — Decodium does not link the Fortran runtime. Frame grammar (compact callsigns, contest exchanges, control phrases, five-character text frames), CRC-12 with the tail-biting K=10 convolutional code and its list decoder, the 4-GFSK waveform and the streaming multi-signal receiver with subtraction and retro sweeps.
- It has been **compared with the original Fortran of WSJT-X**, compiled separately: identical frames bit for bit on 4,323 test messages, identical waveforms, and the same decoder output line by line in 18 scenarios (from -10 to -22 dB, ITU fading, 11-frame messages, overlapping signals).
- Choose **JTTY** in the mode selector (or *Open the JTTY window…* from the menu). The radio stays in its data mode (DIG-U) and moves to the JTTY frequency of the band (14.090, 7.090, 21.090 MHz…). The window shows the QSO frequency (RX ± FTol) and all frequencies; clicking a line moves RX there and takes the callsign.
- **F1–F8** work as in WSJT-X, with `%M`, `%H`, `%Q`, `%E` and `%G`; the default templates go out as compact native frames. Field Day and RTTY Roundup exchange profiles, serial number, and QSO logging when a message starting with "TU" is sent.
- Typed text is sent with Enter and **queued seamlessly** while you are already transmitting; Esc stops. RX and TX frequencies are the waterfall markers. Complete lines go to the decode list and to ALL.TXT. PTT and audio use the same shared output as RTTY.

### FT2: each transmission shown once

- The FT2 asynchronous decoder re-reads the last 3.75 seconds every 100 ms, so the same transmission comes out of several windows. Duplicates were recognised by the 3.75-second slot computed at dispatch time: when the windows containing a transmission fell into two different slots, the same line appeared **twice** — 26 lines out of 120 on the new test bench.
- Now, as JTTY does, a transmission is recognised by the **absolute time it starts** and by its **frequency**. On the bench: duplicates from 26 to **0**, fewer genuine repetitions hidden (a station repeating its report is still shown), no decodes lost. `DECODIUM_FT2_ASYNC_REGISTRO=0` restores the old behaviour.
- The legacy backend was refilling its asynchronous ring buffer with the whole period at every audio block, so the decoder did not see continuous audio; fixed, with a test.

### Downloads

Source ZIP and tar.gz archives are available for this tag. The Windows x64 installer EXE is attached to this release; GitHub workflows add Linux x86_64/aarch64 AppImages with checksum files as they finish.

---

## Italiano

Un modo da tastiera nuovo, **JTTY** di WSJT-X 3.2, riscritto in C++ nativo, e una lista delle decodifiche FT2 asincrone più pulita.

### JTTY — il modo da tastiera di WSJT-X 3.2

- **JTTY** è il modo nuovo introdotto da WSJT-X 3.2.0-rc1 per gli scambi rapidi da contest in stile RTTY e per i QSO da tastiera: si usa come l'RTTY, ma con prestazioni molto migliori sui segnali deboli e pochissimi errori. Non ci sono periodi di TX/RX: si trasmette quando si vuole, a frame 4-GFSK da 1,888 secondi larghi circa 127 Hz.
- Tutto il modem è stato **riscritto in C++**: Decodium non collega il runtime Fortran. La grammatica dei frame (nominativi compatti, scambi da contest, frasi di controllo, frame di testo da cinque caratteri), il CRC-12 con il codice convoluzionale tail-biting K=10 e il suo decodificatore a lista, la forma d'onda 4-GFSK e il ricevitore a flusso multi-segnale con sottrazione e ripassi all'indietro.
- È stato **confrontato con il Fortran originale di WSJT-X**, compilato a parte: frame identici bit per bit su 4.323 messaggi di prova, forme d'onda identiche e la stessa uscita del decodificatore riga per riga in 18 scenari (da -10 a -22 dB, fading ITU, messaggi da 11 frame, segnali sovrapposti).
- Si sceglie **JTTY** nel selettore dei modi (o *Apri la finestra JTTY…* dal menu). La radio resta nel suo modo dati (DIG-U) e va sulla frequenza JTTY della banda (14,090, 7,090, 21,090 MHz…). La finestra mostra la frequenza del QSO (RX ± FTol) e tutte le frequenze; un clic su una riga porta lì l'RX e prende il nominativo.
- **F1–F8** come in WSJT-X, con `%M`, `%H`, `%Q`, `%E` e `%G`; i modelli predefiniti partono come frame nativi compatti. Profili di scambio Field Day e RTTY Roundup, numero progressivo, e registrazione del QSO quando si trasmette un messaggio che comincia con "TU".
- Il testo scritto parte con Invio e, se si è già in trasmissione, **si accoda senza buchi**; Esc ferma. Le frequenze RX e TX sono i marcatori del waterfall. Le righe complete vanno nella lista delle decodifiche e in ALL.TXT. PTT e audio usano la stessa uscita condivisa dell'RTTY.

### FT2: ogni trasmissione una volta sola

- Il decodificatore asincrono FT2 rilegge gli ultimi 3,75 secondi ogni 100 ms, quindi la stessa trasmissione esce da più finestre. I doppioni si riconoscevano dallo slot di 3,75 secondi calcolato al momento del lancio: quando le finestre che contenevano una trasmissione cadevano in due slot diversi, la stessa riga compariva **due volte** — 26 righe su 120 sul nuovo banco di prova.
- Ora, come fa JTTY, una trasmissione si riconosce dall'**istante assoluto in cui comincia** e dalla sua **frequenza**. Sul banco: doppioni da 26 a **0**, meno ripetizioni vere nascoste (la stazione che ripete il rapporto si vede ancora), nessuna decodifica persa. `DECODIUM_FT2_ASYNC_REGISTRO=0` torna al comportamento precedente.
- Il backend legacy riempiva il suo ring asincrono con l'intero periodo a ogni blocco audio, quindi il decodificatore non vedeva audio contiguo; corretto, con un test.

### Download

Per questo tag sono disponibili gli archivi sorgente ZIP e tar.gz. L'installer Windows x64 EXE è allegato a questo rilascio; i workflow di GitHub aggiungono le AppImage Linux x86_64/aarch64 con i file di checksum man mano che finiscono.
