# Changelog / Registro Modifiche

## [1.4.1] - 2026-03-09

### English

Release focused on startup/mode-switch correctness, CAT runtime resilience, and release/documentation alignment.

#### Changed

- Startup mode initialization is now forced through the full mode setup path to avoid partially-initialized FT controls at boot.
- Startup auto mode-from-rig selection is now one-shot after first valid match.
- Remote runtime state/control surface expanded for `Async L2`, `Dual Carrier`, and `Alt 1/2`.
- World-map visibility persistence in splitter layout retained/cleaned.
- Release/docs templates aligned to `v1.4.1` (IT/EN/ES), including macOS quarantine and Linux AppImage extract-run guidance.

#### Fixed

- Fixed initial mode-switch unresponsiveness caused by repeated startup auto-mode forcing.
- Fixed unexpected advanced FT controls appearing at startup before mode initialization completed.
- Fixed forced waterfall foreground on FT2/FT4/FT8/FST4/FST4W mode switches.
- Reduced false CAT offline transitions by tolerating short transient poll failures in polling transceiver.

### Italiano

Release focalizzata su correttezza startup/cambio modalita', resilienza runtime CAT e allineamento release/documentazione.

#### Modificato

- Inizializzazione modalita' startup ora forzata sul percorso completo di setup per evitare controlli FT parziali all'avvio.
- Auto-selezione modalita' startup da frequenza radio resa one-shot dopo il primo match valido.
- Superficie stato/controllo runtime remoto estesa per `Async L2`, `Dual Carrier` e `Alt 1/2`.
- Persistenza visibilita' world-map nel layout splitter mantenuta/rifinita.
- Template release/documentazione allineati a `v1.4.1` (IT/EN/ES), inclusi comandi quarantena macOS e avvio AppImage estratto su Linux.

#### Corretto

- Corretto blocco/ritardo iniziale nel cambio modalita' causato da riforzatura ripetuta auto-mode in startup.
- Corretta comparsa inattesa di controlli FT avanzati in avvio prima del completamento init modalita'.
- Corretta apertura forzata in primo piano del waterfall durante switch FT2/FT4/FT8/FST4/FST4W.
- Ridotte transizioni CAT offline false tollerando failure transitori brevi nel polling transceiver.

### Espanol

Release centrada en correccion de startup/cambio de modo, resiliencia CAT en runtime y alineacion release/documentacion.

#### Cambios

- Inicializacion de modo al arranque forzada por la ruta completa de setup para evitar controles FT parciales.
- Auto-seleccion de modo startup por frecuencia del rig ahora one-shot tras el primer match valido.
- Superficie de estado/control remoto ampliada para `Async L2`, `Dual Carrier` y `Alt 1/2`.
- Persistencia de visibilidad world-map en layout splitter mantenida/ajustada.
- Plantillas de release/docs alineadas a `v1.4.1` (IT/EN/ES), incluyendo comando de cuarentena macOS y arranque AppImage extraido en Linux.

#### Corregido

- Corregida falta de respuesta inicial al cambiar modo por re-forzado repetido de auto-mode en startup.
- Corregida aparicion inesperada de controles FT avanzados al arrancar antes de terminar la inicializacion de modo.
- Corregido enfoque forzado del waterfall en cambios FT2/FT4/FT8/FST4/FST4W.
- Reducidas transiciones CAT offline falsas tolerando fallos transitorios cortos en polling transceiver.

## [1.3.8] - 2026-03-05

### English

Release focused on CAT/network hardening, world-map usability, and compact-layout stability after DX-ped integration.

#### Changed

- Added optional world-map greyline control in Settings -> General.
- Added active map-path distance badge rendering (km/mi based on unit preference).
- Refined compact top-controls layout behavior for small displays, including DX-ped area alignment.
- Hardened TCI endpoint/command parsing path for safer runtime behavior.
- Improved LotW/download pipeline (query-builder URLs, safer redirect policy, duplicate-connection prevention).
- Added window geometry restore safety helper for multi-monitor scenarios.
- Hardened legacy map65/qmap helpers with bounded string/path handling and device-index checks.
- Updated release/docs/workflow defaults to `v1.3.8`, keeping macOS Tahoe/Sequoia + Intel Sequoia/Monterey and Linux AppImage targets.

