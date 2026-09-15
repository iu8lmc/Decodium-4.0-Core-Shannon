# Decodium 4 FT2 v1.0.587

This release carries the fork from v1.0.583 through v1.0.586 and adds the
native C++ SSTV and HAMDRM subsystem, its responsive Decodium workspace,
storage/sharing/diagnostics layers, release packaging, and the final local
hardening and verification work.

## English (British)

### Changes carried from v1.0.583 to v1.0.586

- v1.0.584 reworked the worked-station filters around the base callsign and
  added the `today`, `yesterday` and `ever` scopes without changing the
  existing QSO history contract.
- v1.0.585 carried radio meters through DecoPort, including S-meter, forward
  power, SWR, ALC, PA temperature, drain voltage, drain current, compression
  and the power setting, with explicit capability/state fields.
- v1.0.586 fixed the idle-meter path so those readings are actually reached,
  keeps supply readings fresh outside transmission, and centralises their
  scaling between receive and transmit paths.

### Native SSTV in Decodium4

- Added a native, bounded C++ SSTV subsystem integrated into Decodium's
  existing audio ingress, SoundOutput, CAT/PTT/VOX authority, storage and QML
  services. No external Python, Java or second radio process is required.
- Added a canonical registry and mode specifications for 52 implemented
  analogue rows: Martin M1--M4; Scottie S1--S4 and DX; Robot colour C12/C24/
  C36/C72; Robot B/W 8/12/24/36; Wraase SC2; Pasokon; PD50/90/120/160/180/
  240/290; normal AVT24/90/94; and the required MMSSTV wide and narrow
  families.
- Added standard VIS, narrow VIS, extended VIS and FSK-ID handling, fractional
  timing, resampling, preprocessing, tone/frequency demodulation, AFC and
  slant correction, timing fallback, replay and progressive immutable image
  snapshots.
- Added native RX sessions and TX encoders for the supported Martin, Scottie,
  Robot, Wraase/Pasokon, PD, AVT and MMSSTV families, plus WAV export/replay
  and deterministic loopback coverage.
- Added Robot B/W 8 tail and sparse-row recovery for bounded virtual-audio
  callback gaps. It only activates on a high-coverage frame and never turns a
  low-coverage or broadly damaged frame into a false complete decode.

### Workspace, storage and sharing

- Added the Native SSTV workspace with Receive, Transmit Studio, Gallery,
  Remote Sharing, Digital HAMDRM, Settings and Diagnostics pages.
- Added responsive layouts, readable menus and diagnostics, BETA labelling,
  contrast fixes, compact controls and small-display behaviour for narrow or
  4:3 monitors.
- Added atomic PNG/sidecar storage, SQLite indexing, thumbnails, paging,
  retention previews, safe deletion, QSO logging and bounded background
  workers.
- Added provider-neutral TLS sharing with validated manifests, durable queues,
  inbox/history, expiry and explicit recipient acceptance. End-to-end
  encryption is represented as a fail-closed policy boundary but is not
  implemented in this release; TLS providers may read content.

### Digital HAMDRM

- Added the documented native HAMDRM subset: named profile registry, MOT/BSR
  objects, CRC/segmentation/reassembly, partial-object persistence and
  bounded JPEG2000/OpenJPEG validation.
- Added separate RX/TX waveform, channel-coding, PHY and controller layers
  with existing-audio integration and local structural tests.
- Full QSSTV/EasyPal interoperability, broadcast DRM coverage and live RF
  interoperability remain outside the claimed scope.

### Reliability, tests and packaging

- Added bounded diagnostics, audio-source handoff fencing, explicit shutdown
  ownership and safer source/range validation.
- Added SSTV fuzz smoke coverage, regression fixes for MSK144, FST4 and Base32
  TOTP edge cases, and native SSTV CI/build packaging coverage.
- The local macOS Apple Silicon build succeeds and the selected SSTV/Robot,
  TX-audio, coordinator and QML tests pass. The release workflows build the
  Windows x64 installer, Linux x86_64 and aarch64 AppImages, and supported
  macOS Apple Silicon and Intel DMGs with SHA-256 files.
- GitHub's generated source downloads are complemented by the attached source
  archive for this release.

### Explicit boundaries

- FAX480, HFFAX, WEFAX and AVT Narrow/QRM variants remain catalogue or blocked
  entries and are not advertised as implemented SSTV modes.
- No QSSTV, EasyPal, Robot36/SlowRX, provider or on-air RF interoperability
  result is claimed by this release.

## Italiano

### Modifiche portate dalla v1.0.583 alla v1.0.586

- La v1.0.584 ha riscritto i filtri delle stazioni lavorate sulla base del
  nominativo e ha aggiunto gli ambiti `today`, `yesterday` ed `ever`, senza
  cambiare il contratto della cronologia QSO.
