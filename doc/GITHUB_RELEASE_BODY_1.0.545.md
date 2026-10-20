# Decodium 4 FT2 v1.0.545

Release 1.0.545 adds shared CAT — other programs can now use the radio while
Decodium holds the serial port — and makes the DECØMETER explain itself when it
has nothing to show.

## English (British)

### Shared CAT

A serial port can only be opened by one program at a time, so while Decodium
holds the radio, everything else is locked out. Decodium now re-serves that
connection over TCP using Hamlib's **rigctld** protocol, which every program in
the community already speaks under the name *Hamlib NET rigctl*: Decodium SDR,
WSJT-X, JTDX and logging software connect to it without any of them needing a
change.

- **Settings → CAT → Shared CAT**: switch it on, choose the port, and decide
  what connected programs may do.
- **Read is always allowed**; changing frequency, mode or split requires
  *Allow control*; keying the transmitter requires a **separate** switch, which
  stays disabled until control is granted. Adjusting someone else's frequency
  is an annoyance; putting their radio on the air is not.
- The status line reports what the server is doing and, when it fails to start,
  **why**. The usual cause is another program already listening on that port,
  and that is worth saying rather than leaving the operator to guess.
- Reads are answered from the state Decodium already holds, so sharing adds
  **no extra traffic on the serial line**. Polling the radio for every question
  from every client would congest the bus — on CI-V that has already cost a
  stuck PTT.
- Listening is on `127.0.0.1` only. The default port is **4533** rather than
  the canonical 4532, because 4532 is also where Decodium looks for an
  *external* rigctld when acting as a client, and it is frequently already
  taken. It remains configurable.

With the switch off no port is opened and behaviour is exactly as before.

The wire format was not inferred from documentation: it was established by
connecting the real Hamlib client to a prototype until every operation returned
success. One detail decides whether anything connects at all — `\chk_vfo` must
be answered with `0`, not `CHKVFO 0`. It is recorded in
`doc/cat-condivisa-protocollo.md`.

### DECØMETER says why it is silent

The RF meter used to show an empty display and the words "no CAT telemetry"
even when CAT was perfectly connected — which sends the operator hunting in the
wrong place. It now distinguishes three cases, in amber: no CAT link,
telemetry switched off (naming the setting to enable), and a radio that reports
no meter at all. The display stays readable at rest when there is a warning to
read.

This came from a report that FLrig did not read power. FLrig was not at fault:
the backend declares the meters. Power and SWR polling is simply off by
default and has to be enabled with **PWR and SWR** in the CAT settings.

### Also

- All new interface text is available in all fifteen languages, with zero
  unfinished messages.

---

## Italiano

### CAT condivisa

Una porta seriale la può aprire un solo programma alla volta: finché Decodium
tiene la radio, tutti gli altri restano fuori. Ora Decodium rivende quel
collegamento su TCP con il protocollo **rigctld** di Hamlib, che ogni programma
della comunità parla già sotto il nome *Hamlib NET rigctl*: Decodium SDR,
WSJT-X, JTDX e i log si collegano senza che nessuno debba cambiare nulla.

- **Impostazioni → CAT → CAT condivisa**: si attiva, si sceglie la porta e si
  decide cosa possono fare i programmi collegati.
- **La lettura è sempre concessa**; cambiare frequenza, modo o split richiede
  *Permetti controllo*; mandare in trasmissione richiede un interruttore
  **separato**, che resta spento finché non si concede il controllo. Cambiare
  frequenza a una radio altrui è un fastidio, mandarla in aria è un'altra cosa.
- La riga di stato dice cosa sta facendo il server e, quando non parte, **il
  motivo**. Il caso tipico è un altro programma già in ascolto su quella porta:
  vale la pena dirlo, invece di lasciare l'operatore a indovinare.
- Le letture rispondono dallo stato che Decodium tiene già in memoria: la
  condivisione **non aggiunge un byte sulla seriale**. Interrogare la radio a
  ogni domanda di ogni programma saturerebbe il bus — su CI-V è già costato un
  PTT incollato.
- L'ascolto è solo su `127.0.0.1`. La porta predefinita è **4533** e non la
  canonica 4532, perché la 4532 è anche quella dove Decodium cerca un rigctld
  *esterno* quando fa da client, ed è spesso già occupata. Resta configurabile.

A interruttore spento non si apre alcuna porta e il comportamento è quello di
sempre.

Il formato del dialogo non è stato dedotto dalla documentazione: è stato
stabilito collegando il vero client Hamlib a un prototipo finché ogni
operazione non è tornata a buon fine. Un dettaglio decide se qualcuno si
collega o no — a `\chk_vfo` si risponde `0`, non `CHKVFO 0`. È annotato in
`doc/cat-condivisa-protocollo.md`.

### Il DECØMETER dice perché tace

Il misuratore RF mostrava un display vuoto e la scritta «nessuna telemetria
CAT» anche con il CAT perfettamente collegato, mandando a cercare il guasto nel
posto sbagliato. Ora distingue tre casi, in ambra: CAT assente, telemetria
disattivata (indicando l'impostazione da accendere) e radio che non fornisce
alcun metro. A riposo il display resta leggibile quando c'è un avviso da
leggere.

Nasce da una segnalazione su FLrig che non leggeva la potenza. FLrig non
c'entrava: il backend i metri li dichiara. Il polling di potenza e ROS è
semplicemente spento di serie e va acceso con **PWR and SWR** nelle
impostazioni CAT.

### Inoltre

- Tutti i testi nuovi dell'interfaccia sono disponibili in tutte e quindici le
  lingue, con zero messaggi non finiti.
