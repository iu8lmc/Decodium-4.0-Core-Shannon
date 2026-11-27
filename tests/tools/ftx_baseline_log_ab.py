#!/usr/bin/env python3
"""Check complete background-fit outputs and time two shared libraries."""
import argparse
import ctypes as c
from pathlib import Path
import random
import time

p = argparse.ArgumentParser()
p.add_argument('--before', type=Path, required=True)
p.add_argument('--after', type=Path, required=True)
a = p.parse_args()
libs = [c.CDLL(str(path.resolve())) for path in (a.before, a.after)]
for lib in libs:
    lib.ftx_baseline_fit_c.argtypes = [c.POINTER(c.c_float),c.c_int,c.c_int,c.c_int,c.c_float,c.c_int,c.POINTER(c.c_float)]
rng = random.Random(774)
cases=[]
for index in range(250):
    n = [20, 576, 1152, 1920][index%4]
    spectrum=(c.c_float*n)()
    for i in range(n):
        if index%17 == 0:
            spectrum[i] = [0.0, 1.0, 1.0, float('inf'), float('nan')][i%5]
        elif index%11 == 0:
            spectrum[i] = 0.0
        else:
            spectrum[i] = 10**rng.uniform(-12, 5)
    lo = rng.randrange(1,n+1); hi = rng.randrange(lo,n+1)
    cases.append((spectrum,n,lo,hi,0.5 if index%2 else -0.4,index%2))

def run(lib, case):
    spectrum,n,lo,hi,offset,linear=case
    output=(c.c_float*n)()
    lib.ftx_baseline_fit_c(spectrum,n,lo,hi,offset,linear,output)
    return bytes(output)

for index,case in enumerate(cases):
    if run(libs[0],case) != run(libs[1],case):
        raise SystemExit('different baseline: case '+str(index))

def bench(lib):
    start=time.process_time()
    for _ in range(10):
        for case in cases: run(lib,case)
    return (time.process_time()-start)*1000
b1,a1,a2,b2=bench(libs[0]),bench(libs[1]),bench(libs[1]),bench(libs[0])
print(f'cases=250 identical=1 passes=2500 before_ms={b1:.1f},{b2:.1f} after_ms={a1:.1f},{a2:.1f}')
