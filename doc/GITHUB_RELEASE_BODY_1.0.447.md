# Decodium 4 FT2 1.0.447

## Novita' dalla 1.0.445 alla 1.0.447

Questa release integra il lavoro upstream 1.0.446 di Martino e le correzioni locali successive, con focus su stabilita' operativa, CAT, sequencer FT2, logging e pacchetti multipiattaforma.

### Sequencer e FT2

- Integrato l'hardening del sequencer FT2 asincrono introdotto in 1.0.446.
- Migliorata la gestione degli stati interni del bridge durante le transizioni operative.
- Aggiornate le impostazioni UI correlate al sequencer e le traduzioni principali.

### CAT e interfacce radio

- Rafforzato il keep-alive CAT opzionale per interfacce con LED di attivita', senza ripristinare il polling aggressivo che poteva causare timeout su CI-V.
- Il keep-alive usa traffico CAT leggero e si disattiva automaticamente dopo errori ripetuti.
- Migliorata la compatibilita' con configurazioni Hamlib seriali e interfacce esterne.

### Stabilita' e prestazioni

- Aggiornati componenti bridge, audio, waterfall e pannelli TX con correzioni di robustezza.
- Rafforzati percorsi di logging verso servizi remoti.
- Aggiornati test e configurazione CI per coprire meglio i percorsi modificati.

### Release e pacchetti

- Versione locale allineata a `1.0.447`.
- Installer Windows NSIS aggiornato a `1.0.447`.
- Predisposti asset per:
  - sorgenti GitHub;
  - installer Microsoft Windows x64;
  - DMG macOS Apple Silicon;
  - DMG macOS Intel;
  - AppImage Linux x86_64;
  - AppImage Linux aarch64.

## Verifiche locali

- Build locale macOS Apple Silicon completata con target `decodium_qml`.
- Test mirati superati:
  - `test_qt_helpers`;
  - `test_streaming_list_model`;
  - test funzionali interni sui nuovi percorsi di controllo.
