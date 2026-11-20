# Decodium 4 FT2 1.0.1003

## English (UK)

Version 1.0.1003 contains the performance, decoder-lifecycle, resource-cleanup and diagnostic work accumulated since **1.0.1002** (`3529dae3`). The main aim is to reduce avoidable application overhead and improve responsiveness, including on less recent computers, whilst preserving decoding settings. The existing Low CPU profile retains its intentional sensitivity/resource trade-off.

### Interface and settings overhead

- Decode-list delegates now use directly indexed, immutable field keys instead of repeatedly scanning the role table and constructing strings. A compile-time ordering check and tests cover invalid roles, missing values and whole-entry access.
- Already-initialised settings profiles use their persistent initialisation marker to avoid enumerating every key on each access. Legacy profile discovery, migration and profile switching remain supported; profile state is not cached across changes.
- A static GPU map no longer requests animation-rate redraws unnecessarily. Clock and greyline updates continue at whole-second resolution; viewport changes, contact paths, transmission and earthquake animations retain their update paths. Setters still request immediate updates.

### Decoder lifecycle and resource handling

- FT4 decoding can stop cooperatively when its session is explicitly retired or the worker shuts down. A new receive slot alone does not cancel the preceding decode. Cancellation checks cover the native decode path, and generation tracking prevents an obsolete task from becoming current again.
- Decode-session invalidation now retains a monotonically advancing retired-serial boundary. Late callbacks from retired sessions are rejected even after per-request metadata has been removed. Valid, slower results from the current session remain eligible for delivery. This applies to the shared bridge and the affected FT8, legacy JT/Q65 and WSPR hand-offs.
- Queued WSPR work is checked for retirement before and after taking the shared native-runtime mutex. Obsolete queued requests can finish without running the expensive decoder; an already running native WSPR decode is allowed to finish. Temporary WAV files are cleaned up on skipped/error paths as well as normal completion.
- The embedded WSPR decoder now frees previously retained allocations on successful calls and follows a shared cleanup path for invalid arguments, inaccessible directories and input errors. Open output files and allocated buffers are released on the covered failure paths. Repeated-call resource tests and original-version negative controls accompany the fix.
- Q65 supports cooperative retirement of obsolete non-averaging work, including checks around the shared runtime lock and native processing. Coherent averaging is excluded from early cancellation so that its accumulated state can finish processing; stale results are still withheld from the current UI.
- FST4 supports cooperative retirement at the shared lock and candidate-processing boundaries. FST4W is deliberately excluded from early cancellation to preserve its learned callsign state. Retired results are not delivered into the new session.
- These changes reduce obsolete work; they do not introduce wholesale unloading of every decoder or a new decoding-depth policy. The earlier bridge file split belongs to previous releases and is not presented as a new 1.0.1003 feature.

### Diagnostics and isolated testing

- Slow main-thread event reports now include timer interval and owner information. QML timer subclasses can be attributed to their source URL, and guarded object references prevent diagnostics from dereferencing a receiver destroyed during event delivery.
- Period, legacy-state, performance-metrics and decode-list notification timers have explicit diagnostic names. Period processing reports phase timings when a phase reaches the slow-event threshold, helping distinguish progress notifications from early FT8/FT4 dispatch work.
- Laboratory storage is configured before normal migration and graphics/settings probes. Test-mode runs use a private settings location and skip normal operator-data migration; normal application startup retains its existing storage path.
- Local profiling tools cover startup, CPU/RSS, native workers, mode changes, active cancellation and live audio. FT8/FT4/FT2 comparisons use frozen input hashes and message multisets per slot, including multiplicity, rather than merely comparing decode counts. Mixed-mode, mismatched-profile and out-of-window results are rejected by the corresponding comparators.
- Added reproducible crowded FT8 and FT2 fixture tools, noisy FT2 coverage, settled full-row mode-cycle comparisons and two-application virtual-audio tests. Harnesses reserve loopback UDP endpoints, isolate CAT/settings and restore temporary QML and test settings. This addresses laboratory interference; it is not a claim that every operator-side UDP error has been diagnosed.
- Added a Windows A/B package generator, PowerShell runner/comparator, report exporter and Python report analyser. Packages record machine details, executable/input hashes and requested settings; the analyser checks archive structure, run order, configuration, decode output and resource samples. The kit can run on any available Windows PC; an i5 is not required.
- Windows CI now builds and runs focused performance/lifecycle regression gates and validates the PowerShell harness. Additional CMake targets exercise map cadence, FT4/FST4 cancellation, Q65 continuity, WSPR queue/resource cleanup and early laboratory-storage isolation. Baseline methodology and the detailed chronological worklog are included in the source tree.

