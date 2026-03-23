# Changelog / Registro Modifiche

## [1.5.5] - 2026-03-23

### English

Release focused on macOS Preferences correctness across all UI languages, FT2 subprocess diagnostics, FT2 ADIF migration, audio startup recovery, and withdrawal of the experimental RTTY UI from the public release.

#### Added

- Added persistent `jt9_subprocess.log` tracing for FT2/jt9 subprocess launch, stderr, and termination events.
- Added clearer shared-memory and stdout/stderr diagnostics for FT2 subprocess failures.
- Added automatic backup-preserving migration of historical FT2 ADIF records to `MODE=MFSK` + `SUBMODE=FT2`.

#### Changed

- macOS now forces explicit menu roles so only `About`, `Preferences`, and `Quit` use native app-menu roles.
- Settings pages are now scrollable on macOS as well as Linux.
- The incomplete RTTY UI is hidden from menus and settings in the public release path.
- Local version metadata, workflow defaults, release docs, and About text are aligned to semantic version `1.5.5`.

#### Fixed

- Fixed macOS native `Preferences...` opening the wrong action in some translated UI languages.
- Fixed the Settings dialog growing beyond screen height on macOS and hiding the confirm buttons.
- Fixed FT2 subprocess crash reporting being too opaque to diagnose on hardened macOS systems.
- Fixed startup/monitor audio cases where audio only resumed after reopening `Settings > Audio`.

### Italiano

Release focalizzata su correttezza di Preferenze su macOS in tutte le lingue UI, diagnostica del subprocess FT2, migrazione ADIF FT2, recupero audio all'avvio e ritiro della UI RTTY sperimentale dal percorso pubblico della release.

#### Aggiunto

- Aggiunto `jt9_subprocess.log` persistente per tracciare avvio, stderr e terminazione del subprocess FT2/jt9.
- Aggiunta diagnostica piu' chiara per i fallimenti FT2 del subprocess, con dettagli shared-memory e stdout/stderr.
- Aggiunta migrazione automatica con backup dei record ADIF FT2 storici verso `MODE=MFSK` + `SUBMODE=FT2`.

#### Modificato

- Su macOS sono ora forzati ruoli menu espliciti, cosi' solo `About`, `Preferences` e `Quit` usano i ruoli nativi del menu app.
- Le pagine di Settings sono ora scrollabili anche su macOS oltre che su Linux.
- La UI RTTY incompleta e' nascosta da menu e impostazioni nel percorso pubblico della release.
- Metadati versione locali, default workflow, documenti release e testo About sono allineati alla semver `1.5.5`.

#### Corretto

- Corretta l'apertura errata di `Preferences...` su macOS in alcune lingue UI tradotte.
- Corretto il dialog Settings che su macOS poteva superare l'altezza dello schermo e nascondere i pulsanti finali.
- Corretta l'eccessiva opacita' del reporting crash del subprocess FT2 su sistemi macOS hardened.
- Corretti i casi di avvio/monitor in cui l'audio ripartiva solo dopo aver riaperto `Settings > Audio`.

### Espanol

Release centrada en la correccion de Preferencias en macOS para todas las lenguas UI, la diagnostica del subprocess FT2, la migracion ADIF FT2, la recuperacion del audio al arrancar y la retirada de la UI RTTY experimental del camino publico de release.

#### Anadido

- Anadido `jt9_subprocess.log` persistente para trazar arranque, stderr y finalizacion del subprocess FT2/jt9.
- Anadida una diagnostica mas clara para fallos FT2 del subprocess, con detalles de shared-memory y stdout/stderr.
- Anadida migracion automatica con backup de los registros ADIF FT2 historicos hacia `MODE=MFSK` + `SUBMODE=FT2`.

#### Cambios

- En macOS se fuerzan ahora roles explicitos de menu, de forma que solo `About`, `Preferences` y `Quit` usan roles nativos del menu app.
- Las paginas de Settings son ahora scrollables tambien en macOS ademas de Linux.
- La UI RTTY incompleta queda oculta de menus y ajustes en el camino publico de release.
- Metadatos locales de version, defaults de workflow, documentos release y texto About quedan alineados a la semver `1.5.5`.

#### Corregido