#### Fixed

- Fixed external-control mode lock scenario: generic remote Configure traffic no longer forces FT2 mode.
- Hardened UDP control handling by requiring direct target id on control messages.
- Improved network diagnostics text when packets are rejected for target-id mismatch.
- Fixed Linux workflow parsing for Hamlib latest-tag retrieval.
- Fixed Linux build compatibility when Hamlib 4.x fallback API (`rig_get_conf`) is used under stricter warning flags.

### Italiano

Release focalizzata su hardening CAT/rete, usabilita' mappa e stabilita' layout compatto dopo integrazione DX-ped.

#### Modificato

- Aggiunto toggle opzionale greyline mappa in Settings -> General.
- Aggiunto rendering badge distanza sul path mappa attivo (km/mi in base all'unita' selezionata).
- Rifinito comportamento layout controlli top compatti su schermi piccoli, incluso allineamento area DX-ped.
- Hardening percorso endpoint/parsing comandi TCI per maggiore robustezza runtime.
- Migliorato flusso LotW/download (URL su query builder, redirect policy piu' sicura, prevenzione connessioni duplicate).
- Aggiunto helper sicurezza restore geometria finestre per scenari multi-monitor.
- Hardening helper legacy map65/qmap con gestione stringhe/path bounded e controlli indici device.
- Aggiornati default release/doc/workflow a `v1.3.8`, mantenendo target macOS Tahoe/Sequoia + Intel Sequoia/Monterey e Linux AppImage.

#### Corretto

- Corretto scenario di lock modalita' con controllo esterno: traffico Configure remoto generico non forza piu' FT2.
- Hardening gestione controllo UDP richiedendo target id diretto nei messaggi di controllo.
- Migliorata diagnostica rete quando i pacchetti sono rifiutati per mismatch target-id.
- Corretto parsing latest-tag Hamlib nei workflow Linux.
- Corretta compatibilita' build Linux quando viene usato fallback API Hamlib 4.x (`rig_get_conf`) con warning severi.

### Espanol

Release centrada en hardening CAT/red, usabilidad del mapa y estabilidad del layout compacto tras integrar DX-ped.

#### Cambios

- Se anade opcion greyline del mapa en Settings -> General.
- Se anade badge de distancia en la ruta activa del mapa (km/mi segun unidades configuradas).
- Se ajusta el layout compacto de controles superiores en pantallas pequenas, incluyendo alineacion del area DX-ped.
- Hardening de ruta de endpoint/parsing de comandos TCI para mayor robustez runtime.
- Mejorado flujo LotW/descarga (URLs con query builder, redirect policy mas segura, prevencion de conexiones duplicadas).
- Anadido helper de seguridad para restaurar geometria de ventanas en escenarios multi-monitor.
- Hardening de helpers legacy map65/qmap con manejo bounded de strings/rutas y control de indices de dispositivo.
- Defaults de release/docs/workflows actualizados a `v1.3.8`, manteniendo objetivos macOS Tahoe/Sequoia + Intel Sequoia/Monterey y Linux AppImage.

#### Corregido

- Corregido bloqueo de modo con control externo: Configure remoto generico ya no fuerza FT2.
- Hardening del control UDP requiriendo target id directo para mensajes de control.
- Mejora de diagnostico de red cuando los paquetes se rechazan por mismatch de target-id.
- Corregido parsing latest-tag de Hamlib en workflows Linux.
- Corregida compatibilidad de build Linux cuando se usa fallback API Hamlib 4.x (`rig_get_conf`) con warnings estrictos.

## [1.3.7] - 2026-03-03

### English

Release focused on operational UX upgrades (map-driven workflow + new windows), startup/decode reliability, and release/documentation alignment.

#### Changed

- Added interactive world-map contact selection pipeline:
  - map click emits selected contact call/grid;
  - main UI auto-fills DX fields and regenerates standard messages.
- Added configurable behavior `Map: single click starts Tx`.
- Added persistent `View -> World Map` visibility state.
- Added `Ionospheric Forecast` window (HamQSL feed + sun image, timed refresh, persisted geometry).
- Added `DX Cluster` window (band-aware spots, mode filter, timed refresh, persisted geometry).
- Improved map day/night rendering and end-of-QSO path downgrade behavior to clear stale lines.
- Improved compact/two-row top controls layout mapping on small displays.
- Default Qt style now forced to `Fusion` for consistent rendering on macOS variants.
- Updated release/docs/workflow defaults to `v1.3.7`, including Linux/AppImage and Monterey best-effort target wording.

#### Fixed

- Fixed startup decode sequencing edge-case by setting sequence-start timestamp at ingest path before decode.
- Fixed queued cross-thread signal reliability for modulator state updates via explicit Qt metatype registration.
- Fixed legacy branding residue in runtime revision path by removing hardcoded return that injected `mod by IU8LMC...`.

### Italiano

Release focalizzata su upgrade UX operativi (workflow guidato da mappa + nuove finestre), affidabilita' startup/decode e allineamento release/documentazione.

#### Modificato

- Aggiunto pipeline selezione contatti interattiva da mappa:
  - click mappa emette call/grid del contatto selezionato;
  - la UI principale compila automaticamente i campi DX e rigenera i messaggi standard.
- Aggiunto comportamento configurabile `Map: single click starts Tx`.
- Aggiunta persistenza stato visibilita' `View -> World Map`.
- Aggiunta finestra `Ionospheric Forecast` (feed HamQSL + immagine sole, refresh temporizzato, geometria persistita).
- Aggiunta finestra `DX Cluster` (spot allineati banda corrente, filtro modo, refresh temporizzato, geometria persistita).
- Migliorato rendering mappa giorno/notte e comportamento downgrade path a fine QSO per pulire linee stale.
- Migliorata mappatura layout controlli top compatti/2 righe su display piccoli.
- Stile Qt predefinito forzato a `Fusion` per rendering coerente tra varianti macOS.
- Aggiornati default release/doc/workflow a `v1.3.7`, inclusa dicitura target Linux/AppImage e Monterey best-effort.

#### Corretto

- Corretto edge-case sequencing decode in startup impostando timestamp sequence-start nel percorso ingest prima del decode.
- Corretta affidabilita' segnali queued cross-thread per aggiornamenti stato modulator tramite registrazione esplicita metatype Qt.
- Corretta persistenza residuo branding legacy nel percorso runtime revision rimuovendo la return hardcoded che iniettava `mod by IU8LMC...`.

### Espanol

Release centrada en mejoras UX operativas (flujo guiado por mapa + nuevas ventanas), fiabilidad de arranque/decode y alineacion de release/documentacion.

#### Cambios

- Se anade pipeline de seleccion interactiva de contactos en mapa:
  - click en mapa emite call/grid del contacto seleccionado;
  - la UI principal rellena automaticamente campos DX y regenera mensajes estandar.
- Se anade comportamiento configurable `Map: single click starts Tx`.
- Se anade persistencia del estado visible `View -> World Map`.
- Se anade ventana `Ionospheric Forecast` (feed HamQSL + imagen solar, refresco temporizado, geometria persistida).
- Se anade ventana `DX Cluster` (spots por banda actual, filtro de modo, refresco temporizado, geometria persistida).
- Se mejora render dia/noche del mapa y limpieza de rutas stale al final de QSO.
- Se mejora mapeo de layout de controles superiores compactos/2 filas en pantallas pequenas.
- Estilo Qt por defecto forzado a `Fusion` para consistencia de render entre variantes macOS.
- Defaults de release/docs/workflows actualizados a `v1.3.7`, incluyendo Linux/AppImage y objetivo Monterey best-effort.

#### Corregido

- Corregido caso limite de secuenciacion decode al arranque fijando timestamp de secuencia en ingest antes del decode.
- Corregida fiabilidad de senales queued cross-thread del estado del modulador con registro explicito de metatype Qt.
- Corregido residuo de branding legacy en ruta runtime de revision eliminando return hardcoded que inyectaba `mod by IU8LMC...`.

## [1.3.6] - 2026-03-02

### English

Release focused on UX correctness, localization behavior, branding consistency, and CTY/country-display data quality.

#### Changed

- UI diagnostic logging controls refactored:
  - `Diagnostic mode` is no longer locked in an exclusive action group.
  - It now behaves as a true ON/OFF mode, persisted via `DiagnosticEventLogging`.
  - Runtime state is reconstructed from `wsjtx_log_config.ini` content at startup.
- Localization loader behavior updated:
  - any explicit `--language` override now takes strict precedence over locale autoload.
- PSKReporter local station software-id now uses `program_title()` branding string.

#### Fixed

- Fixed inability to immediately disable diagnostic mode without restarting.
- Fixed mixed-language startup when launching with `--language en_GB` on non-English locales.
- Fixed PSKReporter `Using:` field appending legacy `mod by IU8LMC...` suffix.
- Fixed title branding regression by restoring `Fork by Salvatore Raccampo 9H1SR` string.
- Fixed `cty.dat` parser false-failure on large modern prefix-detail blocks by increasing cap from 64 KiB to 4 MiB.
- Fixed country-name suppression in FT2/FT4/FT8 normal operation by restricting points append (`a1/a2/...`) to ARRL Digi mode.

### Italiano

Release focalizzata su correttezza UX, comportamento localizzazione, coerenza branding e qualita' dati CTY/country display.

#### Modificato

- Refactor controlli logging diagnostico UI:
  - `Modalita' diagnostica` non e' piu' vincolata in un gruppo azioni esclusivo.
  - Ora si comporta come ON/OFF reale, persistita via `DiagnosticEventLogging`.
  - Stato runtime ricostruito in avvio dal contenuto di `wsjtx_log_config.ini`.
- Aggiornato comportamento loader localizzazione:
  - qualunque override esplicito `--language` ha ora precedenza rigorosa sull'autoload da locale.
- Software-id stazione locale PSKReporter ora allineato al branding `program_title()`.

#### Corretto

- Corretto il problema che impediva di disattivare subito la modalita' diagnostica senza riavvio.
- Corretto l'avvio in lingua mista quando si usa `--language en_GB` su locale non inglese.
- Corretto il campo `Using:` di PSKReporter che aggiungeva il suffisso legacy `mod by IU8LMC...`.
- Corretta regressione branding titolo con ripristino stringa `Fork by Salvatore Raccampo 9H1SR`.
- Corretto falso errore parser `cty.dat` su blocchi prefisso moderni molto grandi (limite portato da 64 KiB a 4 MiB).
- Corretto oscuramento dei nomi country in FT2/FT4/FT8 uso normale limitando l'append punti (`a1/a2/...`) alla sola modalita' ARRL Digi.

## [1.3.5] - 2026-03-01

### English

Release focused on closing network/security findings, improving FT2 decode behavior, and stabilizing NTP + startup CTY flow.

#### Changed

- FT2 decoder upgraded with multi-period soft averaging path (`avemsg.txt`, averaging clear hook, averaged retry when single-period decode fails).
- FT2 message-averaging UI path enabled consistently in mode-specific visibility logic.
- NTP state handling centralized in MainWindow to avoid desync between status bar/panel toggles.
- NTP bootstrap strategy hardened with:
  - reliable server prioritization (`time.apple.com`, Cloudflare, Google, pool),
  - single-server bootstrap confirmations,
  - IPv4/IPv6-mapped sender key normalization,
  - adaptive RTT gate at bootstrap,
  - automatic pin to `time.apple.com` after repeated UDP no-response.
- Startup `cty.dat` policy refined:
  - immediate auto-download when file is missing,
  - delayed background refresh only when stale (>30 days),
  - HTTPS endpoint enforced for CTY download.
- Release/version metadata aligned to `v1.3.5` across CMake defaults, UI title/about labels, scripts, workflow defaults, and documentation.

#### Fixed

- Cloudlog upload/auth now enforces HTTPS endpoint normalization and avoids blocking nested modal dialogs in network callbacks.
- LotW download no longer silently downgrades TLS to HTTP; cross-thread completion/progress emissions marshaled safely into Qt event loop.
- UDP remote-control surface reduced in MessageClient:
  - sender trust list enforcement,
  - destination id filtering,
  - warning path for untrusted packets.
- ADIF/log integrity hardening:
  - field sanitization before ADIF assembly in `LogBook`,
  - mutex-protected append path in `WorkedBefore`,
  - dynamic `programid` string generation from active fork version.
- FileDownload callback handling hardened to ignore stale replies and avoid manager-level finished fan-out races.
- PSKReporter timer arithmetic fixed (`(MIN_SEND_INTERVAL + 1) * 1000`) and global spot cache protected with mutex.
- Settings trace output now redacts sensitive keys (passwords/tokens/api keys/cloudlog/lotw credentials).
- Windows `killbyname` process enumeration now uses dynamic buffer sizing (no fixed 1000-process truncation).
- NTP status label now reflects active offset during weak-sync hold (no stale `NTP: --` while synced).

### Italiano

Release focalizzata sulla chiusura di finding rete/sicurezza, sul miglioramento del comportamento decode FT2 e sulla stabilita' NTP + CTY in avvio.

#### Modificato

- Decoder FT2 aggiornato con percorso di media multi-periodo (`avemsg.txt`, hook di reset media, retry su media quando il decode single-period fallisce).
- Percorso UI message-averaging FT2 reso coerente nella logica di visibilita' per modalita'.
- Gestione stato NTP centralizzata in MainWindow per evitare desincronizzazioni tra toggle status bar/pannello.
- Strategia bootstrap NTP irrobustita con:
  - priorita' server affidabili (`time.apple.com`, Cloudflare, Google, pool),
  - conferme bootstrap con singolo server,
  - normalizzazione chiavi mittente IPv4/IPv6-mapped,
  - soglia RTT adattiva in bootstrap,
  - pin automatico a `time.apple.com` dopo no-response UDP ripetuti.
- Policy startup `cty.dat` rifinita:
  - auto-download immediato se il file manca,
  - refresh background ritardato solo se stale (>30 giorni),
  - endpoint HTTPS forzato per download CTY.
- Metadati release/versione allineati a `v1.3.5` su default CMake, title/about UI, script, default workflow e documentazione.

#### Corretto

- Upload/auth Cloudlog ora con normalizzazione HTTPS e senza dialog modale bloccante nei callback rete.
- Download LotW non degrada piu' silenziosamente da TLS a HTTP; emissioni completion/progress cross-thread instradate in sicurezza nel loop Qt.
- Superficie controllo remoto UDP ridotta in MessageClient:
  - enforcement sender trusted list,
  - filtro id destinazione,
  - warning su pacchetti non trusted.
- Hardening integrita' ADIF/log:
  - sanitizzazione campi prima della composizione ADIF in `LogBook`,
  - append protetto da mutex in `WorkedBefore`,
  - generazione dinamica `programid` dalla versione fork attiva.
- Gestione callback FileDownload irrobustita per ignorare reply stale e prevenire race da fan-out `finished` del manager.
- Correzione aritmetica timer PSKReporter (`(MIN_SEND_INTERVAL + 1) * 1000`) e protezione mutex per cache spot globale.
- Output trace impostazioni ora con redazione chiavi sensibili (password/token/api key/credenziali cloudlog/lotw).
- Enumerazione processi Windows in `killbyname` ora con buffer dinamico (niente troncamento fisso a 1000 processi).
- Label stato NTP ora mostra l'offset attivo anche durante weak-sync hold (niente `NTP: --` stale quando e' synced).

