# The CRC budget

[Italiano](RAPPORTO_BUDGET_CRC.md) · **English** · [Español](INFORME_PRESUPUESTO_CRC.md)

> Measurements on the LDPC(174,91) decoder of FT2 and FT8: why widening the
> search buys nothing, and what is gained instead by strengthening the
> acceptance test.
>
> *Author: **IU8LMC**. Implementation and measurements carried out with the
> assistance of Claude (Anthropic) under the author's direction.
> GPL-3.0 — 29 August 2026.*

---

## 00 · In short

> The FT8 and FT2 decoder is not limited by how much it searches, but by how it
> accepts. The 14-bit CRC lets one wrong candidate through every 16,384:
> widening the search buys correct and false candidates in the same proportion,
> and does not pay. Adding two bits of message structure to the acceptance test
> halves the phantom callsigns while leaving decodes unchanged.

This report collects what was measured, including **three occasions where the
measurement refuted the prediction**. The refutations are reported in full: they
are the most useful part, because each one would have taken a regression on air
dressed up as an improvement.

Everything is reproducible. The code is header-only under GPL-3.0 in
`Detector/fastldpc/`, the benchmarks in `lab/cpp/`, and every number here comes
from a command that can be re-run.

---

## 01 · Context in three paragraphs

FT8, FT4 and FT2 use the same error-correcting code: the LDPC(174,91) of the FT8
protocol, with 77 message bits plus 14 CRC bits in the 91 information bits, and
83 parity bits. FT2 is a short-cycle mode, 3.75 seconds, where the computation
budget per cycle is tight.

Four levels are worth separating right away, because only the last is the work
of this project. The **class of codes**, LDPC, is Robert Gallager's, 1962,
rediscovered by MacKay and Neal in the 1990s. The **decoding algorithms** —
normalised min-sum, ordered statistics decoding — are established literature, by
Chen and Fossorier for the first, Fossorier and Lin for the second. The
**specific code** (174,91), meaning those 83 parity rows, and the 14-bit CRC with
polynomial `0x2757`, belong to the FT8 protocol, designed by Steve Franke K9AN
and Joe Taylor K1JT and published in QEX. The **decoder** described here is
written from scratch, and uses that code and that CRC unmodified, by choice:
changing them would break bit-exact compatibility with other stations.

The decoder works in two stages. The first is a normalised, layered *min-sum*,
which closes most words at good signal levels. The second is an OSD, *ordered
statistics decoding*, which re-examines only the words the first stage failed to
close: it permutes the columns putting the least reliable bits first, solves,
and tries a number of variants around the solution. The quantity that decides
both the cost and the yield of the second stage is **the number of candidates it
tries**. And that is where the result lies.

---

## 02 · Speed: vectorised min-sum and batch decoding

The first stage was rewritten with AVX2 intrinsics, sixteen words per register
of 16-bit integers, in fixed point Q=1/8 with saturation. Words that have
already converged drop out of the loop. The second stage uses branchless
Gaussian elimination on 256-bit rows, lower-bound pruning, radix sorting and an
incremental CRC syndrome.

The gain that makes everything else possible is the first stage: from **139.8 to
4.7 microseconds** per word, twenty-nine and a half times. It is not an end in
itself — it makes a higher OSD order practical, and that is where the
sensitivity comes from.

LDPC(174,91) over AWGN/BPSK · 20,000 words per point · single thread ·
Ryzen Zen 3, gcc 15.2, `-O3 -march=native`:

| Eb/N0 | FER fast | µs | FER conservative | µs | FER sensitive | µs |
|---:|---:|---:|---:|---:|---:|---:|
| 0.5 dB | 0.851 | 6.3 | 0.401 | 19.0 | **0.265** | 39.6 |
| 1.0 dB | 0.674 | 5.5 | 0.209 | 15.3 | **0.114** | 31.8 |
| 1.5 dB | 0.443 | 5.5 | 0.082 | 12.2 | **0.036** | 21.8 |
| 2.0 dB | 0.223 | 5.3 | 0.024 | 8.9 | **0.0082** | 14.3 |
| 2.5 dB | 0.083 | 4.6 | 0.0050 | 6.1 | **0.0018** | 7.5 |
| 3.0 dB | 0.021 | 2.9 | 0.00095 | 3.2 | **0.00010** | 4.1 |