- Corregida la apertura equivocada de `Preferences...` en macOS en algunas lenguas UI traducidas.
- Corregido el dialogo Settings que en macOS podia superar la altura de pantalla y ocultar los botones finales.
- Corregida la excesiva opacidad del reporting de fallos del subprocess FT2 en sistemas macOS hardened.
- Corregidos los casos de arranque/monitor en los que el audio solo volvia tras reabrir `Settings > Audio`.

## [1.5.4] - 2026-03-22

### English

Release focused on FT2/FT4/FT8 decoder quality, web-app parity, UI/language alignment, downloader and secure-settings hardening, and expanded release/test coverage.

#### Added

- Added FT2 anti-ghost filtering for very weak decodes, with `ghostPass` / `ghostFilt` tracing in `debug.txt`.
- Added web-app `Monitoring ON/OFF`, FT2 `ASYNC` dB display, and `Hide CQ` / `Hide 73` activity filters.
- Added full web-app localization coverage for all bundled UI languages, driven by the desktop app language.
- Added the dedicated `SecureSettings` module.
- Added downloader integration tests plus RFC HOTP/TOTP test vectors for `SHA1`, `SHA256`, and `SHA512`.

#### Changed

- FT2, FT4, and FT8 now use more robust Costas sync strategies and deeper/adaptive subtraction thresholds for weak-signal decoding.
- UTC clock and Astro date/time output now follow the `Day Month Year` plus `UTC` format using the app-selected language.
- User-facing title/version branding keeps `Decodium` and removes the old `v3.0 FT2 "Raptor"` title text.
- The language menu now keeps a single `English` entry.
- Release defaults, workflow defaults, and release documents are aligned to semantic version `1.5.4`.

#### Fixed

- Fixed very weak FT2 ghost or malformed decodes slipping through without a dedicated weak-signal sanity filter.
- Fixed the web app missing live monitoring control, missing ASYNC dB feedback, and drifting to browser language instead of the app language.
- Fixed secure-setting fallback/import edge cases and made LoTW default to `https`.
- Fixed downloader acceptance of bad redirect schemes and unbounded large downloads.
- Fixed silent CAT/transceiver exceptions and reduced blind DXLab startup waits.
- Fixed Linux `GNU ld` unit-test link failures by correcting static-library order.

### Italiano

Release focalizzata su qualita' decoder FT2/FT4/FT8, parita' funzionale della web app, allineamento UI/lingue, hardening di downloader e secure settings, ed estensione della copertura test/release.

#### Aggiunto

- Aggiunto il filtro FT2 anti-ghost per decode molto deboli, con tracing `ghostPass` / `ghostFilt` in `debug.txt`.
- Aggiunti alla web app `Monitoring ON/OFF`, indicatore FT2 `ASYNC` in dB e filtri attivita' `Hide CQ` / `Hide 73`.
- Aggiunta copertura completa della localizzazione web app per tutte le lingue UI bundle, guidata dalla lingua selezionata nell'app desktop.
- Aggiunto il modulo dedicato `SecureSettings`.
- Aggiunti test integrazione downloader e vettori RFC HOTP/TOTP per `SHA1`, `SHA256` e `SHA512`.

#### Modificato

- FT2, FT4 e FT8 usano ora strategie Costas sync piu' robuste e soglie di sottrazione piu' profonde/adattive per il decoding weak-signal.
- Orologio UTC e pannello Astro seguono ora il formato `Giorno Mese Anno` piu' `UTC`, usando la lingua selezionata nell'app.
- Il branding titolo/versione lato utente mantiene `Decodium` e rimuove il vecchio testo `v3.0 FT2 "Raptor"`.
- Il menu lingue mantiene ora una sola voce `English`.
- Default release, default workflow e documentazione release sono allineati alla semver `1.5.4`.

#### Corretto

- Corretti i decode FT2 ghost o malformati molto deboli che passavano senza un filtro weak-signal dedicato.
- Corretta la web app che non esponeva il controllo monitoring, non mostrava i dB ASYNC e usava la lingua del browser invece di quella dell'app.
- Corretti i casi limite di fallback/import dei secure settings e portato LoTW di default a `https`.
- Corretta l'accettazione nel downloader di redirect con scheme non validi e di download troppo grandi senza limite.
- Corretti logging silenziosi delle eccezioni CAT/transceiver e ridotti i blind wait di startup DXLab.
- Corretto il linking dei test su Linux con `GNU ld`, sistemando l'ordine delle librerie statiche.

