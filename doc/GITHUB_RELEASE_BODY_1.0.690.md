# Decodium 4 v1.0.690

This release fixes the Power On and Power Off buttons for a radio connected by direct USB or serial CAT. It is based on 1.0.689.

## Power On and Power Off

- **Power On was always disabled.** The button checks were written with a method that does not exist in JavaScript (`toLower()` instead of `toLowerCase()`), so the button stayed grey whenever a CAT serial port was configured. Fixed in the dashboard and in the SSB window.
- **Power On after a Power Off.** A radio switched off through CAT keeps the CAT session open, and the Power On frame sent over that live session does not wake it. Power On now closes the session and reopens it, so the radio receives its wake-up sequence.
- **No stuck COM port.** Shutting the session down after a Power Off no longer terminates the CAT thread before the serial port is closed. The next Power On no longer fails with "serial port is already open".
- **No PTT retries on a radio that is off.** When the radio was switched off through CAT, the shutdown skips the PTT-off retries, so the session closes within the normal stop window.
- **Failed open releases the port.** If the radio does not answer when the connection opens, the serial port is released, so the next attempt can open it.
- **Port follows the radio.** When Windows gives the radio a different COM number after a power cycle, Decodium finds the single available port with the same device identity and uses it, logging the change. If there is no single match, it does not guess.

## Notes

- Verified on a Yaesu FT-991 over direct USB with Power Off followed by Power On, without restarting Decodium.
- Use a dummy load for the first transmissions of the voice keyer, as in 1.0.686.

## Italiano

Questa release corregge i pulsanti Power on e Power off per una radio collegata via CAT diretto USB o seriale. Parte dalla 1.0.689.

### Power on e Power off

- **Power on era sempre disattivato.** Il controllo dei pulsanti usava un metodo che in JavaScript non esiste (`toLower()` invece di `toLowerCase()`): il pulsante restava grigio con una porta seriale configurata. Corretto nel pannello e nella finestra SSB.
- **Power on dopo un Power off.** Una radio spenta via CAT mantiene aperta la sessione CAT, e il comando di accensione inviato su quella sessione non la sveglia. Ora il Power on chiude la sessione e la riapre, così la radio riceve la sequenza di risveglio.
- **Porta COM non più bloccata.** Chiudendo la sessione dopo un Power off, il thread CAT non viene più terminato prima che la porta sia chiusa. Il Power on successivo non fallisce più con "porta seriale già aperta".
- **Niente tentativi di PTT su una radio spenta.** Se la radio è stata spenta via CAT, la chiusura salta i tentativi di PTT-off, quindi la sessione si chiude nella finestra di stop normale.
- **Apertura fallita: la porta viene rilasciata.** Se la radio non risponde all'apertura, la porta seriale viene liberata, così il tentativo successivo può aprirla.
- **La porta segue la radio.** Quando Windows assegna alla radio un altro numero COM dopo un ciclo di alimentazione, Decodium trova l'unica porta disponibile con la stessa identità del dispositivo e la usa, registrando il cambio. Se non c'è un'unica corrispondenza, non indovina.

### Note

- Verificata su una Yaesu FT-991 via USB diretto: Power off seguito da Power on, senza riavviare Decodium.
- Per le prime trasmissioni del voice keyer usa un carico fittizio, come nella 1.0.686.

## Downloads

The Windows x64 installer EXE is attached to this release.