## [1.3.4] - 2026-02-28

### English

Release focused on closing high-impact TCI/security/concurrency issues and completing release alignment for macOS + Linux targets.

#### Changed

- TCI pseudo-sync waits (`mysleep1..8`) refactored to remove nested `QEventLoop::exec()` behavior.
- TCI TX waveform path now uses guarded snapshot reads of `foxcom_.wave` to reduce cross-thread race risk.
- TOTP generation switched to NTP-corrected time source.
- Critical runtime/network regex paths migrated from `QRegExp` to `QRegularExpression`.
- Release/documentation baseline updated to `v1.3.4` (EN/IT), including Linux requirements and quarantine guidance.

#### Fixed

- Critical TCI binary frame over-read risk by validating frame header/payload bounds before access/copy.
- TCI C++/Fortran shared-buffer boundary in audio ingest hardened with `kin` clamp and bounded writes.
- macOS audio stop/underrun behavior improved for Sequoia-era runtime stability.

### Italiano

Release focalizzata sulla chiusura definitiva di problemi TCI/sicurezza/concorrenza ad alto impatto e sull'allineamento release per target macOS + Linux.

#### Modificato

- Refactor attese pseudo-sync TCI (`mysleep1..8`) con rimozione del comportamento annidato `QEventLoop::exec()`.
- Percorso waveform TX TCI aggiornato con snapshot protetti di `foxcom_.wave` per ridurre rischio race tra thread.
- Generazione TOTP allineata a sorgente tempo corretta NTP.
- Migrazione dei percorsi regex runtime/network critici da `QRegExp` a `QRegularExpression`.
- Baseline release/documentazione aggiornata a `v1.3.4` (EN/IT), inclusi requisiti Linux e istruzioni quarantena.

