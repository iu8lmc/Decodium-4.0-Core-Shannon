#!/usr/bin/env python3
"""Isolate the retired LDPC min-sum prototype and FT2 test ordering."""
import os
from pathlib import Path
import subprocess
from ldpc_minsum_fixture import previous_minsum, candidate_minsum

root=Path(__file__).resolve().parents[2]
source=root/'Detector/FtxLdpc.cpp';test=root/'tests/test_ft2_legacy_cpu.cpp'
original=source.read_text();original_test=test.read_text()
reference=previous_minsum(original,root)
prototype='''      else
        {
          for (int check=0;check<kLdpc17491M;++check) {
            float signs=1.0f,first=1.0e30f,second=1.0e30f;
            int count=tables.nrw[static_cast<size_t>(check)];
            for(int row=0;row<count;++row) {
              float v=toc[static_cast<size_t>(check)][static_cast<size_t>(row)];float a=std::fabs(v);
              signs*=sign_one(-v);if(a<first){second=first;first=a;}else if(a<second)second=a;
            }
            for(int row=0;row<count;++row) {
              float v=toc[static_cast<size_t>(check)][static_cast<size_t>(row)];
              int bit=nm_at(tables,row,check)-1;
              int edge=kLdpc17491ReverseEdges[static_cast<size_t>(row+kLdpc17491MaxRowWeight*check)];
              tov[static_cast<size_t>(bit)][static_cast<size_t>(edge)]=
                  -kAlphaMs*(signs*sign_one(-v))*(std::fabs(v)==first?second:first);
            }
          }
        }
    }
'''
test_anchor='''    ftx_ft2_stage7_set_cancel_c (0);

    // Same non-AVX2 path'''
test_replacement='''    ftx_ft2_stage7_set_cancel_c (0);
    if (std::getenv("DECODIUM_LAB_WEAK_FIRST")) {
        ftx_ft2_stage7_clravg_c ();
        auto const result = decode (weak, 20);
        return result.recovered && result.elapsedMs <= 3000 ? 0 : 1;
    }

    // Same non-AVX2 path'''
if original_test.count(test_anchor)!=1:raise RuntimeError('unexpected test layout')
logs=root/'build-baseline/performance/results';results=[]
try:
 source.write_text(candidate_minsum(original,root))
 test.write_text(original_test.replace('#include <cmath>','#include <cmath>\n#include <cstdlib>',1).replace(test_anchor,test_replacement,1))
 with (logs/'ldpc-minsum-weak-order-build.log').open('w') as output:
  subprocess.run(['cmake','--build','build-baseline','--target','test_ft2_legacy_cpu','-j2'],cwd=root,stdout=output,stderr=subprocess.STDOUT,check=True)
 for label,weak in [('weak-only',True),('normal-sequence',False),('weak-only-repeat',True)]:
  env=dict(os.environ)
  if weak:env['DECODIUM_LAB_WEAK_FIRST']='1'
  else:env.pop('DECODIUM_LAB_WEAK_FIRST',None)
  result=subprocess.run([str(root/'build-baseline/tests/test_ft2_legacy_cpu')],cwd=root,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=60)
  (logs/f'ldpc-minsum-{label}.log').write_text(result.stdout)
  results.append((label,result.returncode));print(label,'exit',result.returncode,flush=True)
  for line in result.stdout.splitlines():
   if 'depth=' in line:print(line,flush=True)
finally:
 source.write_text(original);test.write_text(original_test)
 subprocess.run(['cmake','--build','build-baseline','--target','test_ft2_legacy_cpu','decodium_qml','-j2'],cwd=root,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)
print('results',results)
