#!/usr/bin/env python3
"""Compare cached and original FT2 four-tone DFT on deterministic inputs."""
import argparse
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--baseline', type=Path, required=True)
args = p.parse_args()

def extract(path, name):
    source = path.read_text()
    start = source.index('void dft_symbol_4tones (')
    end = source.index('\nvoid fft_symbol_8tones_ft8', start)
    return source[start:end].replace('dft_symbol_4tones (', name+' (', 1)

program = r'''
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstring>
#include <iostream>
#include <limits>
#include <random>
#include <thread>
using Complex=std::complex<float>;
float abs1(Complex v){return std::abs(v);} float abs2(Complex v){return std::norm(v);}
OLD
NEW
int main(){
 std::mt19937 rng(7183);std::array<std::array<Complex,32>,1024> input{};
 for(auto& row:input)for(auto& x:row)x=Complex(float(int(rng()%20001)-10000)/31.0f,float(int(rng()%20001)-10000)/37.0f);
 for(int i=0;i<32;++i){
  input[0][i]=Complex(i%2 ? -0.0f : 0.0f, -0.0f);
  input[1][i]=Complex(std::numeric_limits<float>::denorm_min(), -std::numeric_limits<float>::denorm_min());
  input[2][i]=Complex(i%2 ? std::numeric_limits<float>::infinity() : 0.0f, 1.0f);
  input[3][i]=Complex(i==7 ? std::numeric_limits<float>::quiet_NaN() : 0.0f, 1.0f);
 }
 std::atomic<int> ready{0},errors{0};std::atomic<bool> go{false};std::array<std::thread,8> workers;
 for(int i=0;i<8;++i)workers[i]=std::thread([&,i]{++ready;while(!go.load())std::this_thread::yield();
  std::array<Complex,4>a{},b{};std::array<float,4>am{},bm{},ap{},bp{};
  old_dft(input[i+4].data(),a,am,&ap);new_dft(input[i+4].data(),b,bm,&bp);
  if(std::memcmp(a.data(),b.data(),sizeof(a))||std::memcmp(am.data(),bm.data(),sizeof(am))||std::memcmp(ap.data(),bp.data(),sizeof(ap)))++errors;});
 while(ready.load()!=8)std::this_thread::yield();go=true;for(auto& worker:workers)worker.join();if(errors)return 2;
 for(auto const& row:input){std::array<Complex,4>a{},b{};std::array<float,4>am{},bm{},ap{},bp{};
  old_dft(row.data(),a,am,&ap);new_dft(row.data(),b,bm,&bp);
  if(std::memcmp(a.data(),b.data(),sizeof(a))||std::memcmp(am.data(),bm.data(),sizeof(am))||std::memcmp(ap.data(),bp.data(),sizeof(ap)))return 1;}
 volatile float sink=0;
 auto run=[&](auto fn){auto start=std::chrono::steady_clock::now();for(int j=0;j<200;++j)for(size_t i=4;i<input.size();++i){auto const& row=input[i];std::array<Complex,4> t{};std::array<float,4> m{},p{};fn(row.data(),t,m,&p);sink+=m[0];}return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();};
 double old1=run(old_dft),new1=run(new_dft),new2=run(new_dft),old2=run(old_dft);
 std::cout<<"inputs=1024 identical=1 old_ms="<<old1<<","<<old2<<" new_ms="<<new1<<","<<new2<<" sink="<<sink<<"\n";
}
'''.replace('OLD', extract(args.baseline, 'old_dft')).replace('NEW', extract(root/'Detector/FtxBitmetrics.cpp', 'new_dft'))
with tempfile.TemporaryDirectory(prefix='ft2-dft-ab-') as tmp:
    source = Path(tmp)/'probe.cpp'; exe = Path(tmp)/'probe'; source.write_text(program)
    for opt in ('-Os', '-O2'):
        subprocess.run(['c++',opt,'-std=c++17','-pthread',str(source),'-o',str(exe)],check=True)
        print('compiler option:', opt, flush=True)
        subprocess.run([str(exe)],check=True)
