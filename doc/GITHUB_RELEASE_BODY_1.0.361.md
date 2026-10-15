# Decodium 4 FT2 1.0.361

Release cumulativa dalla 1.0.356 alla 1.0.361.

## Novita principali

- Toolbar riordinabile con drag and drop magnetico, introdotta in due step per coprire sia la barra principale sia il pannello TX.
- Pannello TX aggiornato per supportare il nuovo ordine personalizzabile dei comandi senza perdere la disposizione salvata.
- Fix FT2 per evitare collisioni in autosequenza: quando una risposta TX3/TX4/TX5 nasce da un CQ del partner, la risposta viene differita allo slot corretto invece di trasmettere sopra l'altra stazione.
- Rafforzato il cooldown dopo RR73/73: un nominativo appena loggato non riapre subito un QSO gia concluso.
- World Clock reso riposizionabile con maniglia di drag, cosi l'utente puo spostarlo senza interferire con la lettura della UI.
- Fix di compatibilita per i build runner Qt 6.11/Linux: lo switch sul backend grafico Qt Quick ora gestisce in modo robusto backend grafici non coperti dalla guardia di versione, evitando failure con `-Werror=switch`.
- Versione applicazione, installer e metadati allineati alla 1.0.361.

## Dettaglio tecnico

- `v1.0.357`: toolbar riordinabile drag and drop magnetico, step 1.
- `v1.0.358`: estensione della toolbar riordinabile al TX Panel, step 2.
- `v1.0.359`: fix collisione FT2, risposta differita TX3/TX4/TX5 dopo CQ del partner.
- `v1.0.360`: fix RR73/73 da nominativo appena loggato, con cooldown piu resiliente.
- `v1.0.361`: World Clock riposizionabile con maniglia dedicata.
- Build release: aggiunto fallback `default` nel logging del backend Qt Quick per evitare errori di compilazione quando Qt espone nuovi valori `QSGRendererInterface::GraphicsApi`.

## File principali modificati

- `qml/decodium/Main.qml`
- `qml/decodium/components/TxPanel.qml`
- `qml/decodium/components/SettingsDialog.qml`
- `DecodiumBridge.cpp`
- `fork_release_version.txt`
- `installer/decodium_setup.iss`
- `installer/decodium_setup_nsis.nsi`

## Asset previsti nella release

- Source code GitHub per il tag `1.0.361`.
- Installer Windows x64 `.exe`.
- Pacchetti macOS Apple Silicon tramite runner GitHub.
- Pacchetti macOS Intel tramite runner GitHub.
- AppImage Linux x86_64.
- AppImage Linux aarch64.
