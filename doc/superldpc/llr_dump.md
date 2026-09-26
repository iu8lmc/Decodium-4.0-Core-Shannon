# Dump degli LLR FT2 (`DECODIUM_LLR_DUMP`)

Serve a separare il **demodulatore** dal **decodificatore**. Decodium produce
gli LLR veri — con la sua sincronizzazione, su rumore e QRM veri — e il
confronto fra decodificatori (min-sum, a priori, decodifica a lista, superldpc)
si fa **dopo, offline, sugli stessi candidati**. Così non serve scrivere in
C++ un decodificatore nuovo prima di sapere se conviene.

## Come si accende

```
DECODIUM_LLR_DUMP=C:\percorso\llr_ft2.bin
```

Senza la variabile non cambia niente: il percorso si legge **una volta sola**
all'avvio, quindi per candidato non c'è nemmeno una `getenv`. Il file si apre
alla prima scrittura, **in append**, e resta aperto fino alla chiusura del
programma; ogni record viene scritto sotto lucchetto (i decodificatori girano
in parallelo) e seguito da `fflush`, così una misura interrotta lascia
comunque i record già scritti.

Scritto in `Detector/FtxFt2Stage7.cpp`, nel ciclo sui candidati, subito dopo
`run_decode_passes` — cioè con gli stessi LLR che sono andati al
decodificatore e con l'esito che ne è uscito.

## Che cosa viene scritto

**Un record per candidato** (non per passata): il candidato è ciò che il
demodulatore ha agganciato a una certa frequenza e a un certo DT, e le passate
sono i tentativi del decodificatore su quegli stessi LLR.

Gli LLR salvati sono l'insieme **`llra`**, quello base costruito dalle
`bitmetrics` senza a priori. Le varianti `llrb..llre` e la passata coerente
non vengono salvate: se servissero, il punto di scrittura è uno solo e si
estende.

## Formato del record

Binario, **little-endian**, **735 byte**, senza riempimenti:

| offset | tipo | campo |
|---|---|---|
| 0 | `uint32` | magic `0x4C4C5231` (`"LLR1"`) |
| 4 | `uint32` | versione, `1` |
| 8 | `double` | inizio del ciclo, secondi UTC |
| 16 | `float` | frequenza stimata, Hz |
| 20 | `float` | DT stimato, s |
| 24 | `float` | punteggio di sync (`smax`) |
| 28 | `uint8` | esito: 1 decodificato, 0 no |
| 29 | `uint8[10]` | messaggio, 77 bit impacchettati (zeri se esito 0) |
| 39 | `float[174]` | LLR |

**Tempo.** È l'ora di sistema all'inizio della decodifica del ciclo, uguale
per tutti i candidati dello stesso ciclo: serve a raggrupparli. In replay di
un WAV **non** è l'ora della registrazione.

**Impacchettamento dei 77 bit.** Il bit `i` sta nel byte `i/8`, nella
posizione `0x80 >> (i%8)`: primo bit nel bit più significativo del primo byte.
Gli ultimi 3 bit del decimo byte sono sempre zero.

## Convenzione di segno — leggere prima di confrontare

Gli LLR sono salvati **con il segno del chiamante**, cioè quello che vuole
`ftx_decode174_91_c`:

> **LLR positivo = bit 1** (`FtxLdpc.cpp`: `cw[bit] = zn[bit] > 0 ? 1 : 0`).

Gli script Python di `superldpc` usano la convenzione **opposta** (LLR positivo
= bit 0): lì i valori vanno **invertiti di segno** prima di darli in pasto al
min-sum. La decodifica a lista e il gate `nd` non dipendono dalla scala; il
min-sum sì, ma poco.

## Scrambling: gli LLR NON stanno nello spazio del messaggio

FT2, come FT4, mescola i 91 bit sistematici con un vettore fisso `rvec`
(`FtxFt2Stage7.cpp`, `raw_rvec`):

```
message77[i] = message91[i] XOR rvec[i]      i = 0..76
```

Gli LLR (e quindi il codeword) stanno nello spazio **message91**; il campo
`messaggio` del record è invece il **message77 vero**, quello confrontabile
con la verità nota. Per confrontare i segni degli LLR con i bit del record
serve lo XOR:

```
segno(llr[i]) == (bits[i] XOR rvec[i])       i = 0..76
```

`rvec` (77 valori):

```
0 1 0 0 1 0 1 0 0 1 0 1 1 1 1 0 1 0 0 0 1 0 0 1 1 0 1 1 0
1 0 0 1 0 1 1 0 0 0 0 1 0 0 0 1 0 1 0 0 1 1 1 1 0 0 1 0 1
0 1 0 1 0 1 1 0 1 1 1 1 1 0 0 0 1 0 1
```

