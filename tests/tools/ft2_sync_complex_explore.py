#!/usr/bin/env python3
"""Explore explicit complex correlation without changing the decoder."""
import ctypes as c
from pathlib import Path
import random
import subprocess
import tempfile
import time

root=Path(__file__).resolve().parents[2]
source=(root/'Detector/FtxSync.cpp').read_text()
# Isolate the scalar correlation tested by this rejected prototype.
source=source.replace('#define DECODIUM_FT2_SYNC_SSE2 1','// scalar benchmark',1)
anchor='float sync4_power (Complex const* cd0, int np, int start, std::array<Complex, 64> const& sync,'
helper='''
inline Complex sync4_product (Complex a, Complex b)
{
  float const real = a.real() * b.real() + a.imag() * b.imag();
  float const imag = a.imag() * b.real() - a.real() * b.imag();
  if (std::isnan(real) && std::isnan(imag)) return a * std::conj(b);
  return Complex(real, imag);
}
'''
replacements={
 'cd0[start + i * 2] * std::conj (sync[static_cast<size_t> (i)])':
   'sync4_product (cd0[start + i * 2], sync[static_cast<size_t> (i)])',
 'cd0[i * 2] * std::conj (sync[static_cast<size_t> (64 - count + i)])':
   'sync4_product (cd0[i * 2], sync[static_cast<size_t> (64 - count + i)])'}
if source.count(anchor)!=1 or any(source.count(k)!=n for k,n in zip(replacements,(2,1))):
    raise RuntimeError('unexpected source structure')
variant=source.replace(anchor,helper+anchor,1)
for old,new in replacements.items():variant=variant.replace(old,new)
with tempfile.TemporaryDirectory(prefix='decodium-sync-product-') as tmp:
    funcs=[]
    for name,text in [('old',source),('new',variant)]:
        cpp=Path(tmp)/(name+'.cpp');so=Path(tmp)/(name+'.so');cpp.write_text(text)
        subprocess.run(['g++','-O3','-std=c++17','-fPIC','-shared',str(cpp),'-o',str(so)],check=True)
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
    for _ in range(500):
        lo=rng.randint(-700,4900);hi=min(5000,lo+rng.randint(0,250));step=rng.choice((1,2,4,7));itwk=rng.randrange(2)
        if run(funcs[0],lo,hi,step,itwk)!=run(funcs[1],lo,hi,step,itwk):
            raise RuntimeError(f'mismatch lo={lo} hi={hi} step={step} itwk={itwk}')
    def bench(fn):
        start=time.process_time()
        for _ in range(120):run(fn,216,1120,4,1)
        return 1000*(time.process_time()-start)
    a,b,c1,d=(bench(funcs[i]) for i in (0,1,1,0))
    print(f'cases=500 identical=1 old_ms={a:.1f},{d:.1f} new_ms={b:.1f},{c1:.1f}',flush=True)
