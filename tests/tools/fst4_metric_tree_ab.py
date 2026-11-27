#!/usr/bin/env python3
"""Compare all full-symbol metric outputs, including exceptional values."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root/'Detector/FtxFst4Core.cpp').read_text()
start = source.index('  float max_metric_for_bit (')
end = source.index('\n  void normalize_bitmetrics', start)
functions = source[start:end]
program = r'''
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <random>
#include <vector>
FUNCTIONS
int main(){
 std::mt19937 rng(79215);
 for(int trial=0;trial<32;++trial){
  std::vector<float> input(65536);for(float& v:input)v=float(int(rng()%20001)-10000)/19.0f;
  if(trial==0)std::fill(input.begin(),input.end(),-0.0f);
  if(trial==1)for(int i=0;i<65536;++i)input[i]=(i%2)?0.0f:-0.0f;
  if(trial==2)std::fill(input.begin(),input.end(),std::numeric_limits<float>::quiet_NaN());
  if(trial==3)std::fill(input.begin(),input.end(),-std::numeric_limits<float>::infinity());
  if(trial==4)std::fill(input.begin(),input.end(),std::numeric_limits<float>::infinity());
  if(trial>4){input[rng()%65536]=std::numeric_limits<float>::quiet_NaN();input[rng()%65536]=std::numeric_limits<float>::infinity();input[rng()%65536]=-std::numeric_limits<float>::infinity();}
  float old[16],cur[16];for(int i=0;i<16;++i)old[i]=max_metric_for_bit(input.data(),65536,15-i,1)-max_metric_for_bit(input.data(),65536,15-i,0);
  full_symbol_metrics(input.data(),cur);
  if(std::memcmp(old,cur,sizeof(old))){for(int i=0;i<16;++i)if(std::memcmp(old+i,cur+i,sizeof(float)))std::cerr<<"mismatch trial="<<trial<<" bit="<<i<<"\n";return 1;}
 }
 std::cout<<"32 full-symbol metric arrays identical bit for bit\n";
}
'''.replace('FUNCTIONS',functions)
with tempfile.TemporaryDirectory(prefix='fst4-metric-tree-') as tmp:
    d=Path(tmp); cpp=d/'probe.cpp'; exe=d/'probe'; cpp.write_text(program)
    for optimization in ('-O2','-Os'):
        subprocess.run([*shlex.split(os.environ.get('CXX','c++')),optimization,'-std=c++17',str(cpp),'-o',str(exe)],check=True)
        print(optimization,flush=True);subprocess.run([str(exe)],check=True)
