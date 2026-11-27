# Decodium 4 FT2 1.0.473

## English

Release highlights (`1.0.472 -> 1.0.473`):

- FT2-Link QSY workflow:
- QSY invitations can now carry an absolute dial target with `<QF:Hz>`, avoiding ambiguity when the two stations do not share the same local dial state.
- the FT2-Link QSY button now inserts the computed absolute QSY target when the current dial frequency is available, with relative tags kept as fallback.
- incoming QSY replies (`<QSYR>`, `<QSYJ>`, `<QJO>`) now close the pending QSY outbound transfer as delivered or failed, preventing stale retries after the peer already answered.
- QSY application is delayed until the local acknowledgement path has time to transmit, reducing the chance of changing frequency before the peer receives the response.
- QSY broadcast payloads now preserve target frequency, label and reason metadata for the dashboard and release workflows.
- FT2-Link lab and loopback validation:
- added lab CLI support for forced dial frequency, scheduled QSY requests, QSY offset, and automatic QSY acceptance.
- lab QSY logs now include requested/effective frequency, target frequency, before/after QSY state and reply result.
- lab QSY processing ignores stale session history from previous runs, so old messages cannot trigger false accepts or rejects.
- verified a full BlackHole 2ch loopback scenario: HELLO, HELLO_ACK, W2300 session, `<QF:14105750>` QSY invite, `<QSYR>` reply and QSY on both sides.
- FT2-Link RF/debug tooling:
- RX failure diagnostics can optionally dump mono 16-bit WAV captures for decoder analysis.
- wide runtime samples include guard silence around W500/W2300 payloads, improving burst edge handling in real audio paths.
- W2300 acquisition/decoder diagnostics now expose more useful RX failure and decode context for RF troubleshooting.
- live W500/W2300 RX now uses the same 48 kHz sample-rate configuration as the application audio path, fixing wide bursts that were visible on the waterfall but not decoded.
- guarded short W2300 bursts now trim leading runtime silence before acquisition, and W500 opportunistic RX keeps enough buffered audio for long out-of-session broadcasts.
- RF Lab WAV replay now routes NARROW at 12 kHz and W500/W2300 at 48 kHz, with profile-specific replay hints so lab validation no longer wastes minutes scanning the wrong decoder path.
- offline W500/W2300 audio-pipeline tests now feed burst audio in chunks but decode once per completed burst, matching the half-duplex simulation and avoiding thousands of premature acquisition scans.
- FT2-Link waveform and weak-signal work:
- W2300 decode improvements continue around acquisition, frequency offset handling, drift tolerance, threaded search and deep/ultra-rate retry paths.
- additional regression coverage exercises W2300 frame round trips, frequency offset correction, chunked audio pipelines and QML adapter behavior.
- FT2-Link UI/runtime fixes:
- FT2-Link mode hides or reduces normal FT2 macro areas that are not useful during the FT2-Link workspace.
- waterfall/panadapter refresh and FT2-Link panel layout were adjusted after the CPU reduction work, preserving smoother visuals while keeping load low.
- file, BBS, broadcast, received-file and unread-state panels were tightened for practical operator use.
- version metadata is aligned to `1.0.473`.

Validation performed locally:

- `cmake --build /Users/salvo/Desktop/Decodium4-build --parallel 4 --target decodium_qml test_ft2link test_ft2link_qml_adapter`
- `test_ft2link` W2300 waveform/symbol mapping/rate-controller subset, including ROBUST, WEAK, DEEP and ULTRA round trips.
- `test_ft2link_qml_adapter` targeted gates for W2300 live round-trip, W500 broadcast RX, live rate adaptation, application payloads, QSY parsing and QSY broadcast metadata.
- `test_ft2link_qml_adapter` RF Lab replay gates for reference WAVs, mild channel impairments, WEAK W2300, ULTRA -3 dB W2300 and the quick channel sweep.
- `ctest -R '^(test_ft2link|test_ft2link_qml_adapter)$' --output-on-failure`
- `git diff --check`
- full two-instance BlackHole 2ch FT2-Link QSY simulation using QSYA/QSYB lab profiles.

Release assets expected from GitHub Actions:

- `Decodium_1.0.473_Setup_x64.exe`
- `decodium4-ft2-1.0.473-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.473-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.473-macos-ventura-x86_64.dmg`
- `decodium4-ft2-1.0.473-macos-sonoma-x86_64.dmg`
- `decodium4-ft2-1.0.473-macos-sequoia-x86_64.dmg`
- `decodium4-ft2-1.0.473-linux-x86_64.AppImage`
- `decodium4-ft2-1.0.473-linux-aarch64.AppImage`
- matching `.zip` and checksum files where produced by the platform workflow.

## Italiano

Punti principali (`1.0.472 -> 1.0.473`):

