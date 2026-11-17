# FT2 bit metric: log I0 and amplitude max-log variants (not adopted)

Companion note: `doc/FT2_FLIPPED_BIT_CAP.md` (the flipped-bit cap, same
corpora; it also lists the experimental environment variables). Both metric
variants described here are experimental and OFF by default.

## What was added

The FT2 bit metric is a log-sum-exp of the energies `beta * |c|^2 / nsym`,
with `beta = 0.5 / noise variance` clamped to 0.01..50. Two variants can be
selected with `DECODIUM_FT2_METRIC`, only for the three non-equalised columns
of `run_ft2_bitmetrics_impl` (`Detector/FtxBitmetrics.cpp`):

- `1` or `i0`: the exact noncoherent likelihood, `log I0 (2 A |c| / N0)` per
  hypothesis. `N0` is the mean of the 48 noise-only tones of the sync symbols,
  `A^2` the mean of the 16 sync tones minus `N0` (not less than `N0`).
  `log I0` comes from a table with step 1/16 up to 64, linearly interpolated.
- `2` or `amp`: max-log on the amplitudes `|c|`, no estimate needed.

The value is compared as a whole string; unset, empty, `0` or anything else
keeps the existing metric, and the code path is then the previous one. The
variable is read once per process; a selected variant (or an unrecognised
value) is reported once on stderr. The equalised branch, the cross-slot
accumulation metric and the coherent metric are not touched.

## Why the expected gain is small

In the decoder the noise variance is about 80-170 (the input is already level
normalised: scaling the audio by 0.1, 0.3 and 3 changes at most two decodes in
1200), so `beta` always sits at its lower clamp 0.01 and the effective energy
coefficient is about 1.2 / N0, not 0.5 / N0. On an idealised model
(noncoherent 4-FSK, 4-symbol blocks, Es/N0 4.5 dB, generalised mutual
information, 12000 blocks, one seed) the result is 1.341 bit/symbol with
coefficient 0.5, 1.382 with 1.2, 1.400 with `log I0` and known parameters and
1.391 with the amplitude max-log: `log I0` is worth about 0.26 dB over
coefficient 0.5 but only about 0.08 dB over what the decoder really uses.
That model is not a decoder-chain measurement.

## Measurement on the full decoder

Full FT2 decoder (`ft2_stage_compare --stages 7 --ndepth 3`, one process per
slot, flipped-bit cap 22 unchanged) on synthetic slots with Gaussian noise and
known truth; slot kinds and seed sets T, H, V, W as in
`doc/FT2_FLIPPED_BIT_CAP.md` (W was built and decoded by an independent
review). A decode that is not in the truth list of its slot is a false decode.
Decodes found / false decodes:

| slots | transmissions | existing | `i0` | `amp` |
| --- | ---: | ---: | ---: | ---: |
| weak, -17..-12 dB (2000 slots) | 16000 | 7227 / 1 | 7296 / 2 | 7157 / 0 |
| strong (1000 slots) | 8000 | 5529 / 0 | 5535 / 0 | 5480 / 0 |
| crowded (750 slots) | 10500 | 8952 / 0 | 8972 / 0 | 8961 / 0 |
| signals under interference (900 slots) | 5400 | 274 / 0 | 294 / 0 | 301 / 0 |
| all with signals (4650 slots) | 39900 | 21982 / 1 | 22097 / 2 | 21899 / 0 |
| noise only (9000 slots) | 0 | 0 | 1 | 1 |
| interference only (1800 slots) | 0 | 0 | 0 | 0 |

Per seed set, all slots with signals: T+H+V (3700 slots) 17496 / 0, 17574 / 2,
17436 / 0; W (950 slots) 4486 / 1, 4523 / 0, 4463 / 0.

- `i0`: +0.5% decodes (+0.45% on T+H+V, +0.8% on W). On T+H+V it gained 252
  decodes and lost 174. False decodes: 3 in all (2 in weak slots, 1 on noise)
  against 1 for the existing metric (a weak slot of W); on T+H+V it was 3
  against 0, on W 0 against 1. The difference is not resolved by these counts.
- `amp`: -0.4% decodes (-0.34% on T+H+V, -0.5% on W; gained 298, lost 358 on
  T+H+V), 1 false decode on noise against 1 on a weak slot.

Neither variant shows a gain that stands clear of the measurement noise at an
equal or lower false-decode rate, so both stay off. On the tuning set T a gain
of 0.7 and 1.4 on `2 A / N0`, a floor of 2, 3 and 4 on `A^2 / N0` and a fixed
value of 3 were also tried: between 1437 and 1451 decodes of 3200, within the
noise; those knobs are not in the code.

Decode time of the full decoder: no measurable change for either variant.
Two sets of three interleaved runs (150 and 210 slots, shared machine) differ
by less than the run-to-run spread of about 5%. The metric function alone,
measured once on a loaded machine: 156 us existing, 175 us `i0`, 74 us `amp`
(indicative).

## Overflow fix in the legacy noise estimate (on by default)

`ft2_legacy_base_noise_var` and `ft2_legacy_equalized_beta` no longer convert
out-of-range powers to `int` (undefined behaviour) or sum 48 terms in an
`int`: the truncation saturates (NaN gives 0) and the sum is 64 bit. Results
are unchanged wherever the old code did not overflow. The new behaviour is
pinned by `tests/test_ft2_bitmetrics_overflow.cpp`.

With `tests/tools/ftx_bitmetrics_compare.cpp` against a library built from
`origin/main` (4000 vectors per seed; seeds 1, 2, 7 and, in the review, 11,
23, 91) every FT8 and FT4 section is bit-identical. The FT2 sections differ
only on vectors where the old code overflowed (extreme levels of the
generator: 80-93 of about 3920 vectors for `ftx_ft2_bitmetrics_c`, 211-279
for the diag entry), so the tool exits 1 against `origin/main`; a copy that
reproduces the old wrap is identical everywhere. No decoder-level slot was
found where the overflow happens: default decoder output is identical to the
pre-change binaries on the shared corpora (FT2 174 lines, FT4 287 lines, FT8
392 lines over 40 cases) and on the 4550 slots of W.

## Limits

Synthetic Gaussian-noise slots only: no fading, drift or on-air recordings;
only depth 3; the my-call / DX-call a-priori passes and the cross-slot state
were not exercised (one process per slot). The false-decode counts are 0 to 3
events, too few to compare rates. The `log I0` table was not checked
numerically against a reference Bessel implementation.
