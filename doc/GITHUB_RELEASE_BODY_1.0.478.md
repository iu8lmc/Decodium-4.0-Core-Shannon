# Decodium 4 FT2 1.0.478

## English

Release highlights (`1.0.477 -> 1.0.478`):

- FT2-Link operation layer:
- added guided QSY support with slot sniffing and busy checks before moving traffic.
- expanded capability beacons so stations can advertise supported profiles, robustness levels and operating features.
- added richer station/session capability display for connected and discovered peers.
- added path verification improvements and stronger relay visibility on receive.

- FT2-Link mailbox, relay and file workflows:
- added mailbox, parking and relay center controls for holding and forwarding traffic.
- added inquiry/peek and privacy controls for station information, mailbox state and remote visibility.
- added real digipeater handling and multi-hop path testing support.
- added BBS file server support with list/get requests, text and binary shared files, request counters and persistence.
- improved received file management with RXF read/unread state, save, copy, delete and clear-read actions.

- FT2-Link UI and layout:
- moved the FT2-Link tool tabs into the bottom control area and cleaned up buttons that do not apply to FT2-Link.
- added a reusable responsive FT2-Link tab bar so controls wrap cleanly on narrow screens without wasting space on wide monitors.
- fixed truncation in INFO, BBS, BCAST, MAIL, CALL, STAT and RXF panels by improving the scrolling/content sizing model.
- replaced the dedicated FT2-Link close button with dock/pop behavior; changing mode remains the way to leave FT2-Link.

- Waveform, diagnostics and lab tools:
- improved W2300/WEAK/DEEP/ULTRA acquisition, CFO/drift handling and robustness testing paths.
- added more no-CAT lab command-line controls for two-instance BlackHole loopback testing.
- added RX/TX diagnostics for audio ingestion, waveform paths and FT2-Link decoder visibility.
- kept UI/panadapter load lower during FT2-Link idle and high-message decode periods.

- WSPR:
- added WSPR TX power selection in dBm/watts and encoded the selected dBm into the transmitted message.
- improved WSPR decode/reporting behavior and distance/metadata display.
- made the WSPR power guard best-effort: it only blocks TX when PWR telemetry is enabled, visible and actually available.

- CAT/radio:
- added backend radio profiles so multiple CAT backends and rigs can be saved, loaded, edited and deleted.
- preserved the selected backend per radio profile and restored the last used profile at startup.
- hardened Hamlib/Icom frequency polling, rejected-band handling, reconnect behavior and shutdown/PTT cleanup.

- Standard FT2/FT4/FT8 usability:
- improved Signal RX/Full Spectrum list performance when many decodes arrive in the same slot.
- reduced UI stalls while scrolling large decode lists.
- continued macOS/CoreAudio TX payload hardening for repeated standard FT2 transmissions.

Validation performed locally:

- `cmake --build "/Users/salvo/Desktop/Decodium4-build" --parallel 4 --target decodium_qml`
- `"/Users/salvo/Desktop/Decodium4-build/tests/test_ft2link_qml_adapter"`: 66 passed, 0 failed.
- two-application FT2-Link loopback testing over crossed BlackHole 2ch/16ch devices without CAT.
- BBS file server loopback: TESTA requested list/file from TESTB, TESTB served the file and TESTA received the payload.
- short runtime/QML launch check after INFO panel scrolling fixes.

Release assets expected from GitHub Actions:

- `Decodium_1.0.478_Setup_x64.exe`
- `decodium4-ft2-1.0.478-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.478-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.478-macos-ventura-x86_64.dmg`
- `decodium4-ft2-1.0.478-macos-sonoma-x86_64.dmg`
- `decodium4-ft2-1.0.478-macos-sequoia-x86_64.dmg`
- `decodium4-ft2-1.0.478-linux-x86_64.AppImage`
- `decodium4-ft2-1.0.478-linux-aarch64.AppImage`
- matching `.zip` and checksum files where produced by the platform workflow.

## Italiano

Punti principali (`1.0.477 -> 1.0.478`):

