# Decodium 4 FT2 v1.0.598

La fase profonda di FT8 è **accesa di serie**. Nella v1.0.597 funzionava ma
restava spenta salvo impostare una variabile d'ambiente; ora parte da sola.

## Italiano

### Che cosa cambia

Una sola cosa, ma sostanziale: la soglia che governa il decode profondo di
recupero scende da 7000 a **2500 ms**, un valore che il budget reale raggiunge
(sul campo si osservano fra 5500 e 6150 ms). Da questa versione ogni slot FT8
riceve due passaggi a profondità 4 con decodifica a priori, invece di uno.

Tutto il resto è già nella v1.0.597, che ha reso possibile questo passo
correggendo il difetto che faceva cadere l'applicazione quando quello stadio
girava.

### Che cosa aspettarsi

**Stazioni più deboli.** Misurato su venti minuti di traffico reale: **8 stazioni
vere in più** su 375, fino a −24 dB, che senza la fase profonda si perdevano.
Circa il 2% in più — modesto ma reale, e prezioso sui DX marginali.

**Più carico.** Due decodifiche a profondità 4 per slot sono onerose anche per un
processore a 32 core. L'applicazione si autoprotegge e salta la fase profonda
negli slot senza margine: se nel registro vedi `cpuPressure=1` o
`cooldownActive=1` è il comportamento voluto, non un guasto.

**Nessun fantasma in più.** I due filtri introdotti nella v1.0.597 restano al
loro posto: la forma ITU del nominativo, applicata a ogni livello di segnale, e
la conferma sotto i −23 dB. In esercizio i nominativi visti una sola volta si
attestano al 25-30%, contro il 19% di riferimento senza fase profonda.

### Come rispegnerla

Se preferisci il comportamento precedente, la variabile
`DECODIUM_FT8_DEEP_MIN_BUDGET` con un valore sopra 6550 disattiva del tutto lo
stadio profondo. Non serve reinstallare nulla.

### Verificato

Avviata senza alcuna variabile: la fase profonda parte al primo minuto
(`depth=4 ft8ap=1`, budget 5842 ms contro la soglia di 2500), zero slot saltati.
La v1.0.597 aveva già dimostrato la tenuta: 23 minuti con 138 lanci consecutivi,
nessun crash, memoria stabile.

---

## English

### What changes

One thing, but a substantial one: the threshold governing the FT8 deep rescue
decode drops from 7000 to **2500 ms**, a value the real budget actually reaches
(5500–6150 ms observed on air). From this version every FT8 slot gets two
depth-4 passes with a-priori decoding instead of one.

Everything else already shipped in v1.0.597, which made this possible by fixing
the defect that crashed the application whenever that stage ran.

### What to expect

**Weaker stations.** Measured over twenty minutes of real traffic: **8 more
genuine stations** out of 375, down to −24 dB, that were lost without the deep
pass. About 2% more — modest but real, and valuable on marginal DX.

**Higher load.** Two depth-4 decodes per slot are demanding even on a 32-core
machine. The application protects itself and skips the deep pass in slots
without headroom: `cpuPressure=1` or `cooldownActive=1` in the log is by design,
not a fault.

**No extra phantoms.** The two filters introduced in v1.0.597 remain in place:
ITU callsign shape, applied at every signal level, and confirmation below
−23 dB. On air, callsigns seen only once settle at 25-30%, against the 19%
baseline without the deep pass.

### Turning it off

If you prefer the previous behaviour, the `DECODIUM_FT8_DEEP_MIN_BUDGET`
variable with a value above 6550 disables the deep stage entirely. No
reinstallation needed.

---

### Packaging

- The AVX2 decoder is selected at runtime, so these binaries work on CPUs
  without AVX2, where the original decoder is used instead.
- GitHub's generated source archives for tag `v1.0.598` are the codebase
  downloads for this release.
