# Decodium 4 v1.0.707

## English (British)

This release is built on v1.0.706 and makes Decodium lighter and faster on modest
computers.

### Faster FT2 decoder

- The FT2 decoder uses about a third of the CPU time per decode of v1.0.706. On a
  240 s test with 80 transmissions the average cost of one decode fell from 687 ms to
  214 ms, and the delay before a decode appears fell from 1.3 s to 0.4 s.
- The decodes are the same: the same messages were decoded, with the same SNR, DT and
  frequency, on two synthetic FT2 scenes and on FT8 recordings, and no false decodes
  appeared.
- FT2 still decodes continuously, so on a fast computer the saving shows up mostly as
  a shorter delay. With **Low CPU mode**, the adaptive decode option or the **Slow PC
  mode** the CPU use drops for real.

### Lighter files

- The Windows installer is about 61 MB instead of 85 MB: libraries that no program
  of the package uses any more are no longer shipped.
- `decodium.exe` is 47 MB instead of 106 MB, so it loads and is scanned faster.
- **Windows 10 version 1809 or later (64-bit) is now required.** Qt 6.11, which
  Decodium is built with, does not run on Windows 7, 8 or older Windows 10.

### New "Light" theme

- Same colours as Ocean Blue, with solid fills instead of translucent ones. While it
  is selected, the effects that the "Low" graphics quality switches off are off too
  (background gradient, pulsing LEDs). The graphics quality you chose is kept and
  comes back when you pick another theme.
- Select it in Settings, Theme.

### Other changes

- The question "Enable Slow PC mode?" at the first start is now asked only on
  computers that look modest (4 logical cores or fewer, less than 8 GB of RAM, or a
  previous start that was slow). The switch stays in Settings for everybody.
- Bug report window: the new **Open diagnostic panel** button opens
  diagnostic.ft2.it with your callsign, the area suggested by the self-check and the
  version already filled in.

### Limits

- The new theme and the Slow PC question have not been compared on very old
  hardware yet.
- The text of the new button and the question wording are English only for now. The
  Slow PC question still says "OpenGL" while Decodium uses Direct3D 11.

### Release contents

- Based on Decodium 4 v1.0.706 (itself built on elisir80/Decodium-4.0-Core-Shannon
  v1.0.705).
- The Windows installer is built from this repository with the fork packaging.

## Italiano

Questa release è costruita sulla v1.0.706 e rende Decodium più leggero e più veloce
sui computer modesti.

### Decoder FT2 più veloce

- Il decoder FT2 usa circa un terzo del tempo di CPU per decodifica rispetto alla
  v1.0.706. In una prova di 240 s con 80 trasmissioni il costo medio di una
  decodifica è passato da 687 ms a 214 ms, e il ritardo prima che una decodifica
  compaia da 1,3 s a 0,4 s.
- Le decodifiche sono le stesse: su due scene FT2 sintetiche e su registrazioni FT8
  sono stati decodificati gli stessi messaggi, con lo stesso SNR, DT e frequenza, e
  non sono comparse decodifiche false.
- FT2 continua a decodificare di continuo, quindi su un computer veloce il
  risparmio si vede soprattutto come ritardo più breve. Con la **modalità Low CPU**,
  l'opzione di decodifica adattiva o la **modalità PC lento** l'uso della CPU cala
  davvero.

### File più leggeri

- L'installer Windows pesa circa 61 MB invece di 85 MB: non vengono più incluse le
  librerie che nessun programma del pacchetto usa.
- `decodium.exe` pesa 47 MB invece di 106 MB, quindi si carica e viene scansionato
  più in fretta.
- **Ora serve Windows 10 versione 1809 o successiva (64 bit).** Qt 6.11, con cui è
  costruito Decodium, non funziona su Windows 7, 8 o Windows 10 più vecchi.

### Nuovo tema "Light"

- Stessi colori di Ocean Blue, con campiture piene al posto di quelle traslucide.
  Finché è selezionato, sono spenti anche gli effetti che la qualità grafica "Low"
  disattiva (sfumatura di sfondo, LED pulsanti). La qualità grafica che avevi scelto
  resta memorizzata e torna quando scegli un altro tema.
- Si seleziona in Impostazioni, Tema.

### Altre modifiche

- La domanda "Attivare la modalità PC lento?" al primo avvio ora viene posta solo sui
  computer che sembrano modesti (4 core logici o meno, meno di 8 GB di RAM, oppure un
  avvio precedente lento). L'interruttore resta nelle Impostazioni per tutti.
- Finestra di segnalazione problemi: il nuovo pulsante **Open diagnostic panel** apre
  diagnostic.ft2.it con nominativo, area suggerita dall'autodiagnosi e versione già
  compilati.

### Limiti

- Il nuovo tema e la domanda sul PC lento non sono ancora stati confrontati su
  hardware molto vecchio.
- Il testo del nuovo pulsante e la formulazione della domanda sono per ora solo in
  inglese. La domanda sul PC lento dice ancora "OpenGL" mentre Decodium usa Direct3D 11.

### Contenuto della release

- Basata su Decodium 4 v1.0.706 (a sua volta costruita su
  elisir80/Decodium-4.0-Core-Shannon v1.0.705).
- L'installer Windows è costruito da questo repository con il pacchetto del fork.
