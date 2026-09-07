# fastldpc — cronologia e misure, v1.0.590 → v1.0.596

**Italiano** · [English below](#english)

> Che cosa è stato introdotto, che cosa è stato ritirato e perché, con i numeri
> di ogni decisione. Le misure vengono da banchi sintetici, da registrazioni
> off-air in 20, 40 e 80 metri e da sessioni dal vivo.
>
> *Autore: **IU8LMC**. Implementazione e misure svolte con l'assistenza di
> Claude (Anthropic) su indicazione dell'autore. GPL-3.0 — 30 agosto 2026.*

---

## In breve

Il decodificatore LDPC di FT2 e FT8 è stato riscritto per lavorare su sedici
parole per volta con istruzioni AVX2. Il guadagno è **di velocità, non di
sensibilità**: a parità di ciò che cerca, trova le stesse stazioni molto più in
fretta. Dove la velocità si traduce in stazioni in più è un caso solo, ma
importante — quando la banda è affollata e il decodificatore originale non fa in
tempo ad arrivare in fondo alla lista dei candidati.

Tre tentativi di convertire quel margine di tempo in sensibilità sono stati
misurati e **tutti e tre hanno dato zero**: dare più tempo, allargare la ricerca
OSD, abbassare la soglia di aggancio. Il collo di bottiglia non è il tempo.

---

## Il decodificatore vettorizzato

Il nucleo min-sum è scritto con intrinseci AVX2 espliciti perché
l'auto-vettorizzazione non ce la fa: i confronti min1/min2/argmin del nucleo
contengono controllo di flusso che GCC 15 e Clang 22 rifiutano entrambi con
*"unsupported control flow in loop"*. Ogni operazione lavora su 16 int16 per
registro YMM.

La scelta avviene **a runtime**: senza AVX2, o per lunghezze di parola diverse da
174/91, si torna al decodificatore originale. I binari pubblicati restano quindi
utilizzabili ovunque.

| modo | prima | dopo | note |
|---|---|---|---|
| FT2, passata di decodifica | 2800-4100 ms | **60-100 ms** | via a blocchi |
| FT8, per wav a −21 dB (banco) | 16128 ms | **785 ms** | via a blocchi |
| FT8, per wav, una parola alla volta | — | 995 ms | quindici corsie ferme |

La differenza fra 995 e 785 ms è tutta lì: decodificando una passata alla volta
quindici corsie su sedici girano a vuoto. Le passate di un tentativo vengono
quindi preparate in anticipo — prepararne una non dipende dall'esito delle altre
— raggruppate per terna `(Keff, maxosd, norder)`, perché la chiamata a blocchi
ne accetta una sola, e decodificate un gruppo per volta. Il risultato è stato
verificato **identico** alla via per passata sugli stessi segnali: zero
discordanze, soglie coincidenti.

---

## Sensibilità: che cosa cambia davvero

### FT8, banco appaiato sugli stessi segnali

| SNR | decodificatore originale | fastldpc |
|---|---|---|
| −20 dB | 6/6 | 6/6 |
| −21 dB | 2/6 | 3/6 |
| −22 dB | 1/6 | 1/6 |

Una sola discordanza su 18 prove, p = 1,00 al test dei segni: **indistinguibili**.

### FT8, traffico reale, due finestre da circa sette minuti

| | decodifiche | nominativi distinti |
|---|---|---|
| decodificatore originale | 663 | 122 |
| fastldpc | 711 | 123 |

Stesse stazioni. Il decodificatore da solo non aggiunge nulla.

### FT8, 19 slot registrati off-air — qui la differenza esiste

| banda | originale | fastldpc | perché |
|---|---|---|---|
| 40 m, affollata | 198 distinti in 306 s | **250 in 75 s** | l'originale impiega ~16 s per slot contro una scadenza di 8: viene troncato a metà della lista dei candidati |
| 80 m, scarica | **56** distinti in 306 s | 52 in 61 s | qui il tempo basta a entrambi, e la propagazione esatta batte l'approssimazione min-sum sui segnali marginali |

Le quattro decodifiche che solo l'originale trovava in 80 metri erano stazioni
autentiche — `DO8JB/YU1LD`, `PE1NAO/M7XRI`, `RA3VME/CT3MD`, `W3UCA/DA6IT` — e
l'originale non perdeva nulla di ciò che trovava fastldpc: era un sovrainsieme.

Non esiste un criterio a priori per scegliere il decodificatore: dipende da
quante stazioni ci sono da trovare, cosa che non si sa prima di cercarle. Il
numero di candidati non discrimina — mediana 717 in 40 metri affollati contro
751 in 80 metri scarichi, perché su banda vuota la ricerca del sync aggancia
rumore.

### La passata di recupero

Da qui la soluzione: si usano entrambi. fastldpc arriva in fondo alla lista e
garantisce di non perdere nulla per scadenza; sui candidati che non hanno dato
nulla si spende il tempo risparmiato per un secondo tentativo con la
propagazione esatta.

| | senza recupero | con recupero | originale da solo |
|---|---|---|---|
| 80 m | 52 distinti, 141 decodifiche, 61 s | **55, 145, 91 s** | 56, 145, 306 s |
| 40 m | 250 distinti, 509 decodifiche, 74 s | **250, 512, 98 s** | 198, 401, 306 s |

Recupera tre delle quattro marginali in banda scarica e mantiene intatto il
vantaggio in banda piena, al prezzo del 30-50% di tempo in più rispetto al solo
fastldpc — restando tre volte più rapido dell'originale. Il numero di recuperi
per ciclo ha un tetto (`DECODIUM_FT8_CLASSIC_RESCUE`, 0 disattiva) perché
ognuno costa come un intero slot del decodificatore lento.

---

## La correzione del segno nel min-sum

Il ramo min-sum del decodificatore condiviso produceva messaggi di segno opposto
a quello del ramo esatto immediatamente sopra, che calcola `2*platanh(-tmn)`.
Confronto appaiato sugli stessi segnali, 2000 parole a Eb/N0 1 dB:

| | decodifiche corrette |
|---|---|
| senza la correzione | 1929 |
| con la correzione | **1986** (+3,0%) |

È l'unico intervento della serie che aggiunge **sensibilità** anziché velocità.
FT8 raggiunge quel ramo ogni volta che la profondità OSD è sotto la soglia del
BP esatto; FT2 sempre.

---

## I nominativi fantasma

### Il problema

La v1.0.590 usava una ricerca OSD larga (ordine 3, span 91/48) che prova circa
**21 400 candidati per parola** contro i circa 600 della stretta. La CRC-14 ne
lascia passare uno sbagliato ogni 16 384, quindi allargare comprava decodifiche
giuste e false nella stessa proporzione: sull'aria **2,8 nominativi inventati
per ciclo**.

I fantasmi si riconoscono da un marcatore semplice: una stazione vera ricompare
ciclo dopo ciclo, un falso da CRC appare una volta sola e mai più. Nelle
sessioni sane pochi nominativi ripetuti molte volte — `CQ DL1LN JO54` nove
volte, `EN35UKR` ventiquattro. Nella sessione malata 601 decodifiche e 597
messaggi distinti.

### Falsi allarmi su puro rumore, per chiamata al decodificatore

| preset | nd_max 0,075 | 0,11 | 0,19 |
|---|---|---|---|
| ordine 3, span 91/48 | 0,00005 | 0,040 | **0,73** |
| ordine 2, span 32 | 0,00005 | 0,004 | 0,037 |
| ordine 1 | **0** | 0,001 | 0,006 |

Con la ricerca larga e il filtro allargato dall'AP, **quasi ogni chiamata**
produceva un fantasma.

### Il prezzo di stringere il filtro

Su 20 000 parole con segnale, a Eb/N0 1 dB con 29 bit noti per ipotesi a priori:

| nd_max | corrette | false |
|---|---|---|
| 0,060 | 19024 | 0 |
| **0,075** | **19115** | **0** |
| 0,085 | 19115 | 3 |
| 0,190 | 19115 | 11 |
| 1,0 (nessun filtro) | 19115 | 11 |

Le corrette sono identiche da 0,075 in su: allargare non ne recupera nessuna,
aggiunge solo false. Sotto 0,075 si inizia a perdere. Il filtro che si allargava
in presenza di bit noti (`nd_max × (N/liberi)²`) era quindi danno puro, ed è
stato rimosso.

### I controlli che mancavano

I filtri erano nella funzione di decodifica a parola singola, che FT2 non chiama
più: usa la via a blocchi, dove `nharderror` veniva calcolato e restituito ma
**non filtrava nulla**. Arrivavano in lista parole che ribaltavano 31, 36 e più
di 40 bit, mentre ogni soglia tarata viveva in codice morto.

Su 74 decodifiche reali di stazioni ripetute, da +11 a −26 dB: `nharderror`
mediana **1**, p99 **16**, massimo **20**. I fantasmi partivano da 23, con una
valle netta fra 19 e 22.

È stato aggiunto anche un test strutturale di plausibilità sui 77 bit del
payload dentro il ciclo di accettazione — tipo di messaggio, struttura dei
nominativi, posizione dei token, intervalli dei campi — che sono vincoli certi,
non soglie statistiche. In modalità FT8 ammette tutti i tipi definiti, perché i
formati da contest che FT2 non usa in FT8 esistono, e filtrarli renderebbe il
decodificatore cieco al traffico di contest.

---

## Tre strade misurate e ritirate

### Dare più tempo

Il decodificatore originale con budget di 16 secondi per slot decodifica
**esattamente** ciò che decodifica con 1,2 secondi. I secondi in più non trovano
nulla.

### Allargare la ricerca OSD

Provata due volte in FT2 e ritirata due volte. La seconda con tutti i controlli
strutturali attivi: sei minuti a zero fantasmi sembravano assolverla, ma erano
sei minuti su una banda senza trasmissioni, e con più tempo i fantasmi sono
tornati copiosi. Il filtro di plausibilità paga circa 2 bit, l'allargamento ne
costa circa 5.

### Abbassare la soglia di aggancio

Su tre finestre di traffico reale sembrava valere **+10% di decodifiche e ottanta
nominativi nuovi**, tutti autentici e ripetuti. Rimisurata sugli **stessi 19 slot
registrati**, soglie 6, 3 e 1 danno il medesimo esito — 275 messaggi distinti e
486 decodifiche — con il tempo che cresce del 15% scendendo.

Il guadagno apparente era **la banda che cambiava fra una finestra e l'altra**.
È l'errore metodologico che ha viziato tre valutazioni in questa serie: su
finestre temporali diverse il traffico varia abbastanza da simulare differenze
del 10%. L'unico confronto che regge è sullo stesso audio registrato.

Alzare la soglia invece conviene: su tre bande, una scala 1,5 non perde
decodifiche e fa risparmiare fra il 32% e il 60% del tempo, perché il
decodificatore esamina circa 740 candidati per passata mentre le stazioni stanno
nei primi 150 scarsi. A 2,0 si comincia a perdere (6 messaggi su 275 in 20
metri, 4 su 250 in 40). Il valore resta al default in attesa di una verifica più
ampia; `DECODIUM_FT8_SYNCMIN_SCALE` lo espone.

---

## Il crash della v1.0.595 e la correzione della v1.0.596

La v1.0.595 abbassava una soglia che teneva spento il decode profondo di
recupero in FT8. L'intento era corretto: la soglia pretendeva un budget di
**7000 ms** mentre il massimo ottenibile è **6550 ms** per costruzione — fine
dello slot più 6800, meno 250 di sicurezza. La condizione non poteva mai essere
soddisfatta, quindi quello stadio non era **mai** stato eseguito, e ogni scarto
veniva letto come sovraccarico del worker armando un raffreddamento di sei slot.

Riattivarlo ha riattivato anche un difetto di memoria latente in quel percorso,
rimasto inutilizzato da quando la soglia è stata introdotta: **corruzione dello
heap** (`0xC0000374` in ntdll) entro circa due minuti di ricezione FT8, sempre
subito dopo il lancio del follow-up, con entrambi i decodificatori.

La v1.0.596 riporta la soglia al valore che tiene spento quello stadio.
Verificata con dodici minuti di ricezione continua senza cadute, contro i meno di
due della v1.0.595.

### Che cosa si sa del difetto

Escluso con prove: **non è il decodificatore** (la stessa configurazione profonda
girata offline su slot registrati non crasha mai); **non è il flag supplemental**
(crasha anche senza); **non è la condivisione del buffer audio** (il worker ne fa
una copia e il decodificatore lo riceve in sola lettura); **non è l'accumulo del
contatore di decodifiche** (viene azzerato a fine decode e il salvataggio riceve
il limite). Una o tre decodifiche profonde vanno a buon fine prima della caduta,
quindi il difetto matura anziché scattare subito.

L'indizio residuo punta alla concorrenza fra il follow-up accodato e il decode
ancora in volo, con lo stadio 4 che mantiene stato globale dietro un mutex
single-flight che non copre tutto ciò che le due richieste si scambiano.

Per riprodurlo: `DECODIUM_FT8_DEEP_MIN_BUDGET=2500` e ricevere FT8 per un paio di
minuti. `DECODIUM_FT8_NO_SUPPLEMENTAL=1` isola quel fattore. Per risolvere gli
stack di un minidump serve una build non strippata: lo strip è applicato da
`CMakeLists.txt` intorno alla riga 1226 (`CMAKE_EXE_LINKER_FLAGS_RELEASE`), non
dal blocco *"Ottimizzazione dimensione"* più in alto.

**Conseguenza da conoscere:** `deepSearch` e `avgDecode` restano senza effetto,
come lo erano da mesi. Non è una perdita rispetto a prima, ma non è nemmeno una
riparazione.

---

## Altre correzioni della serie

- **Dieci impostazioni mute.** Venivano scritte nella sezione generale del file e
  lette dal gruppo del profilo attivo, quindi tornavano ai valori predefiniti a
  ogni riavvio: `Ft8SubpassHarvest` (il pulsante GAL), `Ft2AdaptiveDecode`,
  `Ft2ApHashCache`, `Ft2Conservative`, `Ft2FullDecodeInAutoCq`,
  `Ft2PartnerMemoryEnabled`, `Ft2QuickGiveUpStrong`, `MamMultiStream`,
  `MamMaxStreams`, `MamCqSlots`. Ora la lettura ricade sul valore globale quando
  la chiave non è ancora nel profilo, così i valori già salvati vengono
  recuperati invece di andare persi.
- **Waterfall.** Cliccando un nominativo decodificato ora si chiama quella
  stazione. Prima il clic spostava soltanto la frequenza di trasmissione e per
  chiamare serviva Ctrl+clic, che non si indovina. La frequenza si imposta
  ancora cliccando lontano da un'etichetta, o con Ctrl+clic.
- **Interruttore Fast LDPC** nella barra strumenti, accanto a GAL, con stato
  persistente.
- **Sincronizzazione QML.** In una compilazione dentro i sorgenti il passo
  cancellava la cartella `qml/` del progetto per ricopiarla da sé stessa.

---

## Interruttori a runtime

| variabile | effetto |
|---|---|
| `DECODIUM_FT8_FASTLDPC=0` | FT8 torna al decodificatore originale |
| `DECODIUM_FT8_BATCH=0` | FT8 decodifica una passata alla volta |
| `DECODIUM_FT8_CLASSIC_RESCUE=n` | tetto dei recuperi per ciclo, 0 disattiva |
| `DECODIUM_FT8_SYNCMIN_SCALE=x` | scala la soglia di aggancio dei candidati |
| `DECODIUM_FT8_CAND_LOG=1` | riporta soglia effettiva e numero di candidati |
| `DECODIUM_LDPC_MAX_HARD=n` | soglia sui bit ribaltati |
| `DECODIUM_LDPC_AP_CHECK=0` | disattiva il controllo di coerenza con l'AP |
| `DECODIUM_LDPC_GATE_LOG=1` | mostra che cosa i filtri scartano e perché |
| `DECODIUM_FT2_DISABLE_FASTLDPC=1` | FT2 torna al decodificatore originale |
| `FASTLDPC_TIPI=tutti` | non esclude alcun tipo di messaggio |

---

## Avvertenze sulle misure

Le misure vengono da tre registrazioni off-air (20, 40 e 80 metri), da banchi
sintetici e da sessioni dal vivo. **Non** coprono l'intera gamma delle condizioni
di propagazione, e l'equilibrio fra i due decodificatori dipende da quante
stazioni ci sono da trovare — cosa che non si conosce prima di cercarle.

I confronti su finestre temporali diverse non sono affidabili per differenze
inferiori al 10%: il traffico cambia abbastanza da simularle. Ogni affermazione
quantitativa in questo documento che riguarda la sensibilità viene da un
confronto **appaiato** — stesso audio, stesse forme d'onda generate — oppure è
dichiarata come non conclusiva.

---
---

<a name="english"></a>

# fastldpc — history and measurements, v1.0.590 → v1.0.596

[Italiano sopra](#fastldpc--cronologia-e-misure-v10590--v10596) · **English**

> What was introduced, what was withdrawn and why, with the numbers behind each
> decision. Measurements come from synthetic benches, from off-air recordings on
> 20, 40 and 80 metres, and from live sessions.

## In brief

The LDPC decoder for FT2 and FT8 was rewritten to work on sixteen words at a
time using AVX2. The gain is **speed, not sensitivity**: for the same search, it
finds the same stations far more quickly. There is one case where speed does
turn into more stations, and it matters — when the band is busy and the original
decoder cannot reach the end of the candidate list in time.

Three attempts to convert that time margin into sensitivity were measured and
**all three returned nothing**: more time, a wider OSD search, a lower sync
threshold. Time is not the bottleneck.

## The vectorised decoder

| mode | before | after |
|---|---|---|
| FT2 decode pass | 2800-4100 ms | **60-100 ms** |
| FT8 per wav at −21 dB (bench) | 16128 ms | **785 ms** |
| FT8 per wav, one word at a time | — | 995 ms |

Decoding one pass at a time leaves fifteen lanes of sixteen idle. Passes are now
prepared in advance, grouped by their `(Keff, maxosd, norder)` triple and decoded
a group at a time, with output verified **identical** to the per-pass path.

## Sensitivity

Paired bench on FT8, same waveforms: one discordance in 18 trials, p = 1.00 —
indistinguishable. Live traffic over two seven-minute windows: 711 decodes from
123 callsigns against 663 from 122 — the same stations.

On 19 recorded slots the difference appears:

| band | original | fastldpc |
|---|---|---|
| 40 m, busy | 198 distinct in 306 s | **250 in 75 s** |
| 80 m, quiet | **56** distinct in 306 s | 52 in 61 s |

On a busy band the original takes about 16 s per slot against an 8 s deadline
and is cut off partway through the candidate list. On a quiet band there is time
for both, and exact belief propagation beats the min-sum approximation on
marginal signals — the four extra decodes were genuine stations.

The recovery pass runs both: fastldpc reaches the end of the list, then the
original re-decodes the candidates that produced nothing. On 80 m it recovers
three of the four marginals (55 distinct, 145 decodes, 91 s against 306); on
40 m the advantage is untouched (250 distinct, 512 decodes, 98 s).

## Sign correction in the min-sum branch

The shared decoder's min-sum branch produced messages opposite in sign to the
exact branch above it. Paired comparison over 2000 words at Eb/N0 1 dB: **1986
correct decodes against 1929**, +3.0%. The only change in the series that adds
sensitivity rather than speed.

## Phantom callsigns

A wide OSD search tries about 21 400 candidates per word against roughly 600. A
14-bit CRC admits one wrong candidate in 16 384, so widening buys correct and
false decodes in the same proportion: **2.8 invented callsigns per cycle** on
air. False alarms on pure noise, per decoder call: **0.73** for order 3 with the
gate widened to 0.19, **0** for order 1 at 0.075.

Tightening costs nothing: correct decodes are identical from nd_max 0.075
upwards (19115 of 20000 at Eb/N0 1 dB), while false ones drop from 11 to 0. The
gate that widened in the presence of a-priori bits was pure harm and was removed.

The checks sat in the single-word function that FT2 no longer calls: the batch
path computed `nharderror` but never rejected on it. Over 74 real decodes of
repeated stations, from +11 to −26 dB, `nharderror` has median **1**, p99 **16**,
maximum **20**; phantoms started at 23.

## Three measured dead ends

**More time**: the original decoder with a 16-second budget decodes exactly what
it decodes with 1.2 seconds.

**A wider OSD search**: tried twice in FT2 and withdrawn twice. Six minutes at
zero phantoms seemed to absolve it, but on a band with no transmissions six
minutes prove nothing.

**A lower sync threshold**: appeared to be worth +10% and eighty new callsigns
across three live windows. Re-measured on the **same 19 recorded slots**,
thresholds 6, 3 and 1 decode identically — 275 distinct messages, 486 decodes —
while costing up to 15% more time. The apparent gain was the band changing
between windows. Raising it to 1.5 instead loses nothing and saves 32-60% of the
time across three bands.

## The v1.0.595 crash and the v1.0.596 fix

v1.0.595 lowered a threshold demanding a 7000 ms budget when the obtainable
maximum is 6550 ms by construction, so the FT8 deep follow-up had **never** run.
Re-enabling it also re-enabled a latent memory fault: heap corruption
(`0xC0000374`) within about two minutes of FT8 reception. v1.0.596 restores the
threshold; verified over twelve minutes of continuous reception.

Ruled out with evidence: not the decoder (the same configuration offline never
crashes), not the supplemental flag, not audio buffer sharing, not counter
accumulation. One to three deep decodes complete first, so it builds up. The
remaining lead is concurrency between the queued follow-up and the decode still
in flight. Reproduce with `DECODIUM_FT8_DEEP_MIN_BUDGET=2500`.

**Consequence:** `deepSearch` and `avgDecode` remain without effect, as they had
been for months. Not a loss against before, but not a repair either.

## Caveats

Measurements come from three off-air recordings, synthetic benches and live
sessions. They do **not** cover the full range of propagation conditions.
Comparisons across different time windows are unreliable below 10%: traffic
varies enough to simulate them. Every quantitative claim about sensitivity here
comes from a **paired** comparison — same audio, same generated waveforms — or is
stated as inconclusive.
