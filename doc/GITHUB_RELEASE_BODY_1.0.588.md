# Decodium 4 FT2 v1.0.588

This fork release follows v1.0.587 and fixes native SSTV transmission, which
worked once per session and then refused to start again.

## English (British)

### SSTV transmits more than once per session

- Only the first SSTV transmission after starting Decodium went out. Every
  later attempt - another calibration tone, or an image - was refused with
  "SSTV TX preflight is not ready", and the radio was never keyed. Restarting
  the program bought exactly one more transmission.
- `start()` and `startPrepared()` publish the coordinator state on their way to
  their own preflight, and that publish still carries the terminal state of the
  previous, finished session. The bridge read that as "the transmission is
  over" and released the audio route that the caller had pinned a few lines
  earlier, so the preflight immediately after found no pinned output device and
  rejected the start. The first transmission escaped it only because there was
  no previous session to report.
- A guard now spans pinning the route and the return of the start call: while
  it is raised, a state notification belonging to the old session no longer
  dismantles the route being armed for the new one. Image, calibration and
  HAMDRM transmissions all take the same path and are all fixed. A rejected
  start still releases its route, so nothing is left held.

### The two refusals now say what is wrong

- The SSTV TX preflight weighs twenty-five conditions and used to report one
  fixed sentence for all of them, pointing at "audio/CAT or VOX" even when the
  cause was an armed Enable TX or an audio device not yet released. It now
  names the conditions that block, so the message reads
  `SSTV TX preflight is not ready: Enable TX is armed for another mode`.
- "Decodium TX audio output is unavailable" now distinguishes a device that was
  never pinned, one that has gone away, and one reporting an unusable channel
  count. These two changes are what made the bug above reproducible.

### UDP port 2237 conflicts are visible in the log

- With Decodium running, GridTracker, JTAlert or Log4OM could stop receiving
  decodes on 2237 with no error anywhere: the client binds with SO_REUSEADDR,
  so a second program binds the same port successfully but the datagrams reach
  only one of them.
- The bind is now logged with the port requested, the port obtained, whether it
  is a fixed shared port or an ephemeral one, and the interface. When the
  listen port equals the server port - the setting that takes the datagrams
  away from other programs - the log says so and points at 0, which means
  "choose automatically" and is the shipped default.

### Packaging and source availability

- The tagged source code is available through GitHub's generated source-code
  downloads for v1.0.588.
- Release workflows publish the Windows x64 installer, Linux x86_64 and
  aarch64 AppImages with SHA-256 checksums, and macOS DMGs with SHA-256
  checksums for the supported Apple Silicon and Intel runner targets.
- macOS application ZIP files remain intentionally excluded; only DMG packages
  and their checksums are published.

## Italiano

### L'SSTV trasmette più di una volta per sessione

- Andava in onda soltanto la prima trasmissione SSTV dopo l'avvio di Decodium.
  Ogni tentativo successivo - un altro tono di calibrazione, oppure
  un'immagine - veniva rifiutato con «il controllo preliminare TX SSTV non è
  pronto», e la radio non veniva mai messa in trasmissione. Riavviare il
  programma concedeva esattamente un'altra trasmissione.
- `start()` e `startPrepared()` pubblicano lo stato del coordinatore mentre
  procedono verso il proprio controllo preliminare, e quella pubblicazione porta
  ancora lo stato terminale della sessione precedente, già conclusa. Il bridge
  lo interpretava come «la trasmissione è finita» e rilasciava la rotta audio
  che il chiamante aveva fissato poche righe prima: il controllo successivo non
  trovava più alcun dispositivo fissato e rifiutava l'avvio. La prima
  trasmissione si salvava solo perché non c'era una sessione precedente da
  segnalare.
- Un presidio copre ora l'intervallo fra il fissaggio della rotta e il ritorno
  della chiamata di avvio: finché è alzato, una notifica di stato che appartiene
  alla vecchia sessione non smonta più la rotta preparata per quella nuova.
  Immagine, calibrazione e HAMDRM seguono lo stesso percorso e sono tutte
  corrette. Un avvio rifiutato continua a rilasciare la propria rotta, quindi
  nulla resta occupato.

### I due rifiuti dicono finalmente cosa non va

- Il controllo preliminare TX SSTV valuta venticinque condizioni e ne riportava
  una frase sola per tutte, che rimandava ad «audio/CAT o VOX» anche quando la
  causa era un Enable TX ancora armato o un dispositivo audio non ancora
  rilasciato. Ora nomina le condizioni che bloccano, così il messaggio diventa
  `SSTV TX preflight is not ready: Enable TX is armed for another mode`.
- «Decodium TX audio output is unavailable» distingue ora un dispositivo mai
  fissato, uno sparito e uno che riporta un numero di canali inutilizzabile.
  Sono queste due modifiche ad aver reso riproducibile il difetto qui sopra.

### I conflitti sulla porta UDP 2237 si vedono nel log

- Con Decodium in funzione, GridTracker, JTAlert o Log4OM potevano smettere di
  ricevere i decode sulla 2237 senza alcun errore da nessuna parte: il client
  si lega con SO_REUSEADDR, quindi un secondo programma si lega alla stessa
  porta senza problemi, ma i datagrammi arrivano a uno solo dei due.
- Il bind viene ora registrato con la porta richiesta, quella ottenuta, se si
  tratta di una porta fissa condivisa o automatica, e l'interfaccia. Quando la
  porta di ascolto coincide con quella del server - l'impostazione che sottrae
  i datagrammi agli altri programmi - il log lo dice e indica lo 0, che
  significa «scegline una automaticamente» ed è il valore predefinito.

### Packaging e disponibilità del sorgente

- Il codice sorgente taggato è disponibile tramite i download del codice
  generati da GitHub per la v1.0.588.
- I workflow di release pubblicano l'installer Windows x64, le AppImage Linux
  x86_64 e aarch64 con checksum SHA-256, e i DMG macOS con checksum SHA-256
  per i runner Apple Silicon e Intel supportati.
- Gli ZIP dell'applicazione macOS restano esclusi intenzionalmente: vengono
  pubblicati soltanto i pacchetti DMG e i rispettivi checksum.
