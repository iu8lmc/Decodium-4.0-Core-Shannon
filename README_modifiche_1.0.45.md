# README modifiche da 1.0.44 a 1.0.45

Data documento: 2026-04-25

Base di confronto:
- tag di partenza: `1.0.44`
- nuova versione: `1.0.45`
- ramo di lavoro locale: `win`
- target di pubblicazione richiesto: `origin/main`

Obiettivo della sessione:
- stabilizzare Decodium4 dopo l'import delle modifiche da Decodium3 e dal ramo `origin/win`
- correggere regressioni su audio, decode FT8/FT4, CAT, web app, Live Map, MAM queue e impostazioni
- ripristinare un comportamento multipiattaforma coerente su Windows, macOS e Linux
- preparare una release Windows generata dal runner GitHub

## Sintesi tecnica

La 1.0.45 non e' una release cosmetica. Il lavoro principale e' stato fatto su:

- catena audio RX/TX e interazione con il decoder legacy embedded
- CAT/Hamlib/OmniRig/TCI e persistenza delle impostazioni seriali
- web app e comandi remoti frequenza/banda/modo
- Live Map, PSK Reporter e mappa dei path TX/RX
- impostazioni QML, menu macOS, dialog setup e controlli che non avevano feedback
- robustezza Windows/Linux, inclusi AppImage, scroll, massimizzazione finestra e release workflow

## 1. Versione e release

File coinvolti:
- `fork_release_version.txt`
- `.github/workflows/build-windows-x64.yml`
- `.github/workflows/build-sign-release.yml`

Modifiche:
- versione fork aggiornata da `1.0.44` a `1.0.45`
- il workflow Windows resta agganciato ai tag numerici `x.y.z`, quindi il tag `1.0.45` genera l'installer Windows dal runner
- il job SignPath del workflow Windows viene eseguito solo sui tag `v*`
- il workflow SignPath separato non parte piu su ogni push a `main`, evitando fallimenti immediati quando i segreti SignPath non sono configurati

Impatto:
- tag `1.0.45` produce release GitHub con codebase e installer Windows non firmato
- i push a `main` non vengono piu marcati falliti da un workflow di firma non configurato

## 2. Audio RX: eliminazione doppia cattura e fix livelli

File coinvolti:
- `Audio/AudioDevice.cpp`
- `Audio/AudioDevice.hpp`
- `Audio/soundin.cpp`
- `Audio/soundin.h`
- `DecodiumAudioSink.h`
- `Detector/Detector.cpp`
- `Detector/Detector.hpp`
- `widgets/mainwindow.cpp`
- `widgets/mainwindow.h`
- `DecodiumBridge.cpp`
- `DecodiumBridge.h`
- `DecodiumLegacyBackend.cpp`
- `DecodiumLegacyBackend.h`

Problema:
- dopo alcune modifiche il waterfall era veloce con il doppio ricevitore, ma il rischio era aprire due stream audio sullo stesso device
- su Windows/Linux questa condizione puo portare a buffer concorrenti, livelli non coerenti e segnali degradati
- il primo tentativo di velocizzare il panadapter riducendo il block-size del detector ha reso fluida la grafica, ma poteva impedire l'avvio decode perche il decoder legacy usa il passo del detector per arrivare a `m_hsymStop`

Modifiche:
- il decoder torna a usare il suo block-size originale
- aggiunto un tap PCM separato nel `Detector`, usato solo per alimentare panadapter/waterfall QML
- il tap visuale campiona i dati gia selezionati dal canale audio corretto, senza aprire un secondo `QAudioSource`
- il bridge riceve il feed PCM legacy e lo mette nel ring buffer FFT della UI moderna
- quando il backend legacy e' attivo, il feed grafico moderno non riapre una cattura audio parallela
- il livello RX in embedded viene limitato a massimo gain unitario, evitando clipping quando lo slider UI supera il punto neutro
- la variazione del livello RX viene riapplicata al detector anche se il valore cambia poco, per evitare stati UI/engine divergenti
- `SoundInput` mantiene volume Qt a `1.0` e delega il gain reale al sink/detector
- gestione piu robusta di `IdleState` e reset audio Qt
- su macOS il `QAudioSource` viene distrutto in ritardo dopo `stop()`, per ridurre crash CoreAudio/QtMultimedia su listener device disconnect