#### Corretto

- Rischio critico over-read frame binari TCI risolto con validazione completa header/payload prima di accesso/copia.
- Boundary buffer condiviso C++/Fortran in ingest audio TCI irrobustito con clamp `kin` e scritture limitate.
- Migliorato comportamento audio macOS (stop/underrun) per maggiore stabilita' runtime su Sequoia.

## [1.3.3] - 2026-02-28

### English

Release focused on upstream feature import from the original Raptor repository plus fork UI/runtime refinements.

#### Changed

- Imported upstream Raptor feature set:
  - B4 strikethrough in Band Activity for worked-on-band stations.
  - Auto CQ caller FIFO queue (max 20) with automatic next-caller processing after QSO completion.
  - TX slot red bracket overlay (`[ ]`) on waterfall for FT2/FT8/FT4.
  - Automatic `cty.dat` refresh/download at startup when missing or older than 30 days.
  - FT2 decoder tuning updates (`syncmin` adaptive profile, extended AP types for Tx3/Tx4, deep-search threshold relaxations).
- Added responsive top-controls layout for small displays with automatic 2-row split.
- Updated release/build metadata to installer build tag `2602281900`.
- Release artifacts remain DMG/ZIP/SHA256 for macOS and AppImage/SHA256 for Linux (no `.pkg`), with added best-effort Intel Monterey target in GitHub workflows.

