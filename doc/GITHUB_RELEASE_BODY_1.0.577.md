# Decodium 4 FT2 v1.0.577 — pre-release

Transmission over DecoPort. Marked **pre-release** because it has not been tried
on the air: it has been reasoned through and built, not proved.

## English (British)

### Transmitting through a radio that is somewhere else

Until now DecoPort could hear a remote radio and tune it, but the gateway
declared `CAN-TX = 0` and transmission stayed local. Both ends are now wired.

On the computer running the application, the modem's waveform no longer goes to
the local sound card. It is sent to the remote radio, and everything else about
a transmission — the sequencer, the logging, the end of the slot — is unchanged.

On the computer with the radio, that audio goes into the USB codec and the PTT
goes up.

### Sent early, played on time

Twelve seconds of FT8 is over three hundred datagrams, and throwing them onto
the network at once is how you lose some. They go out a second and a half ahead
of the playing time, in 40 ms frames that fit inside one network packet, each
carrying the exact instant it must reach the modulator. The gateway holds each
frame until that instant.

This is the point of the timestamps. A modem transmitting FT8 must start within
a few tens of milliseconds of the slot boundary; playing whatever arrives
whenever it arrives would add the network's jitter straight into that alignment.
Sending early with a stated playing time puts the jitter in a buffer instead.

The waveform is 48 kHz and holds nothing above 3 kHz, so one sample in four
carries it at 12 kHz without aliasing — the rate the gateway already declares
for the stream. At the far end it goes back to 48 kHz by linear interpolation,
whose images land above 6 kHz where the radio's own SSB filter removes them.

### The one thing that must not go wrong

A transmitter keyed over a network can be left keyed by a network. If the client
dies, if the link drops, if the audio stops half way, the radio would sit there
with an empty carrier until somebody noticed.

So the PTT raised from a distance carries a deadline. Playing audio renews it;
three seconds after the audio stops, the PTT drops on its own, with a line in
the log. Closing the application drops it too. The gateway refuses the PTT
outright if its own CAT is not connected, or if that radio is already
transmitting locally — and in those cases it declares `CAN-TX = 0`, so a client
is told before it tries rather than after.

### Trying it without going on the air

`tools/decoport_probe.py --serve` now declares itself able to transmit and
**measures** what it is sent instead of playing it: how many frames, how many
seconds of audio, the peak level, how many frames arrived late, and how long the
PTT was held. Nothing reaches a radio.

Point the application at that invented radio and transmit. If the summary shows
roughly 12.6 s of audio for FT8, a peak well under 32767 and no late frames, the
chain is right. Only then try it on a real one, with a hand near the radio.

### Validation

Both targets build and link. The transmit path has **not** been exercised end to
end, on the air or otherwise; that is why this is a pre-release. What has been
verified is that the invented radio still announces, connects and streams with
the new context flags.

## Italiano

### Trasmettere con una radio che sta altrove

Fino a ieri DecoPort sapeva ascoltare una radio remota e sintonizzarla, ma il
gateway dichiarava `CAN-TX = 0` e la trasmissione restava locale. Adesso sono
collegati tutti e due i capi.

Sul computer dove gira l'applicazione la forma d'onda del modem non va piu' alla
scheda audio locale: va alla radio remota, e tutto il resto della trasmissione —
il sequencer, la messa a log, la fine dello slot — resta com'era.

Sul computer con la radio quell'audio entra nel codec USB e il PTT sale.

### Spedito in anticipo, suonato in orario

Dodici secondi di FT8 sono piu' di trecento datagrammi, e rovesciarli sulla rete
tutti insieme e' il modo di perderne. Partono un secondo e mezzo prima dell'ora
di riproduzione, a frame di 40 ms che stanno dentro un pacchetto di rete, ognuno
con l'istante esatto in cui deve raggiungere il modulatore. Il gateway trattiene
ogni frame fino a quell'istante.

E' a questo che servono i timestamp. Un modem che trasmette in FT8 deve partire
entro poche decine di millisecondi dal confine di slot; suonare quello che arriva
quando arriva sommerebbe il jitter della rete a quell'allineamento. Spedire in
anticipo con un'ora di riproduzione lo mette invece dentro un buffer.

La forma d'onda sta a 48 kHz e non ha niente sopra i 3 kHz, quindi uno su quattro
la porta a 12 kHz senza alias — la frequenza che il gateway gia' dichiara per
quel flusso. All'altro capo torna a 48 kHz per interpolazione lineare, le cui
immagini finiscono sopra i 6 kHz, dove il filtro SSB della radio le toglie.

### L'unica cosa che non si puo' sbagliare

Un trasmettitore alzato da una rete puo' essere lasciato alzato da una rete. Se
il client muore, se cade il collegamento, se l'audio finisce a meta', la radio
resterebbe li' a portante vuota finche' qualcuno non se ne accorge.

Percio' il PTT alzato da lontano porta con se' una scadenza. L'audio riprodotto
la rinnova; tre secondi dopo che l'audio smette, il PTT scende da solo, con una
riga nel log. Anche chiudere l'applicazione lo abbassa. Il gateway rifiuta il PTT
in partenza se il proprio CAT non e' connesso o se quella radio sta gia'
trasmettendo per conto suo — e in quei casi dichiara `CAN-TX = 0`, cosi' il
client lo sa prima di provarci invece che dopo.

### Provarla senza andare in aria

`tools/decoport_probe.py --serve` adesso si dichiara capace di trasmettere e
**misura** quello che gli mandi invece di suonarlo: quanti frame, quanti secondi
di audio, il livello di picco, quanti frame sono arrivati in ritardo e per quanto
e' rimasto su il PTT. Alla radio non arriva niente.

Punta l'applicazione su quella radio inventata e trasmetti. Se il riepilogo dice
circa 12,6 s di audio per l'FT8, un picco ben sotto 32767 e nessun frame in
ritardo, la catena e' giusta. Solo allora provala su una vera, con una mano
vicino alla radio.

### Verifiche

Entrambi i target compilano e linkano. Il percorso di trasmissione **non** e'
stato esercitato da un capo all'altro, ne' in aria ne' altrimenti: e' per questo
che questa e' una pre-release. Quello che e' stato verificato e' che la radio
inventata continua ad annunciarsi, a collegarsi e a trasmettere il flusso con i
nuovi flag di contesto.