- Flusso QSY FT2-Link:
- gli inviti QSY possono ora trasportare il target dial assoluto con `<QF:Hz>`, evitando ambiguita' quando le due stazioni non hanno lo stesso stato frequenza locale.
- il pulsante QSY di FT2-Link inserisce il target QSY assoluto calcolato quando la frequenza dial corrente e' disponibile, mantenendo i tag relativi come fallback.
- le risposte QSY in arrivo (`<QSYR>`, `<QSYJ>`, `<QJO>`) chiudono il transfer QSY pendente come consegnato o fallito, evitando retry vecchi dopo che il corrispondente ha gia' risposto.
- l'applicazione del QSY viene ritardata per dare tempo al percorso ACK locale di trasmettere, riducendo il rischio di cambiare frequenza prima che il peer riceva la risposta.
- i broadcast QSY conservano frequenza target, label e reason per dashboard e workflow di release.
- Validazione lab e loopback FT2-Link:
- aggiunti flag CLI lab per frequenza dial forzata, QSY schedulato, offset QSY e accettazione QSY automatica.
- i log lab QSY includono frequenza richiesta/effettiva, target, stato prima/dopo il QSY ed esito della risposta.
- il processing lab QSY ignora cronologia sessione vecchia da run precedenti, quindi messaggi storici non possono causare accettazioni o rifiuti falsi.
- verificato uno scenario completo via BlackHole 2ch: HELLO, HELLO_ACK, sessione W2300, invito QSY `<QF:14105750>`, risposta `<QSYR>` e QSY su entrambi i lati.
- Strumenti RF/debug FT2-Link:
- le diagnostiche RX failure possono opzionalmente salvare capture WAV mono 16-bit per analisi decoder.
- i sample runtime wide includono silenzio di guardia attorno ai payload W500/W2300, migliorando la gestione dei bordi burst nei percorsi audio reali.
- diagnostica acquisizione/decoder W2300 arricchita con contesto piu' utile per troubleshooting RF.
- la RX live W500/W2300 ora usa la stessa configurazione sample-rate a 48 kHz del percorso audio applicativo, correggendo burst wide visibili sul waterfall ma non decodificati.
- i burst W2300 brevi con guardia runtime rimuovono il silenzio iniziale prima dell'acquisizione, e la RX opportunistica W500 mantiene abbastanza audio per broadcast lunghi fuori sessione.
- il replay WAV RF Lab ora instrada NARROW a 12 kHz e W500/W2300 a 48 kHz, con hint profilo dedicati per evitare minuti persi a cercare sul decoder sbagliato durante la validazione lab.
- i test pipeline audio offline W500/W2300 ora alimentano il burst a chunk ma decodificano una volta a burst completo, coerente con la simulazione half-duplex ed evitando migliaia di acquisizioni premature.
- Lavoro waveform e weak-signal FT2-Link:
- continuano i miglioramenti W2300 su acquisizione, offset frequenza, tolleranza drift, ricerca threaded e retry deep/ultra-rate.
- nuova copertura regression per round trip frame W2300, correzione offset frequenza, pipeline audio chunked e comportamento QML adapter.
- Fix UI/runtime FT2-Link:
- in modo FT2-Link vengono nascosti o ridotti gli spazi macro FT2 normali che non servono nella workspace FT2-Link.
- waterfall/panadapter e layout pannello FT2-Link rifiniti dopo il lavoro di riduzione CPU, preservando visuali piu' fluide con carico contenuto.
- pannelli file, BBS, broadcast, file ricevuti e stati unread resi piu' pratici per uso operativo.
- metadati versione allineati a `1.0.473`.

Validazione locale eseguita:

- `cmake --build /Users/salvo/Desktop/Decodium4-build --parallel 4 --target decodium_qml test_ft2link test_ft2link_qml_adapter`
- subset `test_ft2link` waveform/symbol mapping/rate-controller W2300, inclusi round trip ROBUST, WEAK, DEEP e ULTRA.
- gate mirati `test_ft2link_qml_adapter` per round-trip live W2300, RX broadcast W500, rate adaptation live, payload applicativi, parsing QSY e metadati broadcast QSY.
- gate RF Lab `test_ft2link_qml_adapter` per WAV reference, impairment mild, W2300 WEAK, W2300 ULTRA -3 dB e sweep rapido del canale.
- `ctest -R '^(test_ft2link|test_ft2link_qml_adapter)$' --output-on-failure`
- `git diff --check`
- simulazione completa FT2-Link QSY con due istanze lab QSYA/QSYB via BlackHole 2ch.

Asset attesi dai runner GitHub Actions:

- `Decodium_1.0.473_Setup_x64.exe`
- `decodium4-ft2-1.0.473-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.473-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.473-macos-ventura-x86_64.dmg`
- `decodium4-ft2-1.0.473-macos-sonoma-x86_64.dmg`
- `decodium4-ft2-1.0.473-macos-sequoia-x86_64.dmg`
- `decodium4-ft2-1.0.473-linux-x86_64.AppImage`
- `decodium4-ft2-1.0.473-linux-aarch64.AppImage`
- relativi `.zip` e checksum dove prodotti dal workflow della piattaforma.
