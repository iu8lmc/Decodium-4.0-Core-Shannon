#!/usr/bin/env python3
"""Compare FT2/FT4 candidate-search outputs across two builds."""
import argparse
import ctypes as c
from pathlib import Path
import time
import wave

p = argparse.ArgumentParser()
p.add_argument('--before', type=Path, required=True)
p.add_argument('--after', type=Path, required=True)
p.add_argument('--ft2', type=Path, required=True)
p.add_argument('--ft4', type=Path, required=True)
a = p.parse_args()
libs = [c.CDLL(str(x.resolve())) for x in (a.before, a.after)]
fp = c.POINTER(c.c_float)
for lib in libs:
    for mode in (2, 4):
        getattr(lib, f'ftx_getcandidates{mode}_c').argtypes = [fp,c.c_float,c.c_float,c.c_float,c.c_float,c.c_int,fp,fp,c.POINTER(c.c_int),fp]
        getattr(lib, f'ftx_ft{mode}_sync_candidates_c').argtypes = [fp,c.c_int,c.c_float,c.c_float,c.c_int,fp,fp]

def samples(path, nmax):
    with wave.open(str(path), 'rb') as wav:
        if (wav.getnchannels(),wav.getsampwidth(),wav.getframerate()) != (1,2,12000):
            raise ValueError('expected 12000 Hz mono 16-bit WAV')
        pcm=wav.readframes(wav.getnframes())
    raw=(c.c_int16*(len(pcm)//2)).from_buffer_copy(pcm)
    return (c.c_float*nmax)(*list(raw)[:nmax])

for mode,path,nmax,nh1 in ((2,a.ft2,45000,576),(4,a.ft4,72576,1152)):
    audio=samples(path,nmax)
    for sync in (False, True):
        def run(lib):
            maxcand=300
            if sync:
                freqs=(c.c_float*maxcand)(); scores=(c.c_float*maxcand)()
                n=getattr(lib,f'ftx_ft{mode}_sync_candidates_c')(audio,nmax,500.0,1500.0,maxcand,freqs,scores)
                return n,bytes(freqs),bytes(scores)
            avg=(c.c_float*nh1)(); candidates=(c.c_float*(2*maxcand))(); base=(c.c_float*nh1)(); n=c.c_int()
            getattr(lib,f'ftx_getcandidates{mode}_c')(audio,500.0,1500.0,1.3,1000.0,maxcand,avg,candidates,c.byref(n),base)
            return n.value,bytes(avg),bytes(candidates),bytes(base)
        before,after=[run(lib) for lib in libs]
        if before != after: raise SystemExit(f'FT{mode} sync={sync}: outputs differ')
        def bench(lib):
            start=time.process_time()
            for _ in range(3): run(lib)
            return (time.process_time()-start)*1000
        b1,a1,a2,b2=bench(libs[0]),bench(libs[1]),bench(libs[1]),bench(libs[0])
        print(f'FT{mode} sync={sync} candidates={before[0]} identical=1 '
              f'before_ms={b1:.1f},{b2:.1f} after_ms={a1:.1f},{a2:.1f}',flush=True)
