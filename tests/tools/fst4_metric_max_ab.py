#!/usr/bin/env python3
"""Compare actual previous/current FST4 max-metric helpers bit for bit."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
path = 'Detector/FtxFst4Core.cpp'
before = subprocess.check_output(['git', 'show', 'HEAD:' + path], cwd=root, text=True)
after = (root / path).read_text()
def extract(text, name):
    start = text.index('  float max_metric_for_bit (')
    end = text.index('\n  void normalize_bitmetrics', start)
    return text[start:end].replace('max_metric_for_bit', name, 1)
source = r'''
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <random>
#include <vector>
OLD_FUNCTION
NEW_FUNCTION
int main() {
    std::mt19937 rng(0x4781);
    for(int count : {1,3,4,16,255,256,1000,65536}) {
        std::vector<float> x(count);
        for(int trial=0;trial<12;++trial) {
            for(float& v : x) v=float(int(rng()%20001)-10000)/37.0f;
            if(trial==1) std::fill(x.begin(),x.end(),-0.0f);
            if(trial==2) x[count/2]=std::numeric_limits<float>::quiet_NaN();
            if(trial==3) x[count/2]=std::numeric_limits<float>::infinity();
            if(trial==4) x[count/2]=-std::numeric_limits<float>::infinity();
            for(int bit=0;bit<16;++bit) for(int value=0;value<2;++value) {
                float a=oldMetric(x.data(),count,bit,value), b=newMetric(x.data(),count,bit,value);
                if(std::memcmp(&a,&b,sizeof a)!=0) {std::cerr<<"mismatch "<<count<<" "<<trial<<" "<<bit<<" "<<value<<"\n"; return 1;}
            }
        }
    }
    std::vector<float> x(65536); for(float& v:x)v=float(rng()%10000);
    volatile float sink=0;
    auto start=std::chrono::steady_clock::now();
    for(int n=0;n<50;++n) for(int bit=0;bit<16;++bit) for(int v=0;v<2;++v) sink+=oldMetric(x.data(),x.size(),bit,v);
    auto mid=std::chrono::steady_clock::now();
    for(int n=0;n<50;++n) for(int bit=0;bit<16;++bit) for(int v=0;v<2;++v) sink+=newMetric(x.data(),x.size(),bit,v);
    auto end=std::chrono::steady_clock::now();
    std::cout<<"bitwise equivalence passed; old_ms="<<std::chrono::duration<double,std::milli>(mid-start).count()
             <<" new_ms="<<std::chrono::duration<double,std::milli>(end-mid).count()<<" sink="<<sink<<"\n";
}
'''.replace('OLD_FUNCTION', extract(before, 'oldMetric')).replace('NEW_FUNCTION', extract(after, 'newMetric'))
with tempfile.TemporaryDirectory(prefix='fst4-metric-ab-') as tmp:
    cpp = Path(tmp) / 'probe.cpp'
    exe = Path(tmp) / 'probe'
    cpp.write_text(source)
    for optimization in ('-O2', '-Os'):
        subprocess.run([*shlex.split(os.environ.get('CXX', 'c++')), optimization, '-std=c++17', str(cpp), '-o', str(exe)], check=True)
        print(optimization, flush=True)
        subprocess.run([str(exe)], check=True)
