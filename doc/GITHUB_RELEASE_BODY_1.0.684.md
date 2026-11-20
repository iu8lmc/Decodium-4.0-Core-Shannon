# Decodium 4 v1.0.684

This release adds an optional local voice and assistant toolkit to the SSB window, and makes the CW module start only when you open it.

## Optional packs (install only what you use)

Each pack is separate: install it, or remove it to free the disk space. Nothing is sent to the internet while you use them, and no account is needed. Downloads are checked with SHA256 before use.

- **Voice** (about 150 MB): the Piper engine (MIT) with an Italian voice (paola, CC0 dataset) and an English voice (amy, CC BY-SA 4.0, mimic3-voices dataset). Attribution is required for the English voice.
- **Speech recognition** (about 490 MB): the whisper.cpp multilingual model `small` (MIT). The recognition engine is not yet part of the download in this release, so this pack is shown as "engine not yet available".
- **Assistant** (about 1.1 GB): llama.cpp (MIT) with the Qwen2.5 1.5B Instruct model, Q4_K_M quantisation (Apache-2.0). It answers in text, in a chat inside the SSB window, on this computer.

The Piper engine runs as a separate program. It bundles espeak-ng, which is GPL-3.0; it is not linked into Decodium.

## Voice keyer (SSB window)

- Eight messages, F1 to F8. Each one has a label, a text, and an optional recording.
- Record: hold the button while speaking, 48 kHz from the microphone, saved at 12 kHz.
- Play sends the message on the SSB transmit path (PTT, audio, release). It only transmits when you press Play. Esc stops it.
- With the Voice pack installed, a message with text but no recording is read aloud.
- Hold to listen transcribes what you say, when the recognition engine is present.

## Assistant

- Type a question and read the answer in the chat. The assistant does not touch the radio.
- "Send last reply by voice" transmits the reply through the SSB path. It is labelled because it goes on air.

## CW

- The CW module starts the first time you open its window. It no longer starts at launch, and the window is not reopened automatically when Decodium starts in CW mode.

## SSB receive

- Software AGC for the listening audio (Off, Auto, Fast, Medium, Slow, Long), from DECODIUM SDR. It works only on the speaker copy, not on the decoder.
- Radio controls read and set by CAT when the radio supports them (AF, squelch, noise reduction and blanker, compressor, VOX, IF shift, notch, preamp, attenuator, ANF, lock).

## Fixes

- Remote audio: a failed monitor output no longer crashes Decodium (see 1.0.683).

## Italiano

Questa release aggiunge alla finestra SSB un kit vocale e assistente locale, opzionale, e fa partire il modulo CW solo quando lo apri.

### Pacchetti opzionali

Ogni pacchetto si installa o si rimuove a parte. Durante l'uso non si va su internet e non serve alcun account. I download sono verificati con SHA256 prima dell'uso.

- **Voce** (circa 150 MB): motore Piper (MIT) con una voce italiana (paola, dataset CC0) e una inglese (amy, CC BY-SA 4.0, dataset mimic3-voices). Per la voce inglese è richiesta l'attribuzione.
- **Riconoscimento vocale** (circa 490 MB): modello whisper.cpp multilingue `small` (MIT). Il motore di riconoscimento non fa ancora parte del download: il pacchetto risulta "motore non ancora disponibile".
- **Assistente** (circa 1,1 GB): llama.cpp (MIT) con il modello Qwen2.5 1.5B Instruct, quantizzazione Q4_K_M (Apache-2.0). Risponde in chat nella finestra SSB, sul tuo computer.

Il motore Piper gira come programma separato. Include espeak-ng, che è GPL-3.0; non è collegato a Decodium.

### Voice keyer (finestra SSB)

- Otto messaggi, da F1 a F8. Ognuno ha un'etichetta, un testo e una registrazione facoltativa.
- Registra: tieni premuto il pulsante mentre parli; l'audio arriva dal microfono a 48 kHz e viene salvato a 12 kHz.
- Play manda il messaggio sul percorso di trasmissione SSB (PTT, audio, rilascio). Trasmette solo quando premi Play. Esc lo ferma.
- Con il pacchetto Voce installato, un messaggio con solo testo viene letto ad alta voce.

### Assistente

- Scrivi una domanda e leggi la risposta in chat. L'assistente non tocca la radio.
- "Invia l'ultima risposta a voce" trasmette la risposta sul percorso SSB; il nome lo dice perché va in aria.

### CW

- Il modulo CW parte la prima volta che apri la sua finestra. Non parte più all'avvio, e la finestra non si riapre da sola quando Decodium si avvia in modo CW.

### Ricezione SSB

- AGC software per l'audio d'ascolto (Off, Auto, Fast, Medium, Slow, Long), da DECODIUM SDR. Lavora solo sulla copia per l'altoparlante, non sul decoder.
- Comandi della radio letti e impostati via CAT quando la radio li supporta.

### Correzioni

- Audio remoto: un'uscita di monitoraggio che fallisce non fa più chiudere Decodium (vedi 1.0.683).

## Downloads

The Windows x64 installer EXE is attached to this release.
