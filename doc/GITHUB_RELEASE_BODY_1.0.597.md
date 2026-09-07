# Decodium 4 FT2 v1.0.597

Sblocca la fase profonda di FT8, spenta dalla v1.0.595 perché faceva cadere
l'applicazione, e alza la soglia del decodificatore LDPC dove serviva davvero.

## Italiano

### La fase profonda di FT8 torna utilizzabile

Dalla v1.0.595 il decode profondo di recupero era tenuto spento da una soglia
irraggiungibile, perché riattivandolo l'applicazione moriva entro due minuti con
corruzione dello heap. La causa non era il decodificatore.

**Era il ridimensionamento del pool OpenMP.** Il worker chiamava
`omp_set_num_threads()` a ogni decodifica con il valore calcolato di volta in
volta (21, 24, 16). Con libgomp ogni cambio distrugge e ricrea il pool, e i
thread che muoiono fanno girare i distruttori dei propri oggetti `thread_local`
dentro `LdrShutdownThread`, quando heap e loader sono già in smontaggio. Da lì
tre sintomi diversi dello stesso difetto, tutti osservati durante l'indagine:

- corruzione silenziosa dello heap, che esplodeva più tardi in una `free`
  qualsiasi (`0xc0000374`, tipicamente sul buffer FFT del downsample — la
  vittima, non la causa);
- segmentation fault dentro Qt su un contatore di riferimenti già liberato,
  visibile solo con PageHeap attivo;
- stallo sul loader lock, con l'applicazione viva ma ferma a decodificare.

La fase profonda non causava nulla: raddoppiando le decodifiche raddoppiava il
ricambio di thread, e rendeva sistematico un difetto altrimenti raro.

**Correzione**: il pool si dimensiona una volta sola, e il carico richiesto
dalla singola passata si applica con `num_threads` sulla regione parallela, che
non lo ridimensiona.

| | prima | dopo |
|---|---|---|
| durata con fase profonda attiva | ~2 minuti | **oltre 23 minuti, senza cadute** |
| lanci della fase profonda | 1-2, poi crash | **138 consecutivi** |
| memoria | in fuga (+80 MB/min) | **stabile a ~935 MB** |

### Soglia LDPC tarata per modo

La soglia sui bit ribaltati era **22 per tutti i modi**: un valore scelto per
FT2 contro i nominativi fantasma, applicato anche a FT8 dove il problema è
l'opposto. Ora è **58 in FT8** — il limite del decodificatore originale — e
resta **22 in FT2**.

Banco appaiato, profondità 4 con a-priori, 180 file:

| | soglia 22 | soglia 58 |
|---|---|---|
| −21 dB | 66% | **80%** |
| −22 dB | 16% | **33%** |
| decodifiche corrette su 180 | 144 | **151** |
| falsi positivi | 10 | **8** |

Migliora su entrambi i fronti: più decodifiche *e* meno fantasmi. Dove la soglia
stretta non decodificava nulla, il decodificatore ripiegava sul nominativo
imparato prima e produceva un falso; con 58 trova quello vero.

### Filtri anti-fantasma

La fase profonda porta stazioni più deboli ma anche più falsi: a −25 dB la
CRC-14 ne lascia passare uno ogni 16384 per puro caso, e in aria i nominativi
visti una sola volta erano saliti dal 19% al 44%. Due criteri nuovi:

- **forma ITU del nominativo** — prefisso, un blocco di cifre, suffisso di sole
  lettere — applicata a ogni livello di segnale, perché i falsi arrivavano fino
  a −3 dB. Misurata su 1.062.004 nominativi d'archivio: ne respinge 518, lo
  **0,049%**, e sono tutti falsi. Passano i prefissi con cifra (`4L7T`,
  `9A6NTK`, `3B8GL`) e i nominativi speciali da evento (`EN35UKR`, `LZ123RF`);
