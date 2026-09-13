# Decodium 4 FT2 1.0.475

## English

Release highlights (`1.0.474 -> 1.0.475`):

- CAT/Hamlib stability:
- hardened Hamlib frequency writes so transient Icom serial timeouts and bus errors no longer force immediate disconnects during band or frequency changes.
- added a deferred write path when Hamlib reports a rig rejection after an internal polling timeout, reducing command storms and slow cascading band changes.
- shortened Icom serial transaction timeouts and retries for adaptive polling so failed CAT reads return faster and keep the UI responsive.
- coalesced queued CAT frequency updates so stale frequency changes are dropped before reaching the rig.
- delayed mode and split synchronization after local Hamlib QSY requests, allowing the rig to settle before follow-up CAT writes.
- delayed split/TX-frequency synchronization while a local QSY settle window is active.
- kept manual RX frequency changes available even after polling backoff has been triggered.
- improved CAT polling recovery logging, including write quiet tick state, to make future field logs easier to diagnose.
- improved PTT-off and shutdown behavior by avoiding unnecessary CAT reconnect pressure after transient errors.

- CAT setup and operator feedback:
- replaced raw Hamlib trace popups with concise user-facing CAT failure messages.
- added specific detection for Yaesu/NewCAT frequency-query timeouts, including FT-991A-style `FA;` no-response failures.
- startup auto-connect CAT failures caused by transient serial I/O now leave Decodium open and the setup UI reachable instead of showing a blocking trace dialog.
- preserved the full technical Hamlib trace in diagnostic logs for support while keeping dialogs readable.
- clarified timeout guidance for COM port, baud rate, RTS/DTR handshake, CAT timeout settings and port ownership checks.

- Version and release metadata:
- version metadata is aligned to `1.0.475`.
- release notes document the stabilization work from `1.0.474` to `1.0.475`.

Validation performed locally:

- `cmake --build /Users/salvo/Desktop/Decodium4-build --parallel 4 --target decodium_qml`

Release assets expected from GitHub Actions:

- `Decodium_1.0.475_Setup_x64.exe`
- `decodium4-ft2-1.0.475-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.475-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.475-macos-ventura-x86_64.dmg`
- `decodium4-ft2-1.0.475-macos-sonoma-x86_64.dmg`
- `decodium4-ft2-1.0.475-macos-sequoia-x86_64.dmg`
- `decodium4-ft2-1.0.475-linux-x86_64.AppImage`
- `decodium4-ft2-1.0.475-linux-aarch64.AppImage`
- matching `.zip` and checksum files where produced by the platform workflow.

## Italiano

Punti principali (`1.0.474 -> 1.0.475`):

- Stabilita' CAT/Hamlib:
- resi piu' robusti i write frequenza Hamlib, cosi' timeout e bus error transitori su seriale Icom non causano piu' disconnessioni immediate durante cambi banda o frequenza.
- aggiunto un percorso di write differito quando Hamlib segnala un rig rejection dopo un timeout interno di polling, riducendo raffiche di comandi e cambi banda a cascata troppo lenti.
- accorciati timeout e retry delle transazioni seriali Icom usate dal polling adattivo, cosi' le letture CAT fallite rientrano piu' rapidamente e l'interfaccia resta reattiva.
- accorpati gli aggiornamenti frequenza CAT in coda, scartando i cambi vecchi prima che arrivino al rig.
- ritardata la sincronizzazione modo/split dopo richieste Hamlib QSY locali, lasciando tempo alla radio di stabilizzarsi prima dei write CAT successivi.
- ritardata la sincronizzazione split/TX-frequency durante la finestra di assestamento QSY locale.
- mantenuti disponibili i cambi manuali di frequenza RX anche dopo l'attivazione del backoff di polling.
- migliorato il logging di recovery del polling CAT, incluso lo stato dei write quiet ticks, per rendere piu' leggibili i log raccolti sul campo.
- migliorato il comportamento PTT-off e chiusura evitando pressione di riconnessione CAT non necessaria dopo errori transitori.

- Setup CAT e feedback operatore:
- sostituiti i popup con trace Hamlib grezza con messaggi CAT sintetici e leggibili.
- aggiunto riconoscimento specifico dei timeout Yaesu/NewCAT sulla query frequenza, incluso il caso FT-991A con comando `FA;` senza risposta.
- i fallimenti CAT in auto-connect all'avvio dovuti a I/O seriale transitorio ora lasciano Decodium aperto e il setup raggiungibile, invece di mostrare un dialog bloccante con trace tecnica.
- mantenuta la trace Hamlib completa nei diagnostic log per supporto e debug, senza saturare il dialog utente.
- chiarite le indicazioni su porta COM, baud rate, handshake RTS/DTR, timeout CAT e controllo di eventuali altri software sulla stessa porta.

- Metadati versione e release:
- metadati versione allineati a `1.0.475`.
- note release aggiornate con il lavoro di stabilizzazione da `1.0.474` a `1.0.475`.

Validazione locale eseguita:

- `cmake --build /Users/salvo/Desktop/Decodium4-build --parallel 4 --target decodium_qml`

Asset attesi dai runner GitHub Actions:

- `Decodium_1.0.475_Setup_x64.exe`
- `decodium4-ft2-1.0.475-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.475-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.475-macos-ventura-x86_64.dmg`
- `decodium4-ft2-1.0.475-macos-sonoma-x86_64.dmg`
- `decodium4-ft2-1.0.475-macos-sequoia-x86_64.dmg`
- `decodium4-ft2-1.0.475-linux-x86_64.AppImage`
- `decodium4-ft2-1.0.475-linux-aarch64.AppImage`
- relativi `.zip` e checksum dove prodotti dal workflow della piattaforma.