- Livello operativo FT2-Link:
- aggiunto QSY guidato con controllo slot e busy check prima di spostare il traffico.
- estesi i beacon con capability flags per dichiarare profili, livelli robusti e funzioni operative supportate.
- migliorata la visualizzazione delle capability nelle stazioni scoperte e nelle sessioni connesse.
- migliorata la verificabilita' dei path e la visibilita' dei relay in ricezione.

- Mailbox, relay e file workflow FT2-Link:
- aggiunto centro mailbox, parking e relay per tenere e inoltrare traffico.
- aggiunti controlli inquiry/peek e privacy per informazioni stazione, stato mailbox e visibilita' remota.
- aggiunta gestione digipeater reale e supporto ai test multi-hop.
- aggiunto BBS file server con richieste list/get, file condivisi testuali e binari, contatori richieste e persistenza.
- migliorata la gestione dei file ricevuti con stato letto/non letto, save, copy, delete e clear-read.

- UI e layout FT2-Link:
- spostati i tab operativi FT2-Link nella barra in basso e rimossi controlli non utili in FT2-Link.
- aggiunta una tab bar FT2-Link responsive, con wrapping pulito su schermi stretti e senza spreco di spazio sui monitor larghi.
- corretti troncamenti nei pannelli INFO, BBS, BCAST, MAIL, CALL, STAT e RXF migliorando scrolling e sizing dei contenuti.
- sostituito il pulsante di chiusura FT2-Link con comportamento dock/pop; per uscire da FT2-Link basta cambiare modo.

- Waveform, diagnostica e strumenti lab:
- migliorata acquisizione W2300/WEAK/DEEP/ULTRA, gestione CFO/drift e percorsi di test robustezza.
- aggiunti controlli command-line no-CAT per test loopback BlackHole con due istanze.
- aggiunta diagnostica RX/TX per ingest audio, waveform e visibilita' del decoder FT2-Link.
- ridotto il carico UI/panadapter in idle FT2-Link e durante slot con molti messaggi.

- WSPR:
- aggiunta selezione potenza TX WSPR in dBm/watt e codifica del dBm scelto nel messaggio trasmesso.
- migliorati decode/reporting WSPR e visualizzazione di distanza/metadati.
- resa best-effort la protezione software sul wattaggio: blocca la TX solo quando la telemetria PWR e' abilitata, visibile e disponibile.

- CAT/radio:
- aggiunti profili backend radio per salvare, caricare, modificare e cancellare configurazioni CAT diverse.
- preservato il backend selezionato per ogni profilo radio e ripristinato l'ultimo profilo usato all'avvio.
- irrobustiti polling Hamlib/Icom, gestione bande rifiutate, riconnessione e cleanup PTT in chiusura.

- Usabilita' FT2/FT4/FT8 standard:
- migliorate prestazioni delle liste Signal RX/Full Spectrum quando arrivano molti decode nello stesso slot.
- ridotti gli stalli UI durante lo scrolling di liste decode grandi.
- continuato l'hardening macOS/CoreAudio sul payload TX nelle trasmissioni FT2 standard ripetute.

Validazione locale eseguita:

- `cmake --build "/Users/salvo/Desktop/Decodium4-build" --parallel 4 --target decodium_qml`
- `"/Users/salvo/Desktop/Decodium4-build/tests/test_ft2link_qml_adapter"`: 66 passati, 0 falliti.
- test FT2-Link con due applicazioni reali su BlackHole 2ch/16ch incrociati, senza CAT.
- test BBS file server: TESTA ha richiesto lista/file da TESTB, TESTB ha servito il file e TESTA ha ricevuto il payload.
- controllo runtime/QML breve dopo i fix di scrolling del pannello INFO.

Asset attesi dai runner GitHub Actions:

- `Decodium_1.0.478_Setup_x64.exe`
- `decodium4-ft2-1.0.478-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.478-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.478-macos-ventura-x86_64.dmg`
- `decodium4-ft2-1.0.478-macos-sonoma-x86_64.dmg`
- `decodium4-ft2-1.0.478-macos-sequoia-x86_64.dmg`
- `decodium4-ft2-1.0.478-linux-x86_64.AppImage`
- `decodium4-ft2-1.0.478-linux-aarch64.AppImage`
- relativi `.zip` e checksum dove prodotti dal workflow della piattaforma.
