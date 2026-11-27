# Decodium 4 v1.0.712

## English (British)

This release is built on v1.0.711 and adds DigiPileup, a set of tools for a station that is
answering many callers at once (a DXpedition, a busy CQ). It works only on the side of the
station running the pile-up: the messages stay standard and the stations calling do not have to
do anything special. **It is experimental**: the elastic cycle and the power budget are off by
default.

### DigiPileup and the PILEUP button

- A new **PILEUP** button sits in the TX panel toolbar, next to MAM (it can be moved like the
  other buttons). Left click switches everything on or off, right click opens a dialog with the
  modes, a live status line and short instructions. "Enable everything" turns on multi-stream,
  the elastic cycle and the power budget; it never keys the transmitter or starts AutoCQ.
- **Lanes (always on in MAM multi-stream).** The distance between simultaneous streams now
  depends on the mode: FT2 170 Hz, FT4 85 Hz, FT8 50 Hz. It used to be the FT8 value for every
  mode, but an FT2 signal is about 167 Hz wide. Measured with two equal streams in white noise:
  FT2 decodes 98-100% at 170 Hz and only 29-65% at 60 Hz.
- **Elastic cycle (FT2, off by default).** Instead of transmitting every second slot (7.5 s) the
  Fox transmits again as soon as every station it wrote to has answered, or is clearly not going
  to. The waiting time of each station is learned. On the air the cycle went from 7.5 s to about
  6.4 s. The frame starts anywhere in the slot, so only receivers that decode asynchronously hear
  it (Decodium always does).
- **Power budget (off by default).** A transmitter limited in peak power loses 20·log10(N) dB per
  stream with N equal streams (6.0 dB for 2, 9.5 dB for 3, 11.9 dB for 4, measured on the real
  waveforms). The budget gives each station only the amplitude its own link needs, from the
  station's own report once it has sent one, and admits as many as fit in the frame. In FT8
  frames with two stations were seen on the air.

### MAM fixes found on the air

- A late RR73 from a slow partner now logs the QSO (it used to be thrown away).
- With "log RR73 when the partner leaves" on, a QSO is logged when the partner acknowledged our
  report, got our RR73 and then went silent. A station can no longer reopen the exchange more than
  twice with a late report (one station was sent 12 RR73).
- Low-confidence decodes ("?") rebuilt from the tail of our own signal no longer rewrite the SNR
  of the report (a station at -22 dB had been sent +09).
- A station that has never been called is no longer dropped as "silent" while the budget keeps it
  waiting; the silence is counted from the last call.
- The max-streams tooltip now gives the real loss per stream.

### Limits

- Experimental. Measured on simulations, on recorded signals and on the air with CQs and a few
  dozen stations; not yet with a real DXpedition pile-up. About half of the callers on the air
  completed the exchange.
- With the elastic cycle, software that decodes only on fixed slots may not hear the Fox.
- Only the Italian and English texts of the new dialog are translated; the other languages show
  English.

### Release contents

- Based on Decodium 4 v1.0.711 (elisir80/Decodium-4.0-Core-Shannon).
- New tests for the elastic cycle and the power budget, and a bench for simultaneous streams in
  `tools/autoimprove`.
- The Windows installer is built from this repository with the fork packaging.

## Italiano

Questa release è costruita sulla v1.0.711 e aggiunge DigiPileup, un insieme di strumenti per una
stazione che risponde a molti chiamanti insieme (una DXpedition, un CQ affollato). Lavora solo
dal lato di chi gestisce il pile-up: i messaggi restano standard e chi chiama non deve fare nulla
di speciale. **È sperimentale**: ciclo elastico e budget di potenza sono spenti di default.

### DigiPileup e il pulsante PILEUP

- Un nuovo pulsante **PILEUP** sta nella barra del pannello TX, accanto a MAM (si sposta come gli
  altri). Click sinistro: accende o spegne tutto. Click destro: apre un dialogo con i modi, una
  riga di stato in tempo reale e brevi istruzioni. «Attiva tutto» accende multi-stream, ciclo
  elastico e budget di potenza; non mette mai in trasmissione e non avvia l'AutoCQ.
- **Corsie (sempre attive nel MAM multi-stream).** La distanza tra gli stream simultanei ora
  dipende dal modo: FT2 170 Hz, FT4 85 Hz, FT8 50 Hz. Prima valeva il valore FT8 per tutti, ma un
  segnale FT2 è largo circa 167 Hz. Misurato con due stream uguali in rumore bianco: l'FT2 si
  decodifica al 98-100% a 170 Hz e solo al 29-65% a 60 Hz.
- **Ciclo elastico (FT2, spento di default).** Invece di trasmettere un secondo slot sì e uno no
  (7,5 s), la Fox ritrasmette appena ogni stazione a cui ha scritto ha risposto, o è chiaro che
  non risponderà. Il tempo di attesa di ogni stazione viene imparato. In aria il ciclo è passato
  da 7,5 s a circa 6,4 s. Il frame parte in un punto qualsiasi dello slot, quindi lo sentono solo
  i ricevitori che decodificano in modo asincrono (Decodium lo fa sempre).
- **Budget di potenza (spento di default).** Un trasmettitore limitato dal picco perde
  20·log10(N) dB per stream con N stream uguali (6,0 dB con 2, 9,5 dB con 3, 11,9 dB con 4,
  misurato sulle forme d'onda vere). Il budget dà a ogni stazione solo l'ampiezza che il suo
  collegamento richiede, ricavata dal report della stazione appena ne ha mandato uno, e ne
  ammette quante ne stanno nel frame. In FT8 si sono visti in aria frame con due stazioni.

### Correzioni al MAM trovate in aria

- Un RR73 in ritardo di un partner lento ora registra il QSO (prima veniva scartato).
- Con «registra l'RR73 quando il partner se ne va» acceso, un QSO viene registrato quando il
  partner ha confermato il nostro report, ha ricevuto l'RR73 e poi è sparito. Una stazione non può
  più riaprire lo scambio più di due volte con un report in ritardo (a una era stato mandato
  l'RR73 12 volte).
- Le decodifiche a bassa fiducia («?») ricostruite dalla coda del nostro stesso segnale non
  riscrivono più lo SNR del report (a una stazione a −22 dB era stato inviato +09).
- Una stazione mai chiamata non viene più scartata come «silenziosa» mentre il budget la tiene in
  attesa; il silenzio si conta dall'ultima chiamata.
- Il tooltip del massimo degli stream ora dà la perdita vera per stream.

### Limiti

- Sperimentale. Misurato su simulazioni, su segnali registrati e in aria con CQ e qualche decina
  di stazioni; non ancora con un vero pile-up di DXpedition. In aria circa la metà dei chiamanti
  ha completato lo scambio.
- Con il ciclo elastico, i programmi che decodificano solo a slot fissi potrebbero non sentire la
  Fox.
- Il nuovo dialogo è tradotto solo in italiano e in inglese; le altre lingue mostrano l'inglese.

### Contenuto della release

- Basata su Decodium 4 v1.0.711 (elisir80/Decodium-4.0-Core-Shannon).
- Nuovi test per il ciclo elastico e il budget di potenza, e un banco per gli stream simultanei in
  `tools/autoimprove`.
- L'installer Windows è costruito da questo repository con il pacchetto del fork.
