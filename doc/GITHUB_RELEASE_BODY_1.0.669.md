# Decodium 4 v1.0.669

## English (UK)

This release adds the CW module (also in the mode bar) and the antenna rotator, built to work with a remote radio.

### CW module

- New window (main menu, "CW - decoder, keyer and macros..."), the same CW module as DecoDXLog: a CW decoder with a live scope, twelve editable macro keys (F1-F12, with {MYCALL} {CALL} {RST} {NR} {EXCH} {NAME}), free text, speed from 5 to 60 wpm, call, report and exchange fields, and a Log QSO button that writes the contact with mode CW.
- Three ways to send CW: an audio tone on the transmit audio path, a serial keyer (DTR or RTS of a COM port) or a K1EL WinKeyer. Esc or Stop interrupts at once.
- The decoder listens to the audio of the radio, local or remote.

### CW with a remote radio

- With a remote radio (DecoPort or Decolink) the CW goes out as an audio tone through the link, like any other mode.
- With Decolink it can also send only the key timing: a few bytes instead of an audio stream, and the gateway next to the radio makes the tone. The rhythm does not depend on the network. Stop releases the PTT immediately. Needs a Decolink gateway with the CW key profile; otherwise it falls back to the audio tone.
- While a CW message is keyed to a remote radio, no other mode can transmit.
- Not yet tested on air. The contest macros and the voice keyer of DecoDXLog are not included.

### CW in the mode bar

- CW is now a mode of the mode bar, like RTTY and JTTY: choosing it opens the CW window, and leaving it stops the keyer.

### Antenna rotator (PRO.SIS.TEL)

- New window (main menu, "Rotator - antenna control..."), ported from DecoRotor: azimuth dial, free heading, memories, locator bearing (short and long path) and a satellite map that shows the stations Decodium is hearing, each with its bearing and distance. Clicking one points the antenna at it.
- Prosistel control boxes (D, Combi-Track and the others), model auto-detected, software limits, stall watchdog, stop if the link drops. A built-in simulator lets you try it without hardware.
- Off by default: it opens no serial port and no network port until you turn it on in the window's Settings tab. The network side (rotctld on 4535, WebSocket on 8765 for the phone apps, web page on 8080, optional token) is a separate switch.

### Downloads

Source ZIP and tar.gz archives are available for this tag. The Windows x64 installer EXE is attached to this release; GitHub workflows add Linux x86_64/aarch64 AppImages with checksum files as they finish.

---

## Italiano

Questa versione aggiunge il modulo CW, pensato per funzionare con una radio remota.

### Modulo CW

- Nuova finestra (menu principale, "CW - decodificatore, manipolatore e macro..."), lo stesso modulo CW di DecoDXLog: decodificatore CW con grafico in tempo reale, dodici tasti macro modificabili (F1-F12, con {MYCALL} {CALL} {RST} {NR} {EXCH} {NAME}), testo libero, velocità da 5 a 60 wpm, campi nominativo, rapporto e scambio, e un pulsante Log QSO che registra il collegamento col modo CW.
- Tre modi di mandare il CW: un tono audio sul percorso audio di trasmissione, un manipolatore seriale (DTR o RTS di una porta COM) o un K1EL WinKeyer. Esc o Stop interrompono subito.
- Il decodificatore ascolta l'audio della radio, locale o remota.

### CW con una radio remota

- Con una radio remota (DecoPort o Decolink) il CW esce come tono audio attraverso il collegamento, come ogni altro modo.
- Con Decolink può mandare anche solo gli istanti del tasto: pochi byte invece di un flusso audio, e il tono lo fa il gateway accanto alla radio. Il ritmo non dipende dalla rete. Stop abbassa subito il PTT. Serve un gateway Decolink con il profilo CW a tasto; altrimenti si torna al tono audio.
- Mentre un messaggio CW è manipolato verso una radio remota, nessun altro modo può trasmettere.
- Non ancora provato in aria. Le macro da contest e il voice keyer di DecoDXLog non sono inclusi.

### CW nella barra dei modi

- Il CW ora è un modo della barra dei modi, come RTTY e JTTY: sceglierlo apre la finestra CW, uscirne ferma il manipolatore.

### Rotore d'antenna (PRO.SIS.TEL)

- Nuova finestra (menu principale, "Rotore - controllo antenna..."), portata da DecoRotor: quadrante dell'azimut, direzione libera, memorie, rotta da locatore (breve e lunga) e mappa satellitare con le stazioni che Decodium sta sentendo, ciascuna con rotta e distanza. Un clic su una stazione punta l'antenna.
- Control box Prosistel (D, Combi-Track e gli altri), modello riconosciuto da solo, finecorsa software, watchdog di stallo, stop se cade il collegamento. Un simulatore integrato permette di provarlo senza hardware.
- Spento di default: non apre nessuna porta seriale né di rete finché non lo accendi dalla scheda Impostazioni della finestra. La parte di rete (rotctld su 4535, WebSocket su 8765 per le app, pagina web su 8080, token facoltativo) ha un interruttore a parte.

### Download

Gli archivi ZIP e tar.gz del sorgente sono disponibili per questo tag. L'installer EXE per Windows x64 è allegato a questa release; i workflow di GitHub aggiungono gli AppImage Linux x86_64/aarch64 con i file di checksum man mano che finiscono.
