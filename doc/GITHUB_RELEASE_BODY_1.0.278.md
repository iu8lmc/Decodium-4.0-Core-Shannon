# Decodium 4 FT2 1.0.278

This release consolidates the weak-signal and live-decode work done after 1.0.274. The main goal is to make FT8 in Decodium4 behave closer to Decodium3 and the established WSJT-X/JTDX family under real traffic: stable SNR reporting, no live FT8 backlog, and retained very-deep hinted FT8 recovery without broad ghost-call risk.

## FT8 Live Decode Stability

- Fixes the embedded single-thread FT8 live path so it waits for the full slot instead of launching partial early decodes in the Decodium4 UI runtime.
- Preserves a complete FT8 slot when the worker is still busy at the next boundary by queueing one full-slot request instead of dropping the decode.
- Adds focused FT8 worker debug lines for start, queued slot, elapsed time, depth, sample count, and row count.
- Corrects `NDepth` handling in the C++ FT8 worker. `NDepth` is a legacy bit mask: the low bits contain the base decode depth, while upper bits enable averaging/correlation/AP options. Values such as `51` now resolve to base depth `3` instead of being clamped to depth `4`.
- Avoids forcing the slowest FT8 supplemental/OSD path on every live slot. This removes the 12-20 second backlog behaviour seen during live FT8 comparison.
- Live comparison against Decodium3 after the fix showed D4 decoding more rows in the same FT8 window, with matched SNR essentially aligned: mean D4-D3 about `+0.39 dB`, median `0 dB`.

## FT8 Very-Deep Hinted Recovery

- Extends the retained FT8 hint path for very weak repeated messages when reliable context already exists.
- Adds strict context guards: same current DX call when a QSO partner is known, same recent repeated message, compatible frequency and DT, and standard FT8 message format.
- Adds hinted candidate injection only inside the narrow retained-message window, instead of lowering thresholds across the whole waterfall.
- Keeps directed exchanges from being revived without a current partner context; without a partner, only safe CQ-like retained hints are eligible.
- Adds hard-error, LLR-distance, tone-energy, and sync guards before a hinted very-deep decode is accepted.
- Keeps the normal FT8 path unchanged for normal-depth live decode; the expensive hinted path remains restricted to the very-deep profile.

## Weak-Signal Regression Tests

- Adds a synthetic FT8 weak decode regression at `-26 dB`.
- Adds a synthetic FT8 hinted very-deep regression at `-27 dB`.
- Re-ran the FT8 weak/deep tests after the live-depth fix:
  - `test_ft8_weak_decode_deep`: passed
  - `test_ft8_hinted_decode_very_deep`: passed

## RX Audio And SNR Parity

- Changes the Qt6/CoreAudio mono input path to capture stereo when available and perform the mono downmix internally.
- Avoids driver/Qt-specific mono channel mapping that was making Decodium4 differ from Decodium3 on some audio devices.
- Replaces the simple 4x decimation used by the modern audio sink with the legacy `fil4` FIR low-pass shape before generating 12 kHz decoder samples.
- Keeps the full decoder audio window available while trimming the waterfall buffer, avoiding accidental loss of decoder samples when the waterfall ring buffer is smaller than the mode decode window.
- Tunes automatic RX input control so it prevents clipping without continuously pushing the RX gain above the useful range. Auto RX now caps itself at the stable input ceiling and raises level more conservatively.

## Cross-Mode Impact

- FT4 and FT2 weak decode paths are not changed by the FT8 `NDepth` mask fix.
- The FT8 change only corrects how the live embedded C++ FT8 worker interprets the legacy depth bit mask.
- The previous FT4/FT2 `-26 dB` behaviour remains valid.

## Italiano

Questa release consolida il lavoro fatto dopo la 1.0.274 su FT8, SNR e audio RX. L'obiettivo era rendere Decodium4 piu' vicino a Decodium3 nella ricezione reale, senza perdere il recupero FT8 molto profondo.

- FT8 live ora aspetta lo slot completo nel runtime embedded di D4, invece di partire troppo presto con decode parziali.
- Se il worker FT8 e' ancora occupato al confine successivo, viene mantenuto un decode completo in coda invece di perderlo.
- `NDepth=51` viene interpretato correttamente come maschera legacy: profondita' base `3` piu' opzioni, non profondita' `4`.
- Questo evita di usare il percorso FT8 piu' pesante su ogni slot e rimuove il backlog da 12-20 secondi visto nei confronti live.
- Nel confronto live con Decodium3, dopo il fix D4 ha decodificato piu' righe nella stessa finestra FT8 e lo SNR delle righe uguali e' risultato allineato: media circa `+0.39 dB`, mediana `0 dB`.
- Il recupero FT8 hinted molto profondo resta attivo, ma solo con contesto affidabile: stesso DX call, frequenza/DT compatibili, messaggio recente e formato FT8 valido.
- Senza partner QSO corrente, il recupero hinted accetta solo messaggi CQ-like sicuri.
- Aggiunti test automatici FT8 a `-26 dB` e FT8 hinted a `-27 dB`.
- Sistemato il percorso audio RX: cattura stereo con downmix mono interno, filtro FIR `fil4` prima dei 12 kHz e protezione contro tagli del buffer decoder causati dal waterfall.
- La regolazione automatica RX resta attiva ma non spinge piu' il livello oltre il tetto stabile, riducendo clipping e discrepanze SNR.
- FT4 e FT2 non sono toccati dal fix `NDepth` FT8; i risultati precedenti a `-26 dB` restano validi.

## Artifacts

- Windows x64 installer
- macOS Apple Silicon DMG/ZIP
- Linux x86_64 AppImage built with Qt 6.11