Impatto:
- niente doppio ricevitore di default
- waterfall/panadapter fluido senza rompere il decode
- FT8/FT4/FT2 non dipendono piu dal ritmo grafico
- riduzione del rischio di segnali bassi o appiattiti per clipping o stream duplicati
- il crash CoreAudio in chiusura/stop audio viene mitigato

## 3. Decode FT8/FT4/FT2 e confronto con Decodium3

File coinvolti:
- `Detector/FtxFt8Stage4.cpp`
- `Detector/Detector.cpp`
- `Detector/Detector.hpp`
- `widgets/mainwindow.cpp`
- `DecodiumBridge.cpp`
- `DecodiumBridge.h`

Problemi analizzati:
- FT8 su Windows, soprattutto in `DEEP`, riceveva segnale ma non decodificava
- FT4 segnalato da alcuni utenti con problema simile
- FT2 risultava generalmente esente
- dopo alcuni TX il programma sembrava non decodificare piu
- alcuni test mostravano D4 con segnali molto piu bassi di D3

Modifiche:
- preservato il flusso legacy del detector per le condizioni di avvio decode
- separato il feed panadapter dalla logica di decode
- ridotta la possibilita che un decode venga bloccato da stato busy rimasto appeso
- mantenuto il path async/worker per FT8/FT4/FT2 senza alterare il buffer condiviso in modo pericoloso
- aggiunti controlli e confronto sulle finestre temporali `ALL.TXT` D3/D4
- verificato che dopo il fix RX a 75 D4 non mostra regressioni: nell'intervallo `2026-04-24 22:58:30..23:00:00 UTC` D4 ha prodotto piu decode di D3

Risultato misurato su `ALL.TXT`, `2026-04-24 22:58:30..23:00:00 UTC`:
- D3: 187 decode totali, 55 CQ, 68 call unici
- D4: 223 decode totali, 60 CQ, 76 call unici
- decode comuni esatti: 184
- delta SNR D4-D3 sui comuni: media `+0.08 dB`, mediana `0 dB`
- delta AF D4-D3 sui comuni: praticamente nullo

Impatto:
- nessuna regressione audio/decode evidente rispetto a D3 nell'ultimo test
- se una riga non appare in UI ma esiste in `ALL.TXT`, il problema e' nel mirror/filtro UI, non nel decoder

## 4. Audio TX/RX dopo lunga esecuzione

File coinvolti:
- `Audio/soundin.cpp`
- `Audio/soundin.h`
- `Modulator/Modulator.cpp`
- `DecodiumBridge.cpp`
- `widgets/mainwindow.cpp`

Problemi:
- utente Windows con audio che dopo circa un'ora si appiattiva in RX
- in TX il waterfall mostrava forma quasi quadra
- l'utente era costretto a riavviare Decodium

Modifiche:
- migliorata gestione degli stati `Idle`, `Suspended`, `Stopped` del device audio
- ridotto il rischio di reset aggressivi su transizioni benign
- preservata la distinzione tra sospensione attesa e stop reale
- pulizia piu esplicita dei buffer e del ring spectrum quando si passa tra monitor, TX e RX
- il gain audio non viene piu moltiplicato in modo incontrollato sul path embedded

Impatto:
- meno probabilita di stream audio che resta in stato incoerente dopo molte transizioni
- minore rischio di RX appiattito o TX saturato dopo lunga esecuzione

## 5. CAT seriale, RTS/DTR e compatibilita Decodium3

File coinvolti:
- `Configuration.cpp`
- `Transceiver/HamlibTransceiver.cpp`
- `DecodiumCatManager.cpp`
- `DecodiumCatManager.h`
- `DecodiumTransceiverManager.cpp`
- `DecodiumTransceiverManager.h`
- `qml/decodium/components/RigControlDialogContent.qml`
- `dist-windows-x64/qml/decodium/components/RigControlDialogContent.qml`

Problemi:
- RTS/DTR in Decodium4 non seguivano sempre la semantica di Decodium3
- quando PTT era CAT/RTS alcune opzioni risultavano bloccate o interpretate in modo diverso
- baud rate e stop bits potevano essere salvati ma visualizzati vuoti o errati al riavvio

Modifiche:
- riallineata la semantica Decodium3:
  - vuoto/default: non forza la linea
  - ON/High: forza linea alta/asserted
  - OFF/Low: forza linea bassa/deasserted
