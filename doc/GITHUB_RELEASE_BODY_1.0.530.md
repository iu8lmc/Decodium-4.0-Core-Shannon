# Decodium 4.0 v1.0.530

Version 1.0.530 consolidates the work delivered in 1.0.528, 1.0.529 and the
current reliability and usability fixes. It improves RTL-SDR packaging, CAT
profile handling, Hamlib frequency changes, Settings, Live Map operation and
FT2-Link satellite configuration.

## English — changes from 1.0.527 to 1.0.530

### RTL-SDR receive support — 1.0.528

- The supported builds include the RTL-SDR receiver path, built with
  librtlsdr 2.0.2. An RTL-SDR may be used as a receive input with its own
  tuning plan, DSP, RF spectrum and audio-output path.
- The FT2-Link diagnostic decoder keeps its explicitly bounded decode budget,
  including the accompanying ultra-low-SNR search safeguards.
- The Windows and macOS build paths remain aligned with the RTL-SDR helper
  requirements.

### CAT profile settings no longer revert at start-up — 1.0.529

- Starting Decodium no longer reapplies the saved CAT-profile snapshot over
  the live CAT configuration.
- A selected backend, port, baud rate, data mode and SWR thresholds therefore
  remain the values chosen by the operator. In particular, a previously saved
  Hamlib profile can no longer silently prevent a TCI configuration from being
  retained.
- Existing profiles continue to work when deliberately loaded; they are not
  modified automatically.

### Reliable Hamlib QSY across modes and bands — 1.0.530

- Application mode and band changes now have one CAT-frequency owner, avoiding
  duplicate QSY transactions that could race one another.
- A requested frequency is retained in the Hamlib queue even when a following
  mode, split or poll update is pending. Ordering remains serialised by the
  transceiver worker.
- The post-QSY check compares the requested and reported dials, with bounded
  retries where the radio has not yet accepted the command.
- A transient CI-V timeout or bus error is no longer treated as confirmation
  that the radio changed frequency. The last genuinely confirmed dial is kept
  until polling verifies the new one, preventing an old-band poll from
  overwriting a new QSY.
- CAT reconnect and PTT/tune dispatches avoid waiting on the GUI thread,
  reducing the risk that a slow serial radio makes the interface unresponsive.

### Settings and detached panels

- Settings pages are split into lazy tab components and warmed at a safe time,
  reducing the work performed by the first Settings interaction while receive
  and decoding are active.
- On Windows, visible always-on-top pop-out panels are temporarily hidden while
  the modal Settings window is open and restored afterwards. Settings can no
  longer appear behind a pop-out and become impossible to use.

### Live Map and PSK Reporter

- The Live Map PSK Reporter look-back and expiry window is selectable in
  five-minute steps from 5 to 60 minutes, so operators can discard stale spots
  without losing the option for a wider overview.
- The satellite/operational marker card now uses explicit CALL, QRZ, ROTATE
  and TRACK controls. Their labels remain readable on affected Qt/Windows
  styles, and unavailable controls explain why they cannot be used.
- Map snapshot publication is coalesced and short-lived visual refreshes are
  deferred, reducing redundant QML/scene-graph work during decode or PSK bursts
  without changing the displayed data.

### FT2-Link satellite operation

- With a connected Hamlib rig that reports genuine rig split, an empty
  FT2-Link satellite profile can safely import the current RX and TX VFO pair.
- **Read RX/TX from rig** lets the operator refresh the pair explicitly. Both
  paths only read CAT state: they never retune a VFO or change PTT.

## Italiano — cambiamenti dalla 1.0.527 alla 1.0.530

### Supporto ricezione RTL-SDR — 1.0.528

- Le build supportate includono il ricevitore RTL-SDR, compilato con
  librtlsdr 2.0.2. Un RTL-SDR puo' essere usato come ingresso RX con piano di
  sintonia, DSP, spettro RF e uscita audio propri.
- Il decoder diagnostico FT2-Link mantiene il budget di decoding esplicitamente
  limitato, comprese le protezioni per la ricerca a SNR molto basso.