#### Fixed

- Startup mode/frequency mismatch on CAT initialization by using full-list mode auto-alignment.
- Light-theme progress/seconds bar visibility in status area.
- Small-screen control overlap/truncation in top action row.

### Italiano

Release focalizzata su import feature upstream dal repository Raptor originale e rifiniture UI/runtime del fork.

#### Modificato

- Importato il set feature upstream Raptor:
  - Testo barrato B4 in Band Activity per stazioni gia' lavorate in banda.
  - Coda FIFO Auto CQ chiamanti (max 20) con passaggio automatico al prossimo chiamante a fine QSO.
  - Overlay waterfall con parentesi rosse (`[ ]`) sullo slot TX per FT2/FT8/FT4.
  - Refresh/download automatico `cty.dat` all'avvio se mancante o piu' vecchio di 30 giorni.
  - Tuning decoder FT2 (`syncmin` adattivo, AP esteso su Tx3/Tx4, soglie deep-search rilassate).
- Aggiunto layout controlli top responsivo per schermi piccoli con split automatico su 2 righe.
- Aggiornati metadati build/release al build tag installer `2602281900`.
- Artifact release invariati: DMG/ZIP/SHA256 su macOS e AppImage/SHA256 su Linux (nessun `.pkg`), con target Intel Monterey best-effort aggiunto nei workflow GitHub.