- il blocco UI viene gestito come indisponibilita della scelta, non come quarto stato logico
- persistenza corretta di baud rate
- persistenza corretta di stop bits, inclusa visualizzazione del valore `2`
- migliorata inizializzazione dei combo seriali quando il valore salvato non e' gia presente nel modello
- le opzioni CAT restano coerenti su Windows, macOS e Linux

Impatto:
- comportamento piu vicino a Decodium3
- meno rischio di configurazioni radio seriali mostrate vuote dopo riavvio
- setup piu prevedibile per interfacce USB/seriali

## 6. Ricerca radio Hamlib e scroll dei modelli

File coinvolti:
- `qml/decodium/components/RigControlDialogContent.qml`
- `dist-windows-x64/qml/decodium/components/RigControlDialogContent.qml`
- `DecodiumCatManager.cpp`
- `DecodiumCatManager.h`

Problemi:
- la lista radio Hamlib contiene centinaia di modelli
- su Linux AppImage lo scroll con mouse nella finestra modelli non era affidabile
- serviva una ricerca full text

Modifiche:
- aggiunto campo di ricerca testuale per i modelli radio
- filtro full text lato UI sui nomi dei rig
- migliorato il comportamento del popup combo per renderlo piu usabile su Linux/Windows

Impatto:
- selezione radio piu veloce
- meno dipendenza dallo scroll lungo del menu

## 7. OmniRig, Hamlib e TCI

File coinvolti:
- `DecodiumOmniRigManager.cpp`
- `DecodiumOmniRigManager.h`
- `DecodiumCatManager.cpp`
- `DecodiumTransceiverManager.cpp`
- `Transceiver/TCITransceiver.cpp`
- `qml/decodium/CatSettingsDialog.qml`
- `dist-windows-x64/qml/decodium/CatSettingsDialog.qml`

Problemi:
- OmniRig doveva comportarsi come in Decodium3
- TCI/ExpertSDR poteva collegarsi ma non sempre impostava correttamente modo e frequenza
- alcune radio partivano in modo USB normale invece di Data/USB-D quando necessario

Modifiche:
- copiato e riallineato il comportamento di connessione OmniRig rispetto al modello Decodium3
- migliorato il passaggio di stato CAT tra backend nativo, Hamlib, TCI e OmniRig
- separata la modalita radio dalla modalita applicativa Decodium, evitando che stati come `FSK-R` sovrascrivano `FT8`
- migliorata selezione automatica del modo digitale quando la frequenza radio corrisponde a una frequenza FT8/FT4/FT2 nota
- status CAT piu esplicito in UI

Impatto:
- meno casi in cui il waterfall riceve segnale ma il programma non entra nel modo decode corretto
- piu coerenza con Decodium3 su OmniRig

## 8. Web app: frequenza, banda e modo

File coinvolti:
- `Network/RemoteCommandServer.cpp`
- `Network/RemoteCommandServer.hpp`
- `DecodiumBridge.cpp`
- `DecodiumBridge.h`
- `DecodiumSubManagers.h`

Problema:
- dalla web app il modo cambiava, ma frequenza e banda tornavano indietro dopo pochi secondi
- Decodium4 sembrava trattenere/override la frequenza corrente

Modifiche:
- aggiunti e corretti handler remoti per cambio modo, frequenza e banda
- ridotta la possibilita che il poll CAT sovrascriva subito un comando remoto appena ricevuto
- migliorata sincronizzazione tra stato remoto, bridge e transceiver manager
- aggiornati feedback/ack dei comandi remoti

Impatto:
- web app piu coerente: modo, frequenza e banda seguono il comando remoto invece di rimbalzare al valore precedente

## 9. PWR, SWR e blocco TX per SWR alto

File coinvolti:
- `DecodiumBridge.cpp`
- `DecodiumBridge.h`
- `DecodiumTransceiverManager.cpp`
- `DecodiumTransceiverManager.h`
- `qml/decodium/components/StatusBar.qml`
- `dist-windows-x64/qml/decodium/components/StatusBar.qml`
- `qml/decodium/components/RigControlDialogContent.qml`

Problemi:
- `PWR` e `SWR` non arrivavano in dashboard
- la barra di stato non mostrava potenza/SWR come in Decodium3
- `Check SWR` doveva impedire il TX se SWR e' maggiore di 3

Modifiche:
- propagati valori power/SWR dal CAT al bridge e alla status bar
- aggiunta visualizzazione PWR/SWR in dashboard/status bar
- `Check SWR` viene mantenuto come impostazione diagnostica
- aggiunta logica per impedire TX quando `Check SWR` e' attivo e SWR supera la soglia operativa

