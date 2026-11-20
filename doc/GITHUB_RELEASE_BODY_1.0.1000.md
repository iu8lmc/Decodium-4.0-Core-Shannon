# Decodium 4 FT2 1.0.1000

## English

### Why version 1.0.1000?

The jump from 1.0.738 to 1.0.1000 marks a substantial internal milestone: the Decodium Bridge has been split into smaller, focused source files. It is a versioning choice to make that restructuring visible; it does not represent 262 intermediate releases or imply that all future work is complete.

### Changes since 1.0.737

- **Bridge restructuring:** the main `DecodiumBridge.cpp` has been reduced from approximately 62,900 lines to approximately 700. Its responsibilities are organised into focused compilation units, including initialisation, settings and profiles, the legacy backend, CAT and rig telemetry, audio capture and playback, TX scheduling, decoding and filtering, sequencers, QSO and ADIF logging, networking, maps, satellite support, FT2-Link and SSTV. Build definitions include the extracted units in both the QML and legacy application targets. Existing functionality and interfaces are intended to remain compatible.
- **Startup and profiles:** defer embedded legacy initialisation until the main QML interface is ready, with a fallback for startup failures. Allow the waterfall and map staging to advance earlier when monitoring and decoding are inactive. Avoid recursively copying other profiles when initialising a settings profile, preventing excessive settings-file growth. Add startup timing diagnostics and isolate named INI settings in test mode.
- **JT65 decoding:** correct the direction of a native FFT used in the JT65 filter. The correction was compared with the earlier behaviour and an independent WSJT-X reference recording; the forced-native laboratory path recovered the expected messages.
- **TX and legacy audio:** correct HALT/stop handling for bridge-owned CW, FT2-Link and telemetry in legacy TX ownership, and clear the CW active state on natural modulator completion. Use a precise timer for a slot-aligned TX retry. These are deliberate behaviour fixes alongside the refactoring; physical PTT timing has not been established by the virtual-audio checks.
- **SSTV reception:** prevent an invalidated receive pipeline from publishing an old partial image again after a stream reset or source switch. Existing SSTV, Radiofax, HAMDRM and encrypted sharing features from 1.0.737 remain in scope; this release does not claim complete interoperability with every third-party implementation.
- **Build and maintenance:** include the 1.0.737 CI warning-gate correction and the 1.0.738 upstream version-alignment commit. Update build definitions, test sources, laboratory scripts, pre-extraction fixtures and retained A/B evidence for the split Bridge. Windows packaging retains its OpenSSL DLL deployment and installer checks.

### Validation and known limits

The repository contains previous source comparisons, focused A/B checks and functional runs, including two isolated Decodium sessions over virtual audio. Examples include FT8 and FT4 decoding, Auto Call and Target Call selection, QSO/ADIF handling, legacy startup, CW stop and completion, SSTV calibration and a 225-byte Decodium-to-Decodium HAMDRM transfer. The FT8 waveform comparison recorded 606,720 samples at 48 kHz, or 12.640 seconds of generated audio; this does not measure relay or RF duration. Coverage of every moved function has not been established, and the Bridge verification goal remains paused. No additional verification was performed for this release at the user's request.

Earlier Radiofax/FAX480 and Robot C12 comparisons retain documented limitations, including an independent QSSTV FAX480 comparison outside the chosen threshold and intermittent incomplete or edge-affected synthetic images. The Hound/SuperFox RR73 issue described in 1.0.738 is not claimed as fixed. Virtual audio does not demonstrate over-the-air performance or complete EasyPal/QSSTV interoperability.

## Italiano

### Perché la versione 1.0.1000?

Il salto dalla 1.0.738 alla 1.0.1000 segnala un importante traguardo interno: il Bridge di Decodium è stato suddiviso in file sorgente più piccoli e dedicati. È una scelta di numerazione per rendere visibile la riorganizzazione; non rappresenta 262 release intermedie e non significa che ogni attività futura sia conclusa.

