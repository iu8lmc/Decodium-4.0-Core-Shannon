# Decodium 4 FT2 v1.0.594

This release fixes two independent faults that between them kept the FT8 deep
decode stage from ever running: an unreachable budget threshold, and ten
settings that were written to one place and read from another.

## English (British)

### v1.0.594: the FT8 deep stage finally runs

- Fixed an unreachable threshold that disabled the deep follow-up decode.
  Dispatching it required a remaining budget of at least 7000 ms, but the budget
  is `latestCompleteMs - now - 250` and `latestCompleteMs` is the end of the slot
  plus 6800 ms: the theoretical maximum is 6550 ms, even with instantaneous
  dispatch. The condition could never hold, so the deep pass was always
  discarded — and each discard was read as worker backlog, arming a six-slot
  cooldown. The net effect was that a-priori decoding and depth 4 never ran in
  FT8 at all. The threshold is now 2500 ms, which matches what a deep decode
  actually costs with the vectorised decoder; the original value was set for a
  decoder that took around ten seconds per slot.
- Fixed ten settings that were saved to the file's global section but read from
  the active profile's group, so they silently reverted to their defaults on
  every restart: `Ft8SubpassHarvest` (the GAL button), `Ft2AdaptiveDecode`,
  `Ft2ApHashCache`, `Ft2Conservative`, `Ft2FullDecodeInAutoCq`,
  `Ft2PartnerMemoryEnabled`, `Ft2QuickGiveUpStrong`, `MamMultiStream`,
  `MamMaxStreams` and `MamCqSlots`. Their setters now write into the active
  profile like every other setting, and reads go through `profiledSettingsValue`,
  which falls back to the global value when the key is not yet in the profile —
  so values already saved are recovered rather than lost.
- Together these restore all three FT8 decode stages per cycle: the fast pass,
  the deep pass with a-priori decoding at depth 4, and the low-threshold subpass
  harvest. Measured after the fix, a cycle runs depth 4 with AP producing 30
  decodes against the 16-20 of the fast pass alone, and the subpass stage
  dispatches normally. Before the fix neither had ever executed, with GAL
  switched on in the settings the whole time.

### Packaging and compatibility

- GitHub's generated source archives for tag `v1.0.594` are the codebase
  downloads for this release.
- Restoring the deep and harvest stages increases per-cycle decode work. It fits
  comfortably now that the vectorised decoder costs a fraction of the original
  one, but on a machine without AVX2, where the original decoder is used, the
  cycle will be busier than it was in previous releases.

## Italiano

### v1.0.594: lo stadio profondo di FT8 finalmente funziona

- Corretta una soglia irraggiungibile che disattivava il decode profondo di
  recupero. Per lanciarlo serviva un budget residuo di almeno 7000 ms, ma il
  budget è `latestCompleteMs - adesso - 250` e `latestCompleteMs` è la fine
  dello slot più 6800 ms: il massimo teorico è 6550 ms, anche con dispatch
  istantaneo. La condizione non poteva mai essere soddisfatta, quindi la passata
  profonda veniva sempre scartata — e ogni scarto veniva letto come sovraccarico
  del worker, armando un raffreddamento di sei slot. L'effetto netto era che
  decodifica a priori e profondità 4 non venivano mai eseguite in FT8. La soglia
  è ora 2500 ms, che corrisponde a quanto costa davvero un decode profondo con
  il decoder vettorizzato; il valore originale era tarato su un decoder che
  impiegava una decina di secondi per slot.
- Corrette dieci impostazioni che venivano salvate nella sezione generale del
  file ma lette dal gruppo del profilo attivo, quindi tornavano ai valori
  predefiniti a ogni riavvio senza dirlo: `Ft8SubpassHarvest` (il pulsante GAL),
  `Ft2AdaptiveDecode`, `Ft2ApHashCache`, `Ft2Conservative`,
  `Ft2FullDecodeInAutoCq`, `Ft2PartnerMemoryEnabled`, `Ft2QuickGiveUpStrong`,
  `MamMultiStream`, `MamMaxStreams` e `MamCqSlots`. I loro setter ora scrivono
  nel profilo attivo come tutte le altre impostazioni, e la lettura passa da
  `profiledSettingsValue`, che ricade sul valore globale quando la chiave non è
  ancora nel profilo: così i valori già salvati vengono recuperati invece di
  andare persi.
- Insieme, le due correzioni ripristinano tutti e tre gli stadi di decodifica
  FT8 per ciclo: la passata veloce, quella profonda con decodifica a priori a
  profondità 4, e l'harvest col subpass a soglia bassa. Misurato dopo la
  correzione, un ciclo esegue profondità 4 con AP producendo 30 decodifiche
  contro le 16-20 della sola passata veloce, e lo stadio subpass viene lanciato
  regolarmente. Prima nessuno dei due era mai stato eseguito, con GAL acceso
  nelle impostazioni per tutto il tempo.

### Packaging e compatibilità

- Gli archivi sorgente generati da GitHub per il tag `v1.0.594` costituiscono i
  download del codebase di questa release.
- Ripristinare gli stadi profondo e harvest aumenta il lavoro di decodifica per
  ciclo. Ci sta comodamente ora che il decoder vettorizzato costa una frazione
  dell'originale, ma su una macchina senza AVX2, dove viene usato il decoder
  originale, il ciclo sarà più carico rispetto alle release precedenti.