- I percorsi di build Windows e macOS restano allineati ai requisiti del helper
  RTL-SDR.

### I profili CAT non annullano piu' le impostazioni all'avvio — 1.0.529

- All'avvio Decodium non riapplica piu' lo snapshot salvato del profilo CAT
  sopra la configurazione CAT corrente.
- Backend, porta, baud rate, modo dati e soglie SWR rimangono quindi quelli
  scelti dall'operatore. In particolare, un vecchio profilo Hamlib non puo' piu'
  impedire silenziosamente il mantenimento di una configurazione TCI.
- I profili esistenti continuano a funzionare quando vengono caricati
  esplicitamente; non sono modificati automaticamente.

### QSY Hamlib affidabile tra modi e bande — 1.0.530

- I cambi di modo e banda hanno ora un unico gestore della frequenza CAT,
  evitando transazioni QSY duplicate che potevano entrare in conflitto.
- Una frequenza richiesta resta nella coda Hamlib anche se sono in attesa un
  aggiornamento di modo, split o polling. L'ordine resta serializzato dal
  worker del ricetrasmettitore.
- Il controllo dopo il QSY confronta dial richiesto e dial riportato, con retry
  limitati se la radio non ha ancora accettato il comando.
- Un timeout CI-V o un errore transitorio del bus non viene piu' considerato
  conferma del cambio frequenza. L'ultimo dial realmente confermato viene
  mantenuto finche' il polling non verifica quello nuovo, impedendo a un poll
  della banda precedente di sovrascrivere il QSY.
- Riconnessione CAT e invio di PTT/tune evitano attese nel thread grafico,
  riducendo il rischio che una radio seriale lenta renda l'interfaccia poco
  reattiva.

### Impostazioni e pannelli separati

- Le pagine delle impostazioni sono suddivise in componenti caricati a richiesta
  e preriscaldati in un momento sicuro, riducendo il lavoro della prima apertura
  mentre RX e decoding sono attivi.
- Su Windows, i pop-out visibili sempre in primo piano vengono nascosti
  temporaneamente mentre la finestra modale Impostazioni e' aperta e poi
  ripristinati. Le Impostazioni non possono piu' apparire dietro a un pop-out e
  risultare non utilizzabili.

### Live Map e PSK Reporter

- La finestra temporale ed il periodo di scadenza degli spot PSK Reporter nella
  Live Map sono selezionabili da 5 a 60 minuti a passi di 5 minuti, per
  escludere dati non piu' rilevanti senza rinunciare a una panoramica piu'
  ampia quando serve.
- La scheda dei marker satellitari/operativi usa ora controlli espliciti CALL,
  QRZ, ROTATE e TRACK. Le etichette restano leggibili negli stili Qt/Windows
  interessati e i controlli non disponibili spiegano il motivo.
- La pubblicazione degli snapshot della mappa viene accorpata e i brevi refresh
  visivi sono differiti, riducendo il lavoro ridondante di QML/scene graph
  durante burst di decode o PSK senza modificare i dati mostrati.

### Operativita' satellitare FT2-Link

- Con una radio Hamlib connessa che riporta lo split reale della radio, un
  profilo satellitare FT2-Link vuoto puo' importare in sicurezza la coppia VFO
  RX e TX corrente.
- **READ RX/TX FROM RIG** consente di rileggere esplicitamente la coppia. Sia
  l'import automatico sia quello manuale leggono solo lo stato CAT: non
  risintonizzano VFO e non modificano il PTT.

## Release assets

The published GitHub release provides the source archives generated for
`v1.0.530`, the Microsoft Windows x64 installer, macOS Apple Silicon and Intel
DMG packages, and Linux x86_64 and aarch64 AppImages with their SHA-256 files.

La release GitHub pubblicata fornisce gli archivi sorgenti generati per
`v1.0.530`, l'installer Microsoft Windows x64, i pacchetti DMG macOS Apple
Silicon e Intel e gli AppImage Linux x86_64 e aarch64 con i rispettivi file
SHA-256.
