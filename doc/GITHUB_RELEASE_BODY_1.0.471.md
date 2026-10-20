# Decodium 4 FT2 1.0.471

Release focused on FT2-Link RF reliability, W2300 weak-signal testing, live diagnostics, and CAT/Hamlib stability.

## FT2-Link W2300 waveform and decoder

- Added W2300 `WEAK`, `DEEP`, and experimental `ULTRA` rate handling in the waveform layer.
- Added mode symbols, parsing, metric reporting, retry escalation, and handshake negotiation for the new W2300 rate modes.
- Extended W2300 preamble/sync training and added tolerant header acquisition so a small number of training bit errors no longer kills a candidate before CRC validation.
- Added residual CFO search, estimated-offset candidates, drift search hooks, and decision-directed phase tracking for W2300 receive.
- Added weighted soft decisions for repeated/interleaved payload bytes.
- Added CRC-guided packet repair using low-confidence bit ranking for FAST/ROBUST/WEAK/DEEP/ULTRA candidates.
- Increased DEEP interleaving depth for a slower but more robust fallback path.
- Added experimental ULTRA retry path for future very-low-SNR testing. This is intentionally conservative and may be slow; it is ready for further decoder/FEC work.

## FT2-Link RF lab and diagnostics

- Added RF Lab WAV generation and replay support for FT2-Link test frames.
- Added channel impairment simulation for AWGN, frequency offset, drift, fading, clipping, filter shape, burst delay, and sample-rate error.
- Added an automated RF corpus generator/replayer for W2300 operational and stress cases.
- Added richer TX/RX debug reports including W2300 plan, detected rate mode, quality, center/offset estimate, sample offsets, busy state, and replay results.
- Added targeted tests for W2300 rate modes, adaptive retries, RF lab generation/replay, and channel sweeps.

## FT2-Link UI and workflow

- Improved FT2-Link panel layout and retry/status text for W2300 rate modes.
- Updated Deep Search behavior so DEEP and experimental ULTRA retries are gated for faster machines while Low CPU mode can keep the stack lighter.
- Improved received file/BBS/mail state tracking and visible unread indicators in the FT2-Link workflow.
- Added UI hooks for RF lab recording/replay and transport metrics used during manual over-the-air testing.

## CAT and Hamlib stability

- Reduced redundant rig mode writes when the requested CAT mode already matches the current mode.
- Improved local QSY guard handling so manual rig frequency changes can be accepted instead of being mistaken for stale CAT data.
- Preserved passive frequency polling for Icom/Hamlib serial CAT while keeping fragile split/mode/PTT passive reads conservative.
- Reduced repeated CAT state sends when frequency, TX frequency, split, or mode already match the desired state.
- Improved shutdown and stale-state handling around CAT/audio mode transitions.

## Validation

- Built FT2-Link core and RF lab corpus tools locally on macOS.
- Ran targeted FT2-Link tests for ULTRA negotiation, DEEP/ULTRA waveform roundtrip, interleaving repair, and retry rate control.
- Ran RF corpus work through operational W2300 cases and stopped the slow experimental ULTRA -3 dB stress case for later decoder work.

Notes:

- `ULTRA` is experimental and should be treated as a testbed for future soft-decision FEC/acquisition improvements.
- For best W2300 weak-signal testing, both stations should update to 1.0.471.
