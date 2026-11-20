#!/usr/bin/env python3
"""Compare CRC-aware search with a nearest-codeword-first variant, in isolation.

This does not modify the application. Synthetic Gaussian LLR vectors do not
model the entire radio, synchronizer or candidate-selection pipeline.
"""
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
original = (root / 'Detector/FtxFst4Ldpc.cpp').read_text()
variant = original
old = '''      if (!crc_ok (decoded))
        {
          return;
        }
      best_valid = true;'''
new = '''      best_valid = true;'''
assert variant.count(old) == 1
variant = variant.replace(old, new)
old = 'if (!best_valid && order_limit > 0 && span > 0)'
assert variant.count(old) == 1
variant = variant.replace(old, 'if (order_limit > 0 && span > 0)')
old = 'order <= order_limit && !best_valid'
assert variant.count(old) == 1
variant = variant.replace(old, 'order <= order_limit')
old = '''    if (!best_valid)
      {
        return false;
      }

    cw_out = best_codeword;'''
new = '''    if (!best_valid || !crc_ok (message_out))
      {
        message_out.fill (0);
        return false;
      }

    cw_out = best_codeword;'''
assert variant.count(old) == 1
variant = variant.replace(old, new)
tail = r'''
extern "C" void four2a_(std::complex<float>*,int*,int*,int*,int*) {}
extern "C" void gen_fst4wave_(int*,int*,int*,int*,float*,int*,float*,int*,std::complex<float>*,float*) {}
extern "C" void lab_codeword(signed char* out) {
    std::array<signed char,74> m{}; for(int i=0;i<50;++i)m[i]=(i*17+3)%7<3;
    auto cw=encode_from_basis_rows(basis_rows_240_74(50).data(),m.data(),50);
    std::copy(cw.begin(),cw.end(),out);
}
'''
driver = r'''
#include <array>
#include <chrono>
#include <cmath>
#include <dlfcn.h>
#include <iostream>
#include <random>
using Decode=void(*)(float*,int*,int*,int*,signed char*,signed char*,signed char*,int*,int*,float*);
using Generate=void(*)(signed char*);
struct Out{std::array<signed char,74> msg{};std::array<signed char,240> cw{};int type=0,err=-1;float distance=0;};
int main(int argc,char**argv){
 if(argc!=3)return 1; void*a=dlopen(argv[1],RTLD_NOW),*b=dlopen(argv[2],RTLD_NOW);if(!a||!b){std::cerr<<dlerror();return 2;}
 auto generate=reinterpret_cast<Generate>(dlsym(a,"lab_codeword"));auto f=reinterpret_cast<Decode>(dlsym(a,"decode240_74_"));auto g=reinterpret_cast<Decode>(dlsym(b,"decode240_74_"));if(!generate||!f||!g)return 3;
 std::array<signed char,240> code{},mask{};generate(code.data());std::mt19937 rng(81291);std::normal_distribution<float> noise(0,1);
 std::cout<<"[";bool first=true;
 for(float strength:{2.5f,1.5f,1.1f,1.0f,0.9f,0.8f,0.0f}){int beforeGood=0,afterGood=0,beforeBad=0,afterBad=0,lostGood=0,gainedGood=0;double beforeMs=0,afterMs=0;
  for(int trial=0;trial<120;++trial){std::array<float,240> llr{};for(int i=0;i<240;++i)llr[i]=(code[i]?strength:-strength)+noise(rng);int k=66,maxosd=2,order=3;Out x,y;
   auto t1=std::chrono::steady_clock::now();f(llr.data(),&k,&maxosd,&order,mask.data(),x.msg.data(),x.cw.data(),&x.type,&x.err,&x.distance);auto t2=std::chrono::steady_clock::now();g(llr.data(),&k,&maxosd,&order,mask.data(),y.msg.data(),y.cw.data(),&y.type,&y.err,&y.distance);auto t3=std::chrono::steady_clock::now();
   beforeMs+=std::chrono::duration<double,std::milli>(t2-t1).count();afterMs+=std::chrono::duration<double,std::milli>(t3-t2).count();bool bg=x.err>=0&&x.cw==code,ag=y.err>=0&&y.cw==code;beforeGood+=bg;afterGood+=ag;beforeBad+=x.err>=0&&!bg;afterBad+=y.err>=0&&!ag;lostGood+=bg&&!ag;gainedGood+=ag&&!bg;
  }
  if(!first)std::cout<<",";first=false;std::cout<<"{\"strength\":"<<strength<<",\"trials\":120,\"beforeGood\":"<<beforeGood<<",\"afterGood\":"<<afterGood<<",\"beforeBad\":"<<beforeBad<<",\"afterBad\":"<<afterBad<<",\"lostGood\":"<<lostGood<<",\"gainedGood\":"<<gainedGood<<",\"beforeMs\":"<<beforeMs<<",\"afterMs\":"<<afterMs<<"}";
 }
 std::cout<<"]\n";
}
'''
with tempfile.TemporaryDirectory(prefix='fst4w-crc-selection-') as tmp:
    d=Path(tmp);compiler=shlex.split(os.environ.get('CXX','c++'));libs=[]
    for name,source in [('current',original),('nearest',variant)]:
        cpp=d/(name+'.cpp');lib=d/(name+'.so');cpp.write_text(source+'\n'+tail)
        subprocess.run([*compiler,'-Os','-std=c++17','-fPIC','-shared','-I'+str(root),'-I'+str(root/'build-baseline'),str(cpp),'-o',str(lib)],check=True)
        libs.append(lib)
    cpp=d/'driver.cpp';exe=d/'driver';cpp.write_text(driver)
    subprocess.run([*compiler,'-O2','-std=c++17',str(cpp),'-ldl','-o',str(exe)],check=True)
    output=subprocess.check_output([str(exe),*map(str,libs)],text=True)
    print(json.dumps(json.loads(output),indent=2))
