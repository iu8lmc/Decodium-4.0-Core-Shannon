# Decodium 4 FT2 1.0.335

Release di stabilizzazione pubblicata per correggere due problemi emersi dopo la 1.0.334: selezione errata delle periferiche audio quando Windows/Qt presenta piu' dispositivi con lo stesso nome e blocco dell'autosequenza con nominativi speciali.

## Cambiamenti principali dalla 1.0.334

### Audio Qt/Windows

- Decodium salva ora anche l'ID stabile Qt/Windows delle periferiche audio in `audioInputDeviceId` e `audioOutputDeviceId`, non solo il nome visibile mostrato all'utente.
- La selezione RX/TX cerca prima il dispositivo tramite ID stabile; il nome visibile viene usato come fallback solo se il match e' esatto e univoco.
- Quando ci sono piu' periferiche con lo stesso nome, ad esempio piu' `USB Audio CODEC`, Decodium non riscrive piu' automaticamente la preferenza salvata sul dispositivo default.
- Il log di avvio audio mostra chiaramente dispositivo salvato, ID salvato, dispositivo scelto, ID scelto, motivo del match, dispositivo default e numero di input disponibili.
- Dopo l'avvio RX viene loggata una misura di salute audio con RMS, picco, range e clipping, utile per capire se il flusso audio reale e' piatto o arriva dalla periferica sbagliata.
- `SoundInput` e la cache TX distinguono ora dispositivi con stesso nome ma ID diverso, evitando riusi di stream o cache su una periferica omonima non corretta.

### Nominativi speciali e autosequenza

- Corretto il formato dei messaggi FT quando il nominativo locale e' speciale/non standard e il corrispondente DX e' standard.
- Il report viene ora indirizzato al corrispondente, ad esempio `KQ5I <II9MESC> -15`, invece di generare una forma che il peer poteva interpretare come non avanzabile.
- Corretto anche il messaggio `R-report` per lo stesso percorso (`KQ5I <II9MESC> R-15`).
- `RR73` e `73` restano coerenti con il formato compound previsto (`<KQ5I> II9MESC RR73` e `<KQ5I> II9MESC 73`).
- Aggiunti test dedicati per coprire la sequenza con `II9MESC`, `KQ5I` e altri nominativi speciali gia' presenti nella suite.

### Metadati release

- Versione locale allineata a `1.0.335` tramite `fork_release_version.txt`.
- Script installer NSIS allineato a `1.0.335`, inclusi nome output e `VIProductVersion`.
- Note release GitHub e changelog aggiornati per documentare le correzioni rispetto alla 1.0.334.

## Impatto utente

- Gli utenti Windows con piu' dispositivi audio USB omonimi hanno una selezione piu' stabile e diagnosi piu' leggibile nei log.
- Se la periferica salvata non e' piu' presente o non e' distinguibile, il programma lo segnala invece di nascondere il problema dietro un fallback silenzioso.
- Gli operatori con nominativi speciali, ad esempio eventi o call non standard, possono completare l'autosequenza FT senza restare bloccati sullo scambio locator/report.

## Artefatti previsti

- Sorgenti GitHub automatici della release/tag `1.0.335`.
- Installer Microsoft Windows x64: `Decodium_1.0.335_Setup_x64.exe`.
- DMG/ZIP macOS Apple Silicon prodotti dai runner GitHub.
- DMG/ZIP macOS Intel prodotti dai runner GitHub.
- AppImage Linux x86_64 e Linux aarch64 prodotti dai runner GitHub.

## Verifica locale

Prima della pubblicazione sono stati eseguiti:

```bash
git diff --check
cmake --build build-local-macos --target decodium_qml -j2
cmake --build build-local-macos --target test_qt_helpers -j2
./build-local-macos/tests/test_qt_helpers ftx_special_event_call_messages_avoid_standard_truncation
```