### Measurements and validation boundaries

- Local isolated benchmarks measured about **41% less decode-role access time** and **91% less initialised-profile lookup time**. Static-map scheduling fell from **31 to 3 frame requests over 2.5 seconds** in the dedicated test. These figures are component measurements, not whole-application speed-up percentages.
- The final focused local regression suite passed **15/15 tests**. Additional mode/resource tests and native A/B comparisons are documented in `doc/PERFORMANCE_WORKLOG.md`.
- A crowded FT8 receive soak recovered **240/240 expected messages** across 20 slots. FT4 eight-slot comparisons preserved the expected output; a separate 24-slot run recovered every expected message. A noisy FT2 soak recovered the expected message in all **80 slots**. These are bounded laboratory recordings, not a universal guarantee of identical decoding under all conditions.
- An eight-station FT2 live comparison recovered all **64 expected station/slot messages** in both versions. Additional uncertain rows and duplicates varied even between runs of the original version. Direct-file crowded FT2 comparisons produced identical full rows, including in the matched Low CPU test. The live-output variability remains documented rather than hidden by a count-only check.
- Two local application instances demonstrated FT8, FT4 and FT2 transmission/reception through virtual audio. This is functional software/audio-path evidence, not physical RF or Windows hardware validation.
- An intermittent FT4 main-thread stall remains under investigation. The new timer diagnostics assist attribution; this release does **not** claim that stall is fixed. A trial of asynchronous main-window creation was reverted because it did not establish a consistent improvement.
- No general startup speed-up, universal absence of memory leaks, or performance result on a third-generation i5 is claimed. No external Windows A/B hardware report was available when these notes were prepared. Decoder sensitivity and search depth have not been reduced to produce the measurements.

### Source and packages

The application version is **1.0.1003**, with source at tag **v1.0.1003**. GitHub provides source ZIP/tar archives. Release workflows build a Windows x64 installer (`.exe`), macOS Apple Silicon DMGs (Sequoia and Tahoe), macOS Intel DMGs (Ventura, Sonoma and Sequoia), and Linux x86_64/aarch64 AppImages. macOS and Linux packages have accompanying SHA-256 files. The attached assets are the authoritative record of completed uploads; compilation alone does not establish physical-device interoperability.

## Italiano

La versione 1.0.1003 comprende il lavoro su prestazioni, ciclo di vita dei decoder, rilascio delle risorse e diagnostica accumulato dalla **1.0.1002** (`3529dae3`). L'obiettivo principale è ridurre il lavoro superfluo dell'applicazione e migliorare la fluidità, anche sui computer meno recenti, mantenendo le impostazioni di decodifica. Il profilo Low CPU conserva il compromesso intenzionale fra sensibilità e consumo di risorse.

### Interfaccia e accesso alle impostazioni

- Gli elementi della lista decode utilizzano chiavi immutabili indicizzate direttamente, evitando scansioni ripetute della tabella dei ruoli e nuove stringhe ad ogni lettura. Un controllo in compilazione verifica l'ordinamento; i test coprono ruoli non validi, valori mancanti e accesso all'intera riga.
- I profili già inizializzati utilizzano il relativo indicatore persistente per evitare di enumerare tutte le chiavi ad ogni accesso. Restano supportati individuazione dei profili precedenti, migrazione e cambio profilo, senza conservare in cache uno stato che potrebbe diventare obsoleto.
- La mappa GPU statica evita richieste di ridisegno alla frequenza delle animazioni quando non servono. Orologio e greyline continuano ad aggiornarsi al secondo; spostamenti della vista, collegamenti, trasmissione e animazioni dei terremoti mantengono i propri aggiornamenti. Le modifiche alle proprietà richiedono ancora un aggiornamento immediato.