### Espanol

Release centrada en calidad de decoder FT2/FT4/FT8, paridad funcional de la web app, alineacion UI/idiomas, endurecimiento de downloader y secure settings, y ampliacion de la cobertura test/release.

#### Anadido

- Anadido el filtro FT2 anti-ghost para decodes muy debiles, con trazas `ghostPass` / `ghostFilt` en `debug.txt`.
- Anadidos a la web app `Monitoring ON/OFF`, indicador FT2 `ASYNC` en dB y filtros de actividad `Hide CQ` / `Hide 73`.
- Anadida cobertura completa de localizacion web app para todas las lenguas UI bundle, guiada por el idioma seleccionado en la app de escritorio.
- Anadido el modulo dedicado `SecureSettings`.
- Anadidos tests de integracion del downloader y vectores RFC HOTP/TOTP para `SHA1`, `SHA256` y `SHA512`.

#### Cambios

- FT2, FT4 y FT8 usan ahora estrategias Costas sync mas robustas y umbrales de sustraccion mas profundos/adaptativos para decoding weak-signal.
- Reloj UTC y panel Astro siguen ahora el formato `Dia Mes Ano` mas `UTC`, usando el idioma seleccionado en la app.
- El branding titulo/version del lado del usuario mantiene `Decodium` y elimina el antiguo texto `v3.0 FT2 "Raptor"`.
- El menu de idiomas mantiene ahora una sola entrada `English`.
- Defaults release, defaults workflow y documentacion release quedan alineados a la semver `1.5.4`.

#### Corregido

- Corregidos decodes FT2 ghost o malformados muy debiles que pasaban sin un filtro weak-signal dedicado.
- Corregida la web app que no exponia el control monitoring, no mostraba los dB ASYNC y usaba el idioma del navegador en vez del de la app.
- Corregidos casos limite de fallback/import de secure settings y llevado LoTW por defecto a `https`.
- Corregida la aceptacion en el downloader de redirects con scheme no valido y de descargas demasiado grandes sin limite.
- Corregidos logs silenciosos de excepciones CAT/transceiver y reducidos los blind waits de arranque DXLab.
- Corregido el enlazado de tests en Linux con `GNU ld`, ajustando el orden de las librerias estaticas.

## [1.5.3] - 2026-03-22

### English

Release focused on CQRLOG interoperability, FT4/FT8 Wait Features behaviour, local version propagation, and new bundled German/French translations.

#### Added

- Added bundled German (`de`) and French (`fr`) UI translation packs.
- Added the helper `tools/generate_ts_translations.py` to rebuild missing TS bundles from the English base.

#### Changed

- The language menu now exposes only translations that are actually bundled in the app and falls back safely to English if a saved language is no longer available.
- Release defaults and documentation are aligned to semantic version `1.5.3`, including the experimental Hamlib Linux build default.

#### Fixed

- Fixed Linux `CQRLOG wsjtx remote` interoperability by restoring the historical UDP listen-port behaviour and using `WSJTX` as the compatibility client id.
- Fixed local rebuild version drift so changing `fork_release_version.txt` propagates to the compiled app after rebuild/reconfigure.
- Fixed FT4/FT8 `Wait Features + AutoSeq` so a busy or late partner reply now pauses the current TX cycle instead of transmitting over an active QSO.

### Italiano

Release focalizzata su interoperabilita' CQRLOG, comportamento `Wait Features` in FT4/FT8, propagazione versione locale e nuove traduzioni bundle in tedesco/francese.

#### Aggiunto

- Aggiunti i pacchetti traduzione UI bundle tedesco (`de`) e francese (`fr`).
- Aggiunto l'helper `tools/generate_ts_translations.py` per rigenerare i bundle TS mancanti a partire dalla base inglese.

#### Modificato

- Il menu lingue mostra ora solo traduzioni realmente incluse nell'app e ripiega in sicurezza sull'inglese se la lingua salvata non e' piu' disponibile.
- Default release e documentazione sono allineati alla semver `1.5.3`, incluso il default del build Linux sperimentale Hamlib.

#### Corretto

