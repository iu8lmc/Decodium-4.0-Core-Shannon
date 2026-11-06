# Decodium 4 FT2 v1.0.596

Questa release chiude la serie v1.0.590 → v1.0.596, che ha riscritto il
decodificatore LDPC di FT2 e FT8. **Sostituisce la v1.0.595, che è stata
ritirata perché va in crash dopo circa due minuti di ricezione FT8.**

📖 **Cronologia completa con tutte le misure:**
[doc/fastldpc/CHANGELOG.md](https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/blob/v1.0.596/doc/fastldpc/CHANGELOG.md)

---

## Italiano

### Il nuovo decodificatore LDPC (fastldpc)

Il nucleo min-sum è stato riscritto con istruzioni AVX2 esplicite e lavora su
**sedici parole di codice per volta**. Il guadagno è di **velocità**, non di
sensibilità: a parità di ciò che cerca, trova le stesse stazioni molto più in
fretta.

| | originale | fastldpc |
|---|---|---|
| FT8, per file a −21 dB | 16128 ms | **785 ms** |

La scelta avviene **a runtime**: su CPU senza AVX2 si torna automaticamente al
decodificatore originale, quindi i binari pubblicati restano utilizzabili
ovunque.

### Dove la velocità diventa stazioni in più

Su 19 slot registrati fuori onda, la differenza si vede solo quando la banda è
piena e il decodificatore originale non fa in tempo ad arrivare in fondo alla
lista dei candidati:

| banda | originale | fastldpc |
|---|---|---|
| 40 m, affollata | 198 stazioni distinte in 306 s | **250 in 75 s** |
| 80 m, scarica | 56 distinte in 306 s | 52 in 61 s |

In banda scarica il tempo basta a entrambi, e la propagazione esatta batte
l'approssimazione min-sum sui segnali marginali. Per questo la v1.0.593 ha
introdotto la **passata di recupero**: fastldpc arriva in fondo alla lista, e il
tempo risparmiato si spende per un secondo tentativo col decodificatore esatto
sui candidati rimasti a vuoto. Recupera tre delle quattro stazioni marginali in
banda scarica e non toglie nulla al vantaggio in banda piena, restando tre volte
più rapido dell'originale.

### Correzione del segno nel min-sum

Il ramo min-sum produceva messaggi di segno opposto rispetto al ramo esatto.
È l'unico intervento della serie che aggiunge **sensibilità** anziché velocità:

| | decodifiche corrette su 2000 |
|---|---|
| senza la correzione | 1929 |
| con la correzione | **1986** (+3,0%) |

### Nominativi fantasma in FT2

Sono stati azzerati: da 2,8 per ciclo a zero, misurati sul traffico reale.
La ricerca OSD larga è stata provata due volte e ritirata due volte — prova
~21400 candidati per parola contro ~600, e la CRC-14 ne ammette uno ogni 16384.
Il filtro di plausibilità paga circa 2 bit, l'allargamento ne costa 5: non li
copre. Restano attivi il gate sui bit ribaltati, il controllo di coerenza con
l'ipotesi a priori e i controlli strutturali sul messaggio.

### Altre correzioni della serie

- **Clic sul waterfall**: cliccando il nominativo di una stazione ora la si
  chiama, come dalla lista dei decodificati.
- **Dieci impostazioni** che venivano scritte in un posto e lette da un altro,
  e quindi non avevano alcun effetto, ora funzionano.
- **Crash della v1.0.595**: quella release abbassava una soglia che teneva
  spento il decode profondo di recupero in FT8, riattivando un difetto di
  memoria latente in quel percorso (corruzione dello heap, `0xc0000374`, entro
  circa due minuti di ricezione). La soglia è stata riportata al valore
  precedente. Il difetto sottostante resta aperto ed è documentato nel
  changelog per chi vorrà indagarlo.

### Interruttori a runtime

Il comportamento si può cambiare senza ricompilare — l'elenco completo è nel
changelog. I principali:

| variabile | effetto |
|---|---|
| `DECODIUM_FT8_FASTLDPC=0` | torna al decodificatore originale in FT8 |
| `DECODIUM_FT2_DISABLE_FASTLDPC=1` | idem per FT2 |
| `DECODIUM_FT8_CLASSIC_RESCUE` | numero di recuperi per ciclo (0 disattiva) |

### Avvertenza sulle misure

I confronti su finestre temporali diverse non sono affidabili sotto il 10%: la
banda cambia da sola più di quanto cambi il decodificatore. Tutti i numeri qui
sopra vengono da confronti appaiati sugli stessi segnali. Tre miglioramenti
apparenti sono stati attribuiti per errore a modifiche che si sono poi rivelate
ininfluenti, e il changelog li documenta insieme alle tre strade misurate e
abbandonate.

---

## English

### The new LDPC decoder (fastldpc)

The min-sum core has been rewritten with explicit AVX2 intrinsics and processes
**sixteen codewords at a time**. The gain is in **speed**, not sensitivity: for
the same search, it finds the same stations far faster (16128 ms → **785 ms**
per FT8 file at −21 dB). Selection happens **at runtime**, so the published
binaries remain usable on CPUs without AVX2, where the original decoder is used.

### Where speed becomes extra stations

Across 19 off-air recorded slots, the difference appears only when the band is
busy and the original decoder cannot reach the end of the candidate list in
time: on a crowded 40 m, 198 distinct stations in 306 s against **250 in 75 s**.
On a quiet 80 m both have enough time, and exact propagation beats the min-sum
approximation on marginal signals (56 against 52). Hence the **rescue pass**
added in v1.0.593: fastldpc reaches the end of the list, and the time saved is
spent on a second attempt with the exact decoder over the candidates that came
up empty. It recovers three of the four marginal stations on a quiet band while
keeping the advantage on a busy one, and remains three times faster than the
original.

### Sign correction in the min-sum branch

The min-sum branch produced messages of opposite sign to the exact branch. This
is the only change in the series that adds **sensitivity** rather than speed:
1929 → **1986** correct decodes out of 2000 (+3.0%), paired comparison.

### Phantom callsigns in FT2

Reduced to zero, from 2.8 per cycle, measured on live traffic. The wide OSD
search was tried twice and withdrawn twice: it tries ~21400 candidates per word
against ~600, and CRC-14 admits one in 16384. The plausibility filter is worth
about 2 bits while widening costs 5 — it does not cover them.

### Other fixes in the series

- **Waterfall click**: clicking a station's callsign now calls it.
- **Ten settings** that were written to one place and read from another, and so
  had no effect, now work.
- **v1.0.595 crash**: that release lowered a threshold which had been keeping
  the FT8 deep rescue decode switched off, re-enabling a latent memory fault in
  that path (heap corruption, `0xc0000374`, within about two minutes of
  reception). The threshold has been restored. The underlying fault remains open
  and is documented in the changelog.

### A note on measurement

Comparisons across different time windows are unreliable below 10%: the band
changes more on its own than the decoder does. Every number above comes from
paired comparisons on identical signals. Three apparent improvements were
mistakenly credited to changes that later proved to make no difference; the
changelog records them, along with three measured dead ends.

---

### Packaging

- The AVX2 decoder is selected at runtime, so these binaries work on CPUs
  without AVX2, where the original decoder is used instead.
- GitHub's generated source archives for tag `v1.0.596` are the codebase
  downloads for this release.
