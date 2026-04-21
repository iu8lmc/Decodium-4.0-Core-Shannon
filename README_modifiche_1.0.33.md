# README modifiche da reimport 1.0.29 a 1.0.33

Data documento: 2026-04-22

Base di confronto:
- tag locale di partenza: `1.0.29`
- commit base: `ecdc83f` (`Release 1.0.29`)
- questo documento riassume il delta locale accumulato dopo il reimport della `1.0.29` fino allo stato attuale preparato per `1.0.33`

Obiettivo della sessione:
- correggere il comportamento di `Solo CQ` senza perdere le chiamate dirette in `Signal RX`
- migliorare robustezza audio, mapping waterfall/panadapter e overlay callsign
- riallineare default UI e impostazioni persistenti
- preparare il pacchetto locale e GitHub release per la versione `1.0.33`

## Modifiche funzionali principali

### 1. Stabilita audio input e riduzione falsi allarmi

File coinvolti:
- `Audio/soundin.cpp`
- `Audio/soundin.h`

Dettagli:
- aggiunto `emitStatusIfChanged(QString, QAudio::State)` per evitare emissioni ripetute dello stesso stato audio
- `handleStateChanged` non tratta piu `IdleState` con `NoError` come errore reale quando il flusso sta semplicemente attraversando una fase transitoria
- introdotta memoria dell'ultimo stato/notifica emessa:
  - `m_lastStatusMessage`
  - `m_lastReportedState`
  - `m_haveReportedState_`
- reset del tracking quando il device audio parte o si ferma

Impatto:
- meno rumore nello stato `Receiving`
- meno segnalazioni spurie lato input audio

### 2. Panadapter e waterfall allineati al viewport reale

File coinvolti:
- `PanadapterItem.cpp`
- `PanadapterItem.hpp`
- `qml/decodium/components/Waterfall.qml`

Dettagli:
- conversione frequenza<->pixel portata sul viewport reale usando `m_startFreq` e `m_bandwidth` quando disponibili
- gestione pulita dei casi in cui il viewport esce dal range FFT disponibile
- colonne fuori range disegnate nere invece di usare bin errati
- range waterfall esteso da `200..3000` a `0..3200`

Impatto:
- mapping grafico piu corretto
- niente letture sbagliate ai bordi del waterfall o del full spectrum

### 3. Overlay nominativi nel waterfall molto piu controllabile

File coinvolti:
- `PanadapterItem.cpp`
- `PanadapterItem.hpp`
- `qml/decodium/components/Waterfall.qml`

Dettagli:
- aggiunte nuove proprieta per lo stile etichette:
  - `labelFontSize`
  - `labelSpacing`
  - `labelBold`
  - `labelColor`
  - `labelUseCustomColor`
- introdotto algoritmo di assegnazione righe anti-sovrapposizione
- aggiunti controlli UI persistenti:
  - dimensione font
  - spaziatura
  - bold
  - preset colore (`Auto`, `Ciano`, `Bianco`, `Giallo`, `Verde`, `Magenta`, `Arancio`)
- nuove chiavi setting:
  - `uiLabelFontSize`
  - `uiLabelSpacing`
  - `uiLabelBold`
  - `uiLabelColorPreset`

Impatto:
- callsign molto piu leggibili sul waterfall
- personalizzazione diretta senza patch manuali

### 4. Fix centrale su `Solo CQ`: filtra il `Full Spectrum`, non blocca `Signal RX`

File coinvolti:
- `DecodiumBridge.cpp`
- `DecodiumBridge.h`
- `qml/decodium/Main.qml`
- `qml/decodium/components/DecodeWindow.qml`

Dettagli:
- separata l'applicazione del filtro CQ-only tra mirror `Band Activity` legacy e pannello RX
- `Signal RX` puo continuare a mostrare:
  - chi ti chiama direttamente
  - chi risponde alle tue chiamate
  - decode rilevanti per il QSO corrente
- quando un decode viene escluso solo dal filtro CQ-only, ma non da altri filtri, viene comunque inoltrato a `appendRxDecodeEntry(entry)`
- `clearDecodeList()` non svuota piu `Signal RX`
- `clearRxDecodes()` traccia le righe eliminate per evitare che il mirror legacy le reinserisca subito
- la ricostruzione del pannello RX ora riordina cronologicamente e fonde `m_rxDecodeList` e `m_decodeList`

