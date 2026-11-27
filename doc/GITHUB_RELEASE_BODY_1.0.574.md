# Decodium 4 FT2 v1.0.574

v1.0.572 put a radio on the network and let another computer see it, tune it and
receive its audio. What it did not do was let that computer **use** it: the audio
arrived and went nowhere. This release connects the receiving end.

## English (British)

### Using the radio, not just watching it

Connecting to a remote radio showed its frequency, its mode and its S-meter, and
the packets went past. Nothing consumed them. The DecoPort window now has a
**USE THIS RADIO** button, and pressing it makes the remote radio this
application's radio:

- its received audio goes into the decoder, so what the remote radio hears is
  what this computer decodes;
- the local sound card is released, because two sources filling the same buffer
  would mix into noise rather than into two signals;
- its frequency becomes the one shown in the application, **in both
  directions** — turn the dial here and the remote radio moves, move the remote
  radio and the display follows.

The mode is deliberately not mirrored. The radio's mode is DIGU or USB; the
application's is FT8 or FT4. They are different things that only sound alike.
What does happen on connecting is that the remote radio is asked for `DIGU`,
because otherwise it would be listening to its own microphone and there would be
nothing to decode.

If the link drops, the local sound card comes back on its own. Sitting there
decoding silence is the worst possible way to find out that the remote radio has
gone.

### Hearing it

Decoding a radio and listening to it are separate wishes, so they are separate
buttons. With a radio in use, **LISTEN** sends its audio to the speaker as well.
Decodium works at 12 kHz and many sound cards will not open a stream at that
rate; when that happens the monitor moves to 48 kHz and repeats each sample four
times. It is a compromise for a monitor, and it does not touch a single sample of
what goes into the decoder.

### A radio that will not echo itself

If this computer both publishes a radio and uses a remote one, it now stops
publishing while the remote radio is in use. What is on the network is meant to
be the radio attached to *this* computer; re-broadcasting somebody else's would
be wrong on its own, and if two machines connected to each other it would go
round for ever.

### A radio that does not exist

`tools/decoport_probe.py --serve` makes one up. It announces itself like a real
gateway, accepts a connection, answers tuning commands and sends a tone in real
time — measured at a yield of 1.000 over ten seconds. It is there so the
receiving end can be tested without a second radio and without a second
computer, which is exactly the thing that was hard to test about this feature.

### Still not carried

Transmit audio and remote keying. The gateway continues to declare `CAN-TX = 0`,
and transmission stays on the local radio. The window says so rather than
letting you find out mid-QSO.

### Still in English

The DecoPort window has never been in the translation catalogues, in any
language, since it was written; the new controls are in English like everything
else in it. Translating half of it would have made it worse, not better.

### Validation

Both application targets build and link. The DecoPort window was instantiated on
its own to prove it loads clean. The invented radio was driven end to end with
the probe's own client: announcement, connection, a tuning command accepted and
reflected back, and 1000 audio packets in 10.00 s. The path from a remote radio
into this application's decoder has not yet been exercised across two machines.

## Italiano

### Usare la radio, non solo guardarla

Collegarsi a una radio remota ne mostrava la frequenza, il modo e l'S-meter, e i
pacchetti passavano. Nessuno però li consumava. La finestra DecoPort ha adesso un
pulsante **USE THIS RADIO**, e premendolo la radio remota diventa la radio di
questa applicazione:

- il suo audio ricevuto entra nel decoder, così quello che sente la radio remota
  è quello che questo computer decodifica;
- la scheda audio locale viene rilasciata, perché due sorgenti che riempiono lo
  stesso buffer si mescolerebbero in rumore invece che in due segnali;
- la sua frequenza diventa quella mostrata dall'applicazione, **nei due versi** —
  giri la manopola qui e la radio remota si sposta, sposti la radio remota e il
  display la segue.

Il modo non viene specchiato, ed è voluto. Il modo della radio è DIGU o USB,
quello dell'applicazione è FT8 o FT4: sono due cose diverse che si somigliano
solo nel nome. Quello che succede al collegamento è che alla radio remota viene
chiesto `DIGU`, perché altrimenti starebbe ascoltando il proprio microfono e non
ci sarebbe niente da decodificare.

Se il collegamento cade, la scheda audio locale torna da sola. Restare lì a
decodificare il silenzio sarebbe il modo peggiore di accorgersi che la radio
remota non c'è più.

### Sentirla

Decodificare una radio e ascoltarla sono due desideri distinti, quindi sono due
pulsanti distinti. Con una radio in uso, **LISTEN** ne manda l'audio anche in
altoparlante. Decodium lavora a 12 kHz e molte schede audio non aprono un flusso
a quella frequenza; quando succede, l'ascolto passa a 48 kHz ripetendo ogni
campione quattro volte. È un compromesso da ascolto di controllo, e non tocca
nemmeno un campione di quello che entra nel decoder.

### Una radio che non fa eco a se stessa

Se questo computer pubblica una radio e ne usa una remota, adesso smette di
pubblicare finché la remota è in uso. Quello che sta in rete deve essere la radio
attaccata a *questo* computer; ritrasmettere quella di un altro sarebbe sbagliato
di per sé, e se due macchine si collegassero a vicenda girerebbe all'infinito.

### Una radio che non esiste

`tools/decoport_probe.py --serve` se la inventa. Si annuncia come un gateway
vero, accetta un collegamento, risponde ai comandi di sintonia e manda un tono in
tempo reale — misurato con resa 1,000 su dieci secondi. Serve a provare il lato
ricevente senza una seconda radio e senza un secondo computer, che è esattamente
la cosa difficile da provare di questa funzione.

### Ancora non passa

L'audio di trasmissione e il PTT remoto. Il gateway continua a dichiarare
`CAN-TX = 0`, e la trasmissione resta sulla radio locale. La finestra lo dice,
invece di lasciartelo scoprire a metà QSO.

### Ancora in inglese

La finestra DecoPort non è mai entrata nei cataloghi di traduzione, in nessuna
lingua, da quando esiste: i comandi nuovi sono in inglese come tutti gli altri
che ci sono dentro. Tradurne metà l'avrebbe resa peggiore, non migliore.

### Verifiche

Entrambi i target dell'applicazione compilano e linkano. La finestra DecoPort è
stata istanziata da sola per verificare che carichi pulita. La radio inventata è
stata guidata da un capo all'altro con il client del probe: annuncio,
collegamento, un comando di sintonia accettato e rimandato indietro, e 1000
pacchetti audio in 10,00 s. Il percorso da una radio remota fino al decoder di
questa applicazione non è ancora stato esercitato fra due macchine.
