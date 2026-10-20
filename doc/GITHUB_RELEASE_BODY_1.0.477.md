# Decodium 4 FT2 1.0.477

## English

Release highlights (`1.0.476 -> 1.0.477`):

- Standard FT2 QSO reliability:
- fixed repeated macOS/CoreAudio TX sessions that could leave the radio keyed with silent audio after several AutoCQ or QSO transmissions.
- recreated parked CoreAudio output streams before the next payload so repeated TX cycles reopen the selected output device cleanly.
- reduced duplicate TX timeline rows when bridge-owned audio is already mirroring the legacy digital TX path.
- preserved FT2 normal slot parity instead of forcing first/second-period state as if it were a special operation.
- kept standard FT2 async live decoding responsive by limiting the live pass depth unless `DECODIUM_FT2_ASYNC_DEEP=1` is explicitly enabled.
- enabled multi-threaded embedded FT2 async live decode requests so the live path can use the same thread pool strategy as the standard decode flow.

- Signal RX and QSO timeline:
- made Signal RX recognize local calls using the bridge callsign, legacy backend callsign, current TX payload, pending auto-sequence payload and last transmitted payload.
- applied the local-call fallback to legacy mirror rows, `ALL.TXT` replay, FT2 async rows and legacy JT/FT rows.
- allowed direct weak calls already visible in Full Spectrum to remain visible in Signal RX instead of being hidden by the first-contact ghost gate.
- improved partner extraction for directed messages when the bridge callsign and legacy backend callsign are temporarily out of sync.
- added optional `DECODIUM_SIGRX_DEBUG=1` diagnostics for Signal RX routing decisions.

- Runtime lab and loopback tools:
- added lab command-line controls for delayed standard TX, multi-step TX plans, DX call/grid overrides, TX period overrides, standard AutoCQ and passive Wait/Pounce.
- regenerated stale lab TX messages when the lab callsign or grid is applied after startup.
- allowed no-CAT embedded lab sessions on macOS to start bridge audio directly without trying to reconnect CAT first.
- suppressed forced audio input restarts in embedded no-CAT lab mode so crossed BlackHole loopback sessions keep stable capture.

- Validation performed locally:
- `cmake --build /Users/salvo/Desktop/Decodium4-build --parallel 4 --target decodium_qml`
- two-application FT2 loopback testing over crossed BlackHole 2ch/16ch devices without CAT.
- repeated AutoCQ testing with 25 TX attempts and no silent-payload recurrence.
- manual review of decoded QSO rows showing directed RX messages in Full Spectrum and validating the Signal RX routing fix.

Release assets expected from GitHub Actions:

- `Decodium_1.0.477_Setup_x64.exe`
- `decodium4-ft2-1.0.477-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.477-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.477-macos-ventura-x86_64.dmg`
- `decodium4-ft2-1.0.477-macos-sonoma-x86_64.dmg`
- `decodium4-ft2-1.0.477-macos-sequoia-x86_64.dmg`
- `decodium4-ft2-1.0.477-linux-x86_64.AppImage`
- `decodium4-ft2-1.0.477-linux-aarch64.AppImage`
- matching `.zip` and checksum files where produced by the platform workflow.

## Italiano

Punti principali (`1.0.476 -> 1.0.477`):

- Affidabilita' QSO FT2 standard:
- corretto un problema macOS/CoreAudio in cui, dopo varie trasmissioni AutoCQ o QSO, la radio poteva andare in TX ma senza payload audio.
- ricreato lo stream CoreAudio parcheggiato prima del payload successivo, cosi' i cicli TX ripetuti riaprono correttamente la periferica audio selezionata.
- ridotte le righe TX duplicate nella timeline quando l'audio bridge sta gia' facendo mirror del TX digitale legacy.
- preservata la parita' degli slot FT2 standard, evitando forzature del periodo primo/secondo riservate alle operazioni speciali.
- mantenuto rapido il decoder FT2 async live limitando la profondita' del pass live, con deep live attivabile solo tramite `DECODIUM_FT2_ASYNC_DEEP=1`.
- abilitate richieste FT2 async live multi-thread per usare una strategia piu' vicina al decoder standard.

- Signal RX e timeline QSO:
- Signal RX ora riconosce il callsign locale dal bridge, dal backend legacy, dal TX corrente, dal payload auto-sequence pendente e dall'ultimo TX inviato.
- applicato il fallback del callsign locale alle righe legacy mirror, al replay `ALL.TXT`, alle righe FT2 async e alle righe JT/FT legacy.
- le chiamate dirette deboli gia' visibili in Full Spectrum restano visibili anche in Signal RX, invece di essere nascoste dal gate anti-falso-positivo di primo contatto.
- migliorata l'estrazione del corrispondente nei messaggi diretti quando callsign bridge e backend legacy sono temporaneamente disallineati.
- aggiunta diagnostica opzionale `DECODIUM_SIGRX_DEBUG=1` per capire le decisioni di routing verso Signal RX.

- Strumenti lab e loopback:
- aggiunti controlli command-line lab per TX standard ritardato, piani TX multi-step, override DX call/grid, override periodo TX, AutoCQ standard e Wait/Pounce passivo.
- rigenerati i messaggi TX lab obsoleti quando callsign o grid lab vengono applicati dopo l'avvio.
- consentite sessioni lab embedded senza CAT su macOS con avvio diretto dell'audio bridge, senza tentare prima la riconnessione CAT.
- soppressi restart forzati dell'input audio in modalita' embedded no-CAT, cosi' i loopback BlackHole incrociati mantengono una cattura stabile.

Validazione locale eseguita:

- `cmake --build /Users/salvo/Desktop/Decodium4-build --parallel 4 --target decodium_qml`
- test FT2 con due applicazioni reali su BlackHole 2ch/16ch incrociati, senza CAT.
- test AutoCQ ripetuto con 25 tentativi TX senza ricomparsa del payload muto.
- revisione manuale delle righe QSO decodificate in Full Spectrum e verifica del fix di routing verso Signal RX.

Asset attesi dai runner GitHub Actions:

- `Decodium_1.0.477_Setup_x64.exe`
- `decodium4-ft2-1.0.477-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.477-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.477-macos-ventura-x86_64.dmg`
- `decodium4-ft2-1.0.477-macos-sonoma-x86_64.dmg`
- `decodium4-ft2-1.0.477-macos-sequoia-x86_64.dmg`
- `decodium4-ft2-1.0.477-linux-x86_64.AppImage`
- `decodium4-ft2-1.0.477-linux-aarch64.AppImage`
- relativi `.zip` e checksum dove prodotti dal workflow della piattaforma.