### Modifiche dalla 1.0.737

- **Riorganizzazione del Bridge:** il file principale `DecodiumBridge.cpp` è passato da circa 62.900 a circa 700 righe. Le responsabilità sono state organizzate in unità dedicate, tra cui inizializzazione, impostazioni e profili, backend legacy, CAT e telemetria, acquisizione e riproduzione audio, pianificazione TX, decodifica e filtri, sequencer, QSO e log ADIF, rete, mappe, satelliti, FT2-Link e SSTV. Le definizioni di build includono i file estratti sia nel programma QML sia nel target legacy. L'obiettivo è mantenere compatibili funzioni e interfacce esistenti.
- **Avvio e profili:** inizializzazione legacy integrata rinviata alla disponibilità dell'interfaccia QML principale, con ripiego in caso di problemi di avvio. Waterfall e mappa possono avanzare prima quando monitor e decodifica sono inattivi. La creazione di un profilo non copia più ricorsivamente gli altri profili, evitando la crescita eccessiva del file delle impostazioni. Aggiunti strumenti di misurazione dell'avvio e isolamento delle impostazioni INI nominate in modalità test.
- **Decodifica JT65:** corretta la direzione di una FFT usata dal filtro JT65 nativo. La correzione è stata confrontata con il comportamento precedente e con una registrazione di riferimento WSJT-X indipendente; il percorso di laboratorio con backend nativo forzato ha recuperato i messaggi attesi.
- **TX e audio legacy:** corretto HALT/stop per CW, FT2-Link e telemetria gestiti dal Bridge con proprietà TX legacy; lo stato CW attivo viene inoltre azzerato al completamento naturale del modulatore. Un timer preciso gestisce il nuovo tentativo TX allineato allo slot. Sono correzioni intenzionali insieme alla riorganizzazione; le prove audio virtuali non misurano i tempi del PTT fisico.
- **Ricezione SSTV:** una pipeline di ricezione invalidata non ripubblica più una vecchia immagine parziale dopo il reset dello stream o il cambio della sorgente. Restano disponibili le funzionalità SSTV, Radiofax, HAMDRM e condivisione cifrata della 1.0.737; questa release non dichiara interoperabilità completa con ogni applicazione esterna.
- **Build e manutenzione:** incluse la correzione delle verifiche di compilazione CI successiva alla 1.0.737 e l'allineamento upstream alla 1.0.738. Aggiornati definizioni di build, test, strumenti di laboratorio, fixture precedenti all'estrazione e risultati A/B conservati. Il pacchetto Windows mantiene la distribuzione delle DLL OpenSSL e i controlli dell'installer.

### Prove e limiti noti

Il repository contiene confronti del codice, prove A/B mirate e verifiche funzionali precedenti, incluse due sessioni Decodium isolate collegate con audio virtuale. Fra gli esempi: decodifica FT8 e FT4, selezione Auto Call e Target Call, gestione QSO/ADIF, avvio legacy, arresto e completamento CW, calibrazione SSTV e trasferimento HAMDRM di 225 byte tra due Decodium. Il confronto della forma d'onda FT8 ha registrato 606.720 campioni a 48 kHz, pari a 12,640 secondi di audio generato; non misura la durata del relè o del segnale RF. La copertura di ogni funzione spostata non è stata dimostrata e il goal di verifica del Bridge resta in pausa. Su richiesta dell'utente non sono state svolte verifiche aggiuntive per questa release.

Restano documentati i limiti delle precedenti prove Radiofax/FAX480 e Robot C12, incluso il confronto FAX480 con QSSTV fuori dalla soglia scelta e immagini sintetiche talvolta incomplete o con errori al bordo. Il problema Hound/SuperFox RR73 descritto nella 1.0.738 non viene dichiarato risolto. L'audio virtuale non dimostra prestazioni via radio né interoperabilità completa con EasyPal/QSSTV.
