#!/usr/bin/env python3
"""Compare complete FT8 metric outputs across two shared-library builds."""
import argparse
import ctypes as c
from pathlib import Path
import random
import time

p = argparse.ArgumentParser()
p.add_argument('--before', type=Path, required=True)
p.add_argument('--after', type=Path, required=True)
a = p.parse_args()
libs = [c.CDLL(str(x.resolve())) for x in (a.before, a.after)]

def run(lib, data, offset, metric):
    spectrum = (c.c_float*(79*8))()
    nsync = c.c_int()
    llrs = [(c.c_float*174)() for _ in range(5)]
    lib.ftx_ft8_bitmetrics_c(data, 3000, offset, metric, spectrum, c.byref(nsync), *llrs)
    return bytes(spectrum), nsync.value, *[bytes(x) for x in llrs]

rng = random.Random(7481)
inputs = []
for index in range(32):
    data = (c.c_float*6000)()
    for i in range(6000):
        data[i] = rng.uniform(-1, 1) * (0.05 + (i//64 % 13)/7)
    offset = [-30, 0, 20, 460][index % 4]
    metric = index % 4 + 1
    before, after = [run(lib, data, offset, metric) for lib in libs]
    if before != after:
        raise SystemExit('different FT8 metrics in case '+str(index))
    inputs.append((data, offset, metric))

def bench(lib):
    start = time.process_time()
    for _ in range(10):
        for data, offset, metric in inputs:
            run(lib, data, offset, metric)
    return (time.process_time()-start)*1000

b1, a1, a2, b2 = bench(libs[0]), bench(libs[1]), bench(libs[1]), bench(libs[0])
print(f'cases=32 bit_identical=1 passes=320 before_ms={b1:.1f},{b2:.1f} after_ms={a1:.1f},{a2:.1f}')