Against the starting configuration the sensitivity gain is **+0.35 dB**, and
**+1.3 dB** against min-sum alone, at equal false-decode rate, with the chain
sixteen times faster at 2 dB.

### Expected versus measured — batch decoding

| Prediction | Measurement |
|---|---|
| Six times faster: min-sum goes from one word per lane to sixteen. | **1.8 times.** The estimate counted only the min-sum; the OSD stays per-word and becomes the dominant share. |

The gain is real, but one third of what was announced. The prediction was wrong
because it mentally optimised the part that was already fast.

---

## 03 · FT8: the same decoder, the same gain

FT8 and FT2 share the code, so they share the decoder. In Decodium 4 the FT8
path uses `fastldpc` including batch decoding of the passes, with an environment
switch (`DECODIUM_FT8_FASTLDPC=0`) to fall back to the original decoder, and a
rescue mechanism that retries with the classic one for a limited number of
candidates per cycle.

One difference from FT2 deserves emphasis because it is substantive: the
plausibility filter described below runs in FT8 with **all message types
allowed**. The contest formats that never appear in FT2 do exist in FT8, and
filtering them out would blind the decoder precisely on contest days.

Production FT8 stage on one slot generated with `ft8sim` at −18 dB, depth 3, two
runs per configuration:

| LDPC decoder | Run 1 | Run 2 | Decodes |
|---|---:|---:|---:|
| Original `ftx_decode174_91_c` | 71,648 ms | 71,373 ms | 3 |
| **`fastldpc`** | **9,316 ms** | **9,319 ms** | 3 |

**7.7 times faster, with identical decodes.** Repeatability is within 0.4%, and
the same three lines carrying the callsign come out of both configurations: the
gain is all time, no sensitivity traded away.

The number should be read for what it is. `ft8_stage_compare` runs the
production stage comparing several configurations on the same file, so it is a
workload skewed towards the decoder: it is the right ratio for the LDPC part,
not the time of the whole application. It is nevertheless the piece that, in
crowded cycles, decides whether the cycle closes in time.

### The threshold in dB, which is the real metric

The number that really matters for FT8 is not time but the **50% threshold**:
the SNR at which half the signals are decoded, with a signal planted at known
SNR in the 2500 Hz reference bandwidth. Total decode count is not a metric — it
is inflated by easy signals.

Measured with `decode_bench/`, which generates the signals with WSJT-X's
`ft8sim` and therefore has ground truth. Seven points from −19 to −25 dB, 25
noise realisations each, deep profile, message `K1ABC W9XYZ EN37` at 1500 Hz:

| SNR | with `fastldpc` | original decoder | `jt9` deep |
|---:|---:|---:|---:|
| −19 dB | 25/25 | 25/25 | 25/25 |
| −20 dB | 24/25 | 23/25 | 23/25 |
| −21 dB | **11/25** | 7/25 | 9/25 |
| −22 dB | **6/25** | 3/25 | 7/25 |
| −23 dB | 0/25 | 0/25 | 0/25 |

| | 50% threshold |
|---|---:|
| Decodium with **`fastldpc`** | **−20.88 dB** |
| Decodium with the original decoder | −20.66 dB |
| WSJT-X `jt9`, deep profile | −20.75 dB |

**The factor of 7.7 in speed costs no sensitivity.** `fastldpc` comes out 0.22 dB
more sensitive than the original decoder and 0.13 dB more than `jt9`.

On statistical strength the truth must be told: the two informative points are
−21 dB (11/25 against 7/25) and −22 dB (6/25 against 3/25), each at about 1.2
sigma, combining to roughly 1.7. Suggestive, not conclusive. **What can be
stated without reservation is that fastldpc costs no sensitivity**; establishing
the +0.2 dB would need a hundred realisations per point rather than twenty-five.

### The control: it is not the deadline

The bench imposes a deadline per decode, and the original decoder is 7.7 times
slower: the obvious suspicion is that the gap is not quality but time running
out. That is a testable hypothesis, and it must be tested, because it completely
changes what is being measured.

Two points (−21 and −22 dB), 40 realisations, deep profile:

| | threshold | −21 dB | −22 dB | total time |
|---|---:|---:|---:|---:|
| `fastldpc`, 8 s deadline | **−21.29 dB** | 24/40 | 10/40 | 547 s |
| original, 8 s deadline | −21.00 dB | 20/40 | 5/40 | 647 s |
| original, **40 s** deadline | −21.05 dB | 21/40 | 1/40 | **3208 s** |

**Giving the original decoder five times more time changes nothing**: −21.05
against −21.00. The deadline was not the constraint, and the hypothesis was
wrong. The gap is decoder quality, not exhausted time — consistent with the
chain described above: speed does not hand out decibels by itself, it makes a
higher search order *affordable*, and that is what hands them out.

Two remarks on robustness. This run gives a 0.29 dB gap, the previous one 0.22:
two independent samples agreeing in direction and magnitude, together bringing
the signal to roughly 2.4 sigma. And the absolute threshold swings by 0.4 dB
between two runs of the same configuration (−20.88 and −21.29), a reminder of
how little twenty-five or forty realisations per point weigh: it is the paired
comparisons that hold, not the absolute values.

One detail is worth noting without pushing it: at −22 dB the original decoder
given more time does **worse**, 1/40 against 5/40. The numbers are small and the
difference sits at 1.7 sigma, so little can be concluded — but the direction is
exactly that of this report's thesis: more time means more candidates tried, and
more candidates means more CRC false positives crowding out the right one.

The opposite reading also holds, and is the more useful one: Decodium is **on par
with `jt9` in deep profile**. On FT8, the decibels are no longer in the
decoder.

---

## 04 · How many decibels are left in the decoder

Optimising a decoder implicitly assumes that searching better decodes more. That
is not a given, and it can be measured rather than assumed.

A failure has two opposite causes. Either the true word was *more likely* than
the one chosen and the decoder did not find it — then searching more pays. Or
the true word was *less likely* than another valid codeword: there maximum
likelihood itself was wrong, no decoder can do better, and the decibels must be
sought in the demodulator or the sync.

On an AWGN channel the likelihood of a word is the sum of the absolute LLR
values in the bits where it contradicts the hard decision. Comparing the true
word's against the chosen one's, at Eb/N0 = 1 dB over 5000 words, with the
near-optimal configuration (order 3, span 91/48, gate off):

| Cause | Cases | Share |
|---|---:|---:|
| **Code** limit — the true word was less likely | 5 | 1.1% |
| **Search** limit — wrong word accepted | 370 | 79.4% |
| **Search** limit — no word found | 91 | 19.5% |

**98.9% of failures are a search limit.** On paper there was a lot to take:
widening the OSD moved correct decodes from 84.96% to 90.68%, at 1.7 times the
cost.

The same measurement, applied to a quantum LDPC code under code-capacity noise,
gives the opposite answer: **zero recoverable failures**, that is, a decoder
already at the ceiling of minimum-weight decoding. It is a simple criterion for
deciding whether it is worth working on the decoder or stopping, and it is worth
running *before* any optimisation.

---

## 05 · The result: it is the acceptance test that binds

Those 5.7 percentage points of extra decodes cannot be cashed in as they are,
because they arrive together with a pile of false decodes. The right question is
not how many candidates are tried, but how many are accepted by mistake.

The only test deciding whether a candidate is valid is the 14-bit CRC, which
admits one wrong candidate every 2¹⁴ = 16,384. The narrow search tries about 600
candidates per word, the wide one about 21,400. The expected number of CRC false
positives per word follows: **0.04 against 1.3**.

> Widening the search buys correct and false candidates in the same proportion.
> The bottleneck is not the search: it is the acceptance test.

The verification consists in putting both widths on the same curve, sweeping the
anti-phantom gate threshold, with 20,000 signal words and 100,000 pure-noise
candidates. The comparison must be made **at equal phantom counts**, not at
equal threshold — the threshold is not the quantity anybody cares about.

Eb/N0 = 1 dB:

| Configuration | Threshold | Decodes | Phantoms |
|---|---:|---:|---:|
| **Narrow, no filter** *(in service)* | 0.065 | **16,168** | **10** |
| Narrow, no filter | 0.070 | 16,718 | 43 |
| Narrow, no filter | 0.075 | 16,924 | 141 |
| Wide, no filter | 0.065 | 16,810 | 20 |
| Wide, no filter | 0.070 | 17,564 | 81 |
| Wide, no filter | 0.075 | 17,912 | 311 |

