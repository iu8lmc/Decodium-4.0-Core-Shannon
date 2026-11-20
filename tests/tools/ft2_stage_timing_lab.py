#!/usr/bin/env python3
"""Time selected FT2 decode stages on existing synthetic WAVs, then restore."""
import os
from pathlib import Path
import re
import subprocess

root=Path(__file__).resolve().parents[2]
source=root/'Detector/FtxFt2Stage7.cpp'
original=source.read_text()
begin=original.index('void decode_ft2_stage7 (')
end=original.index('\nextern "C" void ftx_ft2_stage7_set_cancel_c',begin)
section=original[begin:end]
entry='  clear_outputs (syncs, snrs, dts, freqs, naps, quals, bits77, decodeds, nout);'
if section.count(entry)!=1:raise RuntimeError('unexpected decoder structure')
instrument='''
  struct LabProfile {
    long long ns[5] {}; int count[5] {};
    std::chrono::steady_clock::time_point start {std::chrono::steady_clock::now()};
    ~LabProfile() { std::fprintf(stderr,"[FT2-LAB-PROFILE] total_us=%lld cand_us=%lld down_us=%lld sync_us=%lld metrics_us=%lld decode_us=%lld counts=%d,%d,%d,%d,%d\\n",
      static_cast<long long>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count()),
      ns[0]/1000,ns[1]/1000,ns[2]/1000,ns[3]/1000,ns[4]/1000,count[0],count[1],count[2],count[3],count[4]); }
  } labProfile;
  struct LabTimer { long long& ns; int& count; std::chrono::steady_clock::time_point start {std::chrono::steady_clock::now()};
    ~LabTimer() { ns+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();++count;} };
#define FT2_LAB_WRAP(I, ...) ([&]() { LabTimer labTimer {labProfile.ns[I],labProfile.count[I]}; return (__VA_ARGS__); }())
'''
section=section.replace(entry,instrument+entry,1)
names=['ftx_getcandidates2_c','ftx_ft2_downsample_c','ftx_sync2d_scan_c','ftx_ft2_bitmetrics_c','run_decode_passes']
for category,name in enumerate(names):
    pattern=re.compile(r'\b'+re.escape(name)+r'\s*\(')
    found=list(pattern.finditer(section))
    if not found:raise RuntimeError('missing '+name)
    for match in reversed(found):
        depth=1;pos=match.end()
        while depth and pos<len(section):
            if section[pos]=='(':depth+=1
            elif section[pos]==')':depth-=1
            pos+=1
        if depth:raise RuntimeError('unbalanced call '+name)
        expression=section[match.start():pos]
        section=section[:match.start()]+f'FT2_LAB_WRAP({category}, '+expression+')'+section[pos:]
section+='\n#undef FT2_LAB_WRAP\n'
try:
    source.write_text(original[:begin]+section+original[end:])
    with (root/'build-baseline/performance/ft2-stage-timing-build.log').open('w') as log:
        subprocess.run(['cmake','--build','build-baseline','--target','ft2_stage_compare','-j2'],cwd=root,
                       stdout=log,stderr=subprocess.STDOUT,check=True)
    env=os.environ.copy();env['QT_QPA_PLATFORM']='offscreen'
    wavs=['busy-ft2-20261115.wav','ft2-weak19-seed1102.wav']
    records=[]
    for wav in wavs:
        cmd=[str(root/'build-baseline/tests/ft2_stage_compare'),'--stages','7','--ndepth','2',
             '--nfqso','1500','--nfa','200','--nfb','4000',
             str(root/'build-baseline/performance/wavs-ft2-ft4'/wav)]
        result=subprocess.run(cmd,cwd=root,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=60,check=True)
        lines=[line for line in result.stdout.splitlines() if '[FT2-LAB-PROFILE]' in line]
        if not lines:raise RuntimeError(result.stdout[-5000:])
        records.append(wav+'\n'+'\n'.join(lines)+'\n'+result.stdout)
        print(wav,*lines,sep='\n',flush=True)
    (root/'build-baseline/performance/results/ft2-stage-timing.log').write_text('\n'.join(records))
finally:
    source.write_text(original)
    subprocess.run(['cmake','--build','build-baseline','--target','ft2_stage_compare','decodium_qml','-j2'],
                   cwd=root,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)