### Ciclo di vita dei decoder e risorse

- FT4 può interrompere cooperativamente il lavoro quando la sessione viene esplicitamente ritirata o il worker viene chiuso. Il semplice arrivo di un nuovo slot non annulla il decode precedente. I controlli coprono il percorso nativo e il contatore di generazione impedisce che un'attività obsoleta torni ad essere considerata corrente.
- L'invalidazione della sessione conserva ora un limite progressivo dei seriali ritirati. Le risposte tardive delle vecchie sessioni vengono scartate anche dopo la rimozione dei metadati della richiesta. I risultati validi ma più lenti della sessione corrente restano utilizzabili. La protezione riguarda il bridge condiviso e i relativi passaggi FT8, JT/Q65 legacy e WSPR.
- Le richieste WSPR in coda vengono controllate prima e dopo l'acquisizione del mutex del runtime nativo condiviso. Il lavoro obsoleto ancora in coda può terminare senza eseguire il decoder costoso; una decodifica WSPR nativa già avviata viene lasciata concludere. I WAV temporanei vengono rimossi anche nei percorsi saltati o con errore.
- Il decoder WSPR incorporato libera allocazioni precedentemente trattenute nelle chiamate riuscite e utilizza una pulizia comune per argomenti non validi, directory inaccessibili ed errori di ingresso. Nei percorsi coperti vengono chiusi i file di uscita e liberati i buffer. La correzione è accompagnata da test sulle chiamate ripetute e controlli negativi sulla versione originale.
- Q65 può interrompere cooperativamente il lavoro obsoleto senza averaging, con controlli intorno al lock condiviso e nell'elaborazione nativa. L'averaging coerente è escluso dall'interruzione anticipata per consentire il completamento dello stato accumulato; i risultati obsoleti non vengono comunque mostrati nella sessione corrente.
- FST4 supporta l'interruzione cooperativa in corrispondenza del lock e delle fasi di elaborazione dei candidati. FST4W resta escluso dall'interruzione anticipata per preservare lo stato dei nominativi appresi. I risultati ritirati non vengono consegnati alla nuova sessione.
- Questi interventi riducono il lavoro ormai inutile; non introducono lo scaricamento indiscriminato di tutti i decoder né una nuova politica di profondità. La precedente suddivisione del bridge in più file appartiene a versioni precedenti e non viene presentata come novità della 1.0.1003.

### Diagnostica e prove isolate

- I rapporti sugli eventi lenti del thread principale includono intervallo e proprietario del timer. Anche le sottoclassi dei timer QML possono essere associate al proprio URL sorgente; riferimenti protetti evitano di accedere a un oggetto distrutto durante la consegna dell'evento.
- I timer di periodo, stato legacy, metriche e notifiche della lista decode hanno nomi diagnostici espliciti. L'elaborazione del periodo registra le durate delle fasi che superano la soglia prevista, distinguendo le notifiche di avanzamento dall'invio anticipato FT8/FT4.
- L'archiviazione del laboratorio viene configurata prima della migrazione ordinaria e dei controlli grafici/impostazioni. Le esecuzioni in modalità test utilizzano impostazioni private e saltano la migrazione dei dati dell'operatore; l'avvio normale mantiene il proprio percorso di archiviazione.
- Gli strumenti locali misurano avvio, CPU/RSS, worker nativi, cambi modo, interruzione attiva e audio continuo. I confronti FT8/FT4/FT2 utilizzano hash degli ingressi congelati e insiemi di messaggi per slot con relative molteplicità, invece del solo numero di decode. I comparatori rifiutano modi o profili incompatibili e risultati fuori dalla finestra prevista.
- Aggiunti generatori riproducibili di scenari affollati FT8 e FT2, prove FT2 con rumore, confronti delle righe complete dopo il cambio modo e prove con due applicazioni e audio virtuale. Il banco riserva gli endpoint UDP locali, isola CAT e impostazioni e ripristina QML e configurazioni temporanee. Questo elimina interferenze del laboratorio; non dimostra che ogni errore UDP nell'ambiente dell'operatore sia stato diagnosticato.
- Aggiunti generatore del pacchetto Windows A/B, esecutore e comparatore PowerShell, esportazione del rapporto e analizzatore Python. I pacchetti registrano macchina, hash di eseguibili/ingressi e impostazioni richieste; l'analizzatore verifica struttura dell'archivio, ordine delle prove, configurazione, messaggi e campioni delle risorse. Il kit può essere eseguito su qualsiasi PC Windows disponibile, senza richiedere un i5.
- La CI Windows compila ed esegue controlli mirati su prestazioni/ciclo di vita e verifica il banco PowerShell. Nuovi target CMake coprono frequenza della mappa, interruzione FT4/FST4, continuità Q65, coda e pulizia WSPR e isolamento iniziale del laboratorio. Metodologia e diario tecnico dettagliato sono inclusi nei sorgenti.

