# RX self-improvement log

Newest first. Every round: hypothesis, measurement, verdict. Negative results stay here so they
are not tried again. Numbers: `rx_score.py` on build 1.0.708 + the change, same scenes.

## Round 3 (2026-10-23) - the same sync-word search for FT4

Question: does FT4 gain from candidates found through the sync words? Yes.
New tools: `tests/ft4_make_test_wav` (FT4 clean signal), `mescola_ft4.py`/`verita_ft4.py` (real-noise
slots, one decoder process per slot: `ft4_stage_compare` takes ONE wav), `ft4_awgn.py` (white-noise
threshold), suite `ft4real` in `rx_score.py` (levels -15..-6 dB, 3 recordings, 200 slots each).

`ftx_ft4_sync_candidates_c` is the FT2 search with FT4 geometry (symbol 576 samples, hop 144, FFT 2304; still
4 frames per symbol and 4 cells per tone). Decoder: `DECODIUM_FT4_SYNC2D` / `ftx_ft4_set_sync_candidates_c`
(DecodiumBridge sets mode 1), `_N` default 6, `_LEGACY` default 5.

| | f4train | f4held | f4held2 | white noise -21..-17 (40 each) | false lines, 960 bg slots |
|---|---|---|---|---|---|
| 1.0.709 | 96 | 55 | 90 | 156 | 3 |
| mode 1 | 125 (+30 %) | 90 (+64 %) | 118 (+31 %) | 156 | 2 (+1 in another 240) |

TRAP found on the way: FT4's spectral candidate list is ordered by FREQUENCY, not by power (FT2's is by power).
Keeping "the first 5" legacy candidates dropped the weak true signal in white noise (-20 dB: 9/20 instead of
14/20); the legacy candidates must be taken by descending power (`by_power`). With that, white noise is identical
to the baseline. Time per set also drops (13 s -> 8 s on f4train).
The FT4 decoder already produced a few garbage lines on plain background (3 in 960 slots) before; mode 1 does not
add to them.

## Round 2 (2026-10-22) - a better centre estimate was the wrong fix; sync-pattern candidates

Question: can a more precise centre of the 4-tone group (instead of the tone-alias trick of round 1)
give the same gain without the CPU cost?  `centre_lab.py` replays the dumped spectra against the truth:
a matched filter on the measured lump shape (median of 50 slots: a flat 13-bin / 135 Hz plateau with
3-bin edges, +6.8 dB at -3 dB) recalls no more true frequencies than the decoder's own 15-cell box
(k=11 at -3 dB: 36 / 34 / 36 of 50). Looking at the missed slots: the injected lump sits on the shoulder
of real stations 15-30 dB stronger; the peak of the slot-average spectrum is the NEIGHBOUR, and no
estimator on that spectrum can find a weak lump hidden by a strong one. Centre estimators: dead end.

What works: candidates from the SYNC WORDS (`ftx_ft2_sync_candidates_c`, FtxCandidates.cpp), the way FT8's
sync2d does it. Spectrogram of one-symbol frames (288 samples, hop 72, FFT 1152), and for every base
frequency and start the 16 known sync symbols (4 words x 4 symbols at 0/33/66/99) each scored as power of
the right tone over the four tones of that symbol (a steady strong interferer raises all four and does not
win). Best start per frequency, local maxima +-3 bins. Prototype recall at k=11, -3 dB: 43/32/43 of 50
(decoder 36/16/29); at -6 dB 31/25/36 (decoder 19/16/22).

Stage7: `DECODIUM_FT2_SYNC2D` / `ftx_ft2_set_sync_candidates_c` (0 off = bit-identical, 1 = sync candidates
first, then the first 5 spectral-peak candidates not within 10 Hz of them, 2 = sync candidates only),
`..._N` (default 6) and `..._LEGACY` (default 5). The SNR shown for the added candidates comes from the
lump ratio at their frequency (`ft2_lump_snr_field`). DecodiumBridge sets mode 1 for everybody.

| | ft2real TRAIN | HELDOUT (2 recordings, 400 slots) | AWGN decoded (T/H) | CPU / dispatch | latency | false |
|---|---|---|---|---|---|---|
| 1.0.708 | 81 | 96 | 140 / 91 | 226 ms | 0.40 s | 0 |
| mode 1, N=6, legacy 5 | 105 (+30 %) | 170 (+77 %) | 141 / 93 | 67 ms (-70 %) | 0.37-0.41 s | 0 |

