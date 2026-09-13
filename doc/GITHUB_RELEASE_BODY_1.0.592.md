# Decodium 4 FT2 v1.0.592

This release converts the FT8 decode passes to batch decoding, which is what
makes the vectorised decoder pay off on a busy band.

## English (British)

### v1.0.592: FT8 decode passes in batches

- Converted the FT8 decode passes to batch decoding. The vectorised decoder
  works on sixteen words per register lane; decoding one pass at a time left
  fifteen lanes idle, which is why v1.0.591 measured FT8 slower per pass than
  the original decoder. The passes of an attempt are now prepared in advance —
  preparing one does not depend on how the others turn out — grouped by their
  `(Keff, maxosd, norder)` triple, since the batch call accepts only one, and
  decoded a group at a time. The surrounding loop is unchanged: it uses a result
  when one is ready and decodes as before otherwise, and the retries that depend
  on the outcome stay individual.
- Verified identical output: on the same generated waveforms the batch and
  per-pass paths produce the same decodes, with no discordances and identical
  thresholds. `DECODIUM_FT8_BATCH=0` returns to per-pass decoding.
- Measured on 19 slots recorded off air on 40 m, where the band was busy, the
  vectorised decoder finds **250 distinct messages against 198** for the
  original one, in 75 seconds against 306. The gain is not better decoding but
  finishing in time: the original decoder takes about 16 seconds per slot
  against a deadline of 8, so it is cut off partway through the candidate list
  and loses what it has not reached. On a quiet band (80 m, 52 decodes) the
  original one instead finds four more, all genuine, because there is time for
  everyone and its exact belief propagation beats the min-sum approximation on
  marginal signals.
- Tried and withdrawn: lowering the sync threshold that decides which signals
  become candidates. Across three windows of live traffic it appeared to be
  worth 10% more decodes, but measured on identical recorded slots, thresholds
  6, 3 and 1 give exactly the same result while costing up to 15% more time. The
  apparent gain was the band changing between windows. Raising the threshold
  instead costs nothing and saves a great deal: on 20 m, 40 m and 80 m a scale
  of 1.5 loses no decodes and saves between 32% and 60% of the time, because the
  decoder examines around 740 candidates per pass while the stations sit in the
  first 150 or so. The scale is available through `DECODIUM_FT8_SYNCMIN_SCALE`
  and is left at its previous default pending wider verification.
- Added `DECODIUM_FT8_CAND_LOG=1`, which reports the effective sync threshold
  and the number of candidates per pass.

### Packaging and compatibility

- GitHub's generated source archives for tag `v1.0.592` are the codebase
  downloads for this release.
- The AVX2 decoder is selected at runtime, so the published binaries remain
  usable on CPUs without AVX2, where the original decoder is used instead.
- Measurements come from three off-air recordings on 20, 40 and 80 metres. They
  have not been repeated across a full range of propagation conditions, and the
  balance between the two decoders depends on how many stations there are to
  find, which cannot be known before looking for them.

## Italiano

### v1.0.592: passate di decodifica FT8 a blocchi

- Convertite a blocchi le passate di decodifica FT8. Il decoder vettorizzato
  lavora su sedici parole per corsia del registro; decodificando una passata
  alla volta quindici corsie restavano ferme, ed è il motivo per cui nella
  v1.0.591 FT8 risultava più lento per passata del decoder originale. Le passate
  di un tentativo vengono ora preparate in anticipo — prepararne una non dipende
  dall'esito delle altre — raggruppate per terna `(Keff, maxosd, norder)`,
  perché la chiamata a blocchi ne accetta una sola, e decodificate un gruppo per
  volta. Il ciclo attorno è rimasto identico: usa il risultato quando è pronto e
  altrimenti decodifica come prima, e i tentativi di recupero che dipendono
  dall'esito restano singoli.
- Verificata l'identità del risultato: sugli stessi segnali generati la via a
  blocchi e quella per passata producono le stesse decodifiche, senza
  discordanze e con soglie identiche. `DECODIUM_FT8_BATCH=0` torna alla
  decodifica per passata.
- Misurato su 19 slot registrati dall'aria in 40 metri, con banda affollata, il
  decoder vettorizzato trova **250 messaggi distinti contro 198** dell'originale,
  in 75 secondi contro 306. Il guadagno non è decodificare meglio ma arrivare in
  fondo: l'originale impiega circa 16 secondi per slot contro un limite di 8,
  quindi viene interrotto a metà della lista dei candidati e perde quelli che non
  ha raggiunto. Su banda scarica (80 metri, 52 decodifiche) è invece l'originale
  a trovarne quattro in più, tutte autentiche, perché lì il tempo basta a
  entrambi e la sua propagazione esatta batte l'approssimazione min-sum sui
  segnali marginali.
- Provato e ritirato: abbassare la soglia di aggancio che decide quali segnali
  diventano candidati. Su tre finestre di traffico reale sembrava valere il 10%
  di decodifiche in più, ma misurata sugli stessi identici slot registrati, le
  soglie 6, 3 e 1 danno esattamente lo stesso risultato e costano fino al 15% di
  tempo in più. Il guadagno apparente era la banda che cambiava fra una finestra
  e l'altra. Alzarla invece non costa nulla e fa risparmiare molto: in 20, 40 e
  80 metri una scala 1,5 non perde decodifiche e fa risparmiare fra il 32% e il
  60% del tempo, perché il decoder esamina circa 740 candidati per passata
  mentre le stazioni stanno nei primi 150 scarsi. La scala resta disponibile con
  `DECODIUM_FT8_SYNCMIN_SCALE` ed è lasciata al valore di prima in attesa di una
  verifica più ampia.
- Aggiunto `DECODIUM_FT8_CAND_LOG=1`, che riporta la soglia di aggancio
  effettiva e il numero di candidati per passata.

### Packaging e compatibilità

- Gli archivi sorgente generati da GitHub per il tag `v1.0.592` costituiscono i
  download del codebase di questa release.
- Il decoder AVX2 viene scelto a runtime, quindi i binari pubblicati restano
  utilizzabili su CPU senza AVX2, dove viene usato il decoder originale.
- Le misure vengono da tre registrazioni off-air in 20, 40 e 80 metri. Non sono
  state ripetute su tutta la gamma delle condizioni di propagazione, e
  l'equilibrio fra i due decoder dipende da quante stazioni ci sono da trovare,
  cosa che non si sa prima di averle cercate.
