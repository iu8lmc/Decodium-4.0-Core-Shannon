## English (UK)

### Decodium 1.0.651

- Improved audio capture stability during startup and mode changes, using a consistent 8,192-frame macOS capture buffer.
- Added monitoring startup grace so FT8/FT4 does not show a transient `monitoring off` state while the audio backend is being armed.
- Hardened JTTY to FT8/FT4 hand-off: clean capture restart, transition-aware watchdog suppression, and a longer JTTY recovery window.
- Preserved final received-sample validation while avoiding false watchdog recovery during legitimate audio reconfiguration.
- Improved the JTTY keyboard transmit field with readable dark-theme text, alignment, padding, selection colours, and a non-overlapping placeholder.
- Updated application and packaging metadata to 1.0.651.

Local validation: `decodium_qml` builds successfully on macOS arm64; settings-profile and UDP client-ID tests pass.

## Italiano

### Decodium 1.0.651

- Migliorata la stabilità della cattura audio all’avvio e durante i cambi modo, con buffer macOS coerente da 8.192 frame.
- Aggiunta una tolleranza all’avvio del monitoraggio per evitare il falso stato `monitoring off` durante l’attivazione del backend audio FT8/FT4.
- Rafforzato il passaggio JTTY verso FT8/FT4: riavvio pulito della cattura, watchdog sospeso durante la transizione e finestra di recupero JTTY più lunga.
- Mantenuto il controllo finale sui campioni ricevuti, evitando recuperi watchdog durante una riconfigurazione audio legittima.
- Migliorato il campo di trasmissione della tastiera JTTY con testo leggibile, allineamento, spaziatura, colori di selezione e placeholder senza sovrapposizioni.
- Aggiornati i metadati dell’applicazione e del packaging a 1.0.651.

Verifica locale: compilazione `decodium_qml` riuscita su macOS arm64; superati i test del profilo impostazioni e dell’identificativo client UDP.
