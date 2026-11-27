# Decodium 1.0.680

This release consolidates the 1.0.679 baseline and adds the latest CW, CAT, audio-device and startup-layout fixes from the main branch.

## English (British)

### Highlights

- Prevented the local serial CW keyer from opening the same COM/TTY already reserved by Hamlib CAT or CAT-controlled PTT. This removes the race that could block the radio connection on Windows and provides a clear setup error instead of silently taking the port.
- Improved CW transmitter setup feedback. TCP endpoints such as `127.0.0.1:4533` are no longer treated as serial ports, and the UI explains that rigctld endpoints belong in CAT settings. Native CAT keying remains available only when Hamlib exposes `send_morse`.
- Kept the CW decoder and its audio path active while the CW window is hidden, with the existing stability filtering and diagnostic telemetry for tone, speed, cost and samples.
- Fixed Linux PulseAudio/PipeWire device selection so a selected Pulse source is applied to the system audio route as well as retained in Decodium's settings. This improves device discovery and selection on Linux Mint LMDE and similar environments.
- Fixed startup panel reparenting so the Live Map waits for real SplitView geometry before being placed. This prevents the map from appearing temporarily in the upper-left corner on slower Windows and macOS window restores.
- Preserved the SSB/CAT improvements from 1.0.679, including AGC handling, CAT readback diagnostics, mode synchronisation and the platform-aware monospace font fallback that removes the missing-font startup warning.
- Added focused CW-controller coverage for CAT-port reservation and the related local-keyer safety path.

The release publishes the source archive and Windows x64 executable through the release workflow. Dedicated GitHub Actions runners publish macOS Intel and Apple Silicon DMGs and Linux x86_64 and aarch64 AppImages to this release.

## Italiano

Questa release consolida la base 1.0.679 e aggiunge le correzioni più recenti del main per CW, CAT, periferiche audio e layout iniziale.

### Novità principali

- Impedito al keyer CW seriale locale di aprire la stessa COM/TTY già riservata da Hamlib CAT o dal PTT controllato via CAT. Questo elimina la condizione di gara che poteva bloccare la connessione alla radio su Windows e mostra un errore chiaro invece di occupare silenziosamente la porta.
- Migliorati i messaggi di configurazione del trasmettitore CW. Gli endpoint TCP come `127.0.0.1:4533` non vengono più trattati come porte seriali e l'interfaccia indica che gli indirizzi rigctld appartengono alle impostazioni CAT. Il keying CAT nativo resta disponibile solo quando Hamlib espone `send_morse`.
- Il decoder CW e il relativo percorso audio restano attivi anche quando la finestra CW viene nascosta, mantenendo il filtro di stabilità e la telemetria diagnostica di tono, velocità, costo e campioni.
- Corretto il percorso delle periferiche Linux PulseAudio/PipeWire: la sorgente Pulse selezionata viene applicata anche alla route audio di sistema oltre a essere salvata nelle impostazioni di Decodium. Questo migliora rilevamento e selezione su Linux Mint LMDE e sistemi simili.
- Corretto il riposizionamento iniziale dei pannelli: la Live Map attende la geometria reale degli SplitView prima di essere inserita. Questo evita che la mappa appaia temporaneamente nell'angolo superiore sinistro durante il ripristino lento della finestra su Windows e macOS.
- Mantenuti i miglioramenti SSB/CAT della 1.0.679, inclusi gestione AGC, diagnostica del readback CAT, sincronizzazione del modo e fallback del font monospace basato sulla piattaforma che elimina il warning di font mancante all'avvio.
- Aggiunta una copertura di test mirata per la prenotazione della porta CAT e per il relativo percorso di sicurezza del keyer locale.

La release pubblica l'archivio del codice e l'eseguibile Windows x64 tramite il workflow di release. Runner GitHub Actions dedicati pubblicano nella stessa release i DMG per macOS Intel e Apple Silicon e gli AppImage Linux per x86_64 e aarch64.
