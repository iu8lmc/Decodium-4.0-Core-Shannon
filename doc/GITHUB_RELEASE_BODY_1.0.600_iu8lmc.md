# Decodium 4 FT2 v1.0.600

Allineamento completo con upstream. Questa versione contiene il lavoro sul
decodificatore FT8 delle v1.0.597 e v1.0.598 — assorbito da upstream, che è
partito proprio da quella base — più tre aggiunte upstream, fra cui il
decodificatore vettorizzato su ARM.

Chi arriva dalla v1.0.596 trova qui tutto quanto segue.

## Italiano

### La fase profonda di FT8 è accesa e funziona (v1.0.597 e v1.0.598)

Era spenta dalla v1.0.595, perché riaccendendola l'applicazione moriva entro due
minuti con corruzione dello heap. **La causa non era il decodificatore**: era il
ridimensionamento del pool OpenMP. Il worker chiamava `omp_set_num_threads()` a
ogni decodifica con il valore calcolato di volta in volta (21, 24, 16), e con
libgomp ogni cambio distrugge e ricrea il pool. I thread che muoiono eseguono i
distruttori dei propri oggetti `thread_local` dentro `LdrShutdownThread`, quando
heap e loader sono già in smontaggio: da lì tre sintomi diversi dello stesso
difetto — corruzione silenziosa dello heap che esplodeva più tardi altrove,
segmentation fault dentro Qt su un contatore di riferimenti già liberato, e
stallo sul loader lock con l'applicazione viva ma ferma.

Il pool si dimensiona ora una volta sola, e il carico della singola passata si
applica con `num_threads` sulla regione parallela, che non lo ridimensiona.

| | prima | dopo |
|---|---|---|
| durata con fase profonda attiva | ~2 minuti | **oltre 23 minuti** |
| lanci consecutivi | 1-2, poi crash | **138** |
| memoria | in fuga (+80 MB/min) | **stabile** |

Dalla v1.0.598 lo stadio è attivo **di serie**: ogni slot FT8 riceve due
passaggi a profondità 4 con decodifica a priori.

### Soglia LDPC tarata per modo

Era 22 per tutti i modi: un valore scelto per FT2 contro i nominativi fantasma,
applicato anche a FT8 dove il problema è l'opposto. Ora **58 in FT8** — il limite
del decodificatore originale — e **22 in FT2**, invariato.

Banco appaiato, profondità 4 con a-priori, 180 file:

| | soglia 22 | soglia 58 |
|---|---|---|
| −21 dB | 66% | **80%** |
| −22 dB | 16% | **33%** |
| decodifiche corrette su 180 | 144 | **151** |
| falsi positivi | 10 | **8** |

Migliora su entrambi i fronti: più decodifiche *e* meno fantasmi.

### Filtri anti-fantasma

La fase profonda porta stazioni più deboli ma anche più falsi: a −25 dB la
CRC-14 ne lascia passare uno ogni 16384 per caso, e in aria i nominativi visti
una sola volta erano saliti dal 19% al 44%. Due criteri:

- **forma ITU del nominativo** (prefisso, un blocco di cifre, suffisso di sole
  lettere), applicata a ogni livello di segnale perché i falsi arrivavano fino a
  −3 dB. Misurata su 1.062.004 nominativi d'archivio: ne respinge 518, lo
  **0,049%**, tutti falsi. Passano i prefissi con cifra (`4L7T`, `9A6NTK`) e i
  nominativi speciali da evento (`EN35UKR`, `LZ123RF`);
- **conferma sotto i −23 dB**: a quel livello si accetta solo un nominativo già
  sentito con segnale affidabile nella sessione.

In esercizio i nominativi visti una sola volta tornano al 25-30%.

### Che cosa aggiunge, in concreto

Misurato su venti minuti di traffico reale: **8 stazioni vere in più** su 375
(circa il 2%), fino a −24 dB. Modesto ma reale, prezioso sui DX marginali.

### Novità upstream in questa versione

- **Decodificatore vettorizzato su ARM (NEON)**: il nucleo min-sum gira ora anche
  su ARM, non solo su x86 con AVX2. È la base per Apple Silicon e per il porting
  mobile.
- **Dispatch del decodificatore separato**: il rilevamento della CPU e la scelta
  fra AVX2, NEON e decodificatore originale sono ora un componente isolato con
  test propri.
- **Correzioni d'interfaccia**: la finestra di conferma QSO non rimpicciolisce
  più il pannello principale su Windows; il dialogo di riserva per il log segue
  la lingua scelta invece di uscire sempre in inglese; il selettore di frequenza
  di lavoro si sposta nella barra delle bande.
- Tenuta della build macOS Intel quando OpenMP non è disponibile.

### Avvertenza sul carico

Due decodifiche a profondità 4 per slot sono onerose anche per un processore a
32 core. L'applicazione si autoprotegge e salta la fase profonda negli slot
senza margine: `cpuPressure=1` o `cooldownActive=1` nel registro è comportamento
voluto. Per rispegnere del tutto lo stadio, la variabile
`DECODIUM_FT8_DEEP_MIN_BUDGET` con un valore sopra 6550.

---

## English

### The FT8 deep pass now works, and is on by default

It had been disabled since v1.0.595 because re-enabling it killed the
application within two minutes with heap corruption. **The decoder was not at
fault**: it was OpenMP pool resizing. The worker called `omp_set_num_threads()`
on every decode with a freshly computed value, and under libgomp every change
destroys and recreates the pool. Dying threads run their `thread_local`
destructors inside `LdrShutdownThread`, when heap and loader are already tearing
down — hence three different symptoms of one defect: silent heap corruption
surfacing later elsewhere, a segmentation fault inside Qt on an already-freed
reference count, and a loader-lock stall with the application alive but idle.

The pool is now sized once, and each pass applies its load with `num_threads` on
the parallel region. Verified: over 23 minutes with 138 consecutive dispatches,
no crashes, memory steady — against a crash after about two minutes before.

### Per-mode LDPC threshold

Formerly 22 for every mode — a value chosen for FT2 against phantom callsigns,
applied to FT8 as well where the problem is the opposite. Now **58 in FT8**, the
original decoder's own limit, and **22 in FT2**. Paired bench at depth 4 with
a-priori over 180 files: at −21 dB from 66% to **80%**, at −22 dB from 16% to
**33%**, correct decodes 144 → **151**, false positives 10 → **8**.

### Phantom filters

Two criteria: **ITU callsign shape** (prefix, one digit block, letters-only
suffix) applied at every signal level, measured on 1,062,004 archived callsigns
where it rejects 518 — **0.049%**, all false; and **confirmation below −23 dB**,
accepting a callsign only if already heard at a reliable level in the session.

### What it actually adds

Measured over twenty minutes of real traffic: **8 more genuine stations** out of
375, down to −24 dB.

### Upstream additions in this version

ARM NEON vectorised decoder, a separate decoder dispatch component with its own
tests, three UI fixes (QSO confirmation window no longer shrinking the dashboard
on Windows, logging fallback dialog following the selected language, compact
working-frequency selector moved to the band row), and macOS Intel build
resilience without OpenMP.

---

### Packaging

- The vectorised decoder is selected at runtime — AVX2 on x86, NEON on ARM,
  original decoder elsewhere — so these binaries work on any CPU.
- GitHub's generated source archives for tag `v1.0.600` are the codebase
  downloads for this release.
