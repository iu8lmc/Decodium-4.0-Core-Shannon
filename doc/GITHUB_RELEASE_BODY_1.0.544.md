# Decodium 4 FT2 v1.0.544

Release 1.0.544 builds on 1.0.543 and adds a new RF measurement instrument,
the DECØMETER, together with the localisation work that the 1.0.543 strings
still required.

## English (British)

### DECØMETER — RF vector meter

- New measuring instrument, reachable from the main menu as
  **📡 DECØMETER — RF Meter…**
- Three concentric arc scales show forward power, reflected power and standing
  wave ratio, with a fourth inner arc for the ALC reading when the radio
  reports one.
- Meter ballistics follow instrument practice: instantaneous attack,
  exponential release and a three-second peak hold, so a brief peak stays
  readable instead of flashing past.
- Ranges of 5, 50, 500 and 5000 W with automatic switching and a smoothed
  transition. The scales are re-labelled to match the range in use, so the
  printed figures always correspond to the watts being measured.
- Three screens, selected with the ◀ ▶ keys or the arrow buttons: power,
  matching and drive.
- The panel can be moved by dragging the faceplate and closed from the corner
  or with Escape.

The readings are not simulated. Forward power, standing wave ratio and ALC
come from the telemetry Decodium already reads from the transceiver over
Hamlib (`RFPOWER_METER_WATTS`, `SWR`, `ALC`). Everything that can be derived
exactly is derived: reflected power from the square of the reflection
coefficient, return loss, mismatch loss, net delivered power, PEP and a
running average.

Where a quantity cannot be measured, the instrument says so rather than
inventing a plausible figure. From the magnitude of the reflection coefficient
alone the resistance of a load is only known to lie between 50/SWR and
50 × SWR, and the reactance cannot be determined at all without phase
information, which no transceiver reports over CAT. The impedance column
therefore shows the interval and states that a vector sensor would be needed
for X. If no CAT link is present, or the radio provides no meter, the display
stays blank instead of showing a convincing zero.

The refresh runs at 25 Hz while there is energy to show and falls back to 5 Hz
when the instrument is at rest, so it costs little on modest computers.

Design: Claude Design, *DECOMETER RF Vector Meter*.

### Localisation

- The 1.0.543 strings for the ADIF import and the CI-V address help text are
  now translated in all fifteen interface languages.
- One of those strings was written in Italian in the source. A source string
  that is not English is never translated for anyone else, so the ADIF import
  button would have appeared in Italian to every other language. It has been
  returned to English and translated.
- The DECØMETER interface is likewise available in all fifteen languages.
- All fifteen catalogues report zero unfinished messages.

### Carried forward from 1.0.543 and earlier

- Large ADIF logbook import, CI-V address persistence, and the CAT, DXCC and
  QSO timestamp fixes from 1.0.543.
- The TCI transmit-audio work from 1.0.542: the sample rate answered by the
  server is now read, each frame declares the rate actually generated, and the
  push fallback for servers that do not pace transmission engages only while
  the transmitter is genuinely keyed.
- Per-destination UDP identity and filters, decode de-duplication, the
  Slow-PC graphics recovery guard and truthful build revision stamping.

---

## Italiano

### DECØMETER — misuratore vettoriale RF

- Nuovo strumento di misura, dal menu principale come
  **📡 DECØMETER — Misuratore RF…**
- Tre scale ad arco concentriche per potenza diretta, potenza riflessa e
  rapporto di onde stazionarie, più un arco interno per l'ALC quando la radio
  lo fornisce.
- Balistica da strumento: attacco istantaneo, rilascio esponenziale e picco
  trattenuto tre secondi, così un picco breve resta leggibile.
- Portate 5, 50, 500 e 5000 W con cambio automatico e transizione morbida. Le
  scale si rietichettano secondo la portata in uso, quindi i numeri scritti
  corrispondono sempre ai watt misurati.
- Tre schermate, con ◀ ▶ o i tasti freccia: potenza, adattamento, pilotaggio.
- Il pannello si sposta trascinando il frontalino e si chiude dall'angolo o
  con Esc.

Le misure non sono simulate. Potenza diretta, ROS e ALC vengono dalla
telemetria che Decodium già legge dal ricetrasmettitore via Hamlib. Tutto ciò
che si può ricavare per via esatta è ricavato: potenza riflessa dal quadrato
del coefficiente di riflessione, perdita di ritorno, perdita per
disadattamento, potenza netta erogata, PEP e media.

Dove una grandezza non è misurabile, lo strumento lo dichiara invece di
mostrare un numero verosimile. Dal solo modulo del coefficiente di riflessione
la resistenza di un carico è nota unicamente entro l'intervallo fra 50/ROS e
50 × ROS, e la reattanza non è determinabile senza l'informazione di fase, che
nessun apparato fornisce via CAT. La colonna dell'impedenza mostra quindi
l'intervallo e dichiara che per X servirebbe un sensore vettoriale. Senza CAT,
o con una radio priva di strumento, il display resta muto invece di mostrare
uno zero credibile.

Il ridisegno gira a 25 Hz finché c'è energia da mostrare e scende a 5 Hz a
strumento fermo, per pesare poco sui computer modesti.

Disegno: Claude Design, *DECOMETER RF Vector Meter*.

### Localizzazione

- Le stringhe della 1.0.543 per l'import ADIF e per l'aiuto sull'indirizzo
  CI-V sono ora tradotte in tutte e quindici le lingue.
- Una di quelle stringhe era scritta in italiano nel sorgente. Una stringa
  sorgente non inglese non viene tradotta per nessun altro, quindi il pulsante
  di import ADIF sarebbe apparso in italiano a chiunque usasse un'altra
  lingua. È stata riportata in inglese e tradotta.
- Anche l'interfaccia del DECØMETER è disponibile in tutte e quindici le
  lingue.
- Tutti e quindici i cataloghi riportano zero messaggi non finiti.

### Ereditato dalla 1.0.543 e precedenti

- Import ADIF di grandi dimensioni, persistenza dell'indirizzo CI-V e le
  correzioni CAT, DXCC e sugli orari dei QSO della 1.0.543.
- Il lavoro sull'audio TCI in trasmissione della 1.0.542.
- Identificativi e filtri UDP per destinazione, deduplica dei decode, guardia
  di ripristino grafico della modalità PC lento e revisione di build veritiera.