- La v1.0.585 ha portato in DecoPort i meter della radio: S-meter, potenza
  diretta, ROS, ALC, temperatura del finale, tensione e corrente di drain,
  compressione e impostazione di potenza, con campi espliciti di capacità e
  stato.
- La v1.0.586 ha corretto il percorso dei meter a riposo, ha mantenuto fresche
  le letture dell'alimentazione anche fuori trasmissione e ha centralizzato la
  scalatura tra ricezione e trasmissione.

### SSTV nativo dentro Decodium4

- È stato aggiunto un sottosistema SSTV nativo C++ a limiti controllati,
  integrato con l'ingresso audio esistente, SoundOutput, l'autorità CAT/PTT/
  VOX, lo storage e la QML di Decodium. Non servono Python, Java o un secondo
  processo radio.
- Il registro canonico comprende 52 righe analogiche implementate: Martin
  M1--M4; Scottie S1--S4 e DX; Robot colore C12/C24/C36/C72; Robot B/W
  8/12/24/36; Wraase SC2; Pasokon; PD50/90/120/160/180/240/290; AVT normale
  24/90/94; e le famiglie MMSSTV wide e narrow richieste.
- Sono stati aggiunti VIS standard, narrow VIS, VIS esteso e FSK-ID, timing
  frazionario, ricampionamento, pre-processing, demodulazione di tono e
  frequenza, AFC e correzione dello slant, fallback temporale, replay e
  snapshot progressivi immutabili delle immagini.
- Sono disponibili sessioni RX ed encoder TX nativi per le famiglie Martin,
  Scottie, Robot, Wraase/Pasokon, PD, AVT e MMSSTV, oltre a esportazione/replay
  WAV e loopback deterministico.
- Per i buchi limitati della coda audio virtuale è stato aggiunto un recupero
  bounded della coda e di poche righe sparse Robot B/W 8. Si attiva solo con
  copertura alta e non trasforma un frame rumoroso o largamente danneggiato in
  un falso frame completo.

### Workspace, storage e condivisione

- È stato aggiunto il workspace Native SSTV con Receive, Transmit Studio,
  Gallery, Remote Sharing, Digital HAMDRM, Settings e Diagnostics.
- Sono stati corretti layout responsive, menu leggibili, diagnostica,
  etichetta BETA, contrasto, controlli compatti e comportamento su monitor
  piccoli o 4:3.
- Sono stati aggiunti storage PNG/sidecar atomico, indice SQLite, thumbnail,
  paginazione, anteprima della retention, cancellazione sicura, logging QSO e
  worker in background a limiti controllati.
- È stata aggiunta la condivisione TLS indipendente dal provider, con manifest
  validati, code persistenti, inbox/storico, scadenza e accettazione esplicita
  del destinatario. La cifratura end-to-end è rappresentata come confine di
  policy fail-closed ma non è implementata in questa release; il provider TLS
  può leggere il contenuto.

### Digital HAMDRM

- È stato aggiunto il sottoinsieme HAMDRM nativo documentato: registro dei
  profili, oggetti MOT/BSR, CRC/segmentazione/ricomposizione, persistenza degli
  oggetti parziali e validazione JPEG2000/OpenJPEG bounded.
- Sono presenti livelli separati RX/TX per waveform, channel coding, PHY e
  controller, con integrazione all'audio esistente e test strutturali locali.
- La piena interoperabilità QSSTV/EasyPal, la copertura DRM broadcast e
  l'interoperabilità RF live restano fuori dall'ambito dichiarato.

### Affidabilità, test e packaging

- Sono stati aggiunti diagnostica bounded, protezione dei passaggi di sorgente
  audio, ownership esplicita dello shutdown e validazione più sicura degli
  input e dei range.
- Sono stati aggiunti fuzz smoke test SSTV, correzioni di regressioni nei casi
  limite MSK144, FST4 e Base32 TOTP, oltre a copertura CI/build per SSTV nativo.
- La build locale macOS Apple Silicon riesce e passano i test selezionati
  SSTV/Robot, audio TX, coordinatore e QML. I workflow di release producono
  installer Windows x64, AppImage Linux x86_64 e aarch64, e DMG macOS Apple
  Silicon e Intel supportati con file SHA-256.
- Ai download sorgente generati da GitHub si aggiunge l'archivio del codice
  allegato a questa release.

### Confini dichiarati

- FAX480, HFFAX, WEFAX e le varianti AVT Narrow/QRM restano voci di catalogo o
  bloccate e non sono pubblicizzate come modalità SSTV implementate.
- Questa release non dichiara risultati di interoperabilità QSSTV, EasyPal,
  Robot36/SlowRX, provider o RF on-air.
