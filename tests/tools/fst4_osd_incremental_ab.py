#!/usr/bin/env python3
"""Compare complete FST4/FST4W OSD outputs using actual old/new sources."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--baseline', type=Path, help='compare against an incremental-XOR source snapshot')
args = parser.parse_args()
old = args.baseline.read_bytes() if args.baseline else subprocess.check_output(['git', 'show', 'HEAD:Detector/FtxFst4Ldpc.cpp'], cwd=root)
new = (root / 'Detector/FtxFst4Ldpc.cpp').read_bytes()
tail = r'''
extern "C" void four2a_(std::complex<float>*,int*,int*,int*,int*) {}
extern "C" void gen_fst4wave_(int*,int*,int*,int*,float*,int*,float*,int*,std::complex<float>*,float*) {}
extern "C" void lab_codeword(int bits, signed char* out) {
    if(bits==74) {
        std::array<signed char,74> message{}; for(int i=0;i<50;++i)message[i]=(i*17+3)%7<3;
#ifdef LAB_NEW
        auto cw=encode_from_basis_rows(basis_rows_240_74(50).data(),message.data(),50);
#else
        auto cw=encode_from_basis_rows(basis_rows_240_74(50),message,50);
#endif
        std::copy(cw.begin(),cw.end(),out);
    } else {
        std::array<signed char,101> message{}; for(int i=0;i<77;++i)message[i]=(i*17+3)%7<3;
#ifdef LAB_NEW
        auto cw=encode_from_basis_rows(basis_rows_240_101(77).data(),message.data(),77);
#else
        auto cw=encode_from_basis_rows(basis_rows_240_101(77),message,77);
#endif
        std::copy(cw.begin(),cw.end(),out);
    }
}
'''
driver = r'''
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <dlfcn.h>
#include <iostream>
#include <random>
using Decode=void(*)(float*,int*,int*,int*,signed char*,signed char*,signed char*,int*,int*,float*);
using Generate=void(*)(int,signed char*);
struct Out{std::array<signed char,101> msg{};std::array<signed char,240> cw{};int type=0,err=0;float distance=0;};
int main(int argc,char**argv){
 if(argc!=3)return 1;void*a=dlopen(argv[1],RTLD_NOW),*b=dlopen(argv[2],RTLD_NOW);if(!a||!b){std::cerr<<dlerror();return 2;}
 auto generate=reinterpret_cast<Generate>(dlsym(a,"lab_codeword"));if(!generate)return 3;
 std::mt19937 rng(8129);int tested=0,decoded=0;double oldMs=0,newMs=0;
 for(int bits:{74,101}){auto oldFn=reinterpret_cast<Decode>(dlsym(a,bits==74?"decode240_74_":"decode240_101_"));auto newFn=reinterpret_cast<Decode>(dlsym(b,bits==74?"decode240_74_":"decode240_101_"));if(!oldFn||!newFn)return 4;
  std::array<signed char,240> code{};generate(bits,code.data());
  for(int trial=0;trial<180;++trial){
   std::array<float,240> llr{};std::array<signed char,240> mask{};
   for(int i=0;i<240;++i){float m=float(1+rng()%1000)/37.0f;bool bit=code[i]!=0;if(trial%3==1&&rng()%20==0)bit=!bit;if(trial%3==2)bit=(rng()&1)!=0;llr[i]=bit?m:-m;if(trial%5==0&&rng()%10==0)mask[i]=1;}
   if(trial%30==17)for(int i=0;i<240;++i)llr[i]=(i%2)?1.0f:-1.0f;
   int keff=bits==74?(trial%2?50:66):(trial%2?77:91),maxosd=trial%6==0?-1:2,order=trial%30==17?4:trial%4;
   Out x,y;auto t1=std::chrono::steady_clock::now();oldFn(llr.data(),&keff,&maxosd,&order,mask.data(),x.msg.data(),x.cw.data(),&x.type,&x.err,&x.distance);auto t2=std::chrono::steady_clock::now();newFn(llr.data(),&keff,&maxosd,&order,mask.data(),y.msg.data(),y.cw.data(),&y.type,&y.err,&y.distance);auto t3=std::chrono::steady_clock::now();
   oldMs+=std::chrono::duration<double,std::milli>(t2-t1).count();newMs+=std::chrono::duration<double,std::milli>(t3-t2).count();
   if(x.msg!=y.msg||x.cw!=y.cw||x.type!=y.type||x.err!=y.err||std::memcmp(&x.distance,&y.distance,sizeof(float))){std::cerr<<"mismatch "<<bits<<" "<<trial<<"\n";return 5;}
   ++tested;if(x.err>=0)++decoded;
  }
 }
 if(!decoded){std::cerr<<"no successful decodes\n";return 6;}
 std::cout<<"vectors="<<tested<<" decoded="<<decoded<<" old_ms="<<oldMs<<" new_ms="<<newMs<<"\n";
}
'''
with tempfile.TemporaryDirectory(prefix='fst4-osd-ab-') as tmp:
    d=Path(tmp);compiler=shlex.split(os.environ.get('CXX','c++'));libs=[]
    for name, source in [('old',old),('new',new)]:
        cpp=d/(name+'.cpp');lib=d/(name+'.so');cpp.write_bytes(source+b'\n'+tail.encode())
        subprocess.run([*compiler,'-Os','-std=c++17','-fPIC','-shared',*(['-DLAB_NEW'] if name=='new' or args.baseline else []),'-I'+str(root),'-I'+str(root/'build-baseline'),str(cpp),'-o',str(lib)],check=True)
        libs.append(lib)
    cpp=d/'driver.cpp';exe=d/'driver';cpp.write_text(driver)
    subprocess.run([*compiler,'-O2','-std=c++17',str(cpp),'-ldl','-o',str(exe)],check=True)
    subprocess.run([str(exe),*map(str,libs)],check=True)