At equal phantom counts the two widths are equivalent, and below the crossover
the narrow one is better. The operating point in service sits right below it.
**Widening the search, on its own, does not pay.**

---

## 06 · Two bits of message structure

If the constraint is the acceptance test, then strengthen the test. And the
information to do so is already there: the 77 payload bits are not an arbitrary
number, they are a message with a structure. A randomly drawn payload almost
never describes possible callsigns.

The check verifies only certain structural constraints:

- that the message type `i3` is among those defined and allowed, and for `i3=0`
  the subtype `n3` as well;
- that length-limited fields fall within their range — free text is 42¹³ inside
  71 bits, the non-standard callsign 38¹¹ inside 58, the ARRL exchange 1..8000
  or a valid multiplier;
- that callsigns have a possible structure: left-aligned suffix, at least one
  suffix letter, a prefix that is neither two digits nor a single digit;
- that a token (CQ, DE, QRZ) does not appear in second position.

No geographic or statistical check: those would reject genuine contacts.

It sits **inside** the OSD acceptance loop, not after it. The distinction is
substantive: a false candidate that passes the CRC but is not a message does not
stop the enumeration, which can still find the right one. Applied afterwards, it
would merely discard the word and leave the hole. It costs almost nothing
because only candidates that have already passed the CRC ever see it, that is
one in 16,384.

Filter strength, over 2 million randomly drawn payloads:

| Setting | Accepted | Filter bits | Factor |
|---|---:|---:|---:|
| All defined types *(used in FT8)* | 0.573 | **0.80** | 1.74× |
| Only used types *(used in FT2)* | 0.268 | **1.90** | 3.73× |

### The gain, and its safety

At equal search and equal threshold, the filter **halves the phantom callsigns
while leaving decodes identical**. This is the part adoptable with no trade-off:
it changes neither search nor threshold, and cannot remove decodes.

| Filter | Decodes | Phantoms / 100,000 |
|---|---:|---:|
| None | 16,168 | 10 |
| All defined types | 16,168 | **7** |
| **Only used types** | **16,170** | **4** |

Safety is verified on two fronts. Over 20,000 realistic messages the filter
rejects none. And over 404 callsigns taken from real ADIF logs, after the
correction below, not one.

### Expected versus measured — the prefix rule

| Rule as written | Validated against the logs |
|---|---|
| A letter must precede the digit of the callsign. Seems obvious. | **Rejects 12 real callsigns out of 404**: S53MJ, S50XX, A61OK, S51RU, S56EPX, S51DM, Z31B, N25BRX, G56KAY, A65DF, Z62NS, E75AA. |

These are letter+digit prefixes: **S5** Slovenia, **A6** United Arab Emirates,
**Z3** North Macedonia, **E7** Bosnia, **Z6** Kosovo. With that rule those
countries would never have been decoded again. The true constraint is weaker: a
prefix may be a letter, letter+letter, letter+digit or digit+letter, never two
digits nor a single digit.

> A plausibility filter is validated against real data, not against one's own
> reasoning. The bench that caught it is `lab/tools/valida_nominativi.py`, and it
> must be re-run every time the rule is touched.

---

## 07 · When the lab is wrong and the band corrects it

With two more bits of filtering, the wide search became worthwhile on paper. The
lab measurement gave, at equal phantom counts, **+4.0% decodes and −40% phantoms
together**: a clear improvement on both axes, no trade-off. It was put into
production.

### Expected versus measured — the wide search on air

| Laboratory | On air |
|---|---|
| On synthetic Gaussian noise: more decodes *and* fewer phantoms. Tried twice. | **Withdrawn twice.** Six minutes at zero phantoms seemed to acquit it, but on a band with no FT2 traffic six minutes prove nothing: with more time the phantoms came back in numbers. |

Real noise is not white Gaussian. Carriers, other modes and QRM produce
correlated LLRs that the OSD latches onto, and a wide search finds structure
where the synthetic model had none. **A bench on synthetic noise can
overestimate, and in this case it did.**

The production configuration stays with the narrow search and the filter
enabled: the gain that survives is the halving of phantoms.

---

## 08 · The quantum excursion, and why it stops here

