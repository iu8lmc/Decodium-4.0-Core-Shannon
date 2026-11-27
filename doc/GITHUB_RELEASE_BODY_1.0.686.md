# Decodium 4 v1.0.686

This release rebuilds the SSB window and keeps it responsive while the local voice is being generated. It is based on 1.0.685 and includes the optional voice and assistant packs introduced in 1.0.684.

## SSB window

- **Resizable two-column layout.** The voice keyer and the assistant are on the left, the optional packs, the PC microphone and the radio CAT controls on the right. A draggable divider sets the width of each column, and the radio frequency, mode and CAT status share a single compact row.
- **Compact message cards.** Each of the eight messages (F1 to F8) is a card with a label, a language, the text, and Record, Preview, AI help, Clear and Play buttons.
- **Translucent assistant panel**, with the conversation always readable.
- **Responsive buttons.** The voice is synthesised in a separate process, with an activity indicator, so the window no longer freezes while a message is being prepared.
- **Preview.** Plays a message on the speakers of this computer only. It never transmits.
- **AI help.** Press it on a message to have the local assistant rewrite the text so that it reads well on the air. The reply replaces the text of that slot.
- **Voice quality for the radio.** Adjustable speed (0.6x to 1.6x), a pause between sentences, normalised level, short fades and a few hundred milliseconds of leading silence so that the transmitter does not cut the first syllable.

## Optional packs

Voice, Speech recognition and Assistant are installed and removed separately, and can be switched off without deleting them. Nothing goes to the internet while they are used, and no account is needed. See the 1.0.684 notes for sizes and licences.

## Notes

- Not yet verified on the air with a real radio: use a dummy load for the first transmissions of the voice keyer.
- The speech recognition engine is downloaded from the `packs-whisper-1` release of this repository.

## Italiano

Questa versione rifà la finestra SSB e la tiene reattiva mentre la voce locale viene generata. Parte dalla 1.0.685 e include i pacchetti opzionali di voce e assistente introdotti nella 1.0.684.

### Finestra SSB

- **Layout a due colonne ridimensionabili.** A sinistra voice keyer e assistente; a destra pacchetti opzionali, microfono del PC e controlli CAT della radio. Un divisore trascinabile regola la larghezza delle colonne; frequenza, modo e stato CAT stanno su una sola riga.
- **Schede compatte.** Ognuno degli otto messaggi (da F1 a F8) è una scheda con etichetta, lingua, testo e i pulsanti Record, Preview, AI help, Clear e Play.
- **Pannello dell'assistente trasparente**, con la conversazione sempre leggibile.
- **Pulsanti reattivi.** La voce viene sintetizzata in un processo separato, con indicatore di attività: la finestra non si blocca più mentre un messaggio viene preparato.
- **Preview.** Riproduce un messaggio solo sugli altoparlanti di questo computer. Non trasmette mai.
- **AI help.** Premilo su un messaggio per far riscrivere il testo dall'assistente locale, in modo che si legga bene in radio. La risposta sostituisce il testo dello slot.
- **Qualità della voce per la radio.** Velocità regolabile (da 0,6x a 1,6x), pausa tra le frasi, livello normalizzato, brevi dissolvenze e qualche centinaio di millisecondi di silenzio iniziale, perché il trasmettitore non tagli la prima sillaba.

### Pacchetti opzionali

Voce, Riconoscimento vocale e Assistente si installano e si rimuovono a parte, e si possono spegnere senza cancellarli. Durante l'uso non si va su internet e non serve alcun account. Dimensioni e licenze sono nelle note della 1.0.684.

### Note

- Non ancora verificato in aria con una radio vera: per le prime trasmissioni del voice keyer usa un carico fittizio.
- Il motore di riconoscimento vocale si scarica dalla release `packs-whisper-1` di questo repository.

## Downloads

The Windows x64 installer EXE is attached to this release.
