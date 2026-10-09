# Decodium 4 FT2 v1.0.585

This fork release follows v1.0.584 and carries the transmitter meters over
the DecoPort protocol.

## English (British)

### The rig meters travel over DecoPort

- The DecoPort context now carries eight meters alongside the existing radio
  state: forward power, SWR, ALC, drain voltage, drain current, PA
  temperature, speech compression and the power setting. Each one is a fixed
  point integer, so no floating point ever crosses the wire and two machines
  read the same number.
- A meter is only sent when it has actually been read. A rig that does not
  report its drain current simply leaves the field out, and the absence is the
  answer: there is no reserved value meaning "unknown", so a client cannot
  mistake one for a reading.
- Transmit meters are only valid while transmitting. At rest a zero would read
  as "no power, perfect SWR", which looks like a station in excellent shape;
  the gateway therefore withholds them instead. The S-meter is the opposite
  and is only sent while receiving.
- Drain voltage, drain current and PA temperature are deliberately not gated
  on transmit: the temperature falls *after* the transmission, which is when
  an operator looks at it, and the supply readings are meaningful at rest.
- `powerSetting` is the odd one out by nature - it is where the operator put
  the knob, not what reached the antenna - and it is included because on most
  rigs it is the only one of the eight that can be read with the transmitter
  at rest.

### Meters read from Hamlib

- The transceiver layer reads the new levels from Hamlib and keeps a validity
  flag for each. A rig without a sensor no longer reports a zero, which on a
  supply voltage would mean "power supply off" and on an SWR meter would mean
  a perfect match.
- The readings are exposed to the interface as properties of the transceiver
  manager, so the same values feed both the local display and the DecoPort
  clients. Two ports answering differently about one radio would be a defect,
  not a choice.
- `doc/DECOPORT_PROTOCOL.md` documents the eight new context fields and their
  scaling.

### Packaging and source availability

- The tagged source code is available through GitHub's generated source-code
  downloads for v1.0.585.
- Release workflows publish the Windows x64 installer, Linux x86_64 and
  aarch64 AppImages with SHA-256 checksums, and macOS DMGs with SHA-256
  checksums for the supported Apple Silicon and Intel runner targets.
- macOS application ZIP files remain intentionally excluded; only DMG packages
  and their checksums are published.

## Italiano

### Gli strumenti della radio viaggiano su DecoPort

- Il contesto DecoPort trasporta ora otto misure accanto allo stato della
  radio: potenza diretta, ROS, ALC, tensione e corrente di finale, temperatura
  del PA, compressione e impostazione di potenza. Ognuna è un intero in scala
  fissa, così sul filo non passa mai virgola mobile e due macchine leggono lo
  stesso numero.
- Una misura viene inviata solo quando è stata davvero letta. La radio che non
  riporta la corrente di finale lascia semplicemente fuori il campo, e
  l'assenza è la risposta: non esiste un valore riservato che significhi «non
  lo so», quindi nessun client può scambiarlo per una lettura.
- I misuratori di trasmissione valgono solo mentre si trasmette. A riposo uno
  zero direbbe «nessuna potenza, ROS perfetto», che somiglia a una stazione
  che va benissimo; il gateway preferisce non mandarli. L'S-meter è l'opposto
  e parte solo in ricezione.
- Tensione, corrente di finale e temperatura del PA non passano invece dal
  filtro «solo in trasmissione»: la temperatura scende *dopo* la trasmissione,
  ed è proprio dopo che la si guarda, mentre le letture dell'alimentazione
  hanno senso anche a riposo.
- `powerSetting` è per natura diverso dagli altri — è dove l'operatore ha messo
  la manopola, non ciò che è arrivato all'antenna — ed è incluso perché sulla
  maggior parte delle radio è l'unico degli otto leggibile con il trasmettitore
  a riposo.

### Misure lette da Hamlib

- Il livello transceiver legge i nuovi valori da Hamlib e mantiene per ciascuno
  una bandiera di validità. Una radio senza sensore non riporta più uno zero,
  che su una tensione di alimentazione vorrebbe dire «alimentatore spento» e su
  un misuratore di ROS «adattamento perfetto».
- Le letture sono esposte all'interfaccia come proprietà del gestore
  transceiver, così gli stessi valori alimentano sia la visualizzazione locale
  sia i client DecoPort. Due porte che rispondono diverso sulla stessa radio
  sarebbero un difetto, non una scelta.
- `doc/DECOPORT_PROTOCOL.md` documenta gli otto nuovi campi di contesto e le
  rispettive scale.

### Packaging e disponibilità del sorgente

- Il codice sorgente taggato è disponibile tramite i download del codice
  generati da GitHub per la v1.0.585.
- I workflow di release pubblicano l'installer Windows x64, le AppImage Linux
  x86_64 e aarch64 con checksum SHA-256, e i DMG macOS con checksum SHA-256
  per i runner Apple Silicon e Intel supportati.
- Gli ZIP dell'applicazione macOS restano esclusi intenzionalmente: vengono
  pubblicati soltanto i pacchetti DMG e i rispettivi checksum.
