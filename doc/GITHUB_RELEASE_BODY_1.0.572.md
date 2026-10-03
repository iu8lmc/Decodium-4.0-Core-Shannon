# Decodium 4 FT2 v1.0.572

This release brings in DecoPort: a way to put an ordinary transceiver on the
network, so another computer can hear it and tune it — whatever radio it is, and
without being told which one. It also absorbs v1.0.571 from the upstream project.

## English (British)

### A radio on the network

A transceiver with a USB CAT port and a USB audio codec has no network
interface. DecoPort gives it one. The gateway publishes the radio; a client
hears the announcement, connects, and from then on tunes it, changes mode and
receives its audio.

The protocol is new. It is inspired by VITA-49 — a framed stream with
timestamps, a context packet describing the source and a command packet
controlling it — but it is smaller, and its vocabulary is neutral rather than
tied to any manufacturer. The header is 28 bytes; a context is a field mask
followed by the fields present, and a command has exactly the same structure,
so the same encoder serves both reading and writing.

The mode names say what they **do**. `DIGU` means "put the USB codec into the
modulator, upper sideband": on a Yaesu that is DATA-USB, on an Icom USB-D,
elsewhere PKT. A client asks for `DIGU` and how it is spelled on the radio at
the other end is the gateway's business.

### Nothing to choose

Unlike a rig-control library, DecoPort never asks which radio you have. The
gateway reads what the operating system already knows about the serial ports and
the sound cards, pairs the ones belonging to the same USB device, and publishes
what it found. It is a passive reading: no port is opened and no command is sent,
so it is safe to run with the CAT connected and the transmitter keyed.

For control it goes through hooks into the application, so it works with the
native backend, with Hamlib, with OmniRig or with TCI without knowing which. And
it does not open or configure the CAT itself: opening the same serial port twice
is the reliable way of not opening it at all.

### The audio, and why the timestamps matter

Received audio goes out in frames of 10 ms, at the sample rate **declared in the
context** rather than assumed — Decodium hands over 12 kHz, not 48, which for a
modem is the whole passband it needs at a quarter of the network bandwidth.
Measured over twenty seconds on a real network: 2002 packets, 20.02 s of audio,
a yield of 1.001.

The timestamp on transmit audio is not when the packet was sent. It is **when
the first sample must reach the modulator**, and the gateway holds the audio
until that instant. A modem transmitting FT8 must start within a few tens of
milliseconds of the slot boundary; playing whatever arrives whenever it arrives
would add network jitter straight into that alignment. Sending early with a
stated playing time moves the jitter into a buffer, where it is harmless.

### A password, because the radio is on the network

Publishing a radio in the clear means letting anyone on the network listen to it
and tune it. So every packet — announcements, context, commands, audio — carries
an HMAC-SHA256 signature truncated to 128 bits, over a key shared by the two
ends.

The password is asked **once**, during installation, and it is not what gets
stored: Decodium turns it into a key with PBKDF2-SHA256 over 200 000 iterations
and discards the password. It is never asked again.

It fails closed. Without a key the gateway refuses to start and the client
refuses to connect; there is no clear-text mode to leave switched on by mistake.
A signature alone would not stop a recorded command from being replayed later,
so packets outside a thirty-second window are dropped. Five bad signatures in a
minute and that sender is ignored for five, with a line in the log. An
unauthenticated packet gets **no reply at all** — replying would tell whoever is
trying that they had found the right door. The signature comparison runs in
constant time, because an ordinary comparison tells the caller how many leading
bytes were right.

Leaving the password empty at installation is a legitimate choice, and means
"do not publish this radio".

### Where you see it

The status bar shows the address the radio is published on — which is what you
need to type on the other computer — or the address of the remote radio in use.
The DecoPort window itself is in the ☰ menu: it shows what was detected, the
radios found on the network, and the one you are connected to.

### Absorbed from upstream v1.0.571

FT8 special callsigns and `/LH` lighthouse activations, back-pressure handling
for the Windows decode list, batched Live Map notifications, and a pair of
compiler diagnostics fixed in the bundled QCustomPlot.

### Cross-platform build integration

The DecoPort implementation is now part of both application targets. This
keeps the classic macOS bundle and the QML frontend linked with the same
network-radio implementation, so the Windows executable and all macOS/Linux
packages contain the feature consistently.

### Validation

Discovery, control and received audio were exercised against a real FT-991A: the
radio was detected on its own, announced on every network interface, tuned by
command from a separate client, and its audio carried in real time. The three
security cases were checked — no password sees nothing, the right password works,
the wrong one gets silence and a five-minute block.

Transmit audio and remote keying are **not** carried yet: the gateway declares
`CAN-TX = 0`. The parallel-CQ transmission added in v1.0.569 remains untested on
the air.

## Italiano

### Una radio in rete

Un ricetrasmettitore con una porta CAT USB e un codec audio USB non ha
un'interfaccia di rete. DecoPort gliela da'. Il gateway pubblica la radio; un
client sente l'annuncio, si collega, e da li' in poi la sintonizza, ne cambia il
modo e ne riceve l'audio.

