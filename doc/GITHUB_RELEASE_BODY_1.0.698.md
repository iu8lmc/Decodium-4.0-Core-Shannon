# Decodium 4 v1.0.698

## English (British)

Decodium 4 v1.0.698 is the maintained fork release following v1.0.697. It focuses on dependable radio control, safe serial-line handling, reliable transitions between CW/legacy and FT/Q65 decoding, recovery of the GPU spectrum path, and a more usable Liquid Glass interface without compromising the classic themes.

### CAT, PTT and radio-control reliability

- Added a versioned CAT configuration snapshot. Changes to the rig profile, CAT endpoint, serial parameters, CI-V address, split mode, PTT method, PTT port, TCI configuration or polling are now coalesced into a controlled reconnect rather than allowing a partially updated connection to be used.
- Reset remembered physical-radio identity whenever the selected profile or endpoint changes, so a reconnect cannot incorrectly inherit the identity of a previous radio or port.
- Made the opening of a serial CAT port electrically safer on Windows, macOS and Linux. Unused DTR and RTS lines are explicitly kept inactive before Hamlib opens a serial device, while deliberate DTR/RTS PTT wiring and hardware RTS/CTS handshaking are preserved.
- Prevented a stale or invalid saved PTT selection from silently becoming the first serial port listed by the operating system. A separate serial PTT method now asks the operator to select a real physical port instead of risking a connection to unrelated hardware.
- Moved optional CAT functions into the common `Transceiver` interface and forwarded them through the Fake It split wrapper. CW keying, ATU controls and radio power controls therefore remain available when the selected split mode wraps Hamlib, without implementation-specific casts.
- Corrected Fake It state handling so that only the expected transmit-VFO echo is suppressed; a genuine operator QSY remains visible while transmitting.
- Hardened Hamlib startup and recovery: failed opens close their serial device, null capability data is handled safely, and controls are exposed only when the selected rig genuinely supports them.
- Improved radios which retain CAT while powered off or in standby. Decodium keeps the viable CAT session for a supported Power On command, pauses unsuitable VFO polling, and detects a front-panel power-on with a bounded low-rate probe before resuming normal polling.
- Disabled unavailable ATU and power actions in the main dashboard and SSB controls instead of presenting commands which would inevitably fail.

### Decode engine, TX safety and spectrum recovery

- Added an explicit audio-capture hand-off policy for mode changes. CW, RTTY/JTTY, SSB, FT modes and Q65 now release or retain the correct audio owner before monitoring is restarted, avoiding a stale legacy monitor or competing capture path after a mode switch.
- Completed the native-to-legacy and legacy-to-native receiver hand-offs, including the bridge-owned Q65 route. This addresses the class of failures where the panadapter and waterfall stopped updating after changing from CW to FT8/FT4 or changing band.
- Strengthened GPU recovery after a source hand-off. Decodium invalidates and rebuilds the Qt Quick/GPU texture path first, performs bounded GPU retries, and defers the CPU/software visual fallback until those retries have genuinely been exhausted.
- Added lifecycle-generation guards to deferred TX work. HALT, QSO completion, QSO reset and AutoCQ changes invalidate outstanding timer/slot callbacks, preventing an old TX6 or CQ request from transmitting after AutoCQ has been switched off.
- Routed text entered through the generic transmit panel to JTTY's own frame generator and shared PTT/audio lifecycle, instead of incorrectly attempting a digital-audio mode unsupported by the audio bridge.

### Q65 continuity and submode safety

- Preserved the explicit Q65 period/submode selected by the interface throughout the bridge and decoder path, including compatibility migration of a legacy plain `Q65` setting to `Q65-60A`.
- Reset coherent averaging for a normal Q65 receive window unless the operator has explicitly enabled it, avoiding data from an earlier station or mode contaminating subsequent decodes.
- Bounded Q65 decode depth and added dispatch diagnostics so a selected Q65 variant can be traced without relying on ambiguous UI state.
- Added regression coverage for three consecutive Q65 receive windows and for both the compatibility default `Q65-60A` and non-default `Q65-60B` paths.