Impatto:
- comportamento coerente con la richiesta operativa:
  - `Solo CQ` restringe il `Full Spectrum`
  - `Signal RX` continua a mostrare le risposte reali e le chiamate dirette

### 5. Riorganizzazione bridge, backend legacy e sync dei decode

File coinvolti:
- `DecodiumBridge.cpp`
- `DecodiumBridge.h`

Dettagli:
- aggiunti alias di setting tra chiavi nuove e legacy, inclusi:
  - `ShowDXCC -> DXCCEntity`
  - `ShowCountryNames -> AlwaysShowCountryNames`
  - `InsertBlankLine -> InsertBlank`
  - `TXMessagesToRX -> Tx2QSO`
- aggiunto segnale `settingValueChanged(QString key, QVariant value)` per aggiornamenti live verso QML
- aggiunto `Q_INVOKABLE bool openAllTxtFolder() const`
- introdotto `shutdownUdpMessageClient()` per spegnere il client standalone quando il backend legacy lo rende ridondante
- aggiunto tracking `m_legacyClearedRxMirrorKeys`
- `legacyIniPath()` ora preferisce `decodium4.ini`, con fallback a `ft2.ini`
- `logAllTxtPath()` preferisce il path legacy di `all.txt`, poi AppData `ALL.TXT`, poi fallback locale

Impatto:
- meno incoerenze tra storage legacy e bridge moderno
- meno duplicazioni tra backend legacy e client UDP interno

### 6. Compatibilita WSJT-X UDP e stato runtime piu pulito

File coinvolti:
- `DecodiumBridge.cpp`

Dettagli:
- il client UDP standalone non parte quando il backend legacy e gia disponibile
- su macOS, in presenza del backend legacy, il traffico WSJT-X viene delegato a quello invece di duplicare sender/status
- `initUdpMessageClient()` usa ora:
  - client id `WSJTX`
  - `revision()` reale invece di `"0"`
- collegati `callsignChanged` e `gridChanged` agli update di stato UDP
- `udpSendStatus()` e `udpSendDecode()` vengono soppressi se il backend legacy e gia attivo
- `udpSendStatus()` include report reale, TR period e avanzamento QSO

Impatto:
- meno collisioni e doppie trasmissioni di stato verso l'ecosistema WSJT-X

### 7. Registrazione WAV per TX e TUNE

File coinvolti:
- `DecodiumBridge.cpp`

Dettagli:
- aggiunte utility:
  - `expandedLocalFilePath`
  - `ensureParentDirectoryForFile`
  - `writeMono16WavFile`
  - `buildTxRecordingPath`
  - `buildTuneWaveform`
  - `buildTxWaveformForMessage`
- sia nel path legacy sia in quello non-legacy, se `m_recordTxEnabled` e attivo, vengono generate registrazioni WAV per TX/TUNE
- i record visual TX salvano anche tempo slot normalizzato e `mode`

Impatto:
- registrazione TX piu prevedibile e uniforme

### 8. UI: rinomina pannelli, default utili e refresh live dei setting

File coinvolti:
- `qml/decodium/Main.qml`
- `qml/decodium/components/DecodeWindow.qml`
- `qml/decodium/components/SettingsDialog.qml`
- `dist-windows-x64/qml/decodium/components/SettingsDialog.qml`

Dettagli:
- `Band Activity` rinominato in `Full Spectrum`
- `RX Frequency` / `RX Freq` rinominato in `Signal RX`
- aggiunti binding live verso `bridge.settingValueChanged`
- introdotte proprieta QML:
  - `showDxccInfo`
  - `showTxMessagesInRx`
- il pannello RX puo nascondere le righe TX quando `TXMessagesToRX` e disattivo
- colonne DXCC/Azimuth rese condizionali e portate a width zero quando la visualizzazione DXCC e disabilitata
- il comando "Open all.txt" passa dal bridge invece di usare URL hardcoded
- sistemata la combo `PTT Method` per risolvere correttamente indice e fallback display
- combo `Audio Output Channel` estesa a `Mono`, `Left`, `Right`, `Both`

