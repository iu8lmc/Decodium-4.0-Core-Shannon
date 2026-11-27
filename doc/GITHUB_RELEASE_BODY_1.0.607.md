# Decodium 4 FT2 v1.0.607

Version 1.0.607 takes upstream 1.0.606 in full and adds a day of decoder work:
the FT8/FT2 LDPC stage is substantially faster and finds slightly more, at the
same false-decode rate.

## English (UK)

### A faster LDPC stage that also finds more

Four changes to the `fastldpc` path, measured together rather than one at a
time — measuring each knob on its own bench and adding the percentages up is
not legitimate, and the first set of numbers produced that way was wrong:

| | before | after |
|---|---|---|
| time per candidate | 80.3 us | **58.8 us** |
| candidates reaching the CRC | 6240 | **4986** |

That is **27% less time** (1.37x) and **20% fewer candidates**, with
**+2.5% decodes** — and that last figure is quoted at equal false-decode rate,
not at a fixed threshold. At a fixed threshold the same bench reports +5.8%,
which flatters the result: `nd_max` is a knob, not a physical property, and two
configurations on the same curve look far apart only because one of them is
working higher up it. +2.5% is the number that survives someone repeating the
measurement.

The changes: `ntau` 13 instead of 14 with pairs taken over the first 64
candidates; the normalisation factor moved from the historic 3/4 of
Chen-Fossorier to 0.578; the a-priori search widened (`span2` 32 to 64), which
deliberately spends part of the gain from the previous two.

Confirmed on the air for `ntau` 13 (paired comparison, two windows of 145 cycles
on 7.074 FT8). Confirmed on the bench, on independent data and against ground
truth, for the normalisation factor and the CQ a-priori — not yet on the air.

### Fewer min-sum iterations

The min-sum iteration cap goes from 30 to 10. Between 30 and 6 the decodes are
identical — the difference, −0.1%, is inside the noise — over three SNRs and
150,000 noise candidates. Ten rather than six because the bench measures on
Gaussian noise, and real signals carry interference that Gaussian noise does not
contain. **A further 5% off the decoder's time, with decodes unchanged.**
`DECODIUM_LDPC_MAX_ITER` restores 30 without recompiling.

### Two experimental a-priori paths, shipped switched off

Two additional a-priori strategies are present but disabled by default
(`DECODIUM_FT8_AP_STORICO`, `DECODIUM_FT8_AP_MSG`): one seeds the decoder from
stations already heard, the other from whole candidate messages.

On the bench they look spectacular — 0.8 dB and 4.4 dB. On the air the second
one is worth **+1.2% of decodes**, and that is not a contradiction: the bench
measures *at the threshold*, where every word is marginal by construction, while
most real signals sit comfortably above it and need no help. A gain measured at
the threshold has to be multiplied by the fraction of traffic that actually sits
there, and that fraction is small.

They are switched off for exactly that reason, and the numbers are recorded here
so that nobody re-measures them at the threshold and announces four decibels.

### A-priori on CQ while you are busy

Being engaged in a contact used to make you slightly deafer to everyone else:
during a QSO, other stations' CQs were decoded without the a-priori treatment
they get when idle. They now get it in FT2, and in FT8 and Q65 as well.

In FT2 the gain was measured as a threshold shift: **+0.4 dB**. In FT8 it was
measured as recovered decodes instead, and the gap closes completely:

| SNR | not in a QSO | in a QSO, now | in a QSO, before |
|---|---|---|---|
| −18 dB | 15/20 | **15/20** | 12/20 |
| −19 dB | 7/20 | **7/20** | 5/20 |

Interpolating a threshold from those figures would give roughly 0.3 dB, but
with twenty files per point that is indicative rather than publishable — so the
0.4 dB figure belongs to FT2 and is not claimed for FT8. What is claimed for
FT8 is that the penalty disappears.

### Narrow decode bands: improved, not fixed

With `nfa`/`nfb` narrower than about 700 Hz, strong signals were being lost
without warning: the noise floor and the peak were being looked for in the wrong
place. The case of an off-centre signal is now correct. **The case of a signal
centred below 500 Hz is still broken** — there is a third mechanism involved
that has not been identified yet. The practical advice remains: do not set the
decode band below 700 Hz.

Non-regression was verified on both affected modes: FT2 identical line by line
(zero ghosts at six SNRs; 25/25, 20/25, 15/25 decodes), FT4 identical over 20
files.

### ARM builds

The NEON kernel was changed along with the AVX2 one, on a machine that cannot
compile it. Its numerical equivalence with AVX2 was therefore proved
exhaustively instead of argued: both instructions were simulated in scalar
arithmetic and compared over all 65536 possible magnitudes, for every
normalisation weight in use. They agree exactly for magnitudes up to 32767 and
disagree above it, where a magnitude read as signed becomes negative — and the
decoder clamps every LLR to 2047, so the operating range sits a factor of
sixteen inside the safe region.

That covers correctness, not compilability, so the aarch64 image was built
before this release was tagged.

### Everything from upstream 1.0.606

Merged in unchanged: the SSTV subsystem isolated into its own translation unit,
the RTTY setup dialog simplified, RTTY macros, and the upstream FT2 decoding
adjustments.

Upstream has also adopted this fork's removal of SSB speech-to-text, so that is
no longer a divergence between the two trees.
