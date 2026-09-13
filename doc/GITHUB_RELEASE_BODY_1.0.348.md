# Decodium 4 FT2 1.0.348

Release finale di stabilizzazione dalla 1.0.347 alla 1.0.348.

Questa build raccoglie le ultime correzioni locali fatte dopo la 1.0.347 e prepara gli asset Windows, macOS e Linux tramite GitHub Actions.

## Modifiche principali

- Audio startup piu' affidabile: all'avvio viene forzato un refresh delle periferiche audio prima dell'auto-monitor, cosi' gli utenti che prima dovevano premere `Refresh` in Setup > Audio dovrebbero ricevere correttamente gia' al primo avvio.
- Crash Linux TX/Tune corretto: gli stop dovuti a errori audio vengono riportati sul thread corretto e i callback `QAudioSink` sono protetti da guardie su oggetto e seriale playback, evitando `QSocketNotifier` da thread sbagliato e `SIGNAL 11`.
- Worked-before corretto per banda e modo: un contatto in 20m FT2 non viene piu' considerato gia' lavorato in 20m FT8 o 40m FT2.
- Autosequenza migliorata per CPU lente e nominativi speciali/lunghi: aumentato il margine nei passaggi TX1/TX2/TX3 e riconosciuti meglio gli hash diretti al partner attivo.
- `Check SWR` non blocca piu' il Tune: il Tune resta utilizzabile per misurare e correggere SWR alto con strumenti esterni; la protezione resta attiva su TX reale e AutoCQ.
- Setup > Display > Decodes: aggiunte opzioni per `Dist` e `Az` in Full Spectrum e Signal RX, piu' la colonna `Freq` configurabile in Signal RX.
- Panadapter piu' pulito: rimossi i marker gialli duplicati 500/1000/1500/2000/2500/3000 Hz, lasciando la scala inferiore come riferimento.
- Versione locale e packaging allineati a `1.0.348` in `fork_release_version.txt`, Inno Setup e NSIS.

## Dettaglio tecnico

- `DecodiumBridge::runPostQmlStartupServices()` aggiorna la cache audio prima di `setMonitoring(true)` quando l'auto-monitor e' abilitato.
- La telemetria e lo stop TX/Tune rientrano sul thread del bridge quando arrivano da callback audio non GUI.
- I callback `QAudioSink::stateChanged` verificano che sink, buffer e seriale playback siano ancora quelli attivi prima di completare o fermare la riproduzione.
- `WorkedSets` mantiene anche `callByBandMode` e il rebuild ADIF normalizza il modo per calcolare la chiave exact call/band/mode.
- La logica autosequence accetta i messaggi diretti all'hash locale quando provengono dal partner attivo e applica retry piu' larghi in modalita' CPU lenta.
- Le colonne decode sono pilotate da impostazioni persistenti `uiFullSpectrumShowDistColumn`, `uiFullSpectrumShowAzColumn`, `uiSignalRxShowFreqColumn`, `uiSignalRxShowDistColumn`, `uiSignalRxShowAzColumn`.

## Asset previsti

- Sorgenti GitHub automatici della release/tag `1.0.348`.
- Installer Microsoft Windows x64: `Decodium_1.0.348_Setup_x64.exe`.
- macOS Apple Silicon: DMG/ZIP Tahoe arm64 e Sequoia arm64.
- macOS Intel: DMG/ZIP Ventura, Sonoma e Sequoia x86_64.
- Linux Intel 64 bit: `decodium4-ft2-1.0.348-linux-x86_64.AppImage`.
- Linux aarch64: `decodium4-ft2-1.0.348-linux-aarch64.AppImage`.

Gli asset binari vengono caricati dai workflow GitHub Actions appena ogni runner termina con successo.
