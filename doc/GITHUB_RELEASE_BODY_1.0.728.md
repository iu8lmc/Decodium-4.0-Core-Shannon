# Decodium 4 FT2 1.0.728

## English (UK)

This release builds on 1.0.727 and brings the current SSTV development work into the main codebase. Several digital and fax capabilities remain experimental; the evidence and limitations below are part of the release description.

### SSTV and image operation

- Adds experimental radiofax/HFFAX/WEFAX reception and transmission paths, configurable phasing and alignment, stop detection, partial-image handling and diagnostic tools. It also adds experimental FAX480 profiles, including Dayton, QSSTV-style and VIS 85/libsstv variants. Mode-specific timing and geometry, independent application compatibility and difficult-channel behaviour remain under investigation.
- Adds AVT-24, AVT-90 and AVT-94 QRM and Narrow-QRM modes, with transmission, reception, mode registration, VIS acquisition and protected countdown handling. A VIS boundary correction improves acquisition when the capture starts at different sample phases.
- Improves receive routing and capture timing, image processing, mode controls, transmission coordination and cancellation, Gallery and QSO integration, and the associated SSTV user interface and diagnostics.

### Encrypted remote sharing

- Adds optional recipient-encrypted remote sharing with X25519 key agreement, HKDF-SHA-256 and authenticated encryption, recipient fingerprints, secure key storage, versioned envelopes, incoming validation and an option to require encryption for new downloads. E2EE requires OpenSSL Crypto at build time and a usable secure-settings backend; builds without these prerequisites may not provide it. The macOS implementation uses the native keychain; other platforms remain unverified.
- Recipient fingerprints must be checked independently. Encryption does not authenticate the sender, prevent a provider from withholding items, or protect an already compromised endpoint. Public-provider interoperability and an independent security audit have not been established; ordinary TLS-only sharing remains available unless encryption is required.

### HAMDRM

- Extends the native, documented HAMDRM subset: profile handling, MOT/CRC processing, waveform and audio adapters, image validation, partial-object persistence, BSR generation and reception, and operator-initiated selective retransmission of segments from the last completed image transmission. Repeated BSR requests and legacy-monitor routing receive additional handling and diagnostics.
- BSR codec comparisons use pinned QSSTV methods, and local tests cover two Decodium processes. This is **not** a claim of complete QSSTV or EasyPal interoperability. A full audio BSR/repair round trip is still unresolved: local trials observed capture stopping after a TX/RX transition. Repair requests are unauthenticated, identifiers may collide, and the cached transmitted image is not retained after restart.

### Reliability, security and verification

- Improves bounded incoming-media checks, queue and settings handling, macOS capture diagnostics, PCM routing, startup/command-line error handling and clean diagnostic-log shutdown. Includes reproducible test fixtures and laboratory tools; test certificates are explicitly public and for localhost testing only.
- The local SSTV suite passed 98/98 before the latest changes, with subsequent focused tests and A/B two-process checks. These checks do not establish operation on real radio hardware, every radio/audio configuration, all operating-system versions, public services, or every third-party protocol variant. Consult `docs/sstv/` for exact coverage, test conditions and outstanding issues.
- This release requests Windows x64, Linux x86-64 and aarch64, and macOS Apple Silicon and Intel packages. Availability and supported OS versions depend on the completed build jobs and the actual assets attached to this release.

## Italiano

Questa versione parte dalla 1.0.727 e porta nel codice principale il lavoro SSTV attualmente disponibile. Diverse funzioni digitali e fax restano sperimentali; le prove e i limiti indicati qui fanno parte della descrizione della release.

### SSTV e immagini

- Aggiunge percorsi sperimentali di ricezione e trasmissione radiofax/HFFAX/WEFAX, fasatura e allineamento configurabili, rilevamento dello stop, gestione delle immagini parziali e strumenti diagnostici. Aggiunge anche profili FAX480 sperimentali, inclusi Dayton, stile QSSTV e VIS 85/libsstv. Temporizzazioni, geometrie, compatibilità con applicazioni indipendenti e comportamento su canali difficili sono ancora in verifica.
- Aggiunge AVT-24, AVT-90 e AVT-94 QRM e Narrow-QRM, con trasmissione, ricezione, registrazione dei modi, acquisizione VIS e gestione del countdown protetto. Una correzione del confine VIS migliora l'acquisizione con diverse fasi dei campioni.
- Migliora instradamento e temporizzazione audio RX, elaborazione delle immagini, controlli dei modi, coordinamento e annullamento TX, integrazione Gallery/QSO, interfaccia SSTV e diagnostica.

### Condivisione remota cifrata

- Aggiunge la cifratura opzionale per destinatario, con scambio chiavi X25519, HKDF-SHA-256 e cifratura autenticata, impronte del destinatario, archiviazione sicura delle chiavi, buste versionate, validazione in ingresso e opzione per richiedere la cifratura ai nuovi download. E2EE richiede OpenSSL Crypto in compilazione e un backend sicuro disponibile; i pacchetti privi di questi requisiti potrebbero non offrirla. Su macOS viene usato il portachiavi nativo; le altre piattaforme restano da verificare.
- Le impronte dei destinatari devono essere verificate indipendentemente. La cifratura non autentica il mittente, non impedisce al provider di nascondere elementi e non protegge un dispositivo già compromesso. Non sono ancora dimostrati interoperabilità con provider pubblici e audit crittografico indipendente; la condivisione con sola TLS rimane disponibile se non si richiede E2EE.

### HAMDRM

- Estende il sottoinsieme nativo HAMDRM documentato: profili, elaborazione MOT/CRC, adattatori di forma d'onda e audio, validazione delle immagini, persistenza degli oggetti parziali, generazione/ricezione BSR e ritrasmissione selettiva avviata dall'operatore dei segmenti dell'ultima immagine trasmessa. Sono stati aggiunti gestione dei BSR ripetuti e diagnostica dell'instradamento con monitor legacy.
- Il codec BSR è confrontato con metodi QSSTV fissati a una revisione e sono disponibili prove locali con due processi Decodium. Questo **non** dimostra l'interoperabilità completa con QSSTV o EasyPal. Il ciclo BSR/riparazione interamente via audio resta aperto: prove locali hanno osservato l'arresto della cattura dopo un cambio TX/RX. Le richieste di riparazione non sono autenticate, gli identificativi possono coincidere e l'immagine trasmessa conservata in memoria non sopravvive al riavvio.

### Affidabilità, sicurezza e verifiche

- Migliora i controlli sui media in ingresso, la gestione di code e impostazioni, la diagnostica della cattura macOS, l'instradamento PCM, la gestione della riga di comando e la chiusura del log diagnostico. Include fixture e strumenti di laboratorio riproducibili; i certificati di test sono pubblici e destinati esclusivamente a localhost.
- La suite SSTV locale ha superato 98 test su 98 prima delle ultime modifiche, seguita da test mirati e prove A/B con due processi. Questi risultati non dimostrano il funzionamento con radio reali, tutte le configurazioni radio/audio, tutte le versioni dei sistemi operativi, servizi pubblici o ogni variante di protocollo di terze parti. In `docs/sstv/` sono riportati copertura, condizioni dei test e problemi aperti.
- Sono previsti pacchetti Windows x64, Linux x86-64 e aarch64, macOS Apple Silicon e Intel. La disponibilità e le versioni dei sistemi operativi supportate dipendono dai job completati e dagli asset effettivamente allegati alla release.
