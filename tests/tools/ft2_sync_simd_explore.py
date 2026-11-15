#!/usr/bin/env python3
"""Compare production FT2 SIMD against the scalar fallback."""
import ctypes as c
import os
from pathlib import Path
import random
import subprocess
import tempfile
import time

root=Path(__file__).resolve().parents[2]
source=(root/'Detector/FtxSync.cpp').read_text()
anchor='#define DECODIUM_FT2_SYNC_SSE2 1'
if source.count(anchor)!=1:raise RuntimeError('unexpected production SIMD guard')
scalar=source.replace(anchor,'// Scalar reference for laboratory comparison',1)
with tempfile.TemporaryDirectory(prefix='decodium-sync-simd-') as tmp:
    funcs=[]
    for name,text in [('old',scalar),('simd',source)]:
        cpp=Path(tmp)/(name+'.cpp');so=Path(tmp)/(name+'.so');cpp.write_text(text)
        optimization=os.environ.get('DECODIUM_LAB_CXX_OPT','-O3')
        if optimization not in ('-O0','-O1','-O2','-O3','-Os'):raise ValueError('invalid optimization')
        flags=['-fcx-limited-range'] if os.environ.get('DECODIUM_LAB_CX_LIMITED')=='1' else []
        subprocess.run(['g++',optimization,*flags,'-std=c++17','-fPIC','-shared',str(cpp),'-o',str(so)],check=True)
        lib=c.CDLL(str(so));f=lib.ftx_sync2d_scan_c
        f.argtypes=[c.POINTER(c.c_float),c.c_int,c.c_int,c.c_int,c.c_int,c.POINTER(c.c_float),c.c_int,c.POINTER(c.c_float),c.POINTER(c.c_int)]
        funcs.append(f)
    rng=random.Random(20261115);n=5000
    data=(c.c_float*(2*n))(*(rng.uniform(-2,2) for _ in range(2*n)))
    tweak=(c.c_float*128)(*(rng.uniform(-1,1) for _ in range(128)))
    def run(fn,lo,hi,step,itwk):
        value=c.c_float();index=c.c_int()
        fn(data,n,lo,hi,step,tweak,itwk,c.byref(value),c.byref(index))
        return bytes(value),index.value
    for case in range(5000):
        if case and case % 500 == 0:
            for j in range(2*n):data[j]=rng.uniform(-2,2)
            for j in range(128):tweak[j]=rng.uniform(-1,1)
        lo=rng.randint(-700,4900);hi=min(5000,lo+rng.randint(0,350));step=rng.choice((1,2,4,7));itwk=rng.randrange(2)
        old,new=[run(fn,lo,hi,step,itwk) for fn in funcs]
        if old!=new:raise RuntimeError(f'mismatch range={lo}:{hi}:{step} tweak={itwk} old={old} new={new}')
    for special in (0.0,float('inf'),float('-inf'),float('nan'),1e38,1e-40):
        for j in range(2*n):data[j]=0.0
        data[2*500]=special
        for lo,hi,step in ((0,1704,4),(480,520,1),(-100,2100,4)):
            old,new=[run(fn,lo,hi,step,1) for fn in funcs]
            if old!=new:raise RuntimeError(f'special mismatch value={special} range={lo}:{hi}:{step} old={old} new={new}')
    for j in range(2*n):data[j]=rng.uniform(-2,2)
    def bench(fn):
        start=time.process_time()
        for _ in range(150):run(fn,216,1120,4,1)
        return 1000*(time.process_time()-start)
    a,b,c1,d=(bench(funcs[i]) for i in (0,1,1,0))
    print(f'cases=5000 identical=1 old_ms={a:.1f},{d:.1f} new_ms={b:.1f},{c1:.1f}')
