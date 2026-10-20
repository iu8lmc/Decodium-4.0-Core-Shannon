# Decodium 4 v1.0.704

## English (British)

This release is built on v1.0.703 and adds a clear warning about a limit of the
radio CAT keyer in the CW window.

### CW window: Stop and the radio CAT keyer

- When CW is sent through the radio's own CAT keyer, **Stop** releases the PTT and
  discards any text Decodium has not sent yet, but some radios have no command to
  empty their keyer. The Yaesu FT-991 is one of them: the text already handed to
  the radio is finished, even after Stop.
- The CW window now says so, next to the Stop button, whenever the radio CAT keyer
  is the active backend. The message suggests choosing a serial keyer in Setup
  for an immediate stop, because the serial keyer is driven by Decodium itself.
- With a local radio in CW mode, the "audio tone" choice is replaced by the radio's
  CAT keyer when the radio provides one. The status line next to Stop now shows
  "radio CAT keyer (audio tone selected)" in that case, and the Setup text explains
  the substitution.
- The behaviour of Stop itself is unchanged.

### Release contents

- Based on v1.0.703 (Stellar Light refresh, brightness and contrast sliders) and,
  through it, on elisir80/Decodium-4.0-Core-Shannon v1.0.702.
- The Windows installer is built from this repository with the fork packaging.

## Italiano

Questa release è costruita sulla v1.0.703 e aggiunge nella finestra CW un avviso
chiaro su un limite del keyer CAT della radio.

### Finestra CW: Stop e keyer CAT della radio

- Quando il CW passa dal keyer CAT della radio, **Stop** rilascia il PTT e scarta
  il testo che Decodium non ha ancora inviato, ma alcune radio non hanno un
  comando per svuotare il loro keyer. Lo Yaesu FT-991 è una di queste: il testo
  già consegnato alla radio viene terminato, anche dopo Stop.
- La finestra CW ora lo dice, accanto al pulsante Stop, ogni volta che il backend
  attivo è il keyer CAT della radio. Il messaggio consiglia di scegliere un serial
  keyer in Setup per un arresto immediato, perché il serial keyer è comandato
  direttamente da Decodium.
- Con una radio locale in modalità CW, la scelta "audio tone" viene sostituita dal
  keyer CAT della radio quando questa lo mette a disposizione. La riga di stato
  accanto a Stop mostra in quel caso "radio CAT keyer (audio tone selected)" e il
  testo di Setup spiega la sostituzione.
- Il comportamento di Stop non cambia.

### Contenuto della release

- Basata sulla v1.0.703 (tema Stellar Light rinnovato, cursori di luminosità e
  contrasto) e, tramite essa, su elisir80/Decodium-4.0-Core-Shannon v1.0.702.
- L'installer Windows è costruito da questo repository con il pacchetto del fork.