### Misure e limiti delle verifiche

- I benchmark locali isolati hanno misurato circa **41% di tempo in meno nella lettura dei ruoli decode** e **91% nell'accesso ai profili inizializzati**. Nel test dedicato, la mappa statica è passata da **31 a 3 richieste di frame in 2,5 secondi**. Sono misure dei singoli componenti, non percentuali di accelerazione dell'intera applicazione.
- La suite locale finale mirata ha superato **15 test su 15**. Altre prove sui modi, sulle risorse e confronti nativi A/B sono documentati in `doc/PERFORMANCE_WORKLOG.md`.
- Una ricezione FT8 affollata ha recuperato **240 messaggi attesi su 240** in 20 slot. I confronti FT4 su otto slot hanno mantenuto i risultati attesi; una prova separata su 24 slot ha recuperato tutti i messaggi previsti. La prova FT2 con rumore ha recuperato il messaggio atteso in tutti gli **80 slot**. Sono registrazioni di laboratorio limitate, non una garanzia universale di risultati identici.
- Il confronto FT2 continuo con otto stazioni ha recuperato tutti i **64 messaggi stazione/slot attesi** in entrambe le versioni. Righe aggiuntive incerte e duplicati variavano anche fra esecuzioni della versione originale. I confronti diretti su file FT2 affollati hanno prodotto righe complete identiche, anche nella prova Low CPU a configurazione equivalente. La variabilità dell'audio continuo resta documentata e non nascosta da un confronto dei soli conteggi.
- Due istanze locali hanno dimostrato trasmissione/ricezione FT8, FT4 e FT2 attraverso audio virtuale. È una verifica funzionale del software e del percorso audio, non una prova RF fisica o su hardware Windows.
- Rimane da identificare un rallentamento intermittente del thread principale in FT4. La nuova diagnostica aiuta a localizzarlo; questa versione **non lo dichiara risolto**. Un esperimento di creazione asincrona della finestra principale è stato annullato perché non dimostrava un miglioramento costante.
- Non vengono dichiarati un'accelerazione generale dell'avvio, l'assenza universale di perdite di memoria o risultati su un i5 di terza generazione. Alla preparazione delle note non era disponibile un rapporto A/B Windows esterno. Sensibilità e profondità di ricerca non sono state ridotte per ottenere le misure.

### Sorgenti e pacchetti

La versione dell'applicazione è **1.0.1003**, con sorgenti al tag **v1.0.1003** e archivi ZIP/tar forniti da GitHub. I workflow generano l'installer Windows x64 (`.exe`), DMG macOS Apple Silicon (Sequoia e Tahoe), DMG macOS Intel (Ventura, Sonoma e Sequoia) e AppImage Linux x86_64/aarch64. I pacchetti macOS e Linux dispongono dei relativi file SHA-256. Gli allegati della release costituiscono il riscontro degli upload completati; la sola compilazione non dimostra l'interoperabilità con dispositivi fisici.

**Full source comparison / Confronto completo dei sorgenti:** https://github.com/elisir80/Decodium-4.0-Core-Shannon/compare/v1.0.1002...v1.0.1003