Impatto:
- comportamento piu vicino a Decodium3
- maggiore protezione in TX quando la radio fornisce SWR via CAT diretto

## 10. PSK Reporter: nome programma e versione

File coinvolti:
- `Network/DecodiumPskReporterLite.cpp`
- `Network/DecodiumPskReporterLite.h`
- `DecodiumBridge.cpp`

Problema:
- su PSK Reporter non veniva inviato il nome programma/versione come Decodium3

Modifiche:
- aggiornato payload PSK Reporter per inviare programma e versione
- valore atteso: `Decodium4` + versione corrente, ad esempio `1.0.45`

Impatto:
- gli spot PSK Reporter identificano correttamente il client Decodium4

## 11. Live Map e World Map

File coinvolti:
- `widgets/worldmapwidget.cpp`
- `DecodiumBridge.cpp`
- `DecodiumBridge.h`
- `qml/decodium/components/LiveMapPanel.qml`
- `dist-windows-x64/qml/decodium/components/LiveMapPanel.qml`
- `qml/decodium/Main.qml`
- `dist-windows-x64/qml/decodium/Main.qml`

Problemi:
- dopo l'import da Decodium3 e il merge di `origin/win`, i pallini delle persone in frequenza non comparivano piu
- Live Map richiesta come pannello staccabile, spostabile e ridimensionabile
- durante TX non comparivano le frecce/path verso il corrispondente

Modifiche:
- migliorato matching callsign per mappa, includendo normalizzazione call/base call
- aggiunta logica per ricostruire path attivi da TX/RX e QSO corrente
- migliorata estrazione grid/call da messaggi decodificati
- aggiunto supporto per pannello Live Map popup/staccabile
- migliorato refresh dei marker e delle linee path
- fallback su grid locale quando disponibile

Impatto:
- mappe piu reattive
- pallini e path piu coerenti con il contenuto di `Signal RX` e con i TX recenti

## 12. Multi-Answer Mode e coda

File coinvolti:
- `DecodiumBridge.cpp`
- `DecodiumBridge.h`
- `qml/decodium/Main.qml`
- `dist-windows-x64/qml/decodium/Main.qml`
- `qml/decodium/components/TxPanel.qml`
- `dist-windows-x64/qml/decodium/components/TxPanel.qml`

Problemi:
- la queue MAM non si popolava
- `Now` veniva aggiornato, ma `Queue` restava vuota
- dopo alcuni TX il programma sembrava non accettare nuovi decode utili

Modifiche:
- rivista la logica di enqueue dei caller
- separato caller attivo da caller in coda
- migliorato filtro per decode diretti alla propria stazione
- migliorato aggiornamento QML di queue/now
- ridotta deduplica troppo aggressiva sui decode utili

Impatto:
- la coda MAM torna a popolarsi con chiamanti validi
- `Now` e `Queue` sono piu coerenti con il traffico reale

## 13. TX finale, 73 e durata trasmissione

File coinvolti:
- `DecodiumBridge.cpp`
- `Modulator/Modulator.cpp`
- `qml/decodium/components/TxPanel.qml`
- `dist-windows-x64/qml/decodium/components/TxPanel.qml`

Problema:
- il 73 finale poteva durare una frazione di secondo invece di occupare lo slot corretto

Modifiche:
- rivista la gestione dei messaggi TX finali
- migliorata la deduplica visuale delle righe TX senza troncare lo stato reale
- corretto il calcolo tempo slot per righe visuali TX
- maggiore protezione contro stop TX prematuri dopo cambio messaggio

Impatto:
- il 73 finale non dovrebbe piu essere troncato dopo pochi istanti

## 14. Menu macOS: Preferences e Quit

File coinvolti:
- `DecodiumLegacyBackend.cpp`
- `DecodiumLegacyBackend.h`
- `DecodiumBridge.cpp`
- `widgets/mainwindow.cpp`
- `widgets/mainwindow.h`

Problemi:
- `Preferences...` dal menu macOS bloccava il programma
- `Quit Decodium` non usciva correttamente

Modifiche:
- aggiunti segnali legacy dedicati per richiesta preferenze e quit
- il bridge intercetta la richiesta e apre l'equivalente setup QML
- quit passa dal percorso applicativo moderno invece di lasciare il backend legacy appeso

