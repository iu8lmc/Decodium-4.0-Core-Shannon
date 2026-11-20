# Decodium 1.0.678

## English

This release fixes local CW transmission on radios whose native Hamlib CAT keyer requires an explicit PTT command, including the Yaesu FT-991A.

- Raises CAT PTT before sending CW text to the radio keyer.
- Releases PTT when the message finishes or when Stop is pressed.
- Keeps the radio in CW mode and does not route CW through USB-DATA audio.

## Italiano

Questa versione corregge la trasmissione CW locale sulle radio il cui keyer CAT Hamlib richiede l’attivazione esplicita del PTT, tra cui Yaesu FT-991A.

- Attiva il PTT CAT prima di inviare il testo CW al manipolatore della radio.
- Rilascia il PTT alla fine del messaggio o premendo Stop.
- Mantiene la radio in modo CW senza instradare il CW attraverso l’audio USB-DATA.
