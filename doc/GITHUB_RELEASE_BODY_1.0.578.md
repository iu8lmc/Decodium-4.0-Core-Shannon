# Decodium 4 FT2 v1.0.578 — pre-release

v1.0.577 wired transmission through DecoPort, and it still did not transmit. The
reason was earlier in the path than the new code: the attempt never got that far.

## English (British)

### It failed before it tried

Before a transmission starts, Decodium prepares the audio. That preparation does
two things: it generates the modem's waveform, and it builds the PCM for the
local sound card. The second part resolves an output device, chooses a format
and lays out the samples for it — and if it cannot, the whole transmission is
abandoned:

    Audio TX: impossibile costruire PCM per il device selezionato

On a computer whose radio is somewhere else, there may be no suitable output
device at all. So the preparation failed, `startTx` returned, and the DecoPort
branch — which sits after the preparation and needs only the waveform — was
never reached.

Local PCM is now skipped when a remote radio is in use, exactly as it already
was for TCI, which has the same property of taking the audio somewhere that is
not a sound card. Building it was not only fatal on that machine; it was work
whose only other effect would have been playing the transmission out of the
client's speakers.

### Saying which state it was in

Every transmit attempt now records whether the remote radio was in use and
whether the link was up, alongside the message. Three rounds of this were spent
not knowing which of those was true, and the log is the place that should have
said so.

### What was verified this time

The gateway on a real FT-991A was interrogated from a separate client and
declares what it should: `CAT AUDIO-IN AUDIO-OUT CAN-TX`, with frequency and
mode. So the receiving half of transmit support — the part that accepts a PTT
and plays the audio — announces itself correctly.

The transmitting half still has not been exercised, which is why this remains a
pre-release. `tools/decoport_probe.py --serve` measures a transmission without
putting anything on the air, and remains the right first test.

### A trap worth knowing about

Listening on the network here showed three radios, and one of them was a gateway
announcing `no radio found` with no capabilities at all — a second computer
publishing a Decodium that has no radio attached, because the gateway starts by
itself. It is easy to connect to that one by mistake and conclude the feature is
broken. Check the entry you connect to names an actual radio.

## Italiano

### Falliva prima di provarci

Prima di trasmettere, Decodium prepara l'audio. Quella preparazione fa due cose:
genera la forma d'onda del modem, e costruisce il PCM per la scheda audio
locale. La seconda parte risolve un dispositivo di uscita, ne sceglie il formato
e ci dispone i campioni — e se non ci riesce, la trasmissione viene abbandonata
in blocco:

    Audio TX: impossibile costruire PCM per il device selezionato

Su un computer la cui radio sta altrove, un dispositivo di uscita adatto puo'
benissimo non esserci. Quindi la preparazione falliva, `startTx` usciva, e il
ramo DecoPort — che sta dopo la preparazione e ha bisogno solo della forma
d'onda — non veniva mai raggiunto.

Il PCM locale adesso si salta quando e' in uso una radio remota, esattamente
come gia' accadeva per TCI, che ha la stessa proprieta' di portare l'audio da
qualche parte che non e' una scheda audio. Costruirlo non era solo fatale su
quella macchina: era lavoro il cui unico altro effetto sarebbe stato far uscire
la trasmissione dagli altoparlanti del client.

### Dire in che stato era

Ogni tentativo di trasmissione registra adesso se la radio remota era in uso e
se il collegamento era su, accanto al messaggio. Ci sono voluti tre giri per non
saperlo, e il log e' il posto che avrebbe dovuto dirlo.

### Cosa e' stato verificato stavolta

Il gateway su un FT-991A vero e' stato interrogato da un client separato e
dichiara quello che deve: `CAT AUDIO-IN AUDIO-OUT CAN-TX`, con frequenza e modo.
Quindi la meta' ricevente del supporto alla trasmissione — quella che accetta un
PTT e suona l'audio — si annuncia correttamente.

La meta' trasmittente non e' ancora stata esercitata, ed e' per questo che resta
una pre-release. `tools/decoport_probe.py --serve` misura una trasmissione senza
mettere niente in aria, e resta la prima prova giusta da fare.

### Una trappola da conoscere

Mettendosi in ascolto sulla rete qui sono comparse tre radio, e una era un
gateway che annunciava `no radio found` senza nessuna capacita' — un secondo
computer che pubblica un Decodium a cui non e' attaccata nessuna radio, perche'
il gateway parte da solo. E' facile collegarsi a quello per sbaglio e concludere
che la funzione non va. Controlla che la voce a cui ti colleghi nomini una radio
vera.