### Liquid Glass, windows and readability of working surfaces

- Corrected the theme transition from Liquid Glass back to Ocean, Stellar, Darkcodium and other classic themes. Shared pop-out surfaces now become fully opaque immediately instead of retaining the old transparent Glass colour.
- Kept the Glass shell for detached windows while restoring an opaque, readable work surface when a Pop/Dock-capable panel is docked. This applies the intended distinction between a detached visual effect and an in-layout operational panel.
- Restored resize handles on all sides and corners of the detached Live Map window, including its frameless Liquid Glass presentation.
- Made the CW window readable over a Glass backdrop using stable content and transcript surfaces. Received and transmitted text now stay inside their respective bordered boxes, use distinct colours, and are separated by a draggable, persisted divider; double-clicking the divider centres the split.
- Improved JTTY readability in Liquid Glass with near-opaque work, input and receive surfaces, and ensured that these surfaces return to fully opaque when a classic theme is chosen.
- Unified the SSB/PC microphone menu entry with the appearance of the other Decodium main-menu tools.
- Added an optional **Decode provenance column** in Setup → Decode. It is disabled by default for a new installation and can be enabled when the operator wants to see the source of every decode.

### Online services, map state and diagnostics

- Clarified the meaning of Decodium's local Offline mode. The DX Cluster and updater now state that services are paused locally rather than implying that Windows, macOS or Linux has lost Internet access; Setup provides a direct **Resume online services** action.
- Separated the local network-safety choice from Live Map visual presets and import/export files. Applying or importing a map preset can no longer silently put the application offline and block Cluster or update checks.
- Hardened Map Intelligence worker cancellation generations with explicit relaxed atomic operations while preserving background-query cancellation and safe grid-selection refreshes. The known GCC/MinGW `stringop-overflow` false positive produced when queued Qt callbacks are inlined is isolated to this translation unit rather than being allowed to block a release build.

### Build, package and release assurance

- Added a mandatory CAT release gate which builds and runs the Hamlib release contract, CAT configuration, split-transceiver, CW CAT text, serial-port registry and PTT transition tests against the same build tree used for packaging.
- Added a Hamlib runtime manifest to release packages. The manifest records the shipped Hamlib library path, version, byte count and SHA-256, so the package can be audited independently of the CI image.
- Made the Linux AppImage and macOS DMG pipelines run the mandatory CAT gate and fail if the packaged runtime does not include the expected Hamlib library and manifest.
- Expanded cross-platform CI coverage for CAT, serial/CW paths and relevant settings changes; added missing Linux `libudev` development dependencies; and pinned the Windows MSYS2 libusb package fetched from the official archive to keep the Windows build reproducible while the rolling package index changes.
- Made the Windows CAT gate expose the Qt, Hamlib and MinGW runtime directories to CTest, preventing the `0xc0000135` missing-DLL startup failure while preserving the existing dependency cache.
- Kept `-Werror` for the whole target while making only the known GCC/MinGW `MapIntelligenceService` queued-callback `stringop-overflow` false positive non-fatal. Linux and Windows therefore retain all other warning checks without a compiler diagnostic preventing their packages from being produced.
- Modernised the Rotor regression assertion for Qt 6.11, replacing a deprecated Qt test API so macOS can retain warning-clean `-Werror` builds rather than weakening its compiler checks.

### Release contents

GitHub automatically provides source ZIP and TAR.GZ archives for tag `v1.0.698`. The GitHub Actions release workflows attach the Windows x64 installer, macOS Apple Silicon and Intel DMGs, and Linux x86_64 and aarch64 AppImages with their checksum files. The release is published only after those workflow artifacts have been checked for presence and non-zero size.

### Local verification before tagging

