# FT2: the 22-flipped-bit cap of superldpc

Companion note: `doc/FT2_BIT_METRIC_VARIANTS.md` (FT2 bit-metric variants,
measured on the same corpora). Both notes describe experimental options that
are OFF by default; the list of their environment variables is at the end of
this file.

## What it is

In FT2 mode the superldpc stage rejects a decoded word when more than 22 hard
decisions had to be flipped (`superldpc_max_hard()` in
`Detector/superldpc/decodium_bridge.cpp`; FT8 mode uses 58, words with all 77
message bits known a priori use 58). `DECODIUM_LDPC_MAX_HARD` forces a value.
The cap was calibrated on on-air traffic (28/08/2026: 74 true decodes had at
most 20 flips, ghost callsigns started at 23).

Other guards on the FT2 path: CRC-14, the OSD distance limit `nd_max` 0.065,
the plausibility filter inside the OSD (`plausible.hpp`), the AP consistency
check, `is_plausible_ft2_decoded_message()` and the 48/60 limit in
`FtxFt2Stage7.cpp`, and the sync thresholds. The learned gate is off in FT2.

## Measurement

Full FT2 decoder (`ft2_stage_compare --stages 7 --ndepth 3`, one slot per
process) on synthetic 3.75 s slots, Gaussian noise, SNR in 2500 Hz. A decode
that is not in the truth list of its slot is a false decode. Slot kinds:

- weak: 8 separated signals at -17..-12 dB (decoding threshold is about -15 dB)
- strong: 3 signals at +8..+25 dB plus 5 weak ones
- crowd: 14 overlapping signals, -17..+10 dB
- qrmsig: 6 weak signals under carriers, FT4 signals and impulses
- noise: noise only
- qrm: carriers, FT4 signals, keyed carriers, impulses; no FT2 signal

Four independent seed sets: T (tuning), H (held-out), V (validation) and W
(built and decoded afterwards by an independent review; signal slots as in
H, 3000 noise and 600 qrm slots).

### Plain cap (`DECODIUM_LDPC_MAX_HARD`)

Held-out set H, decodes found / false decodes:

| cap | weak (3200 tx) | strong (1600) | crowd (2100) | qrmsig (1200) | noise (3000 slots) | qrm (600 slots) |
|-----|----------------|---------------|--------------|---------------|--------------------|-----------------|
| 22  | 1435 / 0       | 1098 / 0      | 1787 / 0     | 48 / 0        | 0                  | 0               |
| 26  | 1614 / 1       | 1167 / 3      | 1817 / 1     | 53 / 0        | 8                  | 0               |
| 30  | 1702 / 10      | 1198 / 9      | 1829 / 9     | 53 / 3        | 55                 | 4               |
| 34  | 1727 / 33      | 1202 / 18     | 1831 / 23    | 52 / 13       | (T: 303)           | 23              |
| 40  | 1729 / 124     | 1203 / 49     | 1834 / 69    | 52 / 68       | (T: 1029)          | 220             |
| 58  | 1731 / 235     | 1203 / 90     | 1831 / 119   | 53 / 177      | not run            | 1092            |
| off | 1731 / 235     | 1203 / 90     | 1831 / 119   | 53 / 179      | not run            | 1112            |

Tuning set T: weak 1441/0, 1648/0, 1742/9, 1769/44, 1772/136, 1771/219 at
22/26/30/34/40/58; noise-only 3000 slots: 0, 6, 76, 303, 1029 false decodes at
22/26/30/34/40.

The cap costs decodes (weak slots: +12% at 26, +19% at 30) but no plain value
above 22 is safe: noise-only slots already give false decodes at 26.

The cap of 22 is not false-decode free either. It gave no false decode on T, H
and V (3700 slots with signals, 6000 noise, 1200 qrm) but one on W
("CQ 0X9URC R KR58" in a weak slot; 950 slots with signals, 3000 noise, 600
qrm): 1 false decode in 4650 slots with signals, 0 in 10800 slots without.
The options below are compared against that rate.

### Cap as a function of sync quality (`DECODIUM_FT2_MAX_HARD_SYNC`)

With the cap removed, dmin, the OSD distance and the LLR magnitudes of false
words overlap those of true words in the 23..34 band. The sync does not: false
words on noise have `ft2_sync_quality()` around 18 of 32 and `smax` below about
1.8, true ones around 27 and above 1.8. The rule: cap C when nsync >= 22 and
smax >= 1.9, else 22. Thresholds were fixed on T before H, V and W were
decoded.

