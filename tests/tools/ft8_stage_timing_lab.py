#!/usr/bin/env python3
"""Temporarily profile major FT8 stage-4 functions and restore the build."""
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parents[2]
source=root/'Detector/FtxFt8Stage4.cpp'
original=source.read_bytes()
search_source=root/'Detector/FtxFt8Search.cpp'
original_search=search_source.read_bytes()
text=original.decode()
anchor='namespace\n{\n'
if text.count(anchor)<1:raise RuntimeError('missing namespace')
definitions='''
struct Ft8LabStats {
  std::atomic<long long> ns[4] {}; std::atomic<int> count[4] {};
  ~Ft8LabStats() { std::fprintf(stderr,"[FT8-LAB-PROFILE] main_us=%lld candidate_us=%lld ldpc_us=%lld sync_us=%lld counts=%d,%d,%d,%d\\n",
    ns[0].load()/1000,ns[1].load()/1000,ns[2].load()/1000,ns[3].load()/1000,
    count[0].load(),count[1].load(),count[2].load(),count[3].load()); }
} ft8LabStats;
struct Ft8LabTimer {
  int id; std::chrono::steady_clock::time_point start{std::chrono::steady_clock::now()};
  ~Ft8LabTimer(){ft8LabStats.ns[id].fetch_add(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count(),std::memory_order_relaxed);ft8LabStats.count[id].fetch_add(1,std::memory_order_relaxed);}
};
#define FT8_LAB_WRAP(ID, ...) ([&](){Ft8LabTimer t {ID};return (__VA_ARGS__);}())
'''
text=text.replace('#include <cctype>','#include <cctype>\n#include <cstdio>',1)
text=text.replace(anchor,anchor+definitions,1)
for marker,category in [('void ft8_ldpc_decode (',2),('bool decode_main_candidate_cpp (',1),('void run_main_passes (',0)]:
    start=text.index(marker)
    brace=text.index('\n{',start)
    text=text[:brace+2]+f'\n  Ft8LabTimer ft8LabTimer {{{category}}};'+text[brace+2:]
start=text.index('void run_main_passes (')
call=text.index('ftx_sync8_search_stage4_c (',start)
open_pos=text.index('(',call);depth=1;end=open_pos+1
while depth:
    if text[end]=='(':depth+=1
    elif text[end]==')':depth-=1
    end+=1
text=text[:call]+'FT8_LAB_WRAP(3, '+text[call:end]+')'+text[end:]
search=original_search.decode().replace('\r\n','\n')
search_defs='''
struct SearchLabStats {
  std::atomic<long long> ns[3] {}; std::atomic<int> count[3] {};
  ~SearchLabStats() { std::fprintf(stderr,"[FT8-SEARCH-PROFILE] baseline_us=%lld var_us=%lld scan_us=%lld counts=%d,%d,%d\\n",
    ns[0].load()/1000,ns[1].load()/1000,ns[2].load()/1000,count[0].load(),count[1].load(),count[2].load()); }
} searchLabStats;
struct SearchLabTimer {
  int id;std::chrono::steady_clock::time_point start{std::chrono::steady_clock::now()};
  ~SearchLabTimer(){searchLabStats.ns[id].fetch_add(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count(),std::memory_order_relaxed);searchLabStats.count[id].fetch_add(1,std::memory_order_relaxed);}
};
#define SEARCH_LAB_WRAP(ID, ...) ([&](){SearchLabTimer t {ID};return (__VA_ARGS__);}())
'''
search=search.replace('#include <algorithm>','#include <algorithm>\n#include <atomic>\n#include <chrono>\n#include <cstdio>',1)
search=search.replace('namespace\n{\n','namespace\n{\n'+search_defs,1)
for marker,category in [('void fill_ft8_stage4_sbase (',0),('extern "C" void ftx_sync8var_c (',1)]:
    start=search.index(marker);brace=search.index('\n{',start)
    search=search[:brace+2]+f'\n  SearchLabTimer searchLabTimer {{{category}}};'+search[brace+2:]
start=search.index('extern "C" void ftx_sync8var_c (')
call=search.index('ftx_sync8var_scan_c (',start)
open_pos=search.index('(',call);depth=1;end=open_pos+1
while depth:
    if search[end]=='(':depth+=1
    elif search[end]==')':depth-=1
    end+=1
search=search[:call]+'SEARCH_LAB_WRAP(2, '+search[call:end]+')'+search[end:]
try:
    source.write_text(text)
    search_source.write_text(search)
    with (root/'build-baseline/performance/ft8-stage-timing-build.log').open('w') as log:
        subprocess.run(['cmake','--build','build-baseline','--target','ft8_stage_compare','-j2'],cwd=root,
                       stdout=log,stderr=subprocess.STDOUT,check=True)
    results=[]
    for wav in ('multi-snr-16-seed1001.wav','multi-snr-20-seed1003.wav'):
        cmd=[str(root/'build-baseline/tests/ft8_stage_compare'),'--stages','4','--depth','2','--max-ms','0',
             str(root/'build-baseline/performance/wavs-ft8-multi'/wav)]
        run=subprocess.run(cmd,cwd=root,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=90,check=True)
        lines=[s for s in run.stdout.splitlines() if '[FT8-LAB-PROFILE]' in s or '[FT8-SEARCH-PROFILE]' in s]
        if len(lines)!=2:raise RuntimeError(run.stdout[-5000:])
        results.append(wav+'\n'+run.stdout)
        print(wav,*lines,sep='\n',flush=True)
    (root/'build-baseline/performance/results/ft8-stage-timing.log').write_text('\n'.join(results))
finally:
    source.write_bytes(original)
    search_source.write_bytes(original_search)
    subprocess.run(['cmake','--build','build-baseline','--target','ft8_stage_compare','decodium_qml','-j2'],
                   cwd=root,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)
