# Decodium 4 v1.0.670

## Italiano

Questa release aggiunge il modulo CW e il rotore d'antenna e completa l'ascolto della radio remota sul PC dell'operatore.

### Audio RX della radio remota

- Attivando **Usa la radio remota**, l'audio ricevuto dalla radio viene riprodotto nelle cuffie o negli altoparlanti locali, anche in CW.
- Aprendo la finestra CW con la radio remota attiva si accende l'ascolto RX.
- Nella finestra CW sono disponibili **Audio radio RX**, **Volume RX** e la scelta dell'uscita audio. Il volume di ascolto non modifica i campioni inviati al decoder.
- Anche la finestra Decolink offre il comando per attivare o silenziare l'ascolto RX.

### Modulo CW

- Decoder CW con grafico del segnale, macro modificabili F1–F12, testo libero, velocità da 5 a 60 WPM e registrazione QSO in modo CW.
- Trasmissione tramite tono audio, manipolatore seriale DTR/RTS o K1EL WinKeyer; manipolazione remota via Decolink con gateway compatibile.
- **Nota TX** è un controllo distinto dall'audio RX: permette di ascoltare localmente il messaggio che si sta trasmettendo, con volume, uscita audio e prova locale senza PTT.
- Il sidetone TX è generato localmente e non costituisce un ritorno RF dalla radio.

### Rotore d'antenna

- Modulo PRO.SIS.TEL derivato da DecoRotor: azimut, memorie, puntamento da locatore e mappa delle stazioni ricevute.
- Modulo disattivato per impostazione predefinita, con simulatore e servizi di rete configurabili separatamente.

### Pacchetto Windows x64

- Installer completo e archivio ZIP con Qt, dipendenze audio, moduli QML, traduzioni, mappe, suoni e strumenti ausiliari, incluso Decodium RX.
- Completati gli import QML necessari agli stili Qt e incluse le dipendenze multimediali richieste nell'installer.
- Compilazione Release riuscita, sei test CW superati, avvio/chiusura del bundle verificati in modalità di test, 270 binari controllati senza dipendenze DLL mancanti e integrità ZIP/SHA-256 verificata.
- La prova con una radio remota reale non è stata eseguita durante questa verifica.

Scaricare **Decodium_1.0.670_Setup_x64.exe** per l'installazione oppure **Decodium_1.0.670_Windows_x64.zip** per la cartella completa. I checksum sono in **SHA256SUMS.txt**.

---

## English

This release adds the CW module and antenna rotator, and enables listening to received remote-radio audio on the operator's computer.

### Remote radio RX audio

- **Use the remote radio** now enables local playback of the received radio audio, including CW signals.
- Opening the CW window with a remote radio active enables RX listening.
- The CW window provides **Radio RX audio**, **RX volume** and output-device selection. Listening volume does not alter samples sent to the decoder.
- The Decolink window also provides an RX listening toggle.

### CW module and antenna rotator

- CW decoder with a live signal display, editable F1–F12 macros, free text, 5–60 WPM and CW QSO logging.
- Audio-tone, serial DTR/RTS and K1EL WinKeyer transmit paths; Decolink remote key timing with a compatible gateway.
- **TX sidetone** is separate from received RX audio. It offers local volume, output selection and a tone test without PTT. It is locally generated, not returned RF audio.
- PRO.SIS.TEL antenna rotator module derived from DecoRotor, with heading, presets, locator bearings, station map and simulator. Disabled by default; network services are separately configurable.

### Windows x64 downloads and verification

The installer and complete ZIP include Qt, multimedia dependencies, QML modules, translations, maps, sounds and auxiliary tools including Decodium RX. Qt style imports and multimedia packaging dependencies have been completed.

Release build and all six CW tests passed. Bundle startup/shutdown was checked in test mode; 270 binaries were checked without unresolved DLL dependencies. ZIP integrity and SHA-256 checks passed. A real remote-radio/on-air test was not performed during this verification.

Use **Decodium_1.0.670_Setup_x64.exe** to install, or **Decodium_1.0.670_Windows_x64.zip** for the complete application folder. See **SHA256SUMS.txt** for checksums.
