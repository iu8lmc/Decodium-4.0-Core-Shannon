# Decodium 4 FT2 v1.0.579 — pre-release

A change of strategy, asked for after three releases that kept not working: DecoPort
stops borrowing the application's CAT and becomes a radio interface in its own right.

## English (British)

### The remote radio is now a CAT backend

Until now the frequency of a remote radio was copied into a box and its
transmission was a special case bolted onto the side. Everything else in the
application — the band selector, split, the sequencer's PTT, every feature that
first asks "is the CAT connected?" — saw no radio at all, because on the
computer using a remote one there is no CAT to connect.

DecoPort is now one of the backends, alongside native, Hamlib, OmniRig, cat4om
and TCI. Taking a remote radio into use switches the application onto it:
`catConnected` becomes the state of the link, the rig name and mode are the
remote radio's, and the seven points from which the application commands a radio
— PTT, transmit PTT, frequency, mode — send their commands over the network
instead of down a serial port that is not there.

Split is deliberately refused: DecoPort carries one frequency, and pretending
there are two would move the receiving one.

The backend the operator actually configured is put aside and restored when the
remote radio is released. It is never written to the settings file — saving a
CAT profile while using a network radio would otherwise have rewritten their
choice as "hamlib", silently, which is exactly the kind of fault that takes a
week to find.

### The gateway can now open the radio itself

The other half of not depending on a CAT: the gateway no longer has to ask the
application what the radio is doing. It reads the USB identity, resolves the
model against **the catalogue Hamlib publishes at runtime** — no hand-written
table of models to maintain, and still no radio for the operator to pick from a
menu — and opens the serial port itself, on its own thread so that a wedged read
cannot freeze the interface.

Verified on the radio here, without touching the bus:

    DecoPort rig driver: [Yaesu FT-991 / FT-991A / FT-DX10]
                         resolved to Hamlib model 1035 (Yaesu FT-991)

It stands down when the port is already the application's own CAT port, and says
so. Two programs on one serial line are not two programs on one serial line;
they are neither of them.

### Validation

Both targets build and link. Model resolution was exercised against the real
FT-991A, as was the refusal to open a port the application already holds. The
CAT backend itself has not been exercised against a remote radio — this is a
pre-release for that reason.

## Italiano

### La radio remota e' un backend CAT

Fino a ieri la frequenza di una radio remota era un numero copiato in una
casella, e la sua trasmissione un caso speciale attaccato di lato. Tutto il
resto dell'applicazione — il selettore di banda, lo split, il PTT del sequencer,
ogni funzione che prima di tutto chiede "il CAT e' connesso?" — non vedeva
nessuna radio, perche' sul computer che ne usa una remota un CAT da connettere
non c'e'.

DecoPort adesso e' uno dei backend, accanto a nativo, Hamlib, OmniRig, cat4om e
TCI. Prendere in uso una radio remota ci commuta sopra l'applicazione:
`catConnected` diventa lo stato del collegamento, nome e modo sono quelli della
radio remota, e i sette punti da cui l'applicazione comanda una radio — PTT, PTT
di trasmissione, frequenza, modo — mandano i comandi in rete invece che su una
seriale che non c'e'.

Lo split viene rifiutato apposta: DecoPort porta una frequenza sola, e fingere
di averne due vorrebbe dire spostare quella di ricezione.

Il backend che l'operatore ha davvero configurato viene messo da parte e
rimesso a posto quando la radio remota si rilascia. Non finisce mai nel file
delle impostazioni: salvare un profilo CAT mentre si usa una radio in rete
avrebbe altrimenti riscritto la sua scelta in "hamlib", di nascosto — che e'
esattamente il tipo di guasto che poi si cerca per una settimana.

### Il gateway ora sa aprire la radio da solo

L'altra meta' del non dipendere da un CAT: il gateway non deve piu' chiedere
all'applicazione cosa sta facendo la radio. Legge l'identita' USB, risolve il
modello contro **il catalogo che Hamlib pubblica a runtime** — nessuna tabella
di modelli da mantenere a mano, e sempre nessuna radio da scegliere in un menu —
e apre la seriale per conto suo, su un thread suo perche' una lettura che si
impunta non possa congelare l'interfaccia.

Verificato sulla radio che c'e' qui, senza toccare il bus:

    DecoPort rig driver: [Yaesu FT-991 / FT-991A / FT-DX10]
                         resolved to Hamlib model 1035 (Yaesu FT-991)

Si fa da parte quando la porta e' gia' quella del CAT dell'applicazione, e lo
dice. Due programmi sulla stessa seriale non sono due programmi sulla stessa
seriale: non sono nessuno dei due.

### Verifiche

Entrambi i target compilano e linkano. La risoluzione del modello e' stata
provata sull'FT-991A vero, come il rifiuto ad aprire una porta che
l'applicazione tiene gia'. Il backend CAT in se' non e' stato esercitato contro
una radio remota: e' per questo che resta una pre-release.