- macOS Qt build of `decodium_qml` completed successfully with Hamlib 4.7 and Qt RHI texture upload enabled.
- 14 focused regression tests passed: CAT configuration and serial-line safety, Hamlib release contract, Fake It split, TX dispatch guard, audio mode hand-off, Q65 continuity and Doppler, GPU recovery, Map Intelligence, Liquid Glass/classic-theme transition, CW window and JTTY window.
- The mandatory CAT release gate also passed 6/6 tests, and the independent QSO/waterfall recovery test passed.

### Operational note

This release contains focused automated regression and package-contract checks. It does not replace a cautious first on-air transmission with each individual radio/interface combination: use a dummy load and confirm CAT/PTT line configuration after changing radio, USB adapter or profile.

## Italiano

Decodium 4 v1.0.698 è la release mantenuta del fork successiva alla v1.0.697. Si concentra su controllo radio affidabile, gestione sicura delle linee seriali, transizioni robuste tra CW/decoder legacy e FT/Q65, recupero del percorso GPU dello spettro e un'interfaccia Liquid Glass più utilizzabile senza compromettere i temi classici.

### Affidabilità CAT, PTT e controllo radio

- Introdotta una fotografia versionata della configurazione CAT. Le modifiche a profilo radio, endpoint CAT, parametri seriali, indirizzo CI-V, split, metodo PTT, porta PTT, configurazione TCI o polling vengono ora riunite in una riconnessione controllata, evitando l'uso di una connessione aggiornata solo a metà.
- L'identità fisica memorizzata della radio viene azzerata quando cambia profilo o endpoint, così una riconnessione non può ereditare per errore l'identità di una radio o porta precedente.
- Resa più sicura elettricamente l'apertura di una porta CAT seriale su Windows, macOS e Linux. Le linee DTR e RTS non usate vengono portate esplicitamente allo stato inattivo prima dell'apertura Hamlib, preservando invece un cablaggio PTT DTR/RTS intenzionale e l'handshake hardware RTS/CTS.
- Evitato che una selezione PTT salvata non valida diventi silenziosamente la prima porta seriale elencata dal sistema operativo. Se si usa un PTT seriale separato, Decodium richiede una vera porta fisica invece di rischiare di collegarsi a un hardware non pertinente.
- Le capacità CAT opzionali sono ora nell'interfaccia comune `Transceiver` e vengono inoltrate anche dal wrapper Fake It. Keyer CW, controlli ATU e controlli di alimentazione della radio restano quindi disponibili quando lo split selezionato avvolge Hamlib, senza cast legati a una singola implementazione.
- Corretto lo stato Fake It: viene ignorata soltanto l'eco prevista del VFO TX, mentre un vero QSY effettuato dall'operatore rimane visibile anche in trasmissione.
- Rafforzati avvio e recupero Hamlib: le aperture fallite chiudono la porta seriale, dati di capacità nulli sono gestiti in sicurezza e i controlli vengono esposti solo quando la radio selezionata li supporta realmente.
- Migliorato il comportamento delle radio che mantengono CAT pur essendo spente o in standby. Decodium conserva la sessione CAT utile per un comando Power On supportato, sospende il polling VFO inappropriato e rileva l'accensione dal pannello frontale con una verifica a frequenza limitata prima di riprendere il polling normale.
- I comandi ATU e alimentazione non disponibili vengono disabilitati nel dashboard principale e nei controlli SSB, invece di mostrare azioni destinate necessariamente a fallire.

### Motore di decodifica, sicurezza TX e recupero dello spettro

