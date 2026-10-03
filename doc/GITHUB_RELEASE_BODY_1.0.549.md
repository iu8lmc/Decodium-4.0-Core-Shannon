# Decodium 4 FT2 v1.0.549

Three things reported by users, all three real, and one of them was a defect
hiding behind a message of ours that said nothing.

## English (British)

### Numeric fields that refused the keyboard

Reported as "I click on it and it will not type". The field was not disabled
and it was not unreachable: its own validator was throwing every keystroke
away.

A port field shows `8000`. Clicking puts the caret between the digits, so the
digit you type makes a five-figure number, past the 65535 limit, and the
validator rejects it. Type again, same thing. Nothing enters, and the field
looks dead. Worse: press Enter after that and the value clamps to **65535** —
a port you never wrote.

Reproduced on the bench with the control exactly as it is in the source:
typing `7300` left the field reading `80700` and committed `65535`. Numeric
fields now select their contents when they take focus, so typing replaces,
which is what anyone means to do; and they can be selected with the mouse,
which they could not before. **43 fields**, not only the DX Cluster one — every
UDP port, the remote HTTP port, the satellite rotator, the macro counters.

### The Settings window ran off small screens

It was fixed at 1500×900. On a 1366×768 laptop it does not fit: it goes off
the screen, and the right-hand column of the grid goes with it — which is
exactly where the DX Cluster port field lives. The two reports were the same
fault seen from two sides.

The window now starts from the size it wants but never takes more than 88% of
the display: 1202×641 on a 1366×768 laptop, unchanged from full HD upwards.
The measurement comes from the screen the window is on, not from the virtual
desktop, so on two monitors it does not spread across both.

### Why the amplifier port would not open

Reported by PA3GYQ, whose Expert 1.5K-FA answered `Port not open: error`
whatever he tried. That message was ours, and it was useless twice over.

With no port typed in, the status fell through to the error branch because the
error string was empty — but nothing had failed; the setup was simply
unfinished. It now says so.

And the case he actually had: the port was already held by SPE's own software.
On Windows a serial port belongs to one program at a time, **reading
included** — opening read-only does not share a port, it only guarantees that
Decodium never writes to it. Qt reports `PermissionError`; Decodium now reads
the code and not just the text, and the message names the port, says which
program is likely holding it, and what to do: close it, or mirror the port and
point Decodium at the copy.

### Two instances of Decodium on one radio

Also asked for: the serial port belongs to one program at a time, and that
holds between two copies of Decodium too. The second one does not take the
port — it connects to the first, which shares it.

The engine could already do this: shared CAT (1.0.545) speaks rigctld, and
Hamlib has a network rig that speaks it, `Hamlib NET rigctl`. Verified rather
than assumed: with the station on the air, `rigctl -m 2 -r 127.0.0.1:4533`
reads 7,100,000 Hz, PKTUSB, VFO A from the radio held by the other instance.

It was simply an invisible road — you had to know to pick that rig out of
three hundred and type the address by hand. **Settings → CAT → Use a shared
CAT** now takes an address and a button; it sets the Hamlib backend and the
network rig itself. Start the second copy with `--rig-name`, which gives it
its own lock file and its own settings profile.

Documented in `doc/cat-condivisa-protocollo.md`, including the two things you
would otherwise find out the hard way: the second instance cannot re-share on
the same port (the first one has it, and now says so), and with writing
enabled on both there is no arbitration — the last one to write wins. For a
second listening position, leave writing off.

### Also

- All new interface text is available in all fifteen languages, with zero
  unfinished messages.
- The ten amplifier frame-parser tests still pass.

---

## Italiano

### Campi numerici che rifiutavano la tastiera

Segnalato così: «ci clicco ma non digita». Il campo non era disabilitato né
irraggiungibile: lo bloccava il suo stesso validatore.

