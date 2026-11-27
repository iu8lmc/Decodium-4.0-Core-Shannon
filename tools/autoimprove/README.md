# Self-improvement loop for the Decodium receiver (RX)

One scoreboard, one rule set, one change at a time. Nothing here transmits, opens a serial
port or touches the user's radio profile: it runs offline bench executables on audio with
known truth.

## Scoreboard

```
cd <repo>
export PATH=/c/msys64/mingw64/bin:$PATH
cd build_689 && ninja ft2_async_bench ft8_stage_compare ft2_stage_compare ft2_make_test_wav
cd ..
python tools/autoimprove/rx_score.py run --build build_689 --label base     # ~10 min
# ... change the code, rebuild ...
python tools/autoimprove/rx_score.py run --build build_689 --label cand1
python tools/autoimprove/rx_score.py compare tools/autoimprove/results/base.json tools/autoimprove/results/cand1.json
```

| suite | what it measures | why it is there |
|---|---|---|
| `ft2` | `ft2_async_bench`: 240 s FT2 scenes with truth (AWGN), decoded / false / CPU per dispatch / latency / 50 % threshold | the path the app really uses (async decoder), cost included |
| `ft2real` | synthetic FT2 slots with known truth summed into REAL band recordings | real QRM and filter shapes; the lab found the losses are in candidate search, which AWGN scenes hide |
| `ft8` | `decode_bench/bench.py`, single signal at known SNR, prod profile | the 50 % threshold in dB is the only sensitivity number that counts |

TRAIN scenes (`t7 t11 t13`, recording `20260907_180610`) are the ones a change is tuned on.
HELDOUT scenes (`h101 h102`, recording `20260909_145733`) are only used to confirm: a change
that gains on TRAIN and loses on HELDOUT is overfitting and is reverted.

Scenes and slot sets are generated once into `%TEMP%/decodium_autoimprove` and then reused;
their hashes are stored in the results and `compare` refuses to compare different audio.

## Rules (`RULES` in rx_score.py)

A candidate is KEPT only if all hold: measurable gain on TRAIN (>= 2 decoded transmissions in
`ft2`, >= 3 slots in `ft2real`, or >= 0.3 dB in `ft8`), nothing lost on HELDOUT, no new false
decodes, CPU per dispatch not above +10 %, latency not above +0.1 s, FT8 threshold not worse by
more than 0.2 dB. Otherwise it is reverted and the reason is written in `LOG.md`.

## Loop (one change per round)

1. Measure the baseline (`run --label base`).
2. Pick ONE hypothesis from `LOG.md` (open ideas) and write it down before touching code.
3. Implement it behind an environment switch or a default-off flag, so the baseline stays
   bit-identical and the change can be A/B tested in the same binary when possible.
4. `run` + `compare`. KEEP -> commit on the `rx-autoimprove` branch with the numbers in the
   message. REVERT -> `git checkout` the files and add the result (also negative ones) to `LOG.md`.
5. Nothing reaches a release without the owner's explicit OK. Nothing is tried on the real radio.

## Known limits

* Synthetic truth is not the air: a gain here is a candidate for a field test, not a result.
* `ft2` CPU and latency come from measured CPU time, so they carry run-to-run noise: run `noise`
  (same as `run`) twice and look at the spread before trusting a small CPU delta.
* Few levels and seeds on purpose (speed): enlarge them before a release-level decision.
