#!/usr/bin/env python3
"""Explore bit-exact SSE2 multiplication of the 49-tap audio FIR."""
from pathlib import Path
import re
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
source=(root/'src/bridge/DecodiumAudioSink.h').read_text()
m=re.search(r'static constexpr std::array<float, kFil4Taps> weights \{\{(.*?)\}\};',source,re.S)
if not m:raise RuntimeError('weights not found')
program=r'''
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <immintrin.h>
#include <iostream>
#include <random>
float w[49]={ WEIGHTS };
__attribute__((noinline)) float oldsum(float const* d) {
 float s=0; for(int i=0;i<49;++i) s+=w[i]*d[i]; return s;
}
__attribute__((noinline)) float newsum(float const* d) {
 float s=0; alignas(16) float p[4]; int i=0;
 for(;i+3<49;i+=4) {
  _mm_store_ps(p,_mm_mul_ps(_mm_loadu_ps(w+i),_mm_loadu_ps(d+i)));
  s+=p[0];s+=p[1];s+=p[2];s+=p[3];
 }
 for(;i<49;++i)s+=w[i]*d[i]; return s;
}
int main(){
 std::mt19937 rng(912781); float d[49];
 for(int j=0;j<100000;++j){for(float &x:d)x=static_cast<int>(rng()%65536)-32768;
  float a=oldsum(d),b=newsum(d); if(std::memcmp(&a,&b,sizeof(a))) {std::cerr<<"mismatch "<<j<<"\n";return 1;}}
 volatile float sink=0;
 for(int pass=0;pass<4;++pass){bool old=pass==0||pass==3; auto start=std::chrono::steady_clock::now();
  for(int j=0;j<1000000;++j){d[j%49]=static_cast<float>((j%65536)-32768);sink+=old?oldsum(d):newsum(d);}
  auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
  std::cout<<(old?"scalar ":"sse2 ")<<ns/1000000.0<<" ms\n";
 }
 std::cout<<"100000 exact sums; guard "<<sink<<"\n";
}
'''.replace('WEIGHTS',m.group(1))
with tempfile.TemporaryDirectory(prefix='audio-fil4-simd-') as tmp:
 p=Path(tmp)/'probe.cpp'; p.write_text(program)
 for opt in ('-Os','-O2'):
  exe=Path(tmp)/'probe'
  subprocess.run(['c++',opt,'-std=c++17','-msse2','-ffp-contract=off',str(p),'-o',str(exe)],check=True)
  print(opt,flush=True)
  subprocess.run([str(exe)],check=True)