**Verificato** il 16/9/2026 su un decode vero (`lab/apsoft/mag12/forte.wav`,
"IU8LMC DL9XYZ -12"): accordo segno/bit **39/77 senza XOR, 77/77 con XOR**, e
30/30 sui trenta LLR di modulo maggiore. Se un giorno l'accordo non fosse più
totale su una decodifica riuscita, il dump sta prendendo gli LLR di un
candidato diverso da quello decodificato.

## Lettura in Python

```python
import numpy as np, struct

REC = struct.Struct("<IIdfffB10s")          # 39 byte, poi 174 float
DIM = REC.size + 4 * 174

def leggi(percorso):
    dati = open(percorso, "rb").read()
    assert len(dati) % DIM == 0, "file troncato o formato diverso"
    for off in range(0, len(dati), DIM):
        magic, ver, t, freq, dt, sync, ok, msg = REC.unpack_from(dati, off)
        assert magic == 0x4C4C5231 and ver == 1
        llr = np.frombuffer(dati, dtype="<f4", count=174, offset=off + REC.size)
        bits = np.unpackbits(np.frombuffer(msg, dtype=np.uint8))[:77]
        yield dict(t=t, freq=freq, dt=dt, sync=sync, ok=bool(ok), bits=bits, llr=llr)

# controllo di sanita' su un record decodificato: deve dare 77/77
# (bits ^ RVEC) == (llr[:77] > 0)
```

## Gli strumenti del banco (16/9/2026)

| strumento | a che serve |
|---|---|
| `lab/tools/mescola_ft2.py` | slot FT2 sintetici dentro rumore vero, con verità nota |
| `lab/tools/verita_ft2.py` | li passa in Decodium e misura decodifica, aggancio e falsi |
| `lab/tools/leggi_llr.py` | legge questo dump (`--controlla` verifica segno e scrambling) |

**Il segnale lo genera lo stack TX del programma** (`tests/ft2_make_test_wav`),
non un modulatore riscritto: la forma d'onda è quella vera. Il DT si comanda
con `--offset-ms`, e vale `dt = offset_ms/1000 - 0.5`.

**Il livello.** `mescola_ft2.py` misura il rumore dello slot di sfondo e scala
il segnale per ottenere l'SNR chiesto in 2500 Hz. Due riferimenti possibili,
`--riferimento`:

- `mediana` (default): mediana della densità spettrale nei ±250 Hz attorno al
  segnale, cioè **il fondo di rumore lì**. Robusto alle portanti.
- `totale`: tutta la potenza nei 2500 Hz attorno al segnale, **QRM compreso**.
  Più severo.

Il riferimento di banda larga (mediana su 300-2700 Hz) era la prima scelta ed
è **sbagliato**: in una banda affollata il rumore locale può stare 10 dB sopra
la mediana di banda, e metà dei frame "a 0 dB" non veniva nemmeno agganciata.

`--evita-qrm` mette il segnale dove lo sfondo è quieto per tutto lo slot
(picco locale entro 6 dB dal fondo). Senza, la frequenza è a caso: più
realistico, ma la curva di soglia misura il QRM invece della sensibilità.

**Il tipo 8 va spento** (`verita_ft2.py` lo fa di default): il banco decodifica
tutti gli slot nello stesso processo, e un messaggio sentito in uno slot
diventa ipotesi negli altri, che qui sono scorrelati. Con il tipo 8 acceso
compaiono righe di slot sbagliati.

**Controllo della catena**: 12 slot generati con il rumore bianco del
generatore (`--snr-db -10`) danno 12 agganci su 12 e 12 decodifiche su 12,
errore di frequenza 0,0 Hz e di DT +0,024 s. Se questo controllo passa e la
misura sul rumore vero no, la differenza è la banda, non gli strumenti.

## Metodo di misura (perché esiste questo file)

1. **Verità nota.** Una registrazione off-air non dice quali messaggi ci
   fossero davvero, quindi non permette di calcolare il FER. Si registrano
   10-20 minuti di banda vera (12 kHz, mono, 16 bit, come WSJT-X) e ci si
   mescolano segnali FT2 **sintetici** con messaggio, SNR, frequenza e DT
   noti: una scala da -24 a -10 dB, ~200 frame per livello.
2. **Decodium produce gli LLR.** Si passano i WAV in replay con
   `DECODIUM_LLR_DUMP` acceso.
   Ordini di grandezza osservati: **da 100 a 210 record per slot** (un
   candidato ciascuno, decodificati e non), 735 byte l'uno, cioè ~150 kB per
   slot: dieci minuti di FT2 stanno in qualche decina di MB.
3. **Il confronto è offline**, sugli stessi candidati: FER per decodificatore
   e per livello di SNR, decodifiche false e accettazioni fuori lista, e
   soprattutto **l'aggancio del segnale** — quante volte il demodulatore ha
   trovato il candidato giusto, e con quale errore di DT e frequenza. È il
   dato che una simulazione pura non può dare.