- Aggiunta una politica esplicita di passaggio della cattura audio durante il cambio modo. CW, RTTY/JTTY, SSB, modi FT e Q65 rilasciano o conservano il proprietario corretto dell'audio prima del riavvio del monitor, evitando un monitor legacy obsoleto o percorsi di cattura in conflitto dopo il cambio modo.
- Completati i passaggi ricevitore da nativo a legacy e da legacy a nativo, incluso il percorso Q65 gestito dal bridge. Questo affronta la classe di problemi in cui panadapter e waterfall smettevano di aggiornarsi dopo il passaggio da CW a FT8/FT4 o dopo il cambio banda.
- Rafforzato il recupero GPU dopo un cambio sorgente. Decodium invalida e ricostruisce prima il percorso texture Qt Quick/GPU, esegue tentativi GPU limitati e rimanda il fallback CPU/software finché tali tentativi non sono stati realmente esauriti.
- Aggiunte guardie di generazione del ciclo di vita al lavoro TX differito. HALT, completamento QSO, reset QSO e modifiche AutoCQ invalidano callback pendenti di timer/slot, impedendo che una vecchia richiesta TX6 o CQ trasmetta dopo che AutoCQ è stato spento.
- Il testo immesso attraverso il pannello TX generico viene ora instradato al generatore di frame e al ciclo PTT/audio condiviso di JTTY, invece di tentare erroneamente un modo audio digitale non supportato dal bridge audio.

### Continuità Q65 e sicurezza dei submode

- Preservati il periodo e il submode Q65 esplicitamente scelti nell'interfaccia lungo tutto il percorso bridge/decoder, inclusa la migrazione di compatibilità della vecchia impostazione semplice `Q65` a `Q65-60A`.
- L'averaging coerente viene resettato per una normale finestra Q65 a meno che l'operatore non lo abbia abilitato esplicitamente, evitando che dati di una stazione o di un modo precedente contaminino le decodifiche successive.
- Limitata la profondità di decodifica Q65 e aggiunte diagnostiche di dispatch, così la variante Q65 selezionata è tracciabile senza dipendere da uno stato UI ambiguo.
- Aggiunta copertura di regressione per tre finestre Q65 consecutive e per i percorsi `Q65-60A` predefinito di compatibilità e `Q65-60B` non predefinito.

### Liquid Glass, finestre e leggibilità delle superfici operative

- Corretto il passaggio dal tema Liquid Glass a Ocean, Stellar, Darkcodium e agli altri temi classici. Le superfici condivise delle finestre staccate tornano immediatamente completamente opache invece di mantenere il vecchio colore trasparente Glass.
- Conservato il guscio Glass per le finestre staccate, ma ripristinata una superficie di lavoro opaca e leggibile quando un pannello con Pop/Dock è agganciato. Viene così rispettata la distinzione prevista tra effetto visivo della finestra staccata e pannello operativo nel layout.
- Ripristinate le maniglie di ridimensionamento su tutti i lati e gli angoli della finestra Live Map staccata, compresa la sua presentazione Liquid Glass senza cornice nativa.
- Resa leggibile la finestra CW sullo sfondo Glass grazie a superfici stabili per contenuto e trascrizioni. Il testo ricevuto e trasmesso rimane nei rispettivi box bordati, usa colori distinti ed è separato da un divisore trascinabile e persistente; un doppio clic sul divisore lo centra.
- Migliorata la leggibilità di JTTY in Liquid Glass con superfici di lavoro, input e ricezione quasi opache; queste tornano pienamente opache quando viene scelto un tema classico.
- Uniformata la voce di menu SSB/PC microphone allo stile degli altri strumenti del menu principale Decodium.
- Aggiunta in Setup → Decode la colonna opzionale **Decode provenance**. È disattivata per impostazione predefinita nelle nuove installazioni e può essere abilitata per vedere la sorgente di ogni decode.

### Servizi online, stato mappa e diagnostica

