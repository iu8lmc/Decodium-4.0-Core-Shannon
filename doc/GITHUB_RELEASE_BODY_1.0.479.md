# Decodium 4 FT2 1.0.479

## English

Release highlights (`1.0.478 -> 1.0.479`):

- WSPR and weak-signal operation:
- completed WSPR dBm/watt handling so the selected power is encoded into the transmitted WSPR message.
- kept the WSPR high-power software guard best-effort: it only blocks TX when CAT power telemetry is enabled, visible and available.
- improved WSPR/Q65 period handling in the RX/TX progress bar so long-period modes use the configured timing instead of the standard 15-second FT cadence.
- added Q65 bridge-generated TX support, including Q65 message validation, waveform generation and slot alignment for Q65/Q65-* labels.
- added Q65 ADIF submode handling for Q65-* variants.

- Q65 decoder and test tooling:
- reworked the native Q65 spectral path to use a thread-local FFT workspace, reducing repeated allocation/planning during decode.
- allowed Q65 primary decode, AP/list decode, averaging and drift recovery stages to continue in a controlled order instead of returning too early after the first stage.
- extended Q65 test/simulation utilities and made optional utility targets/tests conditional when their source files are not present.

- FT2-Link RF/channel behavior:
- separated decoder-observability busy state from real listen-before-transmit busy state.
- raised the LBT-only busy threshold so visible/noisy idle audio no longer blocks CQ/TX with false "channel busy" conditions.
- updated FT2-Link UI status and TX queue logic to use the new LBT busy state while preserving sensitive RX diagnostics.
- added regression coverage for live audio that is visible to the decoder but should not block LBT.

- FT2/FT4/FT8 QSO completion:
- added signoff-watch diagnostics around final 73/RR73 reception so active-partner final decodes can be traced when they are filtered or missed.
- hardened AutoSeq completion after our final signoff when the partner immediately returns to CQ, preventing repeated RR73 loops and preserving autolog/cooldown state.

- UI and operator controls:
- restored `W&P` as a real Wait & Pounce toolbar toggle instead of a passive badge that only appeared after the feature was already active.
- fixed English labels in Setup / Display, including process priority, full screen and CPU pressure text.
- widened the Setup / Display label column and Force Update buttons to prevent text truncation and overlap.
- updated the full-screen overlay text to English.

- macOS/CoreAudio and release/build stability:
- hardened parked CoreAudio TX sink handling by retaining retired sinks detached from QObject ownership, avoiding Qt/CoreAudio destruction crashes on repeated TX/restart paths.
- made optional utility install targets robust when some helper binaries are not built.
- kept the build/test surface compatible with local and GitHub runner configurations.

Validation performed locally:

- `cmake --build "/Users/salvo/Desktop/Decodium4-build" --parallel 4 --target decodium_qml`
- `git diff --check` on the modified tracked source set.

Release assets expected from GitHub Actions:

- `Decodium_1.0.479_Setup_x64.exe`
- `decodium4-ft2-1.0.479-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.479-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.479-macos-ventura-x86_64.dmg`
- `decodium4-ft2-1.0.479-macos-sonoma-x86_64.dmg`
- `decodium4-ft2-1.0.479-macos-sequoia-x86_64.dmg`
- `decodium4-ft2-1.0.479-linux-x86_64.AppImage`
- `decodium4-ft2-1.0.479-linux-aarch64.AppImage`
- matching `.zip` and checksum files where produced by the platform workflow.

## Italiano

Punti principali (`1.0.478 -> 1.0.479`):

- WSPR e operativita' weak-signal:
- completata la gestione potenza WSPR in dBm/watt: il dBm selezionato viene codificato nel messaggio WSPR trasmesso.
- mantenuta best-effort la protezione software WSPR sulla potenza: blocca la TX solo quando la telemetria CAT PWR e' abilitata, visibile e disponibile.
- migliorata la gestione dei periodi WSPR/Q65 nella barra RX/TX, usando il timing configurato invece della cadenza FT standard da 15 secondi.
- aggiunto supporto TX Q65 generato dal bridge, con validazione messaggi, generazione waveform e allineamento slot per Q65/Q65-*.
- aggiunta gestione ADIF dei submode Q65 per le varianti Q65-*.

- Decoder Q65 e strumenti test:
- rivisto il percorso spettrale Q65 nativo con workspace FFT thread-local, riducendo allocazioni e pianificazione FFT ripetute durante il decode.
- permesso alle fasi Q65 primary, AP/list, averaging e drift recovery di proseguire in ordine controllato senza uscire troppo presto dopo il primo stadio.
- estesi test/utility Q65 e resi condizionali target e smoke test opzionali quando i sorgenti helper non sono presenti.

- Comportamento RF/canale FT2-Link:
- separato lo stato busy usato per osservabilita' decoder dallo stato busy reale usato per listen-before-transmit.
- alzata la soglia busy solo per LBT, evitando che audio idle visibile o rumore leggero blocchi CQ/TX con falsi "channel busy".
- aggiornata la UI FT2-Link e la coda TX per usare il nuovo stato LBT busy, mantenendo diagnostica RX sensibile.
- aggiunta copertura regressione per audio live visibile al decoder ma non sufficiente a bloccare LBT.

- Chiusura QSO FT2/FT4/FT8:
- aggiunta diagnostica signoff-watch intorno alla ricezione finale 73/RR73, per tracciare eventuali finali del partner filtrati o mancati.
- irrobustita la chiusura AutoSeq dopo il nostro signoff finale quando il partner torna subito in CQ, evitando loop RR73 e preservando autolog/cooldown.

- UI e controlli operatore:
- ripristinato `W&P` come vero pulsante Wait & Pounce nella toolbar, non piu' come badge passivo visibile solo quando la funzione era gia' attiva.
- corretti testi inglesi in Setup / Display, inclusi process priority, full screen e CPU pressure.
- allargate colonne/etichette e pulsanti Force Update in Setup / Display per evitare troncamenti e sovrapposizioni.
- aggiornato in inglese anche l'overlay fullscreen.

- Stabilita' macOS/CoreAudio e build/release:
- irrobustita la gestione dei CoreAudio TX sink parcheggiati, trattenendo i sink ritirati fuori dalla ownership QObject per evitare crash Qt/CoreAudio nei percorsi TX/restart ripetuti.
- resi robusti gli install target opzionali quando alcuni binari helper non vengono compilati.
- mantenuta compatibilita' del perimetro build/test con configurazioni locali e runner GitHub.

Validazione locale eseguita:

- `cmake --build "/Users/salvo/Desktop/Decodium4-build" --parallel 4 --target decodium_qml`
- `git diff --check` sul set sorgente tracciato modificato.

Asset attesi dai runner GitHub Actions:

- `Decodium_1.0.479_Setup_x64.exe`
- `decodium4-ft2-1.0.479-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.479-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.479-macos-ventura-x86_64.dmg`
- `decodium4-ft2-1.0.479-macos-sonoma-x86_64.dmg`
- `decodium4-ft2-1.0.479-macos-sequoia-x86_64.dmg`
- `decodium4-ft2-1.0.479-linux-x86_64.AppImage`
- `decodium4-ft2-1.0.479-linux-aarch64.AppImage`
- relativi `.zip` e checksum dove prodotti dal workflow della piattaforma.
