# FT8: adjacent-bin candidates (`DECODIUM_FT8_KEEP_NEIGHBOURS`)

Status: optional, off by default. With the variable unset the decoder is
unchanged.

## What was found

The question was where the weak FT8 signals that are not decoded get lost.
Measured on 45 held-out synthetic slots at -22, -21 and -20 dB (AWGN, drift,
phase random walk; 900 signals) plus 20 noise-only slots, depth 3, with the
adaptive levers granted (`FT8_STAGE_COMPARE_GRANT_LEVERS=1`):

- **The sync thresholds are not the bottleneck.** Lowering the minimum Costas
  count for a decode attempt to 3 or 0, or the candidate search threshold by
  30%, gives exactly the same 707 decodes for 1.2 to 1.9 times the time.
  Removing the Costas gates of the BICM-ID and phase-trellis rounds as well
  gives 713 decodes with 7 false decodes and 7.7 times the time.
- **The signals are found.** Of 63 missed signals inspected one by one, every
  one had a search candidate within 3.2 Hz of its true frequency.
- **The right candidate is thrown away.** The search grid is 3.125 Hz. A
  signal between two bins yields two candidates at the same time offset, and
  the search keeps only the stronger one (`fdiff < 4 Hz` and `dt` within
  0.1 s). Around `nfqso` that rule is suspended. Re-running the decoder with
  `nfqso` on each missed signal recovers 35 of 193, with no a priori pass
  involved; a narrow search window with `nfqso` elsewhere recovers none.

The likely reason, not proven: at this SNR the time and frequency estimate,
made on the 21 Costas symbols only, is noisy, and two starting points give
two estimates, one of which is sometimes close enough to decode. Two
alternative explanations were tested and rejected: a joint time-frequency
fine search, at +-2.5 Hz and at +-3.5 Hz, gives 715 and 714 decodes (+1%)
for 15-22% more time, and adds nothing on top of the option below.

## The option

`DECODIUM_FT8_KEEP_NEIGHBOURS=<ratio>` keeps both candidates everywhere when
the weaker sync is at least `<ratio>` times the stronger. `0.01` keeps them
always.

| Setting | Decodes / 900 | at -22 dB / 300 | False, signal slots | False, noise slots | Extra time |
|---|---|---|---|---|---|
| unset (current rule) | 707 | 159 | 0 | 0 | - |
| 0.93 | 721 (+2.0%) | - | 0 | 0 | +6.5% |
| 0.85 | 729 (+3.1%) | - | 0 | 0 | +11% |
| 0.70 | 742 (+5.0%) | - | 1 | 0 | +19% |
| always | 746 (+5.5%) | 183 | 1 | 0 | +26% |

Times are unpaired, on a shared machine. The false-decode counts are too
small to tell the settings apart.

## Why it is off

Each percent of decodes costs about three percent of decode time at every
ratio: the ratio is a dial, not a shortcut. That is a worse exchange than the
BICM-ID second version and the phase-trellis round. Doubling the candidates
treats the symptom; the estimate itself is what should improve (retrying a
failed candidate with the next-best time and frequency hypotheses, or using
the decoder's own output to refine them).

## Limits

Synthetic signals only, one depth, 45 slots; not validated on recordings or
on air. The application with the governor running was not exercised.