- Chiarito il significato della modalità Offline locale di Decodium. DX Cluster e updater indicano ora che i servizi sono sospesi localmente, senza far intendere che Windows, macOS o Linux abbia perso Internet; in Setup è disponibile l'azione diretta **Resume online services**.
- Separata la scelta locale di sicurezza della rete dai preset visivi della Live Map e dai file di import/export. Applicare o importare un preset mappa non può più mettere silenziosamente l'applicazione offline e bloccare Cluster o controlli aggiornamenti.
- Rafforzate le generazioni di annullamento dei worker Map Intelligence con operazioni atomiche esplicite relaxed, preservando annullamento delle query in background e aggiornamento sicuro della selezione grid. Il noto falso positivo `stringop-overflow` di GCC/MinGW generato quando callback Qt accodati vengono inline è isolato a questa sola unità di traduzione, invece di poter bloccare una build di rilascio.

### Build, pacchetti e garanzie di rilascio

- Aggiunto un gate CAT obbligatorio per il rilascio che compila ed esegue i test del contratto Hamlib, configurazione CAT, transceiver split, testo CW CAT, registro porte seriali e transizioni PTT sullo stesso albero di build usato per il packaging.
- Aggiunto un manifest runtime Hamlib ai pacchetti di rilascio. Il manifest registra percorso, versione, numero di byte e SHA-256 della libreria Hamlib realmente distribuita, così il pacchetto può essere verificato indipendentemente dall'immagine CI.
- Le pipeline AppImage Linux e DMG macOS eseguono ora il gate CAT obbligatorio e falliscono se il runtime incluso non contiene la libreria Hamlib e il manifest attesi.
- Estesa la copertura CI multipiattaforma per CAT, percorsi seriali/CW e modifiche Settings rilevanti; aggiunte le dipendenze di sviluppo `libudev` mancanti su Linux; e fissato il pacchetto libusb MSYS2 per Windows, scaricato dall'archivio ufficiale, per rendere riproducibile la build Windows mentre l'indice rolling dei pacchetti cambia.
- Il gate CAT Windows espone ora a CTest le directory runtime Qt, Hamlib e MinGW, evitando l'errore di avvio per DLL mancanti `0xc0000135` senza invalidare la cache delle dipendenze esistente.
- Mantenuto `-Werror` per l'intero target, rendendo non bloccante soltanto il noto falso positivo `stringop-overflow` GCC/MinGW dei callback accodati di `MapIntelligenceService`. Linux e Windows conservano quindi tutti gli altri controlli sui warning senza che una diagnostica del compilatore impedisca di produrre i pacchetti.
- Modernizzata l'asserzione di regressione del Rotore per Qt 6.11, sostituendo un'API Qt di test deprecata; macOS può così mantenere build pulite con `-Werror`, invece di indebolire i controlli del compilatore.

### Contenuto della release

GitHub fornisce automaticamente gli archivi sorgente ZIP e TAR.GZ per il tag `v1.0.698`. I workflow GitHub Actions di rilascio allegano installer Windows x64, DMG macOS Apple Silicon e Intel e AppImage Linux x86_64 e aarch64 con i rispettivi file checksum. La release viene pubblicata solo dopo avere verificato presenza e dimensione non nulla di tali artefatti.

### Verifica locale prima del tag

- La build macOS Qt di `decodium_qml` è stata completata correttamente con Hamlib 4.7 e upload texture Qt RHI abilitato.
- Superati 14 test mirati di regressione: configurazione CAT e sicurezza linee seriali, contratto di rilascio Hamlib, split Fake It, guardia TX dispatch, passaggio audio tra modi, continuità e Doppler Q65, recupero GPU, Map Intelligence, transizione Liquid Glass/tema classico, finestra CW e finestra JTTY.
- Anche il gate CAT obbligatorio ha superato 6/6 test e il test indipendente di recupero QSO/waterfall è passato.

### Nota operativa

Questa release contiene test automatici mirati di regressione e di contratto del pacchetto. Non sostituisce una prima trasmissione prudente in aria per ogni combinazione specifica radio/interfaccia: usare un carico fittizio e verificare la configurazione CAT/PTT dopo aver cambiato radio, adattatore USB o profilo.