The main pass applies the rule to the candidate's own nsync and smax. The
local-rescue and drift-rescue retries apply it to the nsync of the retried
demodulation and to the smax of the candidate they retry (smax comes from the
sync search, which a retry does not repeat). The accumulation, averaged and
external entry paths never raise the cap. All numbers below are for the rule
as implemented, retries included.

Decodes found / false decodes, slots with signals:

| rule        | T (950 slots) | H (950 slots) | V (1800 slots) | W (950 slots) | total (4650) |
|-------------|---------------|---------------|----------------|---------------|--------------|
| 22 (base)   | 4368 / 0      | 4368 / 0      | 8760 / 0       | 4486 / 1      | 21982 / 1    |
| C = 24      | 4477 / 0      | 4476 / 1      | 8984 / 0       | 4618 / 1      | 22555 / 2    |
| C = 26      | 4561 / 0      | 4557 / 1      | 9140 / 1       | 4701 / 1      | 22959 / 3    |
| C = 28      | 4621 / 0      | 4597 / 2      | 9243 / 1       | 4743 / 1      | 23204 / 4    |
| C = 30      | 4664 / 0      | 4622 / 3      | 9309 / 6       | 4776 / 4      | 23371 / 13   |

That is +2.6%, +4.4%, +5.6% and +6.3% decodes for 1, 2, 3 and 12 false decodes
more than the base (39900 transmissions).

Weak slots only (2000 slots, 16000 transmissions): 7227, 7623, 7887, 8051,
8168 decodes at 22/24/26/28/30, with 1, 1, 1, 1, 5 false decodes.

No-signal slots: C = 30 gave 0 false decodes in 9000 noise-only slots (T, H,
W) and 0 in 1800 qrm slots; C = 24, 26 and 28 gave 0 in 3000 noise-only and
600 qrm slots (W only). The plain 22 gave 0 on the same slots.

The false decodes that remain are wrong words found on a real signal that did
not decode (or on the residue of a subtracted strong one): the sync is good
because a signal is there. 5 of the 6 on V are in the strong slots; the 3
extra ones on W are in weak slots.

Decode time: no measurable change. Two sets of three interleaved runs (200
and 210 slots, one process per slot, shared machine) differ by less than
the run-to-run spread of about 5%.

## Result

The rule is not false-decode neutral (2, 3, 4 and 13 false decodes at C = 24,
26, 28 and 30 against 1 at 22 in 4650 slots with signals), so it is OFF by
default and FT2 decodes exactly as before. It is experimental. To try it on
the air:

    DECODIUM_FT2_MAX_HARD_SYNC=26        raised cap (unset or <= 22: off)
    DECODIUM_FT2_MAX_HARD_SYNC_NSYNC=22  minimum ft2_sync_quality()
    DECODIUM_FT2_MAX_HARD_SYNC_SMAX=1.9  minimum smax

`DECODIUM_LDPC_MAX_HARD` still overrides everything. FT8 mode ignores the
request.

Limits of the measurement: synthetic AWGN slots only, no fading or drift, no
on-air recordings; only `--ndepth 3`; my call / DX call a-priori passes and
the cross-slot state were not exercised (one process per slot). The false
decode counts are small (1 to 13 events), so the rates are known only roughly.

## Experimental environment variables added with these notes

The project keeps no central list of `DECODIUM_*` variables; these four are
documented here. All are read once per process, are receiver-only and change
nothing when unset.

| variable | default | effect |
|----------|---------|--------|
| `DECODIUM_FT2_MAX_HARD_SYNC` | unset (off) | raised flipped-bit cap for sync-confirmed FT2 candidates; values <= 22 or > 174 mean off |
| `DECODIUM_FT2_MAX_HARD_SYNC_NSYNC` | 22 | minimum `ft2_sync_quality()` for the raised cap |
| `DECODIUM_FT2_MAX_HARD_SYNC_SMAX` | 1.9 | minimum smax for the raised cap |
| `DECODIUM_FT2_METRIC` | unset (off) | FT2 bit metric: `1` or `i0` = log I0, `2` or `amp` = amplitude max-log; any other value keeps the existing metric (see `doc/FT2_BIT_METRIC_VARIANTS.md`) |
