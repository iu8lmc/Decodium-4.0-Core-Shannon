# Decodium 4 FT2 1.0.474

## English

Release highlights (`1.0.473 -> 1.0.474`):

- FT2-Link operator workflow:
- added a fuller alert center with stored items, unread/read state and practical review actions.
- improved BBS, mail, broadcast and received-file views so incoming items are easier to discover, read, copy, save and mark as handled.
- added real binary file payload support in addition to text-file payloads, with safer receive-side storage and metadata handling.
- expanded relay/parking guidance and scheduler state so deferred traffic, relay candidates and parking-style workflows are more visible to the operator.
- added multi-hop/digipeater metadata handling for FT2-Link application payloads and routing-oriented UI feedback.
- tightened broadcast/session traffic behavior to reduce collisions with active session exchanges.
- CAT/radio backend:
- added radio backend profiles covering the selected backend and its related connection parameters, with save/load/delete support.
- the last selected radio profile is remembered on startup; manually switching profile loads the settings and leaves the final radio connection under operator control.
- hardened Hamlib/Icom serial polling with adaptive backoff, recovery logging and reduced redundant queries.
- suppressed frequency/mode writes when the CAT path is already unstable, reducing command storms after bus errors or timeouts.
- avoided split and TX-frequency writes when split mode is configured as `None`.
- separated mode-only changes from frequency writes and skipped redundant frequency writes when the requested value already matches current state.
- improved stale CAT state handling and safer PTT-off behavior during errors and shutdown.
- FT2-Link UI/runtime:
- cleaned up overlapping controls in the FT2-Link workspace and made application panels more compact.
- improved read/unread indicators for BBS, mail and received-file queues.
- improved broadcast tag persistence and input layout.
- added clearer labels and controls for structured FT2-Link application payloads.
- Tools and lab support:
- added `tools/hf-gateway`, a small test gateway for radio/HF-over-IP style FT2/FT2-Link experiments.
- kept temporary lab captures, WAV corpus files and old release assets out of the source release.
- Version metadata is aligned to `1.0.474`.

Validation performed locally:

- `git diff --check`
- `cmake --build /Users/salvo/Desktop/Decodium4-build --parallel 4 --target decodium_qml`

Release assets expected from GitHub Actions:

- `Decodium_1.0.474_Setup_x64.exe`
- `decodium4-ft2-1.0.474-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.474-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.474-macos-ventura-x86_64.dmg`
- `decodium4-ft2-1.0.474-macos-sonoma-x86_64.dmg`
- `decodium4-ft2-1.0.474-macos-sequoia-x86_64.dmg`
- `decodium4-ft2-1.0.474-linux-x86_64.AppImage`
- `decodium4-ft2-1.0.474-linux-aarch64.AppImage`
- matching `.zip` and checksum files where produced by the platform workflow.

## Italiano

Punti principali (`1.0.473 -> 1.0.474`):

- Workflow operativo FT2-Link:
- aggiunto un alert center piu' completo con archivio, stato letto/non letto e azioni pratiche di consultazione.
- migliorate le viste BBS, mail, broadcast e file ricevuti, cosi' gli elementi in arrivo sono piu' facili da trovare, leggere, copiare, salvare e marcare come gestiti.
- aggiunto supporto reale ai payload file binari oltre ai file testuali, con gestione piu' sicura di salvataggio e metadati lato ricezione.
- estesi relay/parking e scheduler, rendendo piu' visibili traffico differito, candidati relay e workflow stile parking.
- aggiunta gestione metadati multi-hop/digipeater per i payload applicativi FT2-Link e feedback UI orientato al routing.
- reso piu' prudente il comportamento broadcast/sessione per ridurre collisioni con scambi di sessione ancora attivi.
- CAT/backend radio:
- aggiunti profili backend radio che includono il backend selezionato e i relativi parametri di connessione, con supporto salva/carica/elimina.
- l'ultimo profilo radio selezionato viene ricordato all'avvio; il cambio manuale profilo carica i parametri ma lascia la connessione finale sotto controllo dell'operatore.
- reso piu' robusto il polling Hamlib/Icom seriale con backoff adattivo, log di recovery e meno query ridondanti.
- soppressi i write frequenza/modo quando la CAT e' gia' instabile, riducendo raffiche di comandi dopo bus error o timeout.
- evitati write split e TX-frequency quando lo split e' configurato su `None`.
- separate le modifiche solo modo dai write frequenza e saltati i write frequenza ridondanti quando il valore richiesto coincide gia' con lo stato corrente.
- migliorata la gestione degli stati CAT vecchi e resa piu' sicura la procedura PTT-off durante errori e chiusura.
- UI/runtime FT2-Link:
- ripulite sovrapposizioni di controlli nella workspace FT2-Link e resi piu' compatti i pannelli applicativi.
- migliorati gli indicatori letto/non letto per code BBS, mail e file ricevuti.
- migliorati persistenza tag broadcast e layout di inserimento.
- aggiunte label e controlli piu' chiari per payload applicativi strutturati FT2-Link.
- Strumenti e supporto lab:
- aggiunto `tools/hf-gateway`, un piccolo gateway di test per esperimenti FT2/FT2-Link radio/HF-over-IP.
- lasciati fuori dal sorgente release capture lab temporanee, corpus WAV e asset di release vecchie.
- metadati versione allineati a `1.0.474`.

Validazione locale eseguita:

- `git diff --check`
- `cmake --build /Users/salvo/Desktop/Decodium4-build --parallel 4 --target decodium_qml`

Asset attesi dai runner GitHub Actions:

- `Decodium_1.0.474_Setup_x64.exe`
- `decodium4-ft2-1.0.474-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.474-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.474-macos-ventura-x86_64.dmg`
- `decodium4-ft2-1.0.474-macos-sonoma-x86_64.dmg`
- `decodium4-ft2-1.0.474-macos-sequoia-x86_64.dmg`
- `decodium4-ft2-1.0.474-linux-x86_64.AppImage`
- `decodium4-ft2-1.0.474-linux-aarch64.AppImage`
- relativi `.zip` e checksum dove prodotti dal workflow della piattaforma.