- **conferma sotto i −23 dB**: a quel livello si accetta solo un nominativo già
  sentito con segnale affidabile nella sessione.

In esercizio i nominativi visti una sola volta tornano al 25-30%, contro il 19%
di riferimento senza fase profonda, e le forme impossibili spariscono del tutto.

### Che cosa aggiunge, in concreto

Misurato su venti minuti di traffico reale: **8 stazioni vere in più** su 375
(circa il 2%), fino a −24 dB, che senza la fase profonda si perdevano. Un
guadagno modesto ma reale, prezioso su DX marginali.

### Avvertenza sul carico

Due decodifiche a profondità 4 per slot sono onerose anche per un processore a
32 core. Il sistema si autoprotegge e salta la fase profonda negli slot senza
margine: è un comportamento voluto, non un difetto.

### Strumenti per chi sviluppa

- `tests/thread_churn_downsample.cpp` riproduce il ricambio di thread senza
  interfaccia né radio, così gli strumenti di analisi memoria girano in secondi.
- `DECODIUM_KEEP_SYMBOLS=ON` in CMake costruisce senza strip quando servono gli
  stack; spenta per default, i binari pubblicati restano identici.

---

## English

### The FT8 deep pass is usable again

Since v1.0.595 the deep rescue decode had been kept off by an unreachable
threshold, because re-enabling it killed the application within two minutes with
heap corruption. The decoder was not at fault.

**It was OpenMP pool resizing.** The worker called `omp_set_num_threads()` on
every decode with a freshly computed value (21, 24, 16). Under libgomp every
change destroys and recreates the pool, and dying threads run their
`thread_local` destructors inside `LdrShutdownThread`, when heap and loader are
already tearing down. Hence three different symptoms of one defect, all observed:
silent heap corruption surfacing later in an unrelated `free` (`0xc0000374`, on
the downsample FFT buffer — the victim, not the cause); a segmentation fault
inside Qt on an already-freed reference count, visible only under PageHeap; and a
loader-lock stall with the application alive but no longer decoding.

The deep pass caused none of it: by doubling the decodes it doubled thread
churn, turning a rare defect into a systematic one.

**Fix**: the pool is sized once, and each pass applies its requested load with
`num_threads` on the parallel region, which does not resize it. Verified on air:
over 23 minutes with the deep pass on every slot, 138 dispatches, no crashes,
memory steady at ~935 MB — against a crash after about two minutes before.

### Per-mode LDPC threshold

The hard-error threshold was **22 for every mode**, a value chosen for FT2
against phantom callsigns and applied to FT8 as well, where the problem is the
opposite. It is now **58 in FT8** — the original decoder's own limit — and stays
**22 in FT2**. Paired bench, depth 4 with a-priori, 180 files: at −21 dB from
66% to **80%**, at −22 dB from 16% to **33%**, correct decodes 144 → **151**,
false positives 10 → **8**. It improves on both counts.

### Phantom filters

The deep pass brings weaker stations but also more false decodes: at −25 dB
CRC-14 admits one in 16384 by chance. Two new criteria: **ITU callsign shape**
(prefix, one digit block, letters-only suffix) applied at every signal level,
measured on 1,062,004 archived callsigns where it rejects 518 — **0.049%**, all
false; and **confirmation below −23 dB**, where a callsign is accepted only if
already heard at a reliable signal level in the session.

### What it actually adds

Measured over twenty minutes of real traffic: **8 more genuine stations** out of
375 (about 2%), down to −24 dB, that would otherwise be lost.

### Load caveat

Two depth-4 decodes per slot are demanding even on a 32-core machine. The
application protects itself and skips the deep pass in slots without headroom —
by design, not a fault.

---

### Packaging

- The AVX2 decoder is selected at runtime, so these binaries work on CPUs
  without AVX2, where the original decoder is used instead.
- GitHub's generated source archives for tag `v1.0.597` are the codebase
  downloads for this release.