Il protocollo e' nuovo. E' ispirato a VITA-49 — un flusso incapsulato con
timestamp, un pacchetto di contesto che descrive la sorgente e uno di comando
che la controlla — ma e' piu' piccolo, e il suo vocabolario e' neutro invece che
legato a un costruttore. L'header e' di 28 byte; un contesto e' una maschera di
campi seguita dai campi presenti, e un comando ha esattamente la stessa
struttura, cosi' lo stesso codificatore serve per leggere e per scrivere.

I nomi dei modi dicono cosa **fanno**. `DIGU` significa "metti il codec USB
dentro il modulatore, banda laterale superiore": su uno Yaesu e' DATA-USB, su un
Icom USB-D, altrove PKT. Il client chiede `DIGU` e come si scriva sulla radio
all'altro capo e' affare del gateway.

### Niente da scegliere

A differenza di una libreria di controllo radio, DecoPort non chiede mai che
radio hai. Il gateway legge quello che il sistema operativo gia' sa sulle porte
seriali e sulle schede audio, accoppia quelle che appartengono allo stesso
apparato USB, e pubblica cio' che ha trovato. E' una lettura passiva: non apre
porte e non manda comandi, quindi e' sicura anche con il CAT connesso e la radio
in trasmissione.

Per il controllo passa da ganci verso l'applicazione, quindi funziona con il
backend nativo, con Hamlib, con OmniRig o con TCI senza saperlo. E il CAT non lo
apre e non lo configura da solo: aprire due volte la stessa porta seriale e' il
modo sicuro per non aprirla.

### L'audio, e perche' i timestamp contano

L'audio ricevuto esce a frame di 10 ms, alla frequenza **dichiarata nel
contesto** invece che presunta — Decodium consegna 12 kHz, non 48, che per un
modem e' tutto il passabanda che serve a un quarto della banda di rete. Misurato
su venti secondi in rete vera: 2002 pacchetti, 20,02 s di audio, resa 1,001.

Il timestamp dell'audio di trasmissione non e' quando il pacchetto e' partito.
E' **quando il primo campione deve raggiungere il modulatore**, e il gateway
trattiene l'audio fino a quell'istante. Un modem che trasmette in FT8 deve
partire entro poche decine di millisecondi dal confine di slot; suonare quello
che arriva quando arriva sommerebbe il jitter di rete a quell'allineamento.
Spedire in anticipo con un'ora di riproduzione sposta il jitter dentro un
buffer, dove non fa danno.

### Una password, perche' la radio e' in rete

Pubblicare una radio in chiaro vuol dire lasciare che chiunque sulla rete la
ascolti e la sintonizzi. Percio' ogni pacchetto — annunci, contesto, comandi,
audio — porta una firma HMAC-SHA256 troncata a 128 bit su una chiave condivisa
dai due capi.

La password si chiede **una volta**, durante l'installazione, e non e' lei a
essere salvata: Decodium la trasforma in una chiave con PBKDF2-SHA256 su 200 000
iterazioni e la butta. Non viene piu' richiesta.

Fallisce chiuso. Senza chiave il gateway si rifiuta di partire e il client di
collegarsi; non esiste una modalita' in chiaro da dimenticare accesa. La firma
da sola non impedirebbe di rigiocare un comando registrato, quindi i pacchetti
fuori da una finestra di trenta secondi vengono scartati. Cinque firme sbagliate
in un minuto e quel mittente resta ignorato per cinque, con una riga nel log. A
un pacchetto non autenticato **non si risponde niente**: rispondere direbbe a
chi prova che ha trovato la porta giusta. Il confronto della firma avviene a
tempo costante, perche' un confronto normale racconta quanti byte iniziali erano
giusti.

Lasciare la password vuota durante l'installazione e' una scelta legittima, e
vuol dire "non pubblicare questa radio".

### Dove lo vedi

La barra di stato mostra l'indirizzo su cui la radio e' pubblicata — che e'
quello da scrivere sull'altro computer — oppure l'indirizzo della radio remota
in uso. La finestra DecoPort sta nel menu ☰: mostra cosa e' stato rilevato, le
radio trovate in rete e quella a cui sei collegato.

### Assorbito dalla v1.0.571 a monte

Nominativi speciali FT8 e attivazioni faro `/LH`, gestione della contropressione
nella lista dei decode su Windows, notifiche della Live Map raggruppate, e due
diagnostiche del compilatore sistemate nel QCustomPlot incluso.

### Integrazione della build multipiattaforma

L'implementazione DecoPort ora fa parte di entrambi i target applicativi. In
questo modo sia il bundle macOS classico sia il frontend QML vengono collegati
con la stessa implementazione della radio in rete, e l'eseguibile Windows e
tutti i pacchetti macOS/Linux includono la funzione in modo coerente.

### Verifiche

Scoperta, controllo e audio ricevuto sono stati provati su un FT-991A vero: la
radio si e' fatta riconoscere da sola, si e' annunciata su ogni interfaccia di
rete, e' stata sintonizzata via comando da un client separato e il suo audio e'
arrivato in tempo reale. I tre casi di sicurezza sono stati verificati — senza
password non si vede nulla, con quella giusta funziona, con quella sbagliata si
ottiene silenzio e cinque minuti di blocco.

L'audio di trasmissione e il PTT remoto **non** passano ancora: il gateway
dichiara `CAN-TX = 0`. I CQ paralleli introdotti con la 1.0.569 restano non
provati in aria.
