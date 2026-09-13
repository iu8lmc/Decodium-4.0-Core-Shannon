# RTTY: audio/AFSK and native FSK

## English (UK)

Decodium's RTTY transmitter generates audio/AFSK, not a separate hardware
FSK keying signal. Select DIGU (DATA-U / USB-D) and configure the radio's
data modulation input for computer audio. The DECODER panel's **Set radio**
button requests DATA-U. DIGL is the lower-sideband alternative; check the
mark/space polarity (REV) when using it.

Native RTTY/FSK modes are distinct from audio data modes. Decodium now blocks
audio transmission in these modes and explains how to select DATA-U. This
does not prevent reception. RTTY-U/RTTY-L are application labels: a radio may
call its native modes RTTY/RTTY-R instead.

The existing QMX USB-audio FSK exception is preserved, including its mode
translation and full-scale audio policy. It must not be applied to other rigs.
Earlier release wording describing DATA-U as unsuitable for RTTY was too broad.

## Italiano

Il trasmettitore RTTY di Decodium genera audio/AFSK, non un segnale separato
di manipolazione FSK hardware. Selezionare DIGU (DATA-U / USB-D) e configurare
l'ingresso di modulazione dati della radio per l'audio del computer. Il pulsante
**Set radio** nel pannello DECODER richiede DATA-U. DIGL è l'alternativa in
banda laterale inferiore: verificare la polarità mark/space tramite REV.

I modi RTTY/FSK nativi sono distinti dai modi dati audio. Decodium blocca ora
la trasmissione audio in questi modi e indica come selezionare DATA-U; la
ricezione resta disponibile. RTTY-U/RTTY-L sono etichette dell'applicazione:
la radio può chiamare i propri modi nativi RTTY/RTTY-R.

Resta l'eccezione QMX per FSK tramite audio USB, con la traduzione dei modi
e il livello audio a fondo scala già previsti. Non va estesa alle altre radio.
Le precedenti note che indicavano DATA-U come inadatto alla RTTY erano troppo
generiche.
