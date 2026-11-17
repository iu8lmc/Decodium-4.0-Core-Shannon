# FT2: the 22-flipped-bit cap of superldpc

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

Three independent seed sets: T (tuning), H (held-out), V (validation).

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

### Cap as a function of sync quality (`DECODIUM_FT2_MAX_HARD_SYNC`)

With the cap removed, dmin, the OSD distance and the LLR magnitudes of false
words overlap those of true words in the 23..34 band. The sync does not: false
words on noise have `ft2_sync_quality()` around 18 of 32 and `smax` below about
1.8, true ones around 27 and above 1.8. The rule: cap C when nsync >= 22 and
smax >= 1.9, else 22. Thresholds were fixed on T before H and V were decoded.

Decodes found / false decodes, slots with signals:

| rule        | T (950 slots) | H (950 slots) | V (1800 slots) | total       |
|-------------|---------------|---------------|----------------|-------------|
| 22 (base)   | 4368 / 0      | 4368 / 0      | 8760 / 0       | 17496 / 0   |
| C = 24      | 4477 / 0      | 4476 / 1      | 8984 / 0       | 17937 / 1   |
| C = 26      | 4561 / 0      | 4557 / 1      | 9140 / 1       | 18258 / 2   |
| C = 28      | 4621 / 0      | 4597 / 2      | 9243 / 1       | 18461 / 3   |
| C = 30      | 4664 / 0      | 4622 / 3      | 9309 / 6       | 18595 / 9   |

Weak slots only (1600 slots, 12800 transmissions): 5771, 6077, 6287, 6419, 6510
decodes at 22/24/26/28/30, with 0, 0, 0, 0, 1 false decodes.

No-signal slots, C = 30: 0 false decodes in 6000 noise-only slots (T and H) and
0 in 1200 qrm slots, the same as the plain 22. C = 24..28 were not run on
no-signal slots.

The false decodes that remain are wrong words found on a real signal that did
not decode (or on the residue of a subtracted strong one): the sync is good
because a signal is there. 5 of the 6 on V are in the strong slots.

Decode time with C = 30: +1.8% on 200 weak slots (24.4 s against 24.0 s, three
runs each).

## Result

The rule is not false-decode neutral, so it is OFF by default and FT2 decodes
exactly as before. To try it on the air:

    DECODIUM_FT2_MAX_HARD_SYNC=26        raised cap (unset or <= 22: off)
    DECODIUM_FT2_MAX_HARD_SYNC_NSYNC=22  minimum ft2_sync_quality()
    DECODIUM_FT2_MAX_HARD_SYNC_SMAX=1.9  minimum smax

`DECODIUM_LDPC_MAX_HARD` still overrides everything. FT8 mode ignores the
request.

Limits of the measurement: synthetic AWGN slots only, no fading or drift, no
on-air recordings; my call / DX call a-priori passes were not exercised.
