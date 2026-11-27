#!/usr/bin/env python3
"""Compare preconjugated FT2 sync templates without changing production."""
import ctypes as c
from pathlib import Path
import random
import subprocess
import tempfile
import time

root=Path(__file__).resolve().parents[2]
source=(root/'Detector/FtxSync.cpp').read_text()
# The old prototype altered the scalar templates, not the production SIMD path.
source=source.replace('#define DECODIUM_FT2_SYNC_SSE2 1','// scalar benchmark',1)
old='''        if (!itwk)
          {
            ref[k] = base[k];
            continue;
          }
        for (size_t n = 0; n < 64; ++n)
          {
            storage[k][n] = ctwk[n] * (*base[k])[n];
          }
        ref[k] = &storage[k];'''
new='''        for (size_t n = 0; n < 64; ++n)
          {
            storage[k][n] = std::conj (itwk ? ctwk[n] * (*base[k])[n] : (*base[k])[n]);
          }
        ref[k] = &storage[k];'''
if source.count(old)!=1 or source.count('std::conj (sync[static_cast<size_t> (i)])')!=2 or source.count('std::conj (sync[static_cast<size_t> (64 - count + i)])')!=1:
    raise RuntimeError('unexpected source structure')
variant=source.replace(old,new,1).replace('std::conj (sync[static_cast<size_t> (i)])','sync[static_cast<size_t> (i)]').replace(
    'std::conj (sync[static_cast<size_t> (64 - count + i)])','sync[static_cast<size_t> (64 - count + i)]')
with tempfile.TemporaryDirectory(prefix='decodium-sync-conjugate-') as tmp:
    libs=[]
    for name,text in [('old',source),('new',variant)]:
        cpp=Path(tmp)/(name+'.cpp');so=Path(tmp)/(name+'.so');cpp.write_text(text)
        subprocess.run(['g++','-O3','-std=c++17','-fPIC','-shared',str(cpp),'-o',str(so)],check=True)
        lib=c.CDLL(str(so));f=lib.ftx_sync2d_scan_c
        f.argtypes=[c.POINTER(c.c_float),c.c_int,c.c_int,c.c_int,c.c_int,c.POINTER(c.c_float),c.c_int,c.POINTER(c.c_float),c.POINTER(c.c_int)]
        libs.append(f)
    rng=random.Random(20261115);n=5000
    data=(c.c_float*(2*n))(*(rng.uniform(-2,2) for _ in range(2*n)))
    tweak=(c.c_float*128)(*(rng.uniform(-1,1) for _ in range(128)))
    def run(fn,lo,hi,step,itwk):
        value=c.c_float();index=c.c_int()
        fn(data,n,lo,hi,step,tweak,itwk,c.byref(value),c.byref(index))
        return bytes(value),index.value
    cases=[]
    for _ in range(180):
        lo=rng.randint(-700,4900);hi=min(5000,lo+rng.randint(0,200));step=rng.choice((1,2,4,7))
        cases.append((lo,hi,step,rng.randrange(2)))
    for case in cases:
        if run(libs[0],*case)!=run(libs[1],*case):raise RuntimeError('mismatch '+repr(case))
    def bench(fn):
        start=time.process_time()
        for _ in range(60):run(fn,216,1120,4,1)
        return 1000*(time.process_time()-start)
    times=[bench(libs[i]) for i in (0,1,1,0)]
    print(f'cases={len(cases)} bit_identical=1 old_ms={times[0]:.1f},{times[3]:.1f} new_ms={times[1]:.1f},{times[2]:.1f}')
