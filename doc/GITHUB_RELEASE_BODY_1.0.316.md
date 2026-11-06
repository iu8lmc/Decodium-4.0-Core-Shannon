# Decodium 1.0.316 — Hotfix `ftxImmediateClickTx` cap FT8/FT4

Release **fork-only iu8lmc**. Non e' presente in elisir80/Decodium-4.0-Core-Shannon.

---

## Problema risolto

Il toggle opt-in **TX immediato al click** (`ftxImmediateClickTx`, default OFF) introdotto in 1.0.314 aveva un cap troppo stretto su FT8/FT4: 2000 ms dall'inizio dello slot.

Su FT8 (slot 15 s) l'utente clicca tipicamente 2–5 s dopo l'inizio del periodo TX: con cap 2000 ms, il click cadeva quasi sempre nel defer — comportamento identico al toggle OFF. Il toggle risultava quindi inutile in pratica.

## Fix

`latestD3CompatibleSyncTxStartMs()` in `DecodiumBridge.cpp` ~riga 4013:

- **Prima (1.0.314):** cap `immediateClickTx=true` fissato a `2000 ms`
- **Dopo (1.0.316):** cap `immediateClickTx=true` = `d3CapMs` = 75% slot
  - FT8: ~11.25 s
  - FT4: ~5.6 s
  - = comportamento reale 1.0.283

Il toggle resta **opt-in, default OFF**. Con toggle OFF il cap FT8/FT4 rimane al valore strict upstream (650 ms circa). FT2 e' invariato (la finestra e' limitata fisicamente dal payload audio, non rilassabile).

## Componenti invariati rispetto a 1.0.315

- FT2 sequencer, adaptive decode, AP cache
- Signoff retry cap FT4/FT8 (ft4Cap=4, ft8Cap=3, ft2Cap=4)
- Tutti i toggle opt-in delle release precedenti

---

*Build: MSYS2 MinGW-w64 / Qt 6.11.0 — Windows 64-bit*
