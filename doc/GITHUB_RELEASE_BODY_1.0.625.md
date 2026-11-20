# Decodium 4 v1.0.625

## English (UK)

v1.0.625 enables four experimental FT8 sensitivity features by default. They were previously present but switched off, and each can still be switched off individually.

### What changes when decoding FT8

- **Iterative demodulation (BICM-ID).** After a decoding attempt fails, the extrinsic information from the min-sum decoder is fed back to the demodulator, which uses it to weight the tone hypotheses for the other bits of the same symbol group, and the attempt is repeated once.
- **Coherent demodulation pass.** The channel phase is estimated from the known Costas synchronisation symbols instead of being ignored, and the resulting metrics are tried as an additional pass alongside the existing blind ones.
- **A priori passes from previously heard stations** are now enabled by default.
- **Energy-domain accumulation in the CQ history** is now enabled by default.

### What this means in practice

More decodes near the noise floor, at the cost of roughly a third more decoding time per slot. On this station's hardware the decoding budget is not exceeded and no decode is truncated, but on a slower computer the margin is smaller.

### Measurements, and their limits

On synthetic benchmarks the iterative loop adds 21.3% more decodes, which corresponds to about 0.3 dB. On 170 recorded real-world slots degraded by 12 dB it adds 14 genuine decodes and 4 spurious ones, the latter all below -23 dB. Measured on air against JTDX, WSJT-X and MSHV listening to the same audio, the three features together produced 16% more decodes than JTDX over five consecutive fifteen-minute windows, with 42-50% of decodes at -16 dB or weaker against 30% for the other programs.

The coherent pass was measured separately. On air it raises the ratio against JTDX from 1.17 to 1.50, and the additional decodes are not spurious: 81-84% of them carry callsigns that reappear in other time slots, and a random word passing the CRC-14 cannot repeat. Against that, on degraded recordings of real signals the same pass produced no gain at all, and that measurement stands unexplained. What remains genuinely suspect is smaller: with the coherent pass enabled, roughly 60 callsigns per fifteen-minute window appear only once, against 0-3 without it.

These figures come from one station across two days and three bands. The relative contribution of each feature has not been separated, and a same-band reference measurement with everything disabled is still missing.

### Switching them off

Set any of these environment variables to `0` before starting Decodium:

    DECODIUM_FT8_BICM=0
    DECODIUM_FT8_COERENTE=0
    DECODIUM_FT8_AP_STORICO=0
    DECODIUM_FT8_STORICO_ENERGIA=0

---

## Italiano

La 1.0.625 accende in modo predefinito quattro funzioni sperimentali di sensibilità per FT8. Erano già presenti ma spente, e ognuna si può ancora spegnere singolarmente.

### Cosa cambia nella decodifica FT8

- **Demodulazione iterativa (BICM-ID).** Dopo un tentativo di decodifica fallito, l'informazione estrinseca del decodificatore min-sum torna al demodulatore, che la usa per pesare le ipotesi di tono degli altri bit dello stesso gruppo di simboli, e il tentativo si ripete una volta.
- **Passata di demodulazione coerente.** La fase del canale viene stimata dai simboli di sincronismo Costas, che sono noti, invece di essere ignorata; le metriche che ne derivano si provano come passata aggiuntiva accanto a quelle cieche già presenti.
- **Passate a priori dalle stazioni già sentite**, ora attive di default.
- **Accumulo a livello di energia nello storico dei CQ**, ora attivo di default.

### Cosa comporta in pratica

Più decodifiche vicino al livello del rumore, al prezzo di circa un terzo di tempo di decodifica in più per ciclo. Sull'hardware di questa stazione il budget di decodifica non viene superato e nessuna decodifica risulta troncata, ma su un computer più lento il margine è minore.

### Le misure, e i loro limiti

Sui banchi sintetici l'anello iterativo aggiunge il 21,3% di decodifiche, pari a circa 0,3 dB. Su 170 slot registrati in aria e degradati di 12 dB aggiunge 14 decodifiche vere e 4 spurie, queste ultime tutte sotto i -23 dB. Misurate in aria contro JTDX, WSJT-X e MSHV in ascolto sullo stesso audio, le tre funzioni insieme hanno prodotto il 16% di decodifiche in più rispetto a JTDX su cinque finestre consecutive da quindici minuti, con il 42-50% delle decodifiche a -16 dB o più deboli contro il 30% degli altri programmi.

La passata coerente è stata misurata a parte. In aria porta il rapporto con JTDX da 1,17 a 1,50, e le decodifiche che aggiunge non sono spurie: l'81-84% porta nominativi che ricompaiono in altri intervalli di tempo, e una parola casuale che supera la CRC-14 non può ripetersi. Di contro, su registrazioni degradate di segnali veri la stessa passata non ha prodotto alcun guadagno, e quella misura resta senza spiegazione. Ciò che rimane davvero sospetto è più piccolo: con la passata coerente attiva compaiono circa 60 nominativi per finestra di quindici minuti che si vedono una volta sola, contro 0-3 senza.

Questi numeri vengono da una sola stazione, due giornate e tre bande. Il contributo delle singole funzioni non è stato separato, e manca una misura di riferimento sulla stessa banda con tutto spento.

### Come spegnerle

Impostare a `0` una qualsiasi di queste variabili d'ambiente prima di avviare Decodium:

    DECODIUM_FT8_BICM=0
    DECODIUM_FT8_COERENTE=0
    DECODIUM_FT8_AP_STORICO=0
    DECODIUM_FT8_STORICO_ENERGIA=0