#### Corretto

- Mismatch modalita'/frequenza all'avvio con CAT tramite auto-allineamento modalita' su lista completa frequenze.
- Visibilita' barra progressi/secondi nel tema chiaro in status bar.
- Sovrapposizioni/troncamenti controlli su display piccoli nella riga azioni superiore.

## [1.3.2] - 2026-02-28

### English

Patch release focused on startup hang mitigation and startup data-file hardening.

#### Changed

- Startup file loads now run asynchronously for `wsjtx.log`, `ignore.list`, `ALLCALL7.TXT`, and old-audio cleanup tasks.
- `WorkedBefore` reload flow now avoids overlapping reload races.
- Release/documentation baseline updated to `v1.3.2` (EN/IT) for macOS + Linux AppImage targets.
- macOS release flow remains DMG/ZIP/SHA256 (no `.pkg` required after shared-memory mmap migration).

#### Fixed

- Reduced startup UI hangs/beachball risks by moving expensive file I/O off the main thread.
- Hardened `CTY.DAT` loading with size guards, parser validation, fallback recovery, and safer reload behavior.
- Added defensive bounds checks for `grid.dat`, `sat.dat`, and `comments.txt` startup parsing.

### Italiano

Patch release focalizzata su mitigazione hang in avvio e hardening dei file dati caricati allo startup.

