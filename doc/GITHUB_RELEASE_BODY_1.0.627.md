# Decodium 4 v1.0.627

## English (UK)

v1.0.627 makes the FT8 sensitivity features **adaptive**. They are no longer either on or off for everyone: Decodium now measures how much of its own decoding budget each slot consumes, and grants them only where the machine has room to spare.

### Why

v1.0.625 and v1.0.626 enabled four experimental features by default: iterative BICM-ID demodulation, the coherent demodulation pass, a priori passes from previously heard stations, and energy-domain accumulation in the CQ history. Together they cost 30 to 50% more decoding time. On a capable machine that time is available. On a modest one it is the difference between finishing the slot and not finishing it, and on an 8-core computer under CPU pressure v1.0.626 crashed during its first deep decode.

### How it decides

Not by counting cores: a recent 8-core outperforms a five-year-old 16-core, and the workload also depends on how crowded the band is. The honest signal is the one already measured every slot — the fraction of the time budget actually consumed.

The rule is deliberately asymmetric:

- it **starts with the features off**, because the first deep slot is precisely the one that failed;
- it **enables** them after four consecutive slots below 50% of the budget with no CPU pressure;
- it **disables** them immediately on the first slot above 80%, or as soon as the decoder is throttled for CPU pressure.

Re-enabling costs a few slots; staying enabled when the slot cannot be completed costs decodes. Each change of state is written to the diagnostic log as a single `[LEVE]` line.

Setting any of the environment variables still overrides the automatic decision, in both directions:

    DECODIUM_FT8_BICM
    DECODIUM_FT8_COERENTE
    DECODIUM_FT8_AP_STORICO
    DECODIUM_FT8_STORICO_ENERGIA

### Also in this release

The v1.0.626 correction for non-standard callsigns, whose replies were discarded by the semantic decode filter in FT8, FT4 and FT2. The rule that recognises the type 4 message form has been moved into the shared pure-rules module and is now covered by unit tests, including cases that fail against both earlier, incorrect versions of the fix.

---

## Italiano

La 1.0.627 rende **adattive** le funzioni di sensibilità per FT8. Non sono più accese o spente per tutti: Decodium misura quanto del proprio budget di decodifica consuma ogni ciclo, e le concede solo dove la macchina ha margine.

### Perché

La 1.0.625 e la 1.0.626 accendevano di default quattro funzioni sperimentali: demodulazione iterativa BICM-ID, passata di demodulazione coerente, passate a priori dalle stazioni già sentite e accumulo a livello di energia nello storico dei CQ. Insieme costano dal 30 al 50% di tempo di decodifica in più. Su una macchina capace quel tempo c'è. Su una modesta è la differenza fra arrivare in fondo al ciclo e non arrivarci: su un computer a 8 core sotto pressione di CPU, la 1.0.626 è andata in crash durante la prima decodifica profonda.

### Come decide

Non contando i core: un otto-core recente batte un sedici-core di cinque anni fa, e il lavoro dipende anche da quanto è affollata la banda. Il segnale onesto è quello già misurato a ogni ciclo, cioè la frazione di budget di tempo realmente consumata.

La regola è asimmetrica di proposito:

- **parte con le funzioni spente**, perché il primo ciclo profondo è proprio quello che ha fallito;
- le **accende** dopo quattro cicli consecutivi sotto il 50% del budget e senza pressione di CPU;
- le **spegne subito** al primo ciclo oltre l'80%, oppure appena il decoder viene limitato per pressione di CPU.

Riaccendere costa qualche ciclo; restare accese quando il ciclo non si chiude costa decodifiche. Ogni cambio di stato viene scritto nel registro diagnostico come una singola riga `[LEVE]`.

Impostare una delle variabili d'ambiente scavalca ancora la decisione automatica, in entrambi i sensi:

    DECODIUM_FT8_BICM
    DECODIUM_FT8_COERENTE
    DECODIUM_FT8_AP_STORICO
    DECODIUM_FT8_STORICO_ENERGIA

### Contiene anche

La correzione della 1.0.626 per i nominativi non standard, le cui risposte venivano scartate dal filtro semantico delle decodifiche in FT8, FT4 e FT2. La regola che riconosce la forma del messaggio di tipo 4 è stata spostata nel modulo condiviso delle regole pure ed è ora coperta da test unitari, compresi casi che falliscono contro entrambe le versioni sbagliate della correzione.
