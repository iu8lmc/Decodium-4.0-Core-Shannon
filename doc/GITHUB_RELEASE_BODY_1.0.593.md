# Decodium 4 FT2 v1.0.593

This release adds a second recovery pass with the original decoder, so FT8 gets
the coverage of the vectorised decoder on a busy band and the precision of exact
belief propagation on marginal signals.

## English (British)

### v1.0.593: dual-pass FT8 decoding

- Added a recovery pass with the original decoder. When the vectorised decoder
  leaves a candidate without a decode and time remains in the cycle, that
  candidate is decoded again with exact belief propagation. The two decoders do
  not win under the same conditions, measured on off-air recordings of 19 slots
  per band: on a busy 40 m the vectorised one finds 250 distinct messages
  against 198 for the original, because the original takes about 16 seconds per
  slot against an 8 second deadline and is cut off partway through the candidate
  list; on a quiet 80 m the original finds four more instead, all genuine
  stations, because there is time for both and exact propagation beats the
  min-sum approximation on marginal signals.
- There is no way to choose in advance which decoder suits the band. It depends
  on how many stations there are to find, which is not known before looking for
  them, and the candidate count does not discriminate: the median is 717 on a
  busy 40 m against 751 on a quiet 80 m, because a quiet band makes the sync
  search latch onto noise. Running both is what resolves it.
- Measured with the recovery pass in place: on 80 m it recovers three of the
  four marginal stations the min-sum missed, bringing the totals level with the
  original decoder (145 decodes) in 91 seconds against 306; on 40 m the large
  advantage is untouched (250 distinct messages) with three more decodes. The
  cost is 30-50% more time than the vectorised decoder alone, still three times
  faster than the original.
- The number of recoveries per cycle is capped, since each one costs as much as
  a whole slot of the slow decoder and a band full of sterile candidates would
  otherwise consume the margin. `DECODIUM_FT8_CLASSIC_RESCUE` sets the cap, 0
  disables recovery.
- Measured live on a busy 40 m over 16 minutes: median 708 ms per cycle against
  the 15 seconds available, 1543 decodes from 133 distinct callsigns, and 42
  decodes below -20 dB with a floor of -23 dB, including intercontinental
  traffic at the limit of the mode.

### Packaging and compatibility

- GitHub's generated source archives for tag `v1.0.593` are the codebase
  downloads for this release.
- The AVX2 decoder is selected at runtime, so the published binaries remain
  usable on CPUs without AVX2, where the original decoder is used instead.
- Measurements come from off-air recordings on 20, 40 and 80 metres plus one
  live session. They have not been repeated across a full range of propagation
  conditions.

## Italiano

### v1.0.593: decodifica FT8 a doppia passata

- Aggiunta una passata di recupero con il decoder originale. Quando il decoder
  vettorizzato lascia un candidato senza decodifica e nel ciclo resta tempo,
  quel candidato viene ridecodificato con la propagazione esatta. I due decoder
  non vincono nelle stesse condizioni, misurato su registrazioni off-air di 19
  slot per banda: in 40 metri con banda piena il vettorizzato trova 250 messaggi
  distinti contro 198 dell'originale, perché l'originale impiega circa 16
  secondi per slot contro una scadenza di 8 e viene troncato a metà della lista
  dei candidati; in 80 metri con banda scarica è l'originale a trovarne quattro
  in più, tutte stazioni autentiche, perché lì il tempo basta a entrambi e la
  propagazione esatta batte l'approssimazione min-sum sui segnali marginali.
- Non c'è modo di scegliere in anticipo quale decoder convenga: dipende da
  quante stazioni ci sono da trovare, cosa che non si sa prima di cercarle, e il
  numero di candidati non discrimina — mediana 717 in 40 metri affollati contro
  751 in 80 metri scarichi, perché su banda vuota la ricerca del sync aggancia
  rumore. Usarli entrambi è ciò che risolve il problema.
- Misurato con il recupero attivo: in 80 metri recupera tre delle quattro
  stazioni marginali perse dal min-sum, riportando i totali al livello del
  decoder originale (145 decodifiche) in 91 secondi contro 306; in 40 metri il
  vantaggio grosso resta intatto (250 messaggi distinti) con tre decodifiche in
  più. Il costo è il 30-50% di tempo in più rispetto al solo vettorizzato,
  restando tre volte più rapido dell'originale.
- Il numero di recuperi per ciclo ha un tetto, perché ognuno costa come un
  intero slot del decoder lento e una banda piena di candidati sterili se li
  mangerebbe tutti. `DECODIUM_FT8_CLASSIC_RESCUE` imposta il tetto, 0 disattiva
  il recupero.
- Misurato dal vivo in 40 metri affollati per 16 minuti: mediana 708 ms per
  ciclo contro i 15 secondi disponibili, 1543 decodifiche da 133 nominativi
  distinti, e 42 decodifiche sotto -20 dB con un minimo di -23 dB, incluso
  traffico intercontinentale al limite del modo.

### Packaging e compatibilità

- Gli archivi sorgente generati da GitHub per il tag `v1.0.593` costituiscono i
  download del codebase di questa release.
- Il decoder AVX2 viene scelto a runtime, quindi i binari pubblicati restano
  utilizzabili su CPU senza AVX2, dove viene usato il decoder originale.
- Le misure vengono da registrazioni off-air in 20, 40 e 80 metri più una
  sessione dal vivo. Non sono state ripetute su tutta la gamma delle condizioni
  di propagazione.
