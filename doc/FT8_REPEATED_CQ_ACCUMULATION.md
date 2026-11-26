# FT8: accumulating repeated CQs across slots

## Finding

A station calling CQ sends the same message every 30 s. The decoder already
keeps the symbol spectra of undecoded CQ-like candidates and combines them with
the next slot before LDPC decoding. Two storage modes exist:

- **magnitude average** of the stored spectra (the original mode);
- **energy sum** (`sqrt(previous energy + current^2)`), which the adaptive
  lever governor granted since 1.0.627 on computers with CPU to spare.

Measured on repeated CQs, the energy-sum storage removes almost all the gain
of the repetition. Computers with the levers granted accumulated worse than
slower computers.

## Change

The governor no longer grants the energy-sum storage. It is used only with an
explicit `DECODIUM_FT8_STORICO_ENERGIA=1`. Nothing else changes: the combiner
in `FtxBitmetrics.cpp` is untouched, and so is the on-air format.

## Measurement

Synthetic AWGN, 24 sequences per level, each with the same 20 signals (10 CQ,
10 other) in 4 consecutive slots with independent noise; 12 more sequences at
-70 dB (noise only). Depth 3, levers granted. Distinct CQs decoded at least
once by slot 4, out of 240:

| Storage | -22 dB | -23 dB | -24 dB | False decodes | CQs on noise-only |
|---|---|---|---|---|---|
| energy sum (before) | 225 | 101 | 8 | 3 | 0 |
| magnitude average (now) | 232 | 153 | 22 | 3 | 0 |
| `DECODIUM_FT8_STORICO_ENERGIA=0` (average, magnitude combiner) | 233 | 150 | 23 | 3 | 0 |

At -23 dB by slot: 29, 57, 78, 101 before; 29, 68, 117, 153 now.

Checks:

- Levers not granted: decode lists identical to 1.0.1008 on the regression set
  (8 recordings x 5 profiles, 0 differences).
- 230 held-out single slots, levers granted: 2331/2920, 1 false decode, both
  before and after (history does not act on isolated slots).

## Limits

- One synthetic AWGN corpus with perfectly repeated signals at fixed frequency
  and timing. Not validated on air, nor with drift or fading between slots.
- Only CQ-like candidates are accumulated. Other repeated messages (a report
  sent again, for instance) get no benefit: 131 of 240 at -23 dB either way.
- At -24 dB 22 of 240 after four slots is still far from what coherent
  combining of four copies would allow in theory (about 6 dB).
