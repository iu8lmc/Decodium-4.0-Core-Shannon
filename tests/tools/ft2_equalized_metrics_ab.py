#!/usr/bin/env python3
"""Compare all FT2 diagnostic metrics from two shared-library builds."""
import argparse
import ctypes as c
from pathlib import Path
import random
import time

p = argparse.ArgumentParser()
p.add_argument('--before', type=Path, required=True)
p.add_argument('--after', type=Path, required=True)
p.add_argument('--bench', action='store_true', help='time complete diagnostic passes on equalized inputs')
a = p.parse_args()
libs = [c.CDLL(str(path.resolve())) for path in (a.before, a.after)]

def run(lib, signal):
    final, base = (c.c_float*618)(), (c.c_float*618)()
    raw, eq = (c.c_float*206)(), (c.c_float*206)()
    cdeq, snr = (c.c_float*(103*32*2))(), (c.c_float*103)()
    bad, used = c.c_int(), c.c_int()
    stats = [c.c_float() for _ in range(6)]
    lib.ftx_ft2_bitmetrics_diag_c(signal, final, base, raw, eq, cdeq, snr,
                                  c.byref(bad), c.byref(used),
                                  *[c.byref(value) for value in stats])
    return (bytes(final), bytes(base), bytes(raw), bytes(eq), bytes(cdeq),
            bytes(snr), bad.value, used.value, *[bytes(value) for value in stats])

rng = random.Random(423)
equalized = 0
equalized_inputs = []
for case in range(32):
    signal = (c.c_float*(103*32*2))()
    for i in range(len(signal)):
        signal[i] = rng.uniform(-1, 1) * (0.05 + ((i//64) % 20)/5)
    before, after = [run(lib, signal) for lib in libs]
    if before != after:
        raise SystemExit('different FT2 metrics in case '+str(case))
    equalized += before[7]
    if before[7]:
        equalized_inputs.append(signal)
if not equalized:
    raise SystemExit('no input exercised the equalized branch')
print(f'full FT2 cases=32 matched=32 equalized branch={equalized}')
if a.bench:
    def bench(lib):
        start = time.process_time()
        for _ in range(20):
            for signal in equalized_inputs:
                result = run(lib, signal)
                if not result[7]:
                    raise RuntimeError('equalized branch was not used')
        return (time.process_time() - start) * 1000
    before1, after1, after2, before2 = bench(libs[0]), bench(libs[1]), bench(libs[1]), bench(libs[0])
    print(f'complete diagnostics passes={20*len(equalized_inputs)} '
          f'before_ms={before1:.1f},{before2:.1f} after_ms={after1:.1f},{after2:.1f}')