- Corretta l'interoperabilita' Linux con `CQRLOG wsjtx remote` ripristinando il comportamento storico della listen port UDP e usando `WSJTX` come client id di compatibilita'.
- Corretto il drift della versione nelle build locali, cosi' un cambio in `fork_release_version.txt` si propaga davvero all'app compilata dopo rebuild/reconfigure.
- Corretto `Wait Features + AutoSeq` in FT4/FT8: una risposta tardiva o una stazione occupata mette ora in pausa il ciclo TX corrente invece di trasmettere sopra un QSO attivo.

### Espanol

Release centrada en interoperabilidad CQRLOG, comportamiento `Wait Features` en FT4/FT8, propagacion de version local y nuevas traducciones bundle en aleman/frances.

#### Anadido

- Anadidos los paquetes de traduccion UI bundle aleman (`de`) y frances (`fr`).
- Anadido el helper `tools/generate_ts_translations.py` para regenerar los bundles TS ausentes a partir de la base inglesa.

#### Cambios

- El menu de idiomas muestra ahora solo traducciones realmente incluidas en la app y vuelve de forma segura a ingles si la lengua guardada ya no esta disponible.
- Los defaults de release y la documentacion quedan alineados a la semver `1.5.3`, incluido el default del build Linux experimental Hamlib.

#### Corregido

- Corregida la interoperabilidad Linux con `CQRLOG wsjtx remote` restaurando el comportamiento historico de la listen port UDP y usando `WSJTX` como client id de compatibilidad.
- Corregido el desfase de version en builds locales para que un cambio en `fork_release_version.txt` se propague de verdad a la app compilada tras rebuild/reconfigure.
- Corregido `Wait Features + AutoSeq` en FT4/FT8: una respuesta tardia o una estacion ocupada pausa ahora el ciclo TX actual en lugar de transmitir sobre un QSO activo.

## [1.5.1] - 2026-03-19

### English

Release focused on in-app update discovery, FT2/FT4/FT8 late-signoff correctness, AutoCQ direct-reply state-machine hardening, map cleanup while transmitting CQ, and PSK Reporter identity alignment.

#### Added

- Added in-app update checks against the GitHub Releases feed.
- Added a manual `Help > Check for updates...` action.
- Added direct asset selection for the updater so macOS and Linux users are taken to the best matching download.

#### Changed

- PSK Reporter program information now uses the exact application title-bar string.
- macOS/Linux release workflows and release documentation are aligned to semantic version `1.5.1`.
- macOS release helper script now defaults to publishing against `elisir80/decodium3-build-macos`.

#### Fixed

- Fixed late `73/RR73` handling after local signoff so completed QSOs log instead of repeating `73/RR73`.
- Fixed timeout-abandon paths so delayed final acknowledgements from the active partner can still recover and log correctly.
- Fixed stale DX partner reuse after returning to CQ.
- Fixed direct-caller transitions that decoded the caller but kept transmitting CQ instead of arming `Tx2`.
- Fixed the first direct FT2 reply reusing a stale report from the previous QSO instead of the current caller SNR.
- Fixed the live world map showing a false active QSO path while transmitting plain CQ.

### Italiano

Release focalizzata su updater interno, correttezza late-signoff FT2/FT4/FT8, hardening della state machine AutoCQ sulle direct reply, pulizia mappa durante il CQ e allineamento identita' PSK Reporter.

#### Aggiunto

- Aggiunto controllo aggiornamenti interno contro il feed GitHub Releases.
- Aggiunta voce manuale `Help > Check for updates...`.
- Aggiunta selezione diretta dell'asset corretto nell'updater per macOS e Linux.

#### Modificato

- Le informazioni programma inviate a PSK Reporter usano ora esattamente la stringa della barra del titolo.
- Workflow release macOS/Linux e documentazione release sono ora allineati alla semver `1.5.1`.
- Lo script helper release macOS usa ora di default il repo `elisir80/decodium3-build-macos`.

#### Corretto

