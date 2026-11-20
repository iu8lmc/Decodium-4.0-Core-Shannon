#!/usr/bin/env python3
"""Compare complete FST4 bit metrics before and after the optimization."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
relative = 'Detector/FtxFst4Core.cpp'
old = subprocess.check_output(['git', 'show', 'HEAD:' + relative], cwd=root)
new = (root / relative).read_bytes()
program = r'''
#include <array>
#include <cmath>
#include <complex>
#include <cstring>
#include <dlfcn.h>
#include <iostream>
#include <random>
#include <vector>
using Fn = void (*)(std::complex<float>*,int*,float*,float*,int*,int*);
int main(int argc,char** argv) {
    if(argc!=3)return 1;
    void* a=dlopen(argv[1],RTLD_NOW);void* b=dlopen(argv[2],RTLD_NOW);
    if(!a||!b){std::cerr<<dlerror()<<"\n";return 2;}
    auto oldFn=reinterpret_cast<Fn>(dlsym(a,"get_fst4_bitmetrics_"));
    auto newFn=reinterpret_cast<Fn>(dlsym(b,"get_fst4_bitmetrics_"));
    if(!oldFn||!newFn)return 3;
    std::mt19937 rng(15123); int tested=0;
    const int sync1[]={0,1,3,2,1,0,2,3},sync2[]={2,3,1,0,3,2,0,1};
    for(int nss : {8,16,40}) for(int trial=0;trial<4;++trial) {
        std::vector<std::complex<float>> input(160*nss);
        for(int s=0;s<160;++s) {
            int tone=rng()%4;
            for(int start : {0,38,76,114,152}) if(s>=start&&s<start+8)
                tone=((start==38||start==114)?sync2:sync1)[s-start];
            for(int j=0;j<nss;++j) {
                float phase=float((tone-1.5)*6.283185307179586*j/nss);
                float noise=float(int(rng()%201)-100)*0.0001f*trial;
                input[s*nss+j]={std::cos(phase)+noise,std::sin(phase)-noise};
            }
        }
        std::array<float,1280> m1{},m2{};std::array<float,640> s1{},s2{};
        int q1=0,q2=0,bad1=0,bad2=0,ns=nss;
        oldFn(input.data(),&ns,m1.data(),s1.data(),&q1,&bad1); ns=nss;
        newFn(input.data(),&ns,m2.data(),s2.data(),&q2,&bad2);
        if(q1!=q2||bad1!=bad2||std::memcmp(m1.data(),m2.data(),sizeof(m1))||std::memcmp(s1.data(),s2.data(),sizeof(s1))) {
            std::cerr<<"mismatch nss="<<nss<<" trial="<<trial<<"\n";return 4;
        }
        if(!bad1)++tested;
    }
    if(tested!=12){std::cerr<<"fixtures did not reach full metrics: "<<tested<<"\n";return 5;}
    std::cout<<"12 complete bitmetric arrays identical bit for bit\n";
}
'''
with tempfile.TemporaryDirectory(prefix='fst4-bitmetrics-ab-') as tmp:
    directory = Path(tmp)
    compiler = shlex.split(os.environ.get('CXX', 'c++'))
    libraries = []
    for name, data in [('old', old), ('new', new)]:
        source, lib = directory / (name+'.cpp'), directory / (name+'.so')
        source.write_bytes(data)
        subprocess.run([*compiler, '-Os', '-std=c++17', '-fPIC', '-shared',
                        '-I'+str(root/'build-baseline'), str(source), '-o', str(lib)], check=True)
        libraries.append(lib)
    source, executable = directory/'probe.cpp', directory/'probe'
    source.write_text(program)
    subprocess.run([*compiler, '-O2', '-std=c++17', str(source), '-ldl', '-o', str(executable)], check=True)
    subprocess.run([str(executable), *map(str, libraries)], check=True)