Impatto:
- menu macOS coerente con app Qt/QML moderna

## 15. Settings dialog, OTP, directory e download file dati

File coinvolti:
- `qml/decodium/components/SettingsDialog.qml`
- `dist-windows-x64/qml/decodium/components/SettingsDialog.qml`
- `DecodiumBridge.cpp`
- `DecodiumBridge.h`

Problemi:
- menu OTP in basso quasi illegibile per mancanza di scroll
- click su directory non apriva Finder/Explorer per selezionare cartella
- `Download CALL3.TXT`/`CTY.dat` dava poco o nessun feedback
- `Download all3.txt` mostrava errore generico non utile

Modifiche:
- aumentato spazio scrollabile del dialog impostazioni
- aggiunto supporto selezione cartella tramite file dialog nativo
- migliorato feedback sui download dei file dati
- validazione del payload `CALL3.TXT` per evitare di salvare pagine HTML/errore
- messaggi di stato/errore piu espliciti

Impatto:
- setup piu usabile su display piccoli
- download file dati piu diagnostico

## 16. Logging, Auto Log e font

File coinvolti:
- `DecodiumBridge.cpp`
- `DecodiumBridge.h`
- `qml/decodium/components/SettingsDialog.qml`
- `dist-windows-x64/qml/decodium/components/SettingsDialog.qml`

Problemi:
- menu Logging mostrava `Prompt to Log` anche quando il comportamento sembrava `Auto Log`
- campi `Font` e `Decoded Font` sembravano non funzionare

Modifiche:
- aggiunti alias setting per chiavi legacy/nuove di logging
- sincronizzati `Prompt to Log`, `Auto Log`, `Direct Log QSO` e opzioni commenti
- aggiunto supporto font picker e fallback font
- migliorata persistenza delle scelte font in QML/bridge

Impatto:
- menu Logging piu coerente con il comportamento reale
- font UI/decoded text configurabili

## 17. Livelli RX/TX in dashboard e setup

File coinvolti:
- `qml/decodium/components/StatusBar.qml`
- `dist-windows-x64/qml/decodium/components/StatusBar.qml`
- `qml/decodium/components/SettingsDialog.qml`
- `DecodiumBridge.cpp`

Problema:
- indicatori livelli dashboard e setup non erano collegati tra loro

Modifiche:
- sincronizzati valori RX input level e TX output level tra dashboard, setup e bridge
- normalizzazione dei valori UI verso il gain reale
- feedback coerente quando si cambia slider da una delle due viste

Impatto:
- dashboard e setup mostrano lo stesso stato operativo

## 18. AppImage/Linux e warning QML

File coinvolti:
- `qml/decodium/components/SettingsDialog.qml`
- `dist-windows-x64/qml/decodium/components/SettingsDialog.qml`
- `qml/decodium/components/StyledComboBox.qml`
- `dist-windows-x64/qml/decodium/components/StyledComboBox.qml`

Problemi:
- AppImage Linux mostrava warning QML `Unable to assign [undefined] to QString/int`
- alcuni combo erano inizializzati con valori non definiti

Modifiche:
- aggiunti fallback espliciti per stringhe e interi
- migliorata inizializzazione dei combo e degli spinbox
- evitati binding verso valori `undefined`

Impatto:
- meno errori terminale su Linux
- dialog impostazioni piu stabile

## 19. Massimizzazione finestra su monitor secondari

File coinvolti:
- `qml/decodium/Main.qml`
- `dist-windows-x64/qml/decodium/Main.qml`

Problema:
- su Windows/Linux il pulsante massimizza non funzionava correttamente su alcuni monitor

Modifiche:
- rivista gestione window state e geometria
- migliorato restore dello stato finestra
- maggiore attenzione al monitor/screen corrente

Impatto:
- massimizzazione piu affidabile su multi-monitor

## 20. Bande 2m, 70cm e frequenze satellitari

File coinvolti:
- `qml/decodium/components/BandSelector.qml`
- `dist-windows-x64/qml/decodium/components/BandSelector.qml`
- `DecodiumBridge.cpp`

Problema:
- Decodium3 aveva supporto operativo per VHF/UHF che mancava o era meno visibile in Decodium4

Modifiche:
- aggiunti pulsanti/entry per 2m e 70cm dove applicabile
- predisposizione per frequenze satellitari note quando disponibili nel modello frequenze
- band selector aggiornato per gestire meglio frequenze fuori HF classiche

