# fastldpc — LDPC decoding for FT8, FT4 and FT2

> Dedicated section on the LDPC(174,91) decoder of Decodium 4.0 Core Shannon:
> design, measurements, and the negative results that changed the decisions.
>
> *Author: **IU8LMC**. Implementation and measurements carried out with the
> assistance of Claude (Anthropic) under the author's direction. GPL-3.0.*

**[Italiano](#in-parole-povere) · [English](#in-plain-words) · [Español](#en-palabras-sencillas)**

---

## In parole povere

**Che cosa fa questo software.** FT8 è un modo con cui i radioamatori si
collegano usando segnali *più deboli del rumore*. Non deboli come una radio che
gracchia: segnali che con l'orecchio non sentiresti proprio, perché il fruscio
di fondo è circa **centoventi volte più forte** del segnale stesso. Il computer
li tira fuori lo stesso. Quello su cui abbiamo lavorato è la parte che fa
esattamente questo: ricostruire il messaggio da qualcosa che è quasi tutto
rumore.

**Come fa.** Il messaggio viene spedito con dei bit in più, di scorta, come
quando detti un nome al telefono e aggiungi «Ancona, Napoli, Torino». Se qualche
lettera si perde, dalle altre si risale. E in fondo c'è una firma di controllo
che dice «sì, questo è il messaggio vero».

**Il problema che abbiamo trovato**, ed è la parte interessante. Quella firma è
corta: **una firma sbagliata su 16 384 passa lo stesso, per caso.** Come una
serratura un po' malandata che ogni tanto si apre con la chiave sbagliata.

Il decoder tira a indovinare fra tanti messaggi possibili. Se ne prova 600,
praticamente mai gli capita un'apertura fortunata. Ma noi volevamo migliorarlo
facendogliene provare 21 000 — e a quel punto le aperture fortunate diventano
più di una per messaggio. **Provare di più non serviva a niente: portava dentro
tanti messaggi finti quanti veri.**

Il rimedio non era provare di più. Era **mettere una serratura migliore**: oltre
alla firma, controllare che il messaggio *assomigli a un nominativo vero*. Un
nominativo ha una forma — lettere, una cifra, altre lettere. Roba a caso quasi
mai ce l'ha.

**Cosa cambia per chi usa il programma.**

- Il decoder è **7,7 volte più veloce**. Su un computer non recentissimo, o con
  la banda affollata, è la differenza fra stare al passo e perdere qualcuno per
  strada.
- I **nominativi fantasma sono dimezzati**. Sono stazioni che il programma ti
  mostra sullo schermo ma che non hanno trasmesso niente: nomi inventati dal
  caso. Chiami e non risponde nessuno. Adesso ne compaiono la metà.
- La **sensibilità non è peggiorata**, anzi di pochissimo migliorata: −20,88 dB
  contro i −20,75 di `jt9`, il programma di riferimento nel mondo. Siamo alla
  pari.

**E la cosa onesta da dire.** Quel piccolo vantaggio, due decimi di decibel, in
aria non lo noteresti: è come mezzo metro di cavo in più o in meno. Il guadagno
vero è la velocità e i fantasmi in meno. Anzi, il risultato più utile è quasi
una brutta notizia: siamo già al livello del riferimento mondiale, quindi **i
decibel non si trovano più qui**. Per sentire ancora più lontano bisognerà
lavorare su un altro pezzo della catena.

→ [**Leggi il rapporto tecnico**](RAPPORTO_BUDGET_CRC.md)

---

## In plain words

**What this software does.** FT8 lets radio amateurs make contacts using signals
*weaker than the noise*. Not weak like a crackly radio: signals you simply could
not hear, because the background hiss is about **a hundred and twenty times
stronger** than the signal itself. The computer digs them out anyway. What we
worked on is the part that does exactly that: reconstructing the message from
something that is almost entirely noise.

**How it does it.** The message is sent with spare bits added, the way you spell
a name over the phone with "Alpha, Bravo, Charlie". If some letters are lost,
the others let you work them out. And at the end there is a check signature that
says "yes, this is the real message".

**The problem we found**, and this is the interesting part. That signature is
short: **one wrong signature in 16,384 passes anyway, by chance.** Like a
slightly worn lock that now and then opens with the wrong key.

The decoder guesses among many possible messages. Try 600 of them and a lucky
opening almost never happens. But we wanted to improve it by trying 21,000 — and
at that point the lucky openings become more than one per message. **Trying more
achieved nothing: it brought in as many fake messages as real ones.**

The fix was not to try more. It was to **fit a better lock**: besides the
signature, check that the message *looks like a real callsign*. A callsign has a
shape — letters, a digit, more letters. Random junk almost never has it.

**What changes for someone using the program.**

- The decoder is **7.7 times faster**. On an older computer, or on a crowded
  band, that is the difference between keeping up and missing stations.
- **Phantom callsigns are halved.** These are stations the program shows on
  screen that never transmitted anything: names invented by chance. You call and
  nobody answers. Now half as many appear.
- **Sensitivity has not got worse**, in fact very slightly better: −20.88 dB
  against the −20.75 of `jt9`, the world reference program. We are on par.

**And the honest thing to say.** That small advantage, two tenths of a decibel,
you would not notice on air: it is like half a metre more or less of coax. The
real gain is the speed and the missing phantoms. In fact the most useful result
is almost bad news: we are already at the level of the world reference, so **the
decibels are no longer here**. To hear further still, another part of the chain
will have to be worked on.

→ [**Read the technical report**](CRC_BUDGET_REPORT.md)

---

## En palabras sencillas

**Qué hace este programa.** FT8 permite a los radioaficionados contactar usando
señales *más débiles que el ruido*. No débiles como una radio que chisporrotea:
señales que con el oído no oirías en absoluto, porque el siseo de fondo es unas
**ciento veinte veces más fuerte** que la señal misma. El ordenador las saca
igualmente. Lo que hemos tocado es justo la parte que hace eso: reconstruir el
mensaje a partir de algo que es casi todo ruido.

**Cómo lo hace.** El mensaje se envía con bits de más, de reserva, como cuando
deletreas un nombre por teléfono y añades «Antonio, Barcelona, Carmen». Si se
pierde alguna letra, con las demás se deduce. Y al final hay una firma de
control que dice «sí, éste es el mensaje verdadero».

**El problema que encontramos**, y es la parte interesante. Esa firma es corta:
**una firma equivocada de cada 16 384 pasa igualmente, por casualidad.** Como
una cerradura algo gastada que de vez en cuando se abre con la llave que no es.

El decodificador prueba entre muchos mensajes posibles. Si prueba 600, casi
nunca le toca una apertura afortunada. Pero queríamos mejorarlo haciéndole
probar 21 000 — y ahí las aperturas afortunadas pasan de una por mensaje.
**Probar más no servía de nada: metía dentro tantos mensajes falsos como
verdaderos.**

El remedio no era probar más. Era **poner una cerradura mejor**: además de la
firma, comprobar que el mensaje *se parezca a un indicativo de verdad*. Un
indicativo tiene una forma — letras, una cifra, más letras. Lo aleatorio casi
nunca la tiene.

**Qué cambia para quien usa el programa.**

- El decodificador es **7,7 veces más rápido**. En un ordenador no muy reciente,
  o con la banda cargada, es la diferencia entre seguir el ritmo y perder
  estaciones por el camino.
- Los **indicativos fantasma se reducen a la mitad**. Son estaciones que el
  programa te muestra en pantalla pero que no han transmitido nada: nombres
  inventados por el azar. Llamas y no contesta nadie. Ahora aparecen la mitad.
- La **sensibilidad no ha empeorado**, es más, ha mejorado muy poco: −20,88 dB
  frente a los −20,75 de `jt9`, el programa de referencia mundial. Estamos a la
  par.

**Y lo honesto que hay que decir.** Esa pequeña ventaja, dos décimas de
decibelio, en el aire no la notarías: es como medio metro más o menos de cable.
La ganancia de verdad es la velocidad y los fantasmas de menos. Es más, el
resultado más útil es casi una mala noticia: ya estamos al nivel de la
referencia mundial, así que **los decibelios ya no están aquí**. Para oír aún
más lejos habrá que trabajar en otra parte de la cadena.

→ [**Lee el informe técnico**](INFORME_PRESUPUESTO_CRC.md)

---

## The technical report · Il rapporto · El informe

| Lingua | Documento |
|---|---|
| 🇮🇹 Italiano | [**Il budget della CRC**](RAPPORTO_BUDGET_CRC.md) |
| 🇬🇧 English | [**The CRC budget**](CRC_BUDGET_REPORT.md) |
| 🇪🇸 Español | [**El presupuesto del CRC**](INFORME_PRESUPUESTO_CRC.md) |

## Cronologia e misure · History and measurements

Che cosa è stato introdotto dalla v1.0.590 alla v1.0.596, che cosa è stato
ritirato e perché, con i numeri di ogni decisione: velocità misurata per modo,
sensibilità a confronto appaiato, i nominativi fantasma, e le tre strade
provate e abbandonate.

| Lingua | Documento |
|---|---|
| 🇮🇹 Italiano / 🇬🇧 English | [**Cronologia e misure**](CHANGELOG.md) |

## The code · Il codice · El código

| Path | Content |
|---|---|
| `Detector/fastldpc/` | The decoder, header-only, integrated into Decodium |
| `Detector/fastldpc/README.md` | Design, performance tables, provenance and attribution |
| `Detector/fastldpc/lab/` | Laboratory: benchmarks, tools, test data |
| `decode_bench/` | FT8 threshold in dB with ground truth (`ft8sim` from WSJT-X) |

## Key figures · I numeri principali · Las cifras principales

| Measurement | Result |
|---|---|
| FT8 50% threshold, deep profile | **−20.88 dB** with fastldpc · −20.66 without · −20.75 for `jt9` |
| Production FT8 stage, one slot at −18 dB | **71.5 s → 9.3 s** (7.7×), identical decodes |
| Min-sum, per word | **139.8 µs → 4.7 µs** (29.7×) |
| Sensitivity against min-sum alone | **+1.3 dB** at equal false-decode rate |
| Plausibility filter, at unchanged search and threshold | **phantoms halved**, decodes unchanged |
| Failures irrecoverable by any decoder, at 1 dB | **1.1%** |

---

## Attribution · Attribuzione · Atribución

Four levels, of which only the last is the work of this project ·
Quattro livelli, di cui solo l'ultimo è opera di questo progetto ·
Cuatro niveles, de los cuales sólo el último es obra de este proyecto:

| Level | |
|---|---|
| **The class of codes** | LDPC — Robert Gallager, MIT, 1962 |
| **The algorithms** | Normalised min-sum (Chen, Fossorier), ordered statistics decoding (Fossorier, Lin) |
| **The specific code** | LDPC(174,91) and CRC-14 (`0x2757`) of the FT8 protocol — Steve Franke K9AN and Joe Taylor K1JT — used **unmodified** to guarantee bit-exact compatibility |
| **The decoder** | `fastldpc`, written from scratch, with the optimisations and measurements described in the report |

Full detail in [`Detector/fastldpc/README.md`](../../Detector/fastldpc/README.md).