- Corretto il trattamento dei `73/RR73` tardivi dopo il signoff locale, cosi' i QSO chiusi vanno a log invece di ripetere `73/RR73`.
- Corretti i path di abbandono per timeout, cosi' i final ack ritardati del partner attivo possono ancora essere recuperati e loggati.
- Corretto il riuso di partner DX stantii dopo il ritorno a CQ.
- Corretti i passaggi caller-diretto in cui il decode era valido ma l'app continuava a trasmettere CQ invece di armare `Tx2`.
- Corretto il primo reply FT2 diretto che riusava un report stantio del QSO precedente invece dell'SNR del caller corrente.
- Corretta la live world map che mostrava un falso collegamento attivo durante il CQ puro.

### Espanol

Release centrada en updater interno, correccion late-signoff FT2/FT4/FT8, endurecimiento de la maquina de estados AutoCQ sobre respuestas directas, limpieza del mapa durante CQ y alineacion de identidad PSK Reporter.

#### Anadido

- Anadida comprobacion de updates contra el feed GitHub Releases.
- Anadida accion manual `Help > Check for updates...`.
- Anadida seleccion directa del asset correcto en el updater para macOS y Linux.

#### Cambios

- La informacion de programa enviada a PSK Reporter usa ahora exactamente la cadena de la barra de titulo.
- Workflows release macOS/Linux y documentacion release alineados ahora con la semver `1.5.1`.
- El script helper de release macOS publica ahora por defecto contra `elisir80/decodium3-build-macos`.

#### Corregido

- Corregido el tratamiento de `73/RR73` tardios tras el signoff local para que los QSO completados vayan a log en lugar de repetir `73/RR73`.
- Corregidos los paths de abandono por timeout para que los final ack retrasados del partner activo todavia puedan recuperarse y logearse.
- Corregido el reuso de partner DX obsoleto tras volver a CQ.
- Corregidos los pasos de caller directo donde el decode era valido pero la app seguia transmitiendo CQ en vez de armar `Tx2`.
- Corregido el primer reply FT2 directo que reutilizaba un reporte obsoleto del QSO anterior en vez del SNR del caller actual.
- Corregido el live world map que mostraba un enlace activo falso durante CQ puro.

## [1.5.0] - 2026-03-19

### English

Release focused on FT8/FT4/FT2 QSO correctness, AutoCQ stability, FT2 Quick QSO evolution, startup audio recovery, decoder sync from upstream, and initial Decodium certificate infrastructure.

#### Added

- Added startup RX-audio recovery logic that can reopen configured audio streams and re-arm monitor state automatically.
- Added `lib/ft2/decode174_91_ft2.f90` and moved FT2 triggered decode to the dedicated FT2 LDPC decoder path.
- Added the `Quick QSO` UI toggle as an alias for FT2 `2 msg`.
- Added AutoCQ diagnostic tracing to `debug.txt`.
- Added Decodium certificate loading/autoload/status integration in the main window.
- Added `tools/generate_cert.py` to generate `.decodium` certificate files.

#### Changed

- Local UI version, release workflows, and GitHub release naming are now aligned to semantic version `1.5.0`.
- Shared LDPC decoders were aligned to the upstream Normalized Min-Sum refresh.
- FT2 Quick QSO flow was refactored to the current short schema with mixed-mode and backward-compatibility handling.
- Linux and macOS release documentation now covers Tahoe/Sequoia/Monterey/AppImage targets with startup guidance and platform requirements.

#### Fixed

- Fixed startup cases where audio devices were selected correctly but RX audio only started after reopening Audio Preferences.
- Fixed standard 5-message FT8/FT4/FT2 QSOs skipping the local final `73` when the other station sent `RR73`/`73` first.
- Fixed FT2 Quick QSO log timing, including missed log-after-own-73 paths.
- Fixed multiple AutoCQ duplicate-rework cases where the app could immediately call the same station again after a completed QSO.
- Fixed queue handoff carrying stale retry counters, reports, or DX state into the next caller.
- Fixed FT8 retry timeouts overcounting decode passes instead of real missed periods.
- Fixed world map / DX context staying on the last QSO after returning to CQ.

### Italiano

Release focalizzata su correttezza QSO FT8/FT4/FT2, stabilita' AutoCQ, evoluzione Quick QSO FT2, recovery audio all'avvio, sync decoder da upstream e infrastruttura iniziale certificati Decodium.

#### Aggiunto

