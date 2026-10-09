# Decodium 4 FT2 1.0.476

## English

Release highlights (`1.0.475 -> 1.0.476`):

- FT2-Link RF/audio reliability:
- aligned the live FT2-Link receive worker with the decimated detector/audio tap used by the main waterfall path.
- improved macOS AudioQueue input routing so selected devices such as BlackHole and USB radio interfaces are explicitly selected instead of silently falling back to the system default.
- added native macOS input fallback diagnostics while keeping Qt audio capture available when CoreAudio cannot open a selected device.
- kept active macOS input streams reusable across duplicate start requests, reducing capture restarts that could cut short control bursts.
- added RX emit/ingest/worker/scan diagnostics for silent-audio and decoder-path investigations.

- FT2-Link waveform and session behavior:
- improved NARROW control acquisition with tolerant preamble/sync matching and exhaustive sample-phase scanning.
- added W2300 live decode threshold tuning for shorter control/data bursts.
- widened NARROW control burst guards for CoreAudio/BlackHole and USB audio paths.
- delayed HELLO_ACK turnaround and increased live retry spacing to reduce half-duplex collisions.
- purged queued live retries once an ACK is received, reducing duplicate retransmissions after successful delivery.
- preserved real playback tail time on macOS/BlackHole so transmitted payloads are not truncated before CoreAudio has drained.

- Loopback and regression validation:
- completed two real Decodium application loopback sessions with TESTA/TESTB over crossed BlackHole 2ch/16ch devices, without CAT.
- validated handshake, bidirectional chat, file, mail, BBS, form and broadcast traffic in both directions.
- refreshed the FT2-Link QML adapter test to simulate radio playback completion like the real bridge.

- Version and release metadata:
- version metadata is aligned to `1.0.476`.
- release notes document the stabilization work from `1.0.475` to `1.0.476`.

Validation performed locally:

- `cmake --build /Users/salvo/Desktop/Decodium4-build --parallel 4 --target decodium_qml`
- `cmake --build /Users/salvo/Desktop/Decodium4-build --parallel 4 --target test_ft2link_qml_adapter`
- `/Users/salvo/Desktop/Decodium4-build/tests/test_ft2link`
- `/Users/salvo/Desktop/Decodium4-build/tests/test_ft2link_qml_adapter twoAdaptersRoundTripApplicationPayloadsAudio`
- manual two-application FT2-Link loopback over BlackHole 2ch/16ch without CAT.

Release assets expected from GitHub Actions:

- `Decodium_1.0.476_Setup_x64.exe`
- `decodium4-ft2-1.0.476-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.476-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.476-macos-ventura-x86_64.dmg`
- `decodium4-ft2-1.0.476-macos-sonoma-x86_64.dmg`
- `decodium4-ft2-1.0.476-macos-sequoia-x86_64.dmg`
- `decodium4-ft2-1.0.476-linux-x86_64.AppImage`
- `decodium4-ft2-1.0.476-linux-aarch64.AppImage`
- matching `.zip` and checksum files where produced by the platform workflow.

## Italiano

Punti principali (`1.0.475 -> 1.0.476`):

- Affidabilita' RF/audio FT2-Link:
- allineato il worker RX live FT2-Link al tap audio/detector decimato usato dal percorso waterfall principale.
- migliorato il routing macOS AudioQueue in modo che periferiche selezionate come BlackHole e interfacce USB radio vengano scelte esplicitamente, senza cadere sul default di sistema.
- aggiunta diagnostica di fallback input nativo macOS, mantenendo disponibile la cattura Qt quando CoreAudio non riesce ad aprire una periferica selezionata.
- riutilizzati gli stream input macOS gia' attivi per richieste di start duplicate, riducendo restart di cattura che potevano tagliare burst di controllo.
- aggiunti log RX emit/ingest/worker/scan per analizzare casi di audio silenzioso e percorso decoder.

- Waveform e sessioni FT2-Link:
- migliorata l'acquisizione NARROW control con matching preamble/sync tollerante e scansione esaustiva della fase campione.
- regolati i threshold W2300 live per burst control/data piu' brevi.
- ampliati i guard dei burst NARROW control per percorsi CoreAudio/BlackHole e USB audio.
- ritardato il turnaround HELLO_ACK e aumentata la distanza tra retry live per ridurre collisioni half-duplex.
- eliminati dalla coda i retry live gia' superati quando arriva un ACK, riducendo ritrasmissioni duplicate dopo consegna riuscita.
- preservato su macOS/BlackHole il tempo reale di coda playback, cosi' i payload TX non vengono troncati prima che CoreAudio abbia svuotato il buffer.

- Validazione loopback e regressione:
- completate due sessioni Decodium reali TESTA/TESTB con BlackHole 2ch/16ch incrociati, senza CAT.
- validati handshake, chat bidirezionale, file, mail, BBS, form e broadcast in entrambe le direzioni.
- aggiornato il test FT2-Link QML adapter per simulare il completamento playback radio come nel bridge reale.

- Metadati versione e release:
- metadati versione allineati a `1.0.476`.
- note release aggiornate con il lavoro di stabilizzazione da `1.0.475` a `1.0.476`.

Validazione locale eseguita:

- `cmake --build /Users/salvo/Desktop/Decodium4-build --parallel 4 --target decodium_qml`
- `cmake --build /Users/salvo/Desktop/Decodium4-build --parallel 4 --target test_ft2link_qml_adapter`
- `/Users/salvo/Desktop/Decodium4-build/tests/test_ft2link`
- `/Users/salvo/Desktop/Decodium4-build/tests/test_ft2link_qml_adapter twoAdaptersRoundTripApplicationPayloadsAudio`
- loopback manuale FT2-Link con due applicazioni reali su BlackHole 2ch/16ch senza CAT.

Asset attesi dai runner GitHub Actions:

- `Decodium_1.0.476_Setup_x64.exe`
- `decodium4-ft2-1.0.476-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.476-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.476-macos-ventura-x86_64.dmg`
- `decodium4-ft2-1.0.476-macos-sonoma-x86_64.dmg`
- `decodium4-ft2-1.0.476-macos-sequoia-x86_64.dmg`
- `decodium4-ft2-1.0.476-linux-x86_64.AppImage`
- `decodium4-ft2-1.0.476-linux-aarch64.AppImage`
- relativi `.zip` e checksum dove prodotti dal workflow della piattaforma.