`rx_score.py compare` verdict KEEP under the original rules. Genie ceiling (true frequency handed to the
decoder) was 108 / 71 on the first two sets: the sync candidates reach 105 / 68, i.e. the candidate search
is no longer the bottleneck. No false lines on the injected sets, the AWGN scenes, nor 1200 slots of plain
recorded background (400 each from three recordings).
Why the CPU falls: ~9 candidates instead of 11-15, and the strong stations are found and subtracted first.
Mode 2 (sync only) is not safe: weak signals live on the whole burst's energy in the average spectrum
(sync words are 15 % of it): AWGN decoded 129/240, 88/160.

Bench change: `ft2_async_bench` now accepts a decode shown up to 0.5 s before the end of the transmission
(CRC-valid decode of a burst missing its last symbols); before, it was counted as false.

The round-1 tone-alias candidates are REMOVED: on top of the sync candidates they add nothing (+0 / +1).

## Round 1 (2026-10-22) - candidate search on real noise

Baseline (1.0.708, FT2): ft2real decoded TRAIN 81/200, HELDOUT 35/200 (rheld) and 61/200 (rheld2).
AWGN scenes: 140/240 train, 91/160 heldout, 228 ms CPU per dispatch, 0.40 s latency.

Where the headroom is: forcing the true frequency onto the decoder (genie, `DECODIUM_FT2_GENIE_F`)
gives 81 -> 108 on TRAIN and 35 -> 71 on HELDOUT. So on real band noise the candidate SEARCH loses up
to a third of the decodable frames (AWGN scenes never show it: there the search is already exact).
Candidate trace of the 119 missed TRAIN slots: only 35 had a candidate within +-12 Hz of the truth;
48 had it one tone (41.67 Hz) away, 16 two tones away, 5 had none. The peak of the 15-cell smoothed
spectrum (FtxCandidates.cpp) lands on the wrong tone of the 4-tone FT2 lump when the band is not white.

### Superseded by round 2: tone-alias candidates (removed)
`DECODIUM_FT2_ALIAS=n` / `ftx_ft2_set_tone_alias_c(n)`: for every candidate also try f0 +- k*41.67 Hz,
k = 1..n, as extra candidates appended after the normal ones; first search at double step; an alias
is dropped unless its sync (smax) beats its parent's by 0.10; skipped if the parent already decoded.
Default OFF (bit-identical) unless the app sets it: DecodiumBridge sets 2, and 0 in Low CPU / Slow-PC.

| n | ft2real TRAIN | HELDOUT rheld | HELDOUT rheld2 | AWGN decoded (T/H) | CPU / dispatch | latency |
|---|---|---|---|---|---|---|
| 0 | 81 | 35 | 61 | 140 / 91 | 228 ms | 0.40 s |
| 1 | 89 | 54 | 83 | 140 / 89 | 271 ms (+19 %) | 0.50 s |
| 2 | 96 | 59 | 92 | 141 / 92 | 286 ms (+25 %) | 0.52 s |

No false lines on the injected sets (600 slots) and none on 1000 slots of plain recorded background.
The CPU rule (+10 %) is exceeded on purpose: the gain is real-band only. Slow computers keep it off.

### Rejected / no gain (do not retry)
* `DECODIUM_FONDO=entrambi`: +5 TRAIN, 0 HELDOUT, +17 % CPU. `mediana`: +3 / -2. `0` (off): -10.
* One-at-a-time env switches on ft2real (no gain on both sets): COERENTE (+1/+1), DRIFT_SEARCH (+1/0),
  AP_SOFT (+0, 19 false lines), AP_CQ=0, SYNC_HZ 8 (-3/-5) and 16 (0/+3), LDPC_MAX_HARD 22 (0),
  38 (+1, 85 false lines), 46 (+1, 313 false lines), SUB_NFILT 1000/4000, FONDO_CELLE 9/39,
  STAGE7_LOCAL_RESCUE_BUDGET 64.
* Alias parents limited to the top-N peaks (N = 4, 6, 8): loses most of the gain (the right parent is
  often a weak peak in a busy band).

### Open ideas
* A better lump-centre estimator in getcandidates2 (half-power edges instead of the smoothed peak)
  would give the alias gain without the CPU cost; needs a savg/sbase dump to test offline.
* `ALIAS=3`, and alias only after the parent failed twice (async path).
* Memory-based knobs (ACCUMULO, STORICO) need repeated-station scenes (`mescola_ft2.py --sequenze`).

## 23/10/2026 - spaziatura degli stream MAM per modo

`multistream_scan.py`: N stream a pari ampiezza, rumore bianco. FT2 si decodifica al 98-100% a 170 Hz, 48-92% a 125 Hz, 15-65% a 85 Hz, <10% a 40 Hz; FT4 100% da 85 Hz in su a 3 dB sopra soglia. Il MAM usava 50/60 Hz (taratura FT8) per tutti i modi: ora FT2 170/180, FT4 85/90, FT8 invariato.
