# Segnalazione regressione upstream 1.0.389 — deep follow-up FT8 "list-only" anche fuori TX

**Per:** Salvatore (elisir80)
**Da:** IU8LMC, audit statico 2026-06-10 su merge 1.0.389
**Severità:** P1 — l'auto-seq non vede i decode AP/depth-4 del partner

## Sintomo atteso on-air
"Il QSO FT8 non si chiude col partner debole": la sua reply arriva in decode-list
ma il sequencer non avanza, e si continua a ripetere CQ o il messaggio corrente.
È la riedizione del sintomo già fixato a maggio con decode-then-decide (deaab40).

## Meccanismo (riga per riga, DecodiumBridge.cpp post-1.0.389)

1. Il **fast pass** di fine slot gira a `depth ≤ 2` con `ft8ap=false`
   (`FT8 final fast pass: ... ft8ap=0`).
2. Il deep follow-up (depth 3-4 + AP) viene accodato in `m_ft8PendingDeepFollowups`
   e dispatchato a fine `onFt8DecodeReady` del fast serial, **sempre** con
   `listOnlyRows=true` → il serial finisce in `m_ft8DeepInTxSerials`.
3. In `onFt8DecodeReady` del deep serial, `ft8DeepInTxListOnly=true` disattiva
   `autoSeqActive`, il rescue-scan FT2, MAM ingest: le righe vanno SOLO in lista.
4. Risultato: la reply del partner debole decodificabile **solo** via AP
   (a-priori = esattamente il QSO in corso!) o depth 3-4 **non avanza mai il
   sequencer**. Prima della 1.0.389 il final pass fuori TX era unico (depth pieno
   + AP) e passava per il processing completo.

Aggravante di timing: il dispatch del deep avviene **dopo** `checkAndStartPeriodicTx()`;
se l'auto-seq fa partire il TX → `busyForTx` → deep skippato; inoltre con budget
residuo < 2400 ms (slot affollato) → skippato. In QSO attivo il deep spesso salta
del tutto.

## Fix proposto (applicato nel fork iu8lmc)

`listOnlyRows = pendingDeep.inTx` invece di `true` nel dispatch del deep
follow-up: list-only SOLO per il follow-up in TX (semantica 1.0.299), processing
completo fuori TX.

Sicurezza contro il doppio processing delle righe già decodificate dal fast:
`recentDecodeDedupKeys` viene ricostruito a ogni invocazione dagli ultimi 300
entry di `m_decodeList` (che al momento del deep contiene già le righe fast) →
le righe duplicate finiscono in `skippedDuplicateDecodeKeys` e il loop auto-seq
le salta esplicitamente (check su `autoSeqDedupKey`). Nessun doppio
`autoSequenceStep`, nessun doppio `checkAndStartPeriodicTx` su righe già viste.

## Note collaterali (non bloccanti)
- `kFt8Early41Ms` 12750→10900: l'early nzhsym=41 parte con ~0,9 s di segnale in
  meno di quanto i 41 hsym presuppongono; se è tuning voluto ok, segnalo solo.
- `seedFt8KnownCqCacheFromAllTxt` nel costruttore parsa ALL.TXT (fino a 8 MB,
  2 regex/riga) sincrono sul main thread: avvio percettibilmente più lento con
  ALL.TXT grandi.
