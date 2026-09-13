# Decodium 4 FT2 v1.0.586

This fork release follows v1.0.585 and fixes the meters that release
introduced: several of them were never actually read from the radio.

## English (British)

### The idle meters are now read

- v1.0.585 added eight meters to the DecoPort context, but the code that reads
  them sat after the receive branch, which returns early. At rest none of them
  were reached.
- The power setting was worse off still: it was asked for with a "not
  transmitting" guard, so the transmit path skipped it and the receive path
  never got to it. It was read in no state at all, and the protocol documented
  a field that was never populated.
- The reading helper now sits before the branch and is used by both sides. In
  receive, the power setting, PA temperature, drain voltage and drain current
  are read on a slowed tick, one pass in four: a knob changes when the operator
  turns it, not twelve times a second.

### Supply readings are fresh, not left over from the last transmission

- Drain voltage and drain current are no longer tied to transmitting. Without
  this the gateway kept sending whatever was last measured during a
  transmission, and on a dial an old number is indistinguishable from one just
  taken.
- Forward power, SWR, ALC and speech compression stay transmit-only: at rest
  they do not exist, and a zero would read as "no power, perfect SWR", which
  looks like a station in excellent shape.
- The scaling for each reading is written once and shared by both paths, so
  the receive and transmit sides cannot disagree about what a volt is.

### Packaging and source availability

- The tagged source code is available through GitHub's generated source-code
  downloads for v1.0.586.
- Release workflows publish the Windows x64 installer, Linux x86_64 and
  aarch64 AppImages with SHA-256 checksums, and macOS DMGs with SHA-256
  checksums for the supported Apple Silicon and Intel runner targets.
- macOS application ZIP files remain intentionally excluded; only DMG packages
  and their checksums are published.

## Italiano

### Gli strumenti a riposo vengono finalmente letti

- La v1.0.585 aveva aggiunto otto misure al contesto DecoPort, ma il codice che
  le legge stava dopo il ramo di ricezione, che esce prima. A riposo non ne
  veniva raggiunta nessuna.
- L'impostazione di potenza stava anche peggio: veniva chiesta con una
  condizione «non in trasmissione», quindi il percorso di trasmissione la
  saltava e quello di ricezione non ci arrivava. Non veniva letta in nessuno
  stato, e il protocollo documentava un campo che non si popolava mai.
- La funzione di lettura sta ora prima del bivio ed è usata da entrambi i rami.
  In ricezione, impostazione di potenza, temperatura del finale, tensione e
  corrente di alimentazione si leggono su un giro rallentato, uno su quattro:
  una manopola cambia quando la gira l'operatore, non dodici volte al secondo.

### Letture dell'alimentazione fresche, non avanzi dell'ultima trasmissione

- Tensione e corrente di finale non sono più legate alla trasmissione. Senza
  questa correzione il gateway continuava a spedire l'ultimo valore misurato
  durante una trasmissione, e su un quadrante un numero vecchio non si
  distingue da uno appena letto.
- Potenza diretta, ROS, ALC e compressione restano invece legate alla
  trasmissione: a riposo non esistono, e uno zero direbbe «nessuna potenza, ROS
  perfetto», che somiglia a una stazione che va benissimo.
- La scala di ogni lettura è scritta una volta sola e condivisa dai due
  percorsi, così ricezione e trasmissione non possono essere in disaccordo su
  quanto vale un volt.

### Packaging e disponibilità del sorgente

- Il codice sorgente taggato è disponibile tramite i download del codice
  generati da GitHub per la v1.0.586.
- I workflow di release pubblicano l'installer Windows x64, le AppImage Linux
  x86_64 e aarch64 con checksum SHA-256, e i DMG macOS con checksum SHA-256
  per i runner Apple Silicon e Intel supportati.
- Gli ZIP dell'applicazione macOS restano esclusi intenzionalmente: vengono
  pubblicati soltanto i pacchetti DMG e i rispettivi checksum.