The same core — vectorised min-sum plus OSD on the most reliable basis — was
carried over to decoding syndromes of quantum LDPC codes: *bivariate bicycle*
codes of Bravyi et al., including the [[144,12,12]], both under code-capacity
noise and under circuit-level noise derived with `stim`.

Three differences from the classical case. The syndrome is not zero, and in the
min-sum this becomes one line: the sign accumulator starts from *s<sub>m</sub>*
instead of zero. There is no CRC, and none is needed, because every candidate
satisfies the syndrome by construction. Success is not recovering the error: a
quantum code is degenerate, and decoding is correct as long as the residual
contains no logical operator.

The work produced verifiable results — among them the proof, by exhaustive
enumeration, that no uniform six-layer schedule can extract the syndrome of BB
codes deterministically, while at seven layers 236 valid ones exist, and the one
adopted preserves the code distance.

### Expected versus measured — the comparison with the literature

| Initial conviction | Literature |
|---|---|
| Little-trodden ground, and 100 to 600 times faster than the reference. | A crowded, fast-moving field. **The library used as reference already contains the fast decoder**, *Localized Statistics Decoding*, created for exactly this problem: the comparison had been made against the slow one. |

The comparison was therefore redone on **`sinter`**, the field's standard
harness: batched, multi-process, the same infrastructure for every decoder, and
reproducible by anyone without our code. Surface code d=5, 50,000 shots per
point, six workers:

| p | fastldpc | BP+LSD | BP+OSD-7 | pymatching |
|---:|---:|---:|---:|---:|
| 0.001 | 3 err · 190 µs | 4 · 1107 µs | 1 · 1039 µs | 7 · 0.9 µs |
| 0.002 | **17** · 119 µs | 30 · 2001 µs | 29 · 2570 µs | 59 · 1.5 µs |
| 0.003 | **86** · 141 µs | 124 · 3268 µs | 97 · 4485 µs | 153 · 2.1 µs |

**8 to 32 times faster than BP+LSD and BP+OSD, with fewer errors than either**,
and the speed advantage is understated: our timing includes the whole round trip
— DEM parsing in Python, file writing, process spawn — while the others run
in-process.

On accuracy the statistical strength must be stated: at p=0.002, 17 against 29
sits at 1.8 sigma; at p=0.003, 86 against 97 at 0.8. Taken one at a time they
are not conclusive; being below at all three points is more so. On par or
slightly better, with speed as the solid advantage.

> **The standard harness immediately caught a bug that three days of in-house
> measurement had missed.** The first sinter run gave 768 errors where the
> in-house bench gave 98: a factor of eight. sinter hands decoders the
> **decomposed** model, where one instruction is split by `^`, and an observable
> appearing in two components **cancels** in GF(2). Collecting targets into a
> list instead of by parity marked 86 instructions out of 1953 as flipping the
> observable when they do not. A silent error: weights, priors and total
> probability mass were all unchanged. It surfaced only because the new
> integration failed to reproduce a number already known — and that is the check
> that should have come first.

The sliding-window decoding implemented to make the cost linear in the number of
rounds is a standard technique, and the depth-7 schedule was already in Bravyi et
al.: it was rediscovered, not discovered.

---

## 09 · What is new and what is not

**The concept is not new.** Exploiting residual source redundancy inside the
channel decoder is *source-controlled channel decoding*, a line of work going
back to Hagenauer in the 1990s. And that the CRC false-positive budget limits
list size is standard coding theory, well documented in the literature on
CRC-aided list decoding of polar and convolutional codes.

**The application is new, and above all the measurement.** In the FT8 and WSJT-X
community there appears to be no prior work building the decodes-versus-phantoms
curve and showing that what binds is the acceptance test and not the search; nor
the operational result that follows, namely halving the phantoms at unchanged
decodes. Also new, as far as can be established, is the use of the "search limit
or code limit" diagnosis as a criterion for deciding *where* to spend effort
before spending it.

---

## 10 · Stated limitations

- The FT2 measurements use synthetic AWGN LLRs, not the output of the real
  4-GFSK demodulator. The noise model **has already overestimated once**, as
  described in section 07.
- Timings are on one machine, one thread, one compiler.
- Restricting to the message types actually used is a **policy**, not a format
  test: a message of an excluded type would never be decoded. In FT8 it is not
  applied, precisely for this reason.
