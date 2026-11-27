#!/usr/bin/env python3
"""Compare production FT8 SIMD and scalar sync metric arrays."""
import ctypes as c
import os
from pathlib import Path
import random
import subprocess
import tempfile
import time

root=Path(__file__).resolve().parents[2]
source=(root/'Detector/FtxSync.cpp').read_text()
anchor='#define DECODIUM_FT8_SYNC_SSE2 1'
if source.count(anchor)!=1:raise RuntimeError('unexpected FT8 SIMD guard')
scalar=source.replace(anchor,'// Scalar reference for laboratory comparison',1)
with tempfile.TemporaryDirectory(prefix='decodium-ft8-simd-') as tmp:
    funcs=[]
    for name,text in [('old',scalar),('new',source)]:
        cpp=Path(tmp)/(name+'.cpp');so=Path(tmp)/(name+'.so');cpp.write_text(text)
        opt=os.environ.get('DECODIUM_LAB_CXX_OPT','-Os')
        if opt not in ('-O0','-O1','-O2','-O3','-Os'):raise ValueError('invalid optimization')
        subprocess.run(['g++',opt,'-fcx-limited-range','-std=c++17','-fPIC','-shared',str(cpp),'-o',str(so)],check=True)
        lib=c.CDLL(str(so));f=lib.ftx_sync8var_scan_c
        f.argtypes=[c.POINTER(c.c_float),c.c_int,c.c_int,c.c_int,c.c_int,c.c_int,c.c_int,c.c_int,c.c_int,c.c_int,c.POINTER(c.c_float),c.POINTER(c.c_byte)]
        funcs.append(f)
    rng=random.Random(20261115);nh1=96;rows=372
    data=(c.c_float*(nh1*rows))(*(rng.uniform(0,100) for _ in range(nh1*rows)))
    def run(fn,lo,hi,jlo,jhi,ipass,lagcc,bail):
        n=nh1*(jhi-jlo+1);scores=(c.c_float*n)();flags=(c.c_byte*n)()
        fn(data,nh1,rows,lo,hi,jlo,jhi,ipass,lagcc,bail,scores,flags)
        return bytes(scores),bytes(flags)
    for case in range(500):
        if case%50==0:
            for n in range(len(data)):data[n]=rng.uniform(0,100)
        lo=rng.randint(1,40);hi=rng.randint(lo,nh1-16);jlo=rng.randint(-62,20);jhi=rng.randint(jlo,min(62,jlo+20))
        args=(lo,hi,jlo,jhi,rng.randint(1,3),rng.randrange(2),rng.randrange(2))
        old,new=[run(fn,*args) for fn in funcs]
        if old!=new:raise RuntimeError('mismatch '+repr(args))
    for special in (0.0, -0.0, float('inf'), float('-inf'), float('nan'), 1e38, 1e-40):
        for n in range(len(data)):data[n]=0.0
        data[nh1*120+20]=special
        for args in ((1,nh1-16,-62,62,1,0,0),(1,nh1-16,-12,12,2,1,0)):
            old,new=[run(fn,*args) for fn in funcs]
            if old!=new:raise RuntimeError('special mismatch '+repr((special,args)))
    for n in range(len(data)):data[n]=rng.uniform(0,100)
    def bench(fn):
        start=time.process_time()
        for _ in range(8):run(fn,1,nh1-16,-62,62,1,0,0)
        return 1000*(time.process_time()-start)
    a,b,c1,d=(bench(funcs[i]) for i in (0,1,1,0))
    print(f'cases=500 arrays_identical=1 old_ms={a:.1f},{d:.1f} new_ms={b:.1f},{c1:.1f}')
