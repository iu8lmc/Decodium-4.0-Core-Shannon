# Decodium 4 FT2 v1.0.591

This fork release carries the changes made after v1.0.589: upstream FT2
decoder improvements from v1.0.590, plus the native SSTV and Apple Silicon
stability work completed locally for v1.0.591.

## English (British)

### v1.0.590: FT2 decoding, decode integrity and waterfall interaction

- Added the `fastldpc` vectorised LDPC path for FT2.  The AVX2 min-sum kernel
  is selected only where supported; other CPUs retain the original decoder
  path at run time.
- Added the Fast LDPC toolbar control and persistence for its state.
- Added a false-decode gate for candidates that satisfy an LDPC syndrome by
  chance.  The gate compares the received soft information with the decoded
  word and exposes the `DECODIUM_LDPC_ND_MAX` and
  `DECODIUM_LDPC_MAX_HARD` thresholds for careful weak-signal tuning.
- Corrected the sign in the shared LDPC min-sum branch.
- Clicking a decoded callsign on the waterfall now calls that station; a click
  away from a callsign, or Ctrl+click, still sets the transmit frequency.
- Fixed the in-source QML synchronisation safeguard and two Qt 6.11
  `QFile::open` diagnostics in MAP65 builds.

### v1.0.591: stable native SSTV monitoring and portable FT2 builds

- Reworked native SSTV receive ownership of the shared audio route.  When SSTV
  uses Decodium's dedicated `SoundInput`, the quiescent legacy backend is no
  longer mistaken for a failed monitor, preventing repeated RX re-arming that
  could starve the panadapter.
- Restored the normal RX start generation after SSTV closes or an SSTV start
  fails.  The dashboard's spectrum timers, PCM feed and monitor now return as
  one coherent state instead of requiring a manual Monitor OFF/ON cycle.
- Kept the panadapter's visual path supplied while native SSTV owns RX, and
  allow the existing accelerated visual path to be used when its GPU and
  pressure guards permit it.  The conservative CPU protections remain active
  when those guards require them.
- Batched SSTV receiver-control persistence into one profile-aware settings
  transaction.  Locking a receive mode now sends the related control values in
  one update instead of performing multiple synchronous settings writes on the
  GUI path.
- Added the selected SSTV receive-device name to the receiver and Settings
  views.  Long device names are safely elided, while the full name remains
  available as a tooltip in the receive view.
- Made the new FT2 `fastldpc` integration build correctly on Apple Silicon and
  other non-x86 targets.  AVX2 compile options and CPU probing are now limited
  to x86; ARM builds use the existing scalar fallback, and GCC-only loop
  pragmas are not passed to Clang under `-Werror`.
- Added QML coverage that verifies mode locking performs one atomic receiver
  controls update.  On macOS Apple Silicon, the application and focused SSTV
  receive, runtime and ingress tests pass for this source tree.

### Distribution and validation boundary

- GitHub's generated source archives for tag `v1.0.591` are the codebase
  downloads for this release.
- The release publishes the unsigned Windows x64 installer, Linux Qt 6.11
  AppImages for x86_64 and aarch64, and the macOS DMGs produced by the Apple
  Silicon and Intel runner matrices, with SHA-256 files where produced by the
  workflows.
- Native SSTV/HAMDRM remains the documented in-tree subsystem.  This release
  does not claim universal over-the-air interoperability with every
  QSSTV/EasyPal mode and does not replace live radio, audio-device or RF
  validation.

## Italiano

### v1.0.590: decodifica FT2, integrità dei decode e interazione waterfall

- Aggiunto il percorso LDPC vettorizzato `fastldpc` per FT2. Il kernel min-sum
  AVX2 viene scelto solo dove supportato; sulle altre CPU il programma conserva
  a runtime il decoder originale.
- Aggiunto il controllo Fast LDPC nella barra strumenti con memorizzazione
  dello stato.
- Aggiunto un filtro per i falsi decode che azzerano una sindrome LDPC per
  caso. Il filtro confronta l'informazione soft ricevuta con la parola
  decodificata e rende disponibili le soglie `DECODIUM_LDPC_ND_MAX` e
  `DECODIUM_LDPC_MAX_HARD` per una regolazione prudente sui segnali deboli.
- Corretto il segno del ramo min-sum nel decoder LDPC condiviso.
- Un clic su un nominativo decodificato nel waterfall ora chiama quella
  stazione; il clic lontano da un nominativo, oppure Ctrl+clic, continua a
  impostare la frequenza di trasmissione.
- Corretta la protezione della sincronizzazione QML nelle build in-source e
  due diagnostiche `QFile::open` di Qt 6.11 nelle build MAP65.

### v1.0.591: monitoraggio SSTV nativo stabile e build FT2 portabili

- Rielaborata la proprieta' della rotta audio condivisa in ricezione SSTV
  nativa. Quando SSTV usa `SoundInput` dedicato di Decodium, il backend legacy
  quiescente non viene piu' scambiato per un monitor caduto: sono cosi'
  evitati ripetuti riavvii RX che potevano affamare il panadapter.
- Ripristinata la normale generazione di avvio RX dopo la chiusura SSTV o dopo
  un avvio SSTV fallito. Timer dello spettro, feed PCM e monitor della dashboard
  tornano insieme a uno stato coerente, senza dover premere manualmente
  Monitor OFF/ON.
- Mantenuto alimentato il percorso visuale del panadapter mentre SSTV nativo
  possiede RX, permettendo l'uso del percorso accelerato esistente quando GPU e
  protezioni di carico lo consentono. Le protezioni CPU conservative restano
  attive quando sono necessarie.
- Raggruppato il salvataggio dei controlli RX SSTV in un'unica transazione di
  impostazioni compatibile con il profilo. Il blocco del modo invia ora i
  valori correlati in un solo aggiornamento, invece di piu' scritture sincrone
  sul percorso GUI.
- Aggiunto il nome del dispositivo di ricezione SSTV selezionato nelle pagine
  Receive e Settings. I nomi lunghi vengono abbreviati in sicurezza e il nome
  completo resta visibile come tooltip nella pagina Receive.
- Resa corretta la compilazione dell'integrazione FT2 `fastldpc` su Apple
  Silicon e sugli altri target non-x86. Opzioni AVX2 e rilevamento CPU sono ora
  limitati a x86; le build ARM usano il fallback scalare esistente e le pragma
  specifiche GCC non vengono passate a Clang con `-Werror`.
- Aggiunta copertura QML che verifica che il blocco del modo effettui un unico
  aggiornamento atomico dei controlli RX. Su macOS Apple Silicon l'applicazione
  e i test SSTV mirati di receive, runtime e ingress passano per questo tree.

### Distribuzione e limite della validazione

- Gli archivi sorgente generati da GitHub per il tag `v1.0.591` costituiscono i
  download del codebase di questa release.
- La release pubblica l'installer Windows x64 non firmato, le AppImage Linux
  Qt 6.11 x86_64 e aarch64 e i DMG macOS prodotti dalle matrici runner Apple
  Silicon e Intel, con file SHA-256 dove generati dai workflow.
- SSTV/HAMDRM nativi restano il sottosistema documentato nel tree. Questa
  release non dichiara interoperabilita' in aria universale con ogni modo
  QSSTV/EasyPal e non sostituisce le prove live con radio, dispositivo audio o
  RF.
