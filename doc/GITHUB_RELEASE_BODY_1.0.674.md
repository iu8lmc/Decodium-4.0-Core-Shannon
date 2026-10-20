# Decodium 1.0.674

## English (British)

This release brings the changes accumulated since 1.0.660, including the new remote-radio and CW capabilities, more reliable FT2/MAM sequencing, and build-quality improvements.

### Highlights

- Added Decolink remote-radio operation, including relay login, remote TX without a local audio device, queued TX audio chunks, a lossless digital audio profile, and one-transmitter-per-station coordination for multi-operator use.
- Improved FT2 and Multi-Answer Mode sequencing by ignoring echoes of our own transmissions and preventing echoed frames from creating or logging false QSOs.
- Added the CW module: decoder, keyers, macros, CW window, remote keying over Decolink, local sidetone, live typing, and radio CAT controls.
- Added the rotator integration and translated its controls across the supported language catalogues.
- Improved FT2 telemetry timing and split-frequency command handling.
- Updated the Windows package to include the complete Decolink installation path and remote-radio support.
- Cleaned up compiler and linker diagnostics: corrected legacy C conversion warnings, fixed invalid fixed-width grid initialisers, enabled CMake policy handling for modern static-library deduplication, and removed duplicate link interfaces without suppressing real errors.
- Fixed the legacy Linux `wsjtx` build by exposing the CW include path and linking the CW module required by `DecodiumBridge`.
- Preserved the existing multi-stream, SSTV, SuperFox, QSO logging, and QML changes accumulated in the local main branch.

The Windows installer, macOS Intel and Apple Silicon DMG packages, and Linux AppImage packages are produced by the GitHub Actions release runners.

## Italiano

Questa release raccoglie le modifiche introdotte dalla 1.0.660, comprese le nuove funzioni radio remote e CW, una gestione più affidabile del sequenziamento FT2/MAM e diversi miglioramenti alla qualità della build.

### Novità principali

- Aggiunto il funzionamento della radio remota tramite Decolink: autenticazione al relay, trasmissione remota anche senza dispositivo audio locale, accodamento dei blocchi audio TX, profilo digitale lossless e coordinamento di un solo trasmettitore per stazione in uso multi-operatore.
- Migliorato il sequenziamento FT2 e Multi-Answer Mode, ignorando gli echi delle nostre trasmissioni ed evitando che frame riflessi generino o registrino QSO errati.
- Aggiunto il modulo CW: decoder, keyer, macro, finestra CW, tasto remoto via Decolink, sidetone locale, digitazione in tempo reale e controlli CAT della radio.
- Integrata la gestione del rotatore e tradotti i relativi controlli nei cataloghi linguistici supportati.
- Migliorata la temporizzazione della telemetria FT2 e la gestione dei comandi di split quando cambia soltanto la frequenza TX.
- Aggiornato il pacchetto Windows con il percorso completo di installazione Decolink e del supporto radio remoto.
- Ripuliti i warning di compilazione e linking: corretti i warning di conversione nel C legacy, sistemati gli inizializzatori delle griglie a larghezza fissa, abilitate le policy CMake moderne per la deduplicazione delle librerie statiche e rimosse le interfacce di linking duplicate senza nascondere errori reali.
- Corretto il build Linux del target legacy `wsjtx`, rendendo disponibile il percorso degli header CW e collegando il modulo CW richiesto da `DecodiumBridge`.
- Conservate le modifiche già presenti nel main locale relative a multi-stream, SSTV, SuperFox, logging dei QSO e interfaccia QML.

Gli installer Windows, i pacchetti DMG per macOS Intel e Apple Silicon e i pacchetti AppImage Linux vengono prodotti dai runner GitHub Actions dedicati.
