#!/usr/bin/env python3
"""Compare sync-search outputs for many frequency ranges and inputs."""
import argparse
import ctypes as c
from pathlib import Path
import random
import time
import wave

p=argparse.ArgumentParser()
p.add_argument('--before',type=Path,required=True)
p.add_argument('--after',type=Path,required=True)
p.add_argument('--ft2',type=Path,required=True)
p.add_argument('--ft4',type=Path,required=True)
a=p.parse_args()
libs=[c.CDLL(str(x.resolve())) for x in (a.before,a.after)]
fp=c.POINTER(c.c_float)
for lib in libs:
    for mode in (2,4):
        fn=getattr(lib,f'ftx_ft{mode}_sync_candidates_c')
        fn.argtypes=[fp,c.c_int,c.c_float,c.c_float,c.c_int,fp,fp]
        fn.restype=c.c_int

def wav_samples(path,n):
    with wave.open(str(path),'rb') as f:
        if (f.getnchannels(),f.getsampwidth(),f.getframerate()) != (1,2,12000):
            raise ValueError('expected 12000 Hz mono 16-bit WAV')
        raw=f.readframes(f.getnframes())
    values=(c.c_int16*(len(raw)//2)).from_buffer_copy(raw)
    return (c.c_float*n)(*list(values)[:n])

rng=random.Random(762)
for mode,path,n in ((2,a.ft2,45000),(4,a.ft4,72576)):
    wav=wav_samples(path,n)
    noise=(c.c_float*n)(*(rng.uniform(-1000,1000) for _ in range(n)))
    silence=(c.c_float*n)()
    ranges=[(0,50),(0,500),(300,700),(900,1100),(500,1500),(0,5000),(4000,5500),(7000,7100)]
    for _ in range(40):
        lo=rng.uniform(-100,5800)
        ranges.append((lo,lo+rng.uniform(1,1500)))
    def run(lib,data,lo,hi,maxn):
        freq=(c.c_float*maxn)();score=(c.c_float*maxn)()
        count=getattr(lib,f'ftx_ft{mode}_sync_candidates_c')(data,n,lo,hi,maxn,freq,score)
        return count,bytes(freq),bytes(score)
    checked=0
    for data in (wav,noise,silence):
        for lo,hi in ranges:
            for maxn in (1,6,40):
                old,new=[run(lib,data,lo,hi,maxn) for lib in libs]
                if old != new: raise SystemExit(f'FT{mode}: mismatch range={lo},{hi} maxn={maxn}')
                checked+=1
    def bench(lib):
        t=time.process_time()
        for _ in range(10):run(lib,wav,900,1100,40)
        return (time.process_time()-t)*1000
    b1,a1,a2,b2=bench(libs[0]),bench(libs[1]),bench(libs[1]),bench(libs[0])
    print(f'FT{mode} cases={checked} identical=1 narrow_band_ms '
          f'before={b1:.1f},{b2:.1f} after={a1:.1f},{a2:.1f}',flush=True)
