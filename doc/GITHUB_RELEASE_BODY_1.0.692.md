# Decodium 4 v1.0.692

## English (British)

Decodium 4 v1.0.692 is the first release that includes both the portable startup fixes of 1.0.691 and the Power On/Off and DecoPort work of the 1.0.690 branch. It adds a live port hub panel to the DecoPort window.

### DecoPort port hub panel

- New PORT HUB section at the top of the DecoPort window. Each port has a LED: red when it is in use, green when it is free, grey when it cannot be checked.
- TCP and UDP ports show the real owner: program name, process ID and state. Decodium's own ports are marked as such.
- Serial ports (real and com0com virtual) show the program holding a handle on the device. Serial ports are never opened to check them, because opening a radio port can move its DTR/RTS lines.
- Limitations: a program running with elevated rights cannot be inspected, so a port it holds may appear free.
- The panel is computed in a background thread and refreshed every 10 seconds while the window is visible.

### Port conflicts

- The rotor HTTP server defaults to port 8082. It used to default to 8080, the same port as the Decodium web server.

### Power On and Power Off (from 1.0.690)

- Power On is enabled again when a direct USB or serial CAT port is configured.
- Power On after a Power Off reopens the CAT session so the radio receives its wake-up sequence.
- The CAT port is released cleanly after a Power Off, so the next Power On no longer fails with "serial port is already open".
- If Windows gives the radio a new COM number, Decodium follows the radio by its device identity when there is exactly one match.

### Notes

- Verified on a Yaesu FT-991 over direct USB: Power Off followed by Power On, without restarting Decodium.
- Use a dummy load for the first transmissions of the voice keyer, as in 1.0.686.

## Italiano

Decodium 4 v1.0.692 è la prima release che include insieme le correzioni di avvio portatile della 1.0.691 e il lavoro su Power On/Off e DecoPort del ramo 1.0.690. Aggiunge alla finestra DecoPort un pannello con lo stato reale delle porte.

### Pannello hub porte DecoPort

- Nuova sezione PORT HUB in cima alla finestra DecoPort. Ogni porta ha un LED: rosso se è occupata, verde se è libera, grigio se non si può verificare.
- Le porte TCP e UDP mostrano il proprietario reale: nome del programma, ID del processo e stato. Le porte di Decodium sono indicate come tali.
- Le porte seriali (reali e virtuali com0com) mostrano il programma che tiene un handle sul dispositivo. Le porte seriali non vengono mai aperte per controllarle, perché aprire una porta della radio può muovere le linee DTR/RTS.
- Limiti: un programma eseguito con privilegi elevati non può essere ispezionato, quindi una porta da lui occupata può risultare libera.
- Il pannello viene calcolato in un thread di lavoro e si aggiorna ogni 10 secondi mentre la finestra è visibile.

### Conflitti di porta

- Il server HTTP del rotore usa di default la porta 8082. Prima usava la 8080, la stessa del web server di Decodium.

### Power On e Power Off (dalla 1.0.690)

- Il Power On è di nuovo attivo quando è configurata una porta CAT diretta USB o seriale.
- Il Power On dopo un Power Off riapre la sessione CAT, così la radio riceve la sequenza di risveglio.
- La porta CAT viene rilasciata correttamente dopo un Power Off, quindi il Power On successivo non fallisce più con "porta seriale già aperta".
- Se Windows assegna alla radio un nuovo numero COM, Decodium segue la radio tramite la sua identità di dispositivo, quando c'è una sola corrispondenza.

### Note

- Verificata con una Yaesu FT-991 via USB diretto: Power Off seguito da Power On, senza riavviare Decodium.
- Per le prime trasmissioni del voice keyer usa un carico fittizio, come nella 1.0.686.

## Downloads

The Windows x64 installer EXE is attached to this release.
