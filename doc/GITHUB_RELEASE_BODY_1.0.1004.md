# Decodium 4 FT2 1.0.1004

## English (UK)

This release contains the performance and resource-lifecycle work accumulated since 1.0.1003, together with CAT and waterfall corrections. Detailed experiments, including changes investigated but not adopted, are recorded in `docs/diagnostics/performance.md` and `docs/diagnostics/2026-11-15-osd-baseline.md`.

### Decoding and resources

- Reduced repeated work in FT8, FT4 and FT2 search and decoding: reused search windows and synchronisation templates, FT2 demodulation phasors, and noise-estimation logarithms; narrowed the evaluated synchronisation ranges and precomputed scalar LDPC connections.
- Improved FT8 worker handling and audio ring snapshots, asynchronous FT2 sample copies and Linux lifetimes for FFT plans, downsampling, filter, subtraction and FT2 workspaces. FST4 OSD table initialisation and cancellation/lifecycle handling were improved; WSPR decoding and resource handling were also updated.
- Kept decoding sensitivity and the intentional Low CPU profile as separate concerns. Experiments which did not establish a safe benefit were not adopted. Frozen-input comparisons cover FT8, FT4, FT2, FST4/FST4W and WSPR; they do not establish identical results for every signal or machine.

### Interface, startup and waterfall

- Reduced unnecessary work for hidden or unchanged windows and indicators, including the world clock, weather preview, timing bar, decode and monitoring indicators, map, logbook, waterfall labels and Decometer. Logbook search and LoTW cache handling were improved.
- Improved QML cache handling and added an isolated AppImage QML snapshot so changing mount paths do not unnecessarily invalidate application resources. Numerous settings and auxiliary windows were adjusted to reduce startup and visibility costs.
- Improved waterfall ring-buffer copying, software palette reconstruction and spectrum updates while the worker is busy. Black on the direct GPU auto-range path no longer compensates away its own threshold. Completing a QSO no longer explicitly clears the waterfall history; the spectrum producer can still be re-armed if necessary.
- The Black and post-log changes have local build and focused mathematical tests; the reported Windows D3D11 case and live post-log continuity still need user validation. A severe Windows UI stall can independently trigger the existing GPU-to-CPU protection.

### CAT: lower-sideband data mode

- Added **Data/Pkt LSB** in Settings → Radio → Mode. It requests DATA-L, mapped by Hamlib to PKTLSB, for radios such as the QRP Labs QMX. Existing **Data/Pkt** continues to select DATA-U/PKTUSB; **None** continues to leave mode selection to the operator. Post-PTT mode verification retains the selected data sideband. This addresses [issue #113](https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/issues/113).
- The mode mapping and existing compatibility cases were tested locally; a physical QMX and its rigctld interaction have not yet been tested by the developers. Until the new build is confirmed, **Mode = None** remains the reporter's verified workaround.

### Validation

- Added and extended profiling, frozen-signal, cancellation, lifecycle, UI, CAT and regression tools. Local A/B work includes virtual-audio reception and repeated FT8/FT4/FT2 mode changes. Virtual audio is not physical RF proof; component benchmarks are not whole-application speed-up claims.
- Windows and Linux/macOS packages are built by GitHub Actions; available release assets and their checksums are the record of completed uploads. No guarantee of universal no-loss decoding or older-hardware performance is implied.

## Italiano

Questa versione comprende gli interventi su prestazioni e ciclo di vita delle risorse effettuati dalla 1.0.1003, insieme a correzioni CAT e waterfall. Le prove dettagliate, compresi gli esperimenti esaminati ma non adottati, sono documentate in `docs/diagnostics/performance.md` e `docs/diagnostics/2026-11-15-osd-baseline.md`.

### Decodifica e risorse

- Ridotto il lavoro ripetuto nella ricerca e decodifica FT8, FT4 e FT2: riutilizzo delle finestre di ricerca e dei template di sincronismo, dei fasori di demodulazione FT2 e dei logaritmi della stima del rumore; limitazione degli intervalli di sincronismo e collegamenti LDPC scalari precalcolati.
- Migliorati il worker FT8 e le copie del buffer audio, le copie asincrone FT2 e la durata su Linux dei piani FFT e dei workspace di downsampling, filtro, sottrazione e FT2. Migliorati inizializzazione delle tabelle OSD FST4, cancellazione e ciclo di vita; aggiornati anche decodifica e gestione delle risorse WSPR.
- Sensibilità e comportamento intenzionale Low CPU restano considerazioni distinte. Gli esperimenti senza un beneficio sicuro dimostrato non sono stati adottati. I confronti con ingressi congelati coprono FT8, FT4, FT2, FST4/FST4W e WSPR; non garantiscono risultati identici per ogni segnale e computer.

### Interfaccia, avvio e waterfall

- Ridotto il lavoro superfluo di finestre e indicatori nascosti o invariati, inclusi orologio mondiale, anteprima meteo, barra temporale, indicatori di decode e monitoraggio, mappa, logbook, etichette waterfall e Decometer. Migliorate la ricerca nel logbook e la gestione della cache LoTW.
- Migliorata la cache QML e aggiunto uno snapshot QML isolato per AppImage, evitando invalidazioni inutili dovute al cambio del percorso di montaggio. Numerose finestre di impostazioni e accessorie sono state aggiornate per ridurre i costi di avvio e visibilità.
- Migliorate le copie del buffer circolare waterfall, la ricostruzione software della palette e gli aggiornamenti dello spettro con worker occupato. Nell'autorange GPU diretto Black non compensa più la propria soglia. La conclusione di un QSO non cancella più esplicitamente lo storico waterfall; il produttore dello spettro può comunque essere riattivato quando necessario.
- Le modifiche Black e post-log hanno superato build locale e test matematici mirati; il caso Windows D3D11 segnalato e la continuità dopo un log reale richiedono ancora verifica dell'utente. Un grave blocco Windows può attivare separatamente la protezione esistente che passa dalla GPU alla CPU.

### CAT: modo dati a banda laterale inferiore

- Aggiunta **Data/Pkt LSB** in Settings → Radio → Mode. Richiede DATA-L, tradotto da Hamlib in PKTLSB, per radio come QRP Labs QMX. **Data/Pkt** mantiene DATA-U/PKTUSB e **None** lascia la scelta all'operatore. La verifica del modo dopo il PTT conserva la banda laterale dati selezionata. Correzione per la [issue #113](https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/issues/113).
- Mappatura e compatibilità delle impostazioni esistenti sono state verificate localmente; gli sviluppatori non hanno ancora provato un QMX fisico con rigctld. Fino alla conferma della nuova build, **Mode = None** rimane la soluzione temporanea verificata dall'utente.

### Verifiche

- Aggiunti e ampliati strumenti di profiling, segnali congelati, cancellazione, ciclo di vita, interfaccia, CAT e regressione. Le prove A/B locali includono ricezione tramite audio virtuale e cambi ripetuti FT8/FT4/FT2. L'audio virtuale non dimostra il funzionamento RF fisico e i benchmark di componenti non rappresentano un'accelerazione dell'intera applicazione.
- I pacchetti Windows, Linux e macOS sono generati da GitHub Actions; gli allegati della release e i checksum indicano gli upload effettivamente completati. Non si garantiscono assenza universale di decode persi o prestazioni su ogni hardware meno recente.

Full changes / Confronto completo: https://github.com/elisir80/Decodium-4.0-Core-Shannon/compare/v1.0.1003...v1.0.1004
