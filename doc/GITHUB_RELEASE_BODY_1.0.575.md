# Decodium 4 FT2 v1.0.575

v1.0.574 fed the remote radio's audio into the decode buffer. It turns out that
filling the buffer is not the same as decoding it.

## English (British)

### Filling a buffer is not decoding

`USE THIS RADIO` injected the remote audio into the same buffer the local sound
card fills, and stopped there. But the decoder is not driven by the buffer — it
is driven by the period timer, and the first thing that timer does on every tick
is give up if monitoring is off:

    if (!m_monitoring) return;

On the computer using a remote radio there is often no local radio and no
working audio input, so monitoring is off and nothing ever looked at those
samples. They arrived, they were counted, and they sat there.

Taking a remote radio into use now starts reception properly — decoders, period
timer, spectrum — and only then releases the local sound card, which `startRx()`
opens at the end and which is exactly what we do not want.

### The watchdogs had to be told

Decodium restarts the local capture on its own when audio stops arriving: an
audio watchdog, and the re-arm that follows a band change. Both are right when
the sound card has gone quiet, and both were wrong here, because the audio has
not stopped — it is coming over the network. Reopening the card would have put
two sources into one buffer.

`startAudioCapture()` now refuses while a remote radio is in use, at the single
point every one of those paths goes through, rather than at each of them.

### Validation

Both targets build and link. The fault was found by reading the decode path
rather than by reproducing it: the injection point, the period timer's first
guard, and every caller of `startAudioCapture()`. It has not been reproduced on
two machines, because the receiving end could not be driven from here.

## Italiano

### Riempire un buffer non è decodificare

`USE THIS RADIO` infilava l'audio remoto nello stesso buffer che riempie la
scheda audio locale, e finiva lì. Ma il decoder non lo guida il buffer: lo guida
il timer di periodo, e la prima cosa che quel timer fa a ogni tick è rinunciare
se il monitor è spento:

    if (!m_monitoring) return;

Sul computer che usa una radio remota spesso non c'è nessuna radio locale e
nessun ingresso audio che funzioni, quindi il monitor è spento e quei campioni
non li ha mai guardati nessuno. Arrivavano, venivano contati, e restavano lì.

Prendere in uso una radio remota adesso avvia la ricezione per intero — decoder,
timer di periodo, spettro — e solo dopo lascia andare la scheda locale, che
`startRx()` apre in fondo ed è esattamente quello che non vogliamo.

### Ai watchdog bisognava dirlo

Decodium riapre da solo la cattura locale quando l'audio smette di arrivare: c'è
un watchdog dell'audio, e c'è il riaggancio dopo un cambio banda. Hanno ragione
tutti e due quando la scheda è ammutolita, e avevano torto tutti e due qui,
perché l'audio non è finito — arriva dalla rete. Riaprire la scheda avrebbe
messo due sorgenti nello stesso buffer.

`startAudioCapture()` adesso si rifiuta finché una radio remota è in uso, nel
punto unico da cui passano tutte quelle strade invece che in ognuna.

### Verifiche

Entrambi i target compilano e linkano. Il difetto è stato trovato leggendo il
percorso di decodifica, non riproducendolo: il punto di iniezione, la prima
guardia del timer di periodo, e tutti i chiamanti di `startAudioCapture()`. Non è
stato riprodotto su due macchine, perché da qui il lato ricevente non si è
riusciti a pilotare.
