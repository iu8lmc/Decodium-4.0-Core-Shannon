#!/usr/bin/env python3
"""Trace scalar LDPC calls for the isolated weak FT2 regression."""
import os
from pathlib import Path
import struct
import subprocess
from ldpc_minsum_fixture import previous_minsum, candidate_minsum

root=Path(__file__).resolve().parents[2];source=root/'Detector/FtxLdpc.cpp';test=root/'tests/test_ft2_legacy_cpu.cpp'
original=source.read_text();original_test=test.read_text();lab=(root/'tests/tools/ldpc_minsum_weak_order_lab.py').read_text()
prototype=lab.split("prototype='''",1)[1].split("'''",1)[0]
reference=previous_minsum(original,root)
candidate=candidate_minsum(original,root)
anchor='''  clear_outputs (message91_out, cw_out, ntype_out, nharderror_out, dmin_out);'''
probe='''  struct Trace {
    FILE* f;float const* llr; signed char const* mask; signed char* msg; signed char* cw;
    int k,osd,order;int* ntype;int* hard;float* dist;
    ~Trace() { if(!f)return; fwrite(&k,4,1,f);fwrite(&osd,4,1,f);fwrite(&order,4,1,f);
      fwrite(llr,4,174,f);fwrite(mask,1,174,f);fwrite(msg,1,91,f);fwrite(cw,1,174,f);
      fwrite(ntype,4,1,f);fwrite(hard,4,1,f);fwrite(dist,4,1,f); }
  } trace { [] {char const* p=std::getenv("DECODIUM_LAB_LDPC_TRACE");return p?std::fopen(p,"wb"):nullptr;}(), llr_in,apmask_in,message91_out,cw_out,Keff,maxosd,norder,ntype_out,nharderror_out,dmin_out};
'''
# The file must be shared by calls within a process and closed on exit.
probe=probe.replace('[] {char const* p=std::getenv("DECODIUM_LAB_LDPC_TRACE");return p?std::fopen(p,"wb"):nullptr;}()', '[] {static FILE* file=[] {char const* p=std::getenv("DECODIUM_LAB_LDPC_TRACE");return p?std::fopen(p,"wb"):nullptr;}();return file;}()')
test_anchor='''    ftx_ft2_stage7_set_cancel_c (0);

    // Same non-AVX2 path'''
test_replace='''    ftx_ft2_stage7_set_cancel_c (0);
    ftx_ft2_stage7_clravg_c ();
    auto const result = decode (weak, 20);
    return result.recovered ? 0 : 1;

    // Same non-AVX2 path'''
if any([original.count(anchor)!=1,original_test.count(test_anchor)!=1]):raise RuntimeError('unexpected source layout')
out=root/'build-baseline/performance/results';traces=[]
try:
 test.write_text(original_test.replace(test_anchor,test_replace,1))
 for label,text in [('reference',reference),('candidate',candidate)]:
  source.write_text(text.replace(anchor,probe+anchor,1))
  with (out/f'ldpc-trace-{label}-build.log').open('w') as log:
   subprocess.run(['cmake','--build','build-baseline','--target','test_ft2_legacy_cpu','-j2'],cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
  path=out/f'ldpc-trace-{label}.bin';env=dict(os.environ,DECODIUM_LAB_LDPC_TRACE=str(path))
  run=subprocess.run([str(root/'build-baseline/tests/test_ft2_legacy_cpu')],cwd=root,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=60)
  (out/f'ldpc-trace-{label}.log').write_text(run.stdout);traces.append(path.read_bytes())
  print(label,'exit',run.returncode,'bytes',len(traces[-1]),*[x for x in run.stdout.splitlines() if 'depth=' in x],flush=True)
 size=12+174*4+174+91+174+12;in_size=12+174*4+174
 if any(len(x)%size for x in traces):raise RuntimeError('truncated trace')
 counts=[len(x)//size for x in traces];print('call counts',counts,flush=True)
 for i in range(min(counts)):
  a=traces[0][i*size:(i+1)*size];b=traces[1][i*size:(i+1)*size]
  if a!=b:
   print('first differing call',i,'same input',a[:in_size]==b[:in_size],'params',struct.unpack('iii',a[:12]),struct.unpack('iii',b[:12]),flush=True);break
 else:print('all shared calls identical',flush=True)
finally:
 source.write_text(original);test.write_text(original_test)
 subprocess.run(['cmake','--build','build-baseline','--target','test_ft2_legacy_cpu','decodium_qml','-j2'],cwd=root,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)
