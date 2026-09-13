# Decodium 4 FT2 1.0.490

## English

This release completes the next stability pass after 1.0.489. It focuses on
keeping the decode pipeline responsive under dense FT8/FT4 activity, avoiding
slow-start behaviour on Windows, preserving map updates and making diagnostic
logging safe during shutdown.

### Incremental decode delivery and display

- Decode entries are de-duplicated through bounded native indexes instead of
  repeated linear scans of the visible history.
- Full Spectrum, Band Activity and Signal RX remain independently updated while
  priority QSO traffic, AutoSeq and local-station messages are handled first.
- Secondary work such as reporting, map projection, alerts, UDP publication and
  timing feedback is deferred through bounded low-priority queues.
- Native and legacy decode paths now share throttled snapshot preparation,
  reducing full-list sorting and normalization on the GUI thread.
- Budgeted decode models have both row and time limits. Large insertions are
  split into smaller chunks, with an extra conservative first-population path
  on Windows to avoid a costly burst of QML delegate creation.
- Existing rows remain visible while snapshot updates continue, preventing
  transient empty Full Spectrum and Signal RX panes.

### FT8 and FT4 decoder scheduling

- FT8 hash seeds and context refreshes are collected asynchronously and applied
  in bounded batches rather than blocking a decode cycle.
- FT4 uses the same asynchronous hash-seed path, avoiding repeated shared
  runtime work between candidates and slots.
- Adaptive FT8 pressure handling is carried explicitly in decode requests and
  only reduces the requested thread budget under measured pressure.
- Startup guards defer non-essential early FT8 work until the QML and GPU path
  has settled, while preserving normal full-slot decoding afterwards.
- Decoder metrics now identify pressure-limited passes and hash preparation
  time, making regressions easier to diagnose across macOS, Windows and Linux.

### Map and diagnostic reliability

- Live Map consumers explicitly report readiness. Deferred contacts are replayed
  when a docked or detached map becomes available, including contacts decoded
  before the pane was opened.
- The diagnostic log now uses one low-priority writer thread with ordered
  writes, bounded duplicate suppression and asynchronous flush requests.
- The diagnostic writer is shut down before Qt static destruction. This fixes a
  macOS exit hang/abort caused by a still-running logging QThread.
- Late logging calls after shutdown are safely ignored instead of recreating the
  writer during process termination.

### Validation

- `decodium_qml` was rebuilt successfully on macOS.
- Clean application shutdown was exercised three times with exit code 0 and no
  remaining process, SIGABRT or `QThread: Destroyed while thread is still
  running` diagnostic.
- The release workflows build the Windows x64 installer, macOS Apple Silicon
  and Intel DMGs, Linux x86_64 AppImage and Linux aarch64 AppImage.

## Italiano

Questa release completa il successivo intervento di stabilita' dopo la 1.0.489.
L'obiettivo e' mantenere reattiva la pipeline FT8/FT4 anche con molti decode,
ridurre i rallentamenti iniziali su Windows, preservare gli aggiornamenti della
mappa e rendere sicuro il diagnostic log in chiusura.

### Consegna decode e visualizzazione incrementali

- Le righe decode usano indici nativi limitati per il deduplica, evitando
  scansioni lineari ripetute della cronologia visibile.
- Full Spectrum, Band Activity e Signal RX vengono aggiornati in modo
  indipendente, mentre traffico QSO prioritario, AutoSeq e messaggi alla
  stazione locale vengono gestiti subito.
- Reporting, proiezione mappa, alert, UDP e feedback timing sono rinviati in
  code a bassa priorita' e con dimensione limitata.
- I percorsi native e legacy condividono snapshot preparati e notificati a
  tranche, riducendo ordinamenti e normalizzazioni complete sul thread grafico.
- I modelli decode hanno limiti sia di righe sia di tempo; gli inserimenti grandi
  sono spezzati in blocchi piccoli, con una prima popolazione particolarmente
  prudente su Windows per evitare picchi nella creazione dei delegate QML.
- Le righe esistenti restano visibili durante l'aggiornamento, evitando che Full
  Spectrum o Signal RX si svuotino temporaneamente.

### Scheduling decoder FT8 e FT4

- Seed hash e refresh del contesto FT8 vengono raccolti in modo asincrono e
  applicati in batch limitati, senza bloccare un ciclo decode.
- FT4 usa lo stesso percorso hash asincrono, evitando lavoro ripetuto tra
  candidati e slot sul runtime condiviso.
- La pressione adattiva FT8 viene propagata esplicitamente nella richiesta di
  decode e riduce i thread solo quando la pressione e' misurata.
- Le protezioni di avvio rinviano il lavoro FT8 non essenziale finche' QML e GPU
  non si sono stabilizzati, mantenendo poi la decodifica completa dello slot.
- Le metriche identificano i passaggi limitati dalla pressione e il tempo di
  preparazione hash, facilitando la diagnosi su macOS, Windows e Linux.

### Affidabilita' mappa e diagnostica

- I consumer della Live Map dichiarano quando sono pronti. I contatti rinviati
  vengono riprodotti quando una mappa docked o staccata torna disponibile,
  inclusi i decode arrivati prima dell'apertura del pannello.
- Il diagnostic log usa ora un solo writer a bassa priorita', con scritture
  ordinate, soppressione limitata dei duplicati e flush asincroni.
- Il writer viene chiuso prima della distruzione statica di Qt. Questo risolve
  su macOS il freeze/abort in uscita causato da un QThread di logging ancora
  attivo.
- I messaggi tardivi dopo lo shutdown vengono ignorati senza riattivare il
  writer durante la terminazione del processo.

### Verifica

- `decodium_qml` e' stato ricompilato con successo su macOS.
- La chiusura pulita dell'applicazione e' stata verificata tre volte con exit
  code 0, senza processi residui, SIGABRT o messaggi `QThread: Destroyed while
  thread is still running`.
- I workflow di release producono installer Windows x64, DMG macOS Apple
  Silicon e Intel, AppImage Linux x86_64 e AppImage Linux aarch64.