Default aggiornati:
- `ShowDXCC`: `true`
- `InsertBlankLine`: `true`
- `DetailedBlank`: `true`
- `TXMessagesToRX`: `true`

Nota:
- il default `Show DXCC` e stato allineato anche nel bundle QML Windows

### 9. `Show DXCC` sempre attivo di default su configurazioni nuove

File coinvolti:
- `DecodiumBridge.cpp`
- `qml/decodium/components/SettingsDialog.qml`
- `dist-windows-x64/qml/decodium/components/SettingsDialog.qml`

Dettagli:
- `getSetting()` usa ora un default effettivo che restituisce `true` quando `ShowDXCC` o l'alias legacy `DXCCEntity` non esistono ancora
- il dialog impostazioni mostra coerentemente il default a `true`

Impatto:
- nuove installazioni e nuovi profili partono con DXCC visibile senza richiedere interventi manuali

### 10. Affinamenti UI secondari

File coinvolti:
- `qml/decodium/components/InfoDialog.qml`
- `qml/decodium/Main.qml`
- `qml/decodium/components/DecodeWindow.qml`

Dettagli:
- `InfoDialog` ampliato da `680x580` a `760x620`
- aggiunta riga credito sviluppatore `Salvatore Raccampo 9H1SR`
- card contatti/feedback rese responsive
- `TextArea` feedback con padding esplicito
- layout RX piu compatto con margini/colonne ritoccati
- messaggio di hint aggiornato per riferirsi al `Full Spectrum`
- ripulito un blocco menu/help non piu allineato alla UI corrente

## Modifiche operative e di file system

### 11. Export, percorsi e cartelle gestite in modo piu robusto

File coinvolti:
- `DecodiumBridge.cpp`

Dettagli:
- `exportCabrillo()` espande il path locale, crea la cartella padre se manca e mostra sempre il path finale corretto
- aggiunto `openAllTxtFolder()` per aprire direttamente la cartella del log `all.txt`

Impatto:
- meno errori silenziosi su export e apertura cartelle

## Delta file e dimensioni

File toccati nel delta locale:
- `Audio/soundin.cpp`
- `Audio/soundin.h`
- `DecodiumBridge.cpp`
- `DecodiumBridge.h`
- `PanadapterItem.cpp`
- `PanadapterItem.hpp`
- `dist-windows-x64/qml/decodium/components/SettingsDialog.qml`
- `qml/decodium/Main.qml`
- `qml/decodium/components/DecodeWindow.qml`
- `qml/decodium/components/InfoDialog.qml`
- `qml/decodium/components/SettingsDialog.qml`
- `qml/decodium/components/Waterfall.qml`

Numeri del delta rispetto a `1.0.29`:
- `1196` inserimenti
- `586` cancellazioni

Suddivisione per file:
- `Audio/soundin.cpp`: `+90 / -60`
- `Audio/soundin.h`: `+17 / -13`
- `DecodiumBridge.cpp`: `+519 / -77`
- `DecodiumBridge.h`: `+17 / -13`
- `PanadapterItem.cpp`: `+145 / -75`
- `PanadapterItem.hpp`: `+56 / -26`
- `dist-windows-x64/qml/decodium/components/SettingsDialog.qml`: `+1 / -1`
- `qml/decodium/Main.qml`: `+43 / -192`
- `qml/decodium/components/DecodeWindow.qml`: `+32 / -14`
- `qml/decodium/components/InfoDialog.qml`: `+51 / -40`
- `qml/decodium/components/SettingsDialog.qml`: `+82 / -65`
- `qml/decodium/components/Waterfall.qml`: `+143 / -10`

## Stato versione e rilascio

Allineamenti eseguiti per la release `1.0.33`:
- `fork_release_version.txt` aggiornato a `1.0.33`
- `installer/decodium_setup.iss` aggiornato a `1.0.33`
- workflow Windows aggiornato per accettare anche tag semver nudi come `1.0.33`
- documentazione SignPath corretta per il caso di release firmata con tag `v<version>`

Uso previsto:
- tag `1.0.33`: build/release Windows unsigned via workflow `Build Windows x64`
- tag `v1.0.33`: path SignPath/release firmata, se e quando i secret sono configurati