- Aggiunta logica di recovery RX-audio all'avvio che puo' riaprire automaticamente gli stream configurati e riarmare il monitor.
- Aggiunto `lib/ft2/decode174_91_ft2.f90` e spostato il triggered decode FT2 sul decoder LDPC dedicato FT2.
- Aggiunto il toggle UI `Quick QSO` come alias del profilo FT2 `2 msg`.
- Aggiunto tracing diagnostico AutoCQ in `debug.txt`.
- Aggiunta integrazione certificati Decodium con load/autoload/stato nella finestra principale.
- Aggiunto `tools/generate_cert.py` per generare file certificato `.decodium`.

#### Modificato

- Versione UI locale, workflow release e naming release GitHub ora allineati alla semver `1.5.0`.
- Decoder LDPC condivisi allineati al refresh upstream Normalized Min-Sum.
- Il flow FT2 Quick QSO e' stato rifattorizzato nello schema corto corrente con gestione mixed-mode e backward compatibility.
- La documentazione release macOS/Linux ora copre target Tahoe/Sequoia/Monterey/AppImage con istruzioni avvio e requisiti piattaforma.

#### Corretto

- Corretti i casi di avvio in cui le periferiche audio erano selezionate correttamente ma l'RX partiva solo dopo aver riaperto Preferenze Audio.
- Corretti i QSO standard a 5 messaggi FT8/FT4/FT2 che saltavano il `73` finale locale quando il corrispondente mandava prima `RR73`/`73`.
- Corretti i tempi di log FT2 Quick QSO, inclusi i path che perdevano il log dopo il proprio `73`.
- Corretti molteplici casi AutoCQ di retrigger duplicato in cui l'app poteva richiamare subito lo stesso nominativo appena lavorato.
- Corretto l'handoff della queue che portava retry, report o stato DX sporchi nel caller successivo.
- Corretto il timeout retry FT8 che sovracontava i passaggi decode invece dei periodi realmente persi.
- Corretta la persistenza mappa mondo / contesto DX sull'ultimo QSO dopo il ritorno a CQ.

### Espanol

Release centrada en correccion de QSO FT8/FT4/FT2, estabilidad AutoCQ, evolucion Quick QSO FT2, recuperacion audio al arrancar, sincronizacion de decoders desde upstream e infraestructura inicial de certificados Decodium.

#### Anadido

- Anadida logica de recuperacion RX-audio al arranque capaz de reabrir automaticamente streams configurados y rearmar el monitor.
- Anadido `lib/ft2/decode174_91_ft2.f90` y movido el triggered decode FT2 al path LDPC dedicado FT2.
- Anadido el toggle UI `Quick QSO` como alias del perfil FT2 `2 msg`.
- Anadido tracing diagnostico AutoCQ en `debug.txt`.
- Anadida integracion de certificados Decodium con load/autoload/estado en la ventana principal.
- Anadido `tools/generate_cert.py` para generar ficheros `.decodium`.

#### Cambios

- Version UI local, workflows release y naming release GitHub alineados ahora con la semver `1.5.0`.
- Decoders LDPC compartidos alineados al refresh upstream Normalized Min-Sum.
- El flow FT2 Quick QSO fue refactorizado al esquema corto actual con manejo mixed-mode y backward compatibility.
- La documentacion release macOS/Linux cubre ahora Tahoe/Sequoia/Monterey/AppImage con guia de arranque y requisitos de plataforma.

#### Corregido

- Corregidos casos de arranque donde los dispositivos de audio estaban bien seleccionados pero RX solo empezaba tras reabrir Preferencias Audio.
- Corregidos QSO estandar de 5 mensajes FT8/FT4/FT2 que saltaban el `73` final local cuando la otra estacion enviaba antes `RR73`/`73`.
- Corregidos tiempos de log FT2 Quick QSO, incluidos paths que perdian el log tras el propio `73`.
- Corregidos varios casos AutoCQ de retrabajo duplicado en los que la app podia volver a llamar enseguida al mismo callsign.
- Corregido el handoff de cola que arrastraba retry, reportes o estado DX sucios al siguiente caller.
- Corregido el timeout retry FT8 que sobrecontaba pasadas decode en vez de periodos realmente perdidos.
- Corregida la persistencia del mapa mundo / contexto DX sobre el ultimo QSO despues de volver a CQ.

## [1.4.9] - 2026-03-17

Previous stable cycle. See the historical release notes for the prior detailed changelog.
