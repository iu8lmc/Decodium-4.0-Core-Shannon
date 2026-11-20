# Decodium 4 FT2 v1.0.547

Release 1.0.547 lets the DECØMETER read a linear amplifier directly, so the
instrument can show the power leaving the amplifier rather than the power
leaving the exciter.

## English (British)

### The DECØMETER can read your amplifier

Until now the RF meter showed what the transceiver reports — typically a few
tens of watts, while the amplifier is putting out several hundred. Decodium can
now read the amplifier itself and show its output.

- **Settings → CAT → Amplifier**: choose the serial port, the speed, and
  whether Decodium should merely listen or actively ask.
- Two buttons appear on the instrument face, **EXC** and **AMP**. They are
  shown only when an amplifier is configured — there is no point offering a
  choice that does not exist.
- **The source is always stated.** Showing 400 W without saying where the
  figure comes from would be worse than showing nothing. If the amplifier goes
  quiet while selected, the instrument falls back to the exciter and says so,
  rather than leaving a blank dial.
- Besides power and SWR, the amplifier reports supply voltage and current,
  heatsink temperatures, and its own warnings and alarms — SWR over limit,
  input overdriving, overheating.

The protocol is SPE's own, taken from the manufacturer's Application
Programmer's Guide and documented in `doc/protocollo-spe-expert.md`. It covers
the Expert 1.3K-FA, 1.5K-FA and 2K-FA families.

### Sharing the port with the amplifier's own software

A serial port can only be opened by one program at a time, so if SPE's Term
software must stay open, Decodium cannot take the port from it. There are two
ways round this, and both work:

- **The RS-232 socket on the back.** SPE ships separate applications for the
  USB and the serial port, so the two are distinct paths.
- **A mirrored virtual port** (com0com and similar). Decodium then runs in
  **listen-only** mode: the amplifier's own software keeps polling as usual and
  Decodium simply reads the replies that already flow on the line.

### What Decodium will never do to your amplifier

- In listen-only mode the port is opened **read-only**. The operating system
  prevents writing, so not even a defect in this code could put a byte on the
  line while the manufacturer's software is talking to the amplifier.
- When Decodium does ask, it sends exactly six constant bytes — the documented
  `Get Status` command. No function anywhere in that code can change band, set
  power, or key the amplifier.
- The port assigned to the radio's CAT is refused outright, so a mistyped port
  cannot send those bytes to the transceiver.
- Malformed frames are rejected on their checksum rather than displayed. On a
  noisy line a truncated frame would otherwise produce an absurd reading at
  exactly the moment full power is being measured.

Ten automated tests cover the frame parser, including checksum failures,
truncated frames, noise, and a frame split across two reads of the serial port.

### Also

- All new interface text is available in all fifteen languages, with zero
  unfinished messages.
- A standalone bridge (`tools/spe-tci-bridge/`) remains available for setups
  that prefer to feed the amplifier's telemetry over TCI, along with a probe
  (`tools/amp-probe/`) that reports whether Hamlib's amplifier backend answers
  on a given unit.

---

## Italiano

### Il DECØMETER può leggere l'amplificatore

Finora il misuratore RF mostrava quello che riporta il ricetrasmettitore —
qualche decina di watt, mentre l'amplificatore ne eroga diverse centinaia.
Decodium può ora leggere l'amplificatore stesso e mostrarne l'uscita.

- **Impostazioni → CAT → Amplificatore**: porta seriale, velocità, e se
  Decodium debba limitarsi ad ascoltare o interrogare.
- Sul frontalino compaiono due pulsanti, **EXC** e **AMP**, solo se un
  amplificatore è configurato: non si offre una scelta che non esiste.
- **La sorgente viene sempre dichiarata.** Mostrare 400 W senza dire da dove
  vengono sarebbe peggio che non mostrarli. Se l'amplificatore tace mentre è
  selezionato, lo strumento ricade sull'eccitatrice e lo scrive, invece di
  lasciare il quadrante vuoto.
- Oltre a potenza e ROS, l'amplificatore riporta tensione e corrente di
  alimentazione, le temperature dei dissipatori, e i propri avvisi e allarmi —
  ROS oltre i limiti, sovrapilotaggio in ingresso, surriscaldamento.

Il protocollo è quello di SPE, preso dalla Application Programmer's Guide del
costruttore e documentato in `doc/protocollo-spe-expert.md`. Copre le famiglie
Expert 1.3K-FA, 1.5K-FA e 2K-FA.

### Condividere la porta con il programma dell'amplificatore

Una porta seriale la apre un solo programma alla volta: se il Term di SPE deve
restare aperto, Decodium non può togliergliela. Ci sono due vie, ed entrambe
funzionano:

- **La presa RS-232 sul retro.** SPE fornisce programmi distinti per la USB e
  per la seriale, quindi sono due percorsi separati.
- **Una porta virtuale rispecchiata** (com0com e simili). Decodium lavora
  allora in **solo ascolto**: il programma del costruttore continua a
  interrogare come sempre, e Decodium si limita a leggere le risposte che già
  passano sulla linea.

### Cosa Decodium non farà mai al vostro amplificatore

- In solo ascolto la porta si apre in **sola lettura**. È il sistema operativo
  a impedire la scrittura: nemmeno un difetto di questo codice potrebbe far
  uscire un byte mentre il programma del costruttore sta dialogando.
- Quando Decodium interroga, invia esattamente sei byte costanti: il comando
  `Get Status` documentato. In quel codice non esiste alcuna funzione capace di
  cambiare banda, impostare la potenza o mandare in trasmissione.
- La porta assegnata al CAT della radio viene rifiutata, così un errore di
  battitura non può inviare quei byte al ricetrasmettitore.
- Le trame malformate vengono scartate sulla somma di controllo invece di
  essere mostrate. Su una linea disturbata una trama mutila darebbe altrimenti
  una lettura assurda proprio nell'istante in cui si misura la piena potenza.

Dieci prove automatiche coprono l'analizzatore delle trame, compresi somma di
controllo errata, trame mutile, rumore, e una trama spezzata fra due letture
della seriale.

### Inoltre

- Tutti i testi nuovi dell'interfaccia sono disponibili in quindici lingue, con
  zero messaggi non finiti.
- Restano disponibili un ponte autonomo (`tools/spe-tci-bridge/`) per chi
  preferisce far arrivare la telemetria via TCI, e una sonda
  (`tools/amp-probe/`) che dice se il backend amplificatori di Hamlib risponde
  su un determinato apparato.
