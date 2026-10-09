# Decodium 4 v1.0.660

## English (UK)

Version 1.0.660 consolidates the AutoCQ and Multi-Answer Mode work delivered in 1.0.659, and completes the operational fixes identified during extended FT2, FT4 and FT8 on-air use. The focus is safe QSO completion, correct retry behaviour, reliable CAT PTT release on macOS, and an accurate visual representation of every carrier actually transmitted by MAM.

### Changes since 1.0.658

- **Active-profile retry policy.** The configured `MaxCallerRetries` value is applied consistently by the bridge and embedded sequencers. An exchange stops after the initial transmission plus the configured repeats instead of continuing indefinitely.
- **Safe QSO progression.** A new caller cannot inherit TX4/TX5 state from an earlier QSO. RR73 is only selected after a report has genuinely been sent to, and acknowledged by, the same station.
- **Late decode recovery without false logs.** A retry-expired MAM exchange is retained briefly for a matching late report or final 73. A received final 73 can complete and log that exact QSO; retry exhaustion or silence alone cannot create a logged contact.
- **Bounded duplicate protection.** The short post-QSO cooldown is pruned in the native MAM path as well as the legacy sequencer. It prevents immediate sign-off loops without excluding a valid caller for the rest of a long session or across a UTC date change.
- **Original-frequency integrity.** Queued callers, late reports and resumed exchanges remain on their decoded audio frequency, rather than being moved to the current base TX frequency.
- **External logging path.** Bridge-committed QSOs are delivered to external UDP loggers with the legacy FT4/FT8 backend active, removing the case where a QSO was present locally but absent from DecoDXLog.

### Multi-Answer Mode and macOS TX

- **Actual MAM TX streams are exposed to the UI.** The bridge now publishes the exact composite payload handed to the transmitter: active QSOs and parallel CQ streams on otherwise free MAM slots.
- **Waterfall and panadapter markers now show every carrier that is actually on air.** During a composite MAM transmission, the historical single-frequency TX guide is suppressed and one static marker is drawn per outgoing stream, including parallel CQ carriers. This makes selected multi-slot capacity visible without adding per-marker animation load to the panadapter.
- **MAM display remains truthful outside TX.** While receiving, the waterfall continues to show active QSO slots only; idle capacity is not displayed as an occupied frequency.
- **HALT releases CAT PTT on macOS.** When bridge-owned native PCM transmission is active, HALT now queues the physical PTT-OFF path before stopping the legacy backend, preventing the application from stopping while the radio remains keyed.
- **TX-payload lifecycle notifications.** Stream markers are cleared promptly on mode changes, payload cleanup, disabled TX and empty dispatches, preventing stale on-air indications.

### Reliability and verification

- Added regression coverage for final-73 recovery, no-log-on-retry-expiry behaviour, MAM cooldown expiry, macOS HALT PTT abort and composed-payload waterfall markers.
- Source archives are generated automatically by GitHub for this tag. The release workflows publish the Windows x64 installer, Apple Silicon and Intel macOS DMGs, and x86_64/aarch64 Linux AppImages with their checksum files.

---

## Italiano

La versione 1.0.660 consolida il lavoro su AutoCQ e Multi-Answer Mode consegnato con la 1.0.659 e completa le correzioni operative emerse durante l'uso reale in FT2, FT4 e FT8. L'obiettivo e' una chiusura QSO sicura, retry corretti, rilascio affidabile del PTT CAT su macOS e una rappresentazione visiva fedele di ogni portante realmente trasmessa dal MAM.

### Modifiche dalla 1.0.658

- **Politica retry del profilo attivo.** Il valore configurato di `MaxCallerRetries` viene applicato in modo coerente dal bridge e dai sequencer integrati. Uno scambio si ferma dopo la trasmissione iniziale piu' le sole ripetizioni configurate, invece di proseguire all'infinito.
- **Avanzamento QSO sicuro.** Un nuovo chiamante non puo' ereditare lo stato TX4/TX5 di un QSO precedente. RR73 viene scelto soltanto dopo che un rapporto e' stato realmente inviato a quella stazione e da essa confermato.
- **Recupero dei decode tardivi senza log errati.** Uno scambio MAM scaduto per retry viene conservato brevemente per un rapporto tardivo corrispondente o un 73 finale. Un 73 finale ricevuto puo' chiudere e mandare a log quel preciso QSO; esaurire i retry o il silenzio non puo' da solo creare un contatto a log.
- **Protezione duplicati limitata nel tempo.** Il breve cooldown post-QSO viene ripulito anche dal percorso MAM nativo, oltre che dal sequencer legacy. Evita i loop immediati di saluto senza escludere una chiamata valida per tutta una lunga sessione o dopo il cambio data UTC.
- **Integrita' della frequenza originale.** Chiamanti in coda, rapporti tardivi e scambi ripresi restano sulla frequenza audio alla quale sono stati decodificati, invece di essere spostati sulla frequenza TX base corrente.
- **Percorso verso i logger esterni.** I QSO confermati dal bridge arrivano ai logger UDP esterni anche con il backend FT4/FT8 legacy attivo, eliminando il caso in cui il QSO fosse presente localmente ma assente da DecoDXLog.

### Multi-Answer Mode e TX macOS

- **Gli stream TX MAM reali sono ora esposti alla UI.** Il bridge pubblica il payload composito esatto inviato al trasmettitore: QSO attivi e CQ paralleli sugli slot MAM altrimenti liberi.
- **I marker waterfall e panadapter mostrano ogni portante effettivamente in aria.** Durante una trasmissione MAM composita, la storica guida TX a frequenza singola viene soppressa e viene disegnato un marker statico per ogni stream uscente, inclusi i CQ paralleli. Cosi' la capacita' multi-slot selezionata e' visibile senza aggiungere animazioni per marker al panadapter.
- **Il display MAM resta fedele anche fuori TX.** In ricezione il waterfall continua a mostrare soltanto gli slot QSO attivi; la capacita' inattiva non viene indicata come frequenza occupata.
- **HALT rilascia il PTT CAT su macOS.** Quando e' attiva la trasmissione PCM nativa gestita dal bridge, HALT ora accoda il percorso fisico PTT-OFF prima di fermare il backend legacy, evitando che il software si fermi mentre la radio resta in trasmissione.
- **Notifiche del ciclo di vita del payload TX.** I marker stream vengono rimossi rapidamente su cambio modo, pulizia payload, TX disabilitata e dispatch vuoti, evitando indicazioni stale di trasmissione.

### Affidabilita' e verifica

- Aggiunta copertura di regressione per recupero 73 finale, assenza di log alla sola scadenza retry, scadenza cooldown MAM, abort PTT di HALT su macOS e marker waterfall del payload composto.
- Gli archivi sorgente sono generati automaticamente da GitHub per questo tag. I workflow di release pubblicano l'installer Windows x64, i DMG macOS Apple Silicon e Intel e le AppImage Linux x86_64/aarch64 con i relativi checksum.