- The quantum comparison now runs on `sinter`, but the accuracy advantage sits
  at 1-2 sigma: more shots are needed to make it solid. On BB codes the
  comparison has not yet been redone on the standard harness.
- The FT8 threshold in dB is now measured (section 03), but over 25 realisations
  per point: the 0.2 dB advantage sits at 1.7 sigma and is not established. A
  sample four times larger is needed.

---

## 11 · Reproducing the measurements

Every table in this report comes from a command. The benches are header-only and
compile with `g++ -O3 -march=native -std=c++17`.

```
lab/cpp/ml_gap.cpp        search limit or code limit
lab/cpp/pareto.cpp        decodes-versus-phantoms curve
lab/cpp/noise_test.cpp    acceptances on pure noise, with timings
lab/cpp/plausible.hpp     the message structure check
lab/tools/gen_test.py     test words; --reali for genuine messages
lab/tools/valida_nominativi.py
                          checks the filter against callsigns from ADIF logs
decode_bench/             FT8 threshold in dB with ground truth
```

One warning that cost time: test data generated **without** `--reali` contains
random payloads, not messages. Measuring a plausibility filter on those makes it
look destructive, because it also rejects the "correct" words — which are not
messages.

---

## 12 · Bibliography

- R. G. Gallager, **"Low-Density Parity-Check Codes"**, MIT, 1962.
  The origin of the class of codes all of this rests on.
  *IRE Trans. Inf. Theory, IT-8, pp. 21–28.*
- S. Franke K9AN, B. Somerville AE6NZ, J. Taylor K1JT, **"The FT4 and FT8
  Communication Protocols"**, QEX. The specification of the LDPC(174,91) code and
  the 14-bit CRC used unmodified in this work.
  <https://wsjt.sourceforge.io/FT4_FT8_QEX.pdf>
- **Joint Source–Channel Decoding**, Wiley. The *source-controlled channel
  decoding* line: exploiting residual source redundancy inside the channel
  decoder.
  <https://onlinelibrary.wiley.com/doi/10.1002/9781118693803.ch10>
- **Design, Performance, and Complexity of CRC-Aided List Decoding of
  Convolutional and Polar Codes for Short Messages**. The trade-off between list
  size and undetected error probability.
  <https://arxiv.org/pdf/2302.07513>
- **Pre-configured Error Pattern Ordered Statistics Decoding for CRC-Polar
  Codes**. <https://arxiv.org/pdf/2309.11836>
- **Linear-Equation Ordered-Statistics Decoding**. Low-complexity OSD variants.
  <https://arxiv.org/pdf/2110.11574>
- **Localized statistics decoding for quantum low-density parity-check codes**,
  Nature Communications, 2025. Attacks the cost of the OSD's Gaussian
  elimination; shipped in the `ldpc` library.
  <https://arxiv.org/pdf/2406.18655>
- **Ambiguity Clustering: an accurate and efficient decoder for qLDPC codes**.
  <https://arxiv.org/pdf/2406.14527>
- **Fully Parallelized BP Decoding for Quantum LDPC Codes Can Outperform
  BP-OSD**. <https://arxiv.org/abs/2507.00254>
- **An almost-linear time decoding algorithm for quantum LDPC codes under
  circuit-level noise**. Sliding-window decoding as an established technique.
  <https://arxiv.org/pdf/2409.01440>
- **BP+OSD**, the reference library used in the comparisons.
  <https://github.com/quantumgizmos/bp_osd>

---

*Technical report on Decodium 4.0 Core Shannon · `fastldpc` · GPL-3.0.*

**Attribution.** `fastldpc` is a decoder written from scratch for Decodium 4.0
Core Shannon. It implements known algorithms — LDPC codes (Gallager, 1962),
normalised min-sum, ordered statistics decoding — with AVX2 vectorisation and
original optimisations. The pair search is modelled on the `npre1`/`npre2` steps
of `osd174_91` in WSJT-X. It operates on the LDPC(174,91) code and the 14-bit CRC
(`0x2757`) of the FT8 protocol, designed by Steve Franke K9AN and Joe Taylor
K1JT, used unmodified to guarantee bit-exact compatibility. The original decoder
remains available and selectable.
