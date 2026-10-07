# Decodium 4 v1.0.669

## English (UK)

This release adds the CW module, built to work with a remote radio.

### CW module

- New window (main menu, "CW - decoder, keyer and macros..."), the same CW module as DecoDXLog: a CW decoder with a live scope, twelve editable macro keys (F1-F12, with {MYCALL} {CALL} {RST} {NR} {EXCH} {NAME}), free text, speed from 5 to 60 wpm, call, report and exchange fields, and a Log QSO button that writes the contact with mode CW.
- Three ways to send CW: an audio tone on the transmit audio path, a serial keyer (DTR or RTS of a COM port) or a K1EL WinKeyer. Esc or Stop interrupts at once.
- The decoder listens to the audio of the radio, local or remote.

### CW with a remote radio

- With a remote radio (DecoPort or Decolink) the CW goes out as an audio tone through the link, like any other mode.
- With Decolink it can also send only the key timing: a few bytes instead of an audio stream, and the gateway next to the radio makes the tone. The rhythm does not depend on the network. Stop releases the PTT immediately. Needs a Decolink gateway with the CW key profile; otherwise it falls back to the audio tone.
- While a CW message is keyed to a remote radio, no other mode can transmit.
- Not yet tested on air. The contest macros and the voice keyer of DecoDXLog are not included.

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

### Download

Gli archivi ZIP e tar.gz del sorgente sono disponibili per questo tag. L'installer EXE per Windows x64 è allegato a questa release; i workflow di GitHub aggiungono gli AppImage Linux x86_64/aarch64 con i file di checksum man mano che finiscono.