Un campo porta mostra `8000`. Il clic mette il cursore fra le cifre, quindi la
cifra digitata forma un numero a cinque posizioni, oltre il limite di 65535, e
il validatore la scarta. Digiti ancora, stessa cosa. Non entra niente, e il
campo sembra morto. Peggio: premendo Invio il valore si aggancia al massimo —
**65535**, una porta che non hai mai scritto.

Riprodotto al banco con il controllo esattamente com'è nel sorgente: digitando
`7300` il campo restava a `80700` e salvava `65535`. Ora i campi numerici si
selezionano tutti quando ricevono il fuoco, così digitare sostituisce, che è
quello che chiunque intende fare; e si possono selezionare col mouse, cosa che
prima non si poteva. **43 campi**, non solo quello del DX Cluster: tutte le
porte UDP, la porta HTTP remota, il rotore del satellite, i contatori delle
macro.

### La finestra Impostazioni usciva dagli schermi piccoli

Era fissa a 1500×900. Su un portatile 1366×768 non ci sta: esce dallo schermo,
e con lei se ne va la colonna di destra della griglia — cioè proprio dove vive
il campo della porta del DX Cluster. Le due segnalazioni erano lo stesso
guasto visto da due lati.

Ora la finestra parte dalla misura che vuole ma non prende mai più dell'88%
dello schermo: 1202×641 su un 1366×768, invariata da full HD in su. La misura
si prende dallo schermo su cui sta la finestra, non dalla scrivania virtuale,
così su due monitor non si allarga a cavallo dei due.

### Perché la porta dell'amplificatore non si apriva

Segnalato da PA3GYQ, il cui Expert 1.5K-FA rispondeva «Porta non aperta:
errore» qualunque cosa provasse. Quel messaggio era nostro, ed era inutile due
volte.

Senza porta indicata, lo stato cadeva sul ramo dell'errore perché la stringa
d'errore era vuota — ma non era fallito niente: era una configurazione da
finire. Ora lo dice.

E il caso che aveva davvero: la porta ce l'aveva già il software di SPE. Su
Windows una seriale appartiene a un solo programma alla volta, **lettura
compresa** — aprire in sola lettura non condivide la porta, garantisce
soltanto che Decodium non ci scriva mai. Qt riporta `PermissionError`; ora
Decodium legge il codice e non solo il testo, e il messaggio nomina la porta,
dice chi probabilmente la tiene e cosa fare: chiudere quel programma, oppure
rispecchiare la porta e puntare Decodium sulla copia.

### Due istanze di Decodium sulla stessa radio

Anche questo chiesto da un utente: la seriale la apre un solo programma alla
volta, e vale anche fra due Decodium. La seconda non prende la porta: si
collega alla prima, che gliela condivide.

Il motore lo sapeva già fare: la CAT condivisa (1.0.545) parla rigctld, e
Hamlib ha un rig di rete che quel protocollo lo parla, `Hamlib NET rigctl`.
Verificato, non dedotto: con la stazione in aria,
`rigctl -m 2 -r 127.0.0.1:4533` legge 7.100.000 Hz, PKTUSB, VFO A dalla radio
tenuta dall'altra istanza.

Era solo una strada invisibile: bisognava sapere di scegliere quel rig fra
trecento e digitare l'indirizzo a mano. **Impostazioni → CAT → Usa una CAT
condivisa** ora ha un indirizzo e un bottone; backend Hamlib e rig di rete li
imposta lui. La seconda copia si avvia con `--rig-name`, che le dà un file di
blocco e un profilo di impostazioni suoi.

Documentato in `doc/cat-condivisa-protocollo.md`, comprese le due cose che
altrimenti si scoprono male: la seconda istanza non può ricondividere sulla
stessa porta (ce l'ha la prima, e ora lo scrive), e con la scrittura abilitata
su entrambe non c'è arbitraggio — l'ultima che scrive vince. Per un secondo
posto di ascolto conviene lasciare la scrittura spenta.

### Inoltre

- Tutti i testi nuovi dell'interfaccia sono disponibili in quindici lingue,
  con zero messaggi non finiti.
- Le dieci prove dell'analizzatore di trame dell'amplificatore restano verdi.