Impatto:
- uso VHF/UHF piu vicino a Decodium3

## 21. Pulizia QML duplicata Windows

File coinvolti:
- `qml/...`
- `dist-windows-x64/qml/...`

Dettagli:
- molte modifiche QML sono state replicate sia nella sorgente principale `qml/` sia nella copia `dist-windows-x64/qml/`
- questo mantiene coerente il bundle Windows creato dal runner

Impatto:
- il pacchetto Windows contiene le stesse correzioni della UI locale

## 22. File principali modificati

Core/audio/decode:
- `Audio/AudioDevice.cpp`
- `Audio/AudioDevice.hpp`
- `Audio/soundin.cpp`
- `Audio/soundin.h`
- `DecodiumAudioSink.h`
- `Detector/Detector.cpp`
- `Detector/Detector.hpp`
- `Detector/FtxFt8Stage4.cpp`
- `Modulator/Modulator.cpp`
- `widgets/mainwindow.cpp`
- `widgets/mainwindow.h`

Bridge/backend:
- `DecodiumBridge.cpp`
- `DecodiumBridge.h`
- `DecodiumLegacyBackend.cpp`
- `DecodiumLegacyBackend.h`
- `DecodiumSubManagers.h`

CAT/radio:
- `DecodiumCatManager.cpp`
- `DecodiumCatManager.h`
- `DecodiumTransceiverManager.cpp`
- `DecodiumTransceiverManager.h`
- `DecodiumOmniRigManager.cpp`
- `DecodiumOmniRigManager.h`
- `Transceiver/HamlibTransceiver.cpp`
- `Transceiver/TCITransceiver.cpp`

Network/reporting:
- `Network/RemoteCommandServer.cpp`
- `Network/RemoteCommandServer.hpp`
- `Network/DecodiumPskReporterLite.cpp`
- `Network/DecodiumPskReporterLite.h`

QML/UI:
- `qml/decodium/Main.qml`
- `qml/decodium/CatSettingsDialog.qml`
- `qml/decodium/components/BandSelector.qml`
- `qml/decodium/components/LiveMapPanel.qml`
- `qml/decodium/components/RigControlDialog.qml`
- `qml/decodium/components/RigControlDialogContent.qml`
- `qml/decodium/components/SettingsDialog.qml`
- `qml/decodium/components/StatusBar.qml`
- `qml/decodium/components/StyledComboBox.qml`
- `qml/decodium/components/TxPanel.qml`
- `qml/dialogs/QSYDialog.qml`

Bundle Windows QML:
- `dist-windows-x64/qml/decodium/...`
- `dist-windows-x64/qml/dialogs/QSYDialog.qml`

Mappe:
- `widgets/worldmapwidget.cpp`

Release:
- `fork_release_version.txt`
- `.github/workflows/build-windows-x64.yml`
- `.github/workflows/build-sign-release.yml`

## 23. Verifiche locali effettuate

Build locale:
- target: `decodium_qml`
- comando: `cmake --build /Users/salvo/Desktop/Decodium4-build --parallel 4 --target decodium_qml`
- risultato: build completata con successo

Controlli diff:
- `git diff --check` eseguito sui file toccati durante i fix audio/panadapter
- nessun errore whitespace bloccante

Confronto decode:
- analisi `ALL.TXT` D3/D4 su piu finestre UTC del 24 aprile 2026
- ultima verifica `22:58:30..23:00:00 UTC`: D4 superiore a D3 per numero decode totale e CQ

## 24. Note operative per test post-release

Test consigliati:
- FT8 su Windows con `DEEP` attivo per almeno 20 minuti
- FT4 su Windows con segnale reale e monitor continuo
- RX dopo TX ripetuti in AutoCQ/MAM
- cambio frequenza/banda/modo da web app
- CAT Hamlib con IC-7300: baud rate, stop bits, Data/Pkt, PWR/SWR
- OmniRig con setup Decodium3 equivalente
- AppImage Linux: apertura setup, scroll rig list, warning QML
- Live Map: marker, popup, frecce path durante TX/RX

Rischi residui:
- il runner Windows produce installer non firmato per il tag numerico `1.0.45`
- SignPath richiede segreti GitHub configurati e resta limitato ai tag `v*`
- eventuali problemi non presenti in `ALL.TXT` ma visibili in UI vanno cercati nel mirror QML/bridge, non nel decoder