#### Modificato

- I caricamenti file in avvio ora sono asincroni per `wsjtx.log`, `ignore.list`, `ALLCALL7.TXT` e cleanup dei vecchi file audio.
- Flusso di reload `WorkedBefore` aggiornato per evitare race da reload sovrapposti.
- Baseline release/documentazione aggiornata a `v1.3.2` (EN/IT) per target macOS + Linux AppImage.
- Il flusso release macOS resta DMG/ZIP/SHA256 (nessun `.pkg` richiesto dopo migrazione shared memory a mmap).

#### Corretto

- Ridotto il rischio di hang/beachball in avvio spostando I/O pesante fuori dal main thread UI.
- Loader `CTY.DAT` irrobustito con limiti dimensione, validazione parser, recovery fallback e reload piu' sicuro.
- Aggiunte guardie difensive su parsing startup di `grid.dat`, `sat.dat` e `comments.txt`.

## [1.3.1] - 2026-02-28

### English

Patch release focused on version alignment, Linux CI/release completion, and cross-platform packaging consistency.

#### Changed

- Program title release identifier now sourced from `FORK_RELEASE_VERSION` instead of hardcoded legacy text.
- Release documentation updated to `v1.3.1` in English and Italian.
- Added Linux x86_64 AppImage release flow alongside existing macOS matrices.

#### Fixed

- Linux build compatibility with Hamlib variants lacking `rig_get_conf2` (fallback to `rig_get_conf`).
- Linux CI link issues by adding missing `libudev` development/runtime dependencies.
- Linux CI verify/package steps now target `ft2` executable (not legacy `wsjtx` path).
- Tahoe macOS release compatibility target aligned to `26.0` for bundled dependency checks.

### Italiano

Patch release focalizzata su allineamento versione, completamento CI/release Linux e coerenza packaging multipiattaforma.

#### Modificato

- Identificativo release nel titolo programma ora letto da `FORK_RELEASE_VERSION` invece che hardcoded legacy.
- Documentazione release aggiornata a `v1.3.1` in inglese e italiano.
- Aggiunto flusso release Linux x86_64 AppImage insieme alle matrici macOS esistenti.

#### Corretto

- Compatibilita' build Linux con varianti Hamlib senza `rig_get_conf2` (fallback a `rig_get_conf`).
- Problemi di link Linux CI risolti aggiungendo dipendenze `libudev` development/runtime.
- Step verify/package Linux CI ora puntano all'eseguibile `ft2` (non al path legacy `wsjtx`).
- Target compatibilita' release macOS Tahoe allineato a `26.0` per i controlli dipendenze bundle.

## [1.3.0] - 2026-02-28

### English

Release focused on security hardening, macOS shared-memory stability, CAT resilience,
and DT/NTP behavior tuning.

#### Changed

- macOS shared memory migrated to `SharedMemorySegment` with Darwin `mmap` backend.
- Release flow updated to generate platform-specific DMG/ZIP/SHA256 assets:
  - Tahoe arm64
  - Sequoia arm64
  - Sequoia x86_64
- `.pkg` generation removed from standard release path (no longer required for shm sysctl bootstrap).
- FT2/FT4/FT8 mode-specific NTP and DT sampling tuning refined.

#### Fixed

- CAT reconnect instability when external bridge/logger state changed at runtime.
- Startup mode mismatch when rig frequency implied a different digital mode.
- Multiple NTP edge cases (sample quality filtering, sparse-set handling, fallback coherence).
- DT status presentation now aligned with effective timing context.

### Italiano

Release focalizzata su hardening sicurezza, stabilita' shared memory su macOS, resilienza CAT
e tuning del comportamento DT/NTP.

#### Modificato

- Memoria condivisa macOS migrata a `SharedMemorySegment` con backend Darwin `mmap`.
- Flusso release aggiornato con asset DMG/ZIP/SHA256 specifici per piattaforma:
  - Tahoe arm64
  - Sequoia arm64
  - Sequoia x86_64
