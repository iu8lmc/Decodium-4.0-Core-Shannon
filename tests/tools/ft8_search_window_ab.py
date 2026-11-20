#!/usr/bin/env python3
"""Compare complete FT8 search outputs before/after window caching."""
import argparse
import ctypes as c
from pathlib import Path
import time
import wave

p = argparse.ArgumentParser()
p.add_argument('--before', type=Path, required=True)
p.add_argument('--after', type=Path, required=True)
p.add_argument('--wav', type=Path, required=True)
a = p.parse_args()
with wave.open(str(a.wav), 'rb') as wav:
    if (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) != (1, 2, 12000):
        raise ValueError('expected 12000 Hz mono 16-bit WAV')
    pcm = wav.readframes(wav.getnframes())
samples = (c.c_int16*(len(pcm)//2)).from_buffer_copy(pcm)
data = (c.c_float*len(samples))(*samples)
libs = [c.CDLL(str(x.resolve())) for x in (a.before, a.after)]
fp = c.POINTER(c.c_float)
for lib in libs:
    lib.ftx_sync8_search_c.argtypes = [fp,c.c_int,c.c_float,c.c_float,c.c_float,c.c_float,c.c_int,fp,c.POINTER(c.c_int),fp]
    lib.ftx_sync8_search_stage4_c.argtypes = [fp,c.c_int,c.c_float,c.c_float,c.c_float,c.c_float,c.c_int,c.c_int,c.c_int,fp,c.POINTER(c.c_int),fp]

def run(lib, stage4):
    maxcand = 300
    candidates = (c.c_float*(4*maxcand))()
    baseline = (c.c_float*1920)()
    count = c.c_int()
    common = (data, len(data), 300.0, 2700.0, 1.3, 1500.0, maxcand)
    if stage4:
        lib.ftx_sync8_search_stage4_c(*common, 1, 20, candidates, c.byref(count), baseline)
    else:
        lib.ftx_sync8_search_c(*common, candidates, c.byref(count), baseline)
    return count.value, bytes(candidates), bytes(baseline)

for stage4 in (False, True):
    before, after = [run(lib, stage4) for lib in libs]
    if before != after:
        raise SystemExit('different search outputs, stage4='+str(stage4))
    def bench(lib):
        start = time.process_time()
        for _ in range(3): run(lib, stage4)
        return (time.process_time()-start)*1000
    b1, a1, a2, b2 = bench(libs[0]), bench(libs[1]), bench(libs[1]), bench(libs[0])
    print(f'stage4={stage4} candidates={before[0]} identical=1 '
          f'before_ms={b1:.1f},{b2:.1f} after_ms={a1:.1f},{a2:.1f}')
