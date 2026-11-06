# Decodium 4 FT2 v1.0.580

Connecting to a remote radio put it into **USB** instead of leaving it in the
data mode. On the air that means transmitting the room: the microphone, not the
modem.

## English (British)

### The wrong word sent down the wire

DecoPort's vocabulary is functional. `DIGU` does not mean "upper sideband"; it
means *put the USB codec into the modulator, upper sideband* — DATA-USB on a
Yaesu, USB-D on an Icom, PKT elsewhere. How it is spelled at the far end is the
gateway's business.

The application also has a setting for the mode to put a radio in, and it holds
a local radio's spelling: `USB`, or `DATA-U`. v1.0.579 started sending that
setting straight down the wire, which was wrong twice over. `DATA-U` is not a
word DecoPort knows, so it did nothing; and `USB` *is* a word it knows, so the
remote radio dutifully went to plain USB — where the modulator listens to the
microphone and the modem's audio goes nowhere.

Over a network link the audio always arrives at the far radio's USB codec, so
the mode to ask for is always the data one. The configured setting is now
translated rather than forwarded: anything lower-sideband asks for `DIGL`,
everything else `DIGU`, and the log says both what was asked and what was
configured. When the radio is already in that mode, nothing is sent at all —
before, the command went out again on every sync.

### Not changed

The mode hook on the gateway side was checked and left alone. A `DIGU` arriving
at a Decodium acting as gateway reaches `setMode()`, which recognises it as a
radio mode rather than an application mode, records it and keeps FT8 running.
That was already right.

### Validation

Builds and links. The fault was found by reading the path from the setting to
the wire; the translation has not been exercised against a radio from here.

## Italiano

### La parola sbagliata mandata sul filo

Il vocabolario di DecoPort è funzionale. `DIGU` non vuol dire "banda laterale
superiore": vuol dire *metti il codec USB dentro il modulatore, banda laterale
superiore* — DATA-USB su uno Yaesu, USB-D su un Icom, PKT altrove. Come si
scriva all'altro capo è affare del gateway.

L'applicazione ha anche un'impostazione per il modo in cui mettere la radio, e
quella contiene la grafia di una radio locale: `USB`, oppure `DATA-U`. La
1.0.579 ha cominciato a mandare quell'impostazione tale e quale sul filo, ed era
sbagliato due volte. `DATA-U` non è una parola che DecoPort conosce, quindi non
faceva niente; e `USB` invece la conosce, quindi la radio remota andava
diligentemente in USB — dove il modulatore ascolta il microfono e l'audio del
modem non va da nessuna parte.

Su un collegamento in rete l'audio arriva sempre al codec USB della radio
lontana, quindi il modo da chiedere è sempre quello dei dati. L'impostazione
adesso viene tradotta invece che inoltrata: tutto ciò che è banda laterale
inferiore chiede `DIGL`, il resto `DIGU`, e il log dice sia cosa è stato chiesto
sia cosa era configurato. Se la radio è già in quel modo non parte niente —
prima il comando ripartiva a ogni sincronizzazione.

### Cosa non è stato toccato

Il gancio del modo sul lato gateway è stato controllato e lasciato com'era. Un
`DIGU` che arriva a un Decodium che fa da gateway finisce in `setMode()`, che lo
riconosce come modo della radio e non dell'applicazione, lo registra e lascia
l'FT8 dov'era. Quello era già giusto.

### Verifiche

Compila e linka. Il difetto è stato trovato leggendo il percorso
dall'impostazione al filo; la traduzione non è stata esercitata contro una radio
da qui.