- Generazione `.pkg` rimossa dal percorso release standard (non piu' necessaria per bootstrap sysctl shm).
- Rifinito il tuning NTP e campionamento DT per FT2/FT4/FT8.

#### Corretto

- Instabilita' CAT in riconnessione quando lo stato del bridge/logger esterno cambiava a runtime.
- Mismatch modalita' all'avvio quando la frequenza radio implicava un modo digitale differente.
- Diversi edge case NTP (filtro qualita' campioni, gestione set server sparsi, coerenza fallback).
- Presentazione DT in status bar allineata al contesto temporale effettivo.

## [1.2.1] - 2026-02-27

### English

Maintenance release focused on UI cleanliness and map accuracy.

#### Changed

- Removed soundcard drift display/alternation in status bar and Time Sync panel; drift measurement disabled in Detector.
- Shortened world map contact lifetime to 2 minutes to prevent stale call popups.
- Program title updated to fork `v1.2.1`.

#### Fixed

- Prevented SC drift label from reappearing via legacy code path.

### Italiano

Release di manutenzione focalizzata su pulizia UI e correttezza mappa.

#### Modificato

- Rimossa la visualizzazione/alternanza del drift scheda audio in status bar e pannello Time Sync; misura di drift disabilitata nel Detector.
- Durata contatti mappa ridotta a 2 minuti per evitare chiamate fantasma.
- Titolo programma aggiornato alla fork `v1.2.1`.

#### Corretto

- Evitata la ricomparsa dell’etichetta SC drift tramite codice legacy.

## [1.2.0] - 2026-02-26

### English

Fork release based on Decodium v3.0 SE Raptor with macOS-focused runtime stabilization and robust DT/NTP timing control.

#### Added

- Fork release identity `v1.2.0` in title/release metadata.
- `tx-support.log` for TX/PTT/audio troubleshooting.
- Startup microphone preflight on macOS.
- NTP robust sync controls:
  - weak-sync deadband + confirmations,
  - cluster/coherence filtering,
  - sparse-server jump confirmation,
  - lock hold behavior during temporary outages.

#### Changed

- App namespace and bundle path standardized to `ft2.app` / `ft2`.
- macOS subprocess layout fixed (`jt9` colocated in bundle runtime path).
- TX start/stop sequencing hardened for CAT/PTT lag and races.
- Audio settings restore made more defensive for channels/devices.
- FT2 band defaults auto-merged for legacy profiles missing FT2 entries.
- Graph windows branding aligned to Decodium.

#### Fixed

- `execve` failures caused by legacy `wsjtx.app` assumptions.
- Intermittent "PTT active but no modulation" cases in repeated FT2 cycles.
- Late microphone popup appearing during active operation.
- Invalid persisted channel states causing silent TX/RX.

### Italiano

Release fork basata su Decodium v3.0 SE Raptor con stabilizzazione runtime su macOS e controllo temporale DT/NTP robusto.

#### Aggiunto

- Identita release fork `v1.2.0` in titolo/metadati release.
- `tx-support.log` per diagnostica TX/PTT/audio.
- Preflight permesso microfono all'avvio su macOS.
- Controlli sync NTP robusti:
  - deadband weak-sync + conferme,
  - filtro cluster/coerenza,
  - conferma salti con pochi server,
  - comportamento hold del lock su outage temporanei.

#### Modificato

- Namespace app e path bundle standardizzati su `ft2.app` / `ft2`.
- Layout sottoprocesso macOS corretto (`jt9` co-locato nel runtime bundle).
- Sequenza start/stop TX resa piu robusta contro lag/race CAT/PTT.
- Ripristino impostazioni audio reso piu difensivo per canali/device.
- Default banda FT2 auto-merge per profili legacy senza voci FT2.
- Branding finestre grafico allineato a Decodium.

#### Corretto

- Errori `execve` causati da assunzioni legacy su `wsjtx.app`.
- Casi intermittenti "PTT attivo ma senza modulazione" nei cicli FT2 ripetuti.
- Popup microfono tardivo durante l'operativita.
- Stati canale persistiti non validi che causavano TX/RX muta.

### CI/Build

- Release matrix macOS su:
  - Apple Silicon Tahoe
  - Apple Silicon Sequoia
  - Apple Intel Sequoia
- Artifact naming allineato a `v1.2.0`.
