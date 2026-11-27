#!/usr/bin/env python3
"""Test whether FT2 weak recovery depends on message-history age."""
import os
from pathlib import Path
import subprocess
from ldpc_minsum_fixture import previous_minsum, candidate_minsum

root=Path(__file__).resolve().parents[2];source=root/'Detector/FtxLdpc.cpp';test=root/'tests/test_ft2_legacy_cpu.cpp'
original=source.read_text();original_test=test.read_text();lab=(root/'tests/tools/ldpc_minsum_weak_order_lab.py').read_text()
prototype=lab.split("prototype='''",1)[1].split("'''",1)[0]
reference=previous_minsum(original,root)
candidate=candidate_minsum(original,root)
seed_start=original_test.index('    // The weak fixture exercises the default historical-message verification,')
seed_end=original_test.index('    ftx_ft2_stage7_clravg_c ();\n    auto const weakResult', seed_start)
legacy_test=original_test[:seed_start]+original_test[seed_end:]
anchor='''    ftx_ft2_stage7_clravg_c ();
    auto const weakResult = decode (weak, 20);'''
replacement='''    if (std::getenv("DECODIUM_LAB_WAIT_HISTORY"))
        std::this_thread::sleep_for (std::chrono::milliseconds (3000));
    std::cout << "[WEAK-START]" << std::endl;
    ftx_ft2_stage7_clravg_c ();
    auto const weakResult = decode (weak, 20);'''
if legacy_test.count(anchor)!=1:raise RuntimeError('unexpected test structure')
out=root/'build-baseline/performance/results';results=[]
try:
 test.write_text(legacy_test.replace('#include <cmath>','#include <cmath>\n#include <cstdlib>',1).replace(anchor,replacement,1))
 for label,text in [('candidate',candidate),('reference',reference)]:
  source.write_text(text)
  with (out/f'ft2-history-{label}-build.log').open('w') as log:
   subprocess.run(['cmake','--build','build-baseline','--target','test_ft2_legacy_cpu','-j2'],cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
  for wait in (False,True):
   env=dict(os.environ,DECODIUM_FT2_AP_MSG_LOG='1')
   if wait:env['DECODIUM_LAB_WAIT_HISTORY']='1'
   else:env.pop('DECODIUM_LAB_WAIT_HISTORY',None)
   run=subprocess.run([str(root/'build-baseline/tests/test_ft2_legacy_cpu')],cwd=root,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=60)
   suffix='delayed' if wait else 'immediate';(out/f'ft2-history-{label}-{suffix}.log').write_text(run.stdout)
   part=run.stdout.split('[WEAK-START]',1)[-1];ap_count=part.count('[APMSG]')
   lines=[x for x in part.splitlines() if 'depth=' in x]
   print(label,suffix,'exit',run.returncode,'APMSG',ap_count,lines[:1],flush=True);results.append((label,suffix,run.returncode,ap_count,lines[:1]))
finally:
 source.write_text(original);test.write_text(original_test)
 subprocess.run(['cmake','--build','build-baseline','--target','test_ft2_legacy_cpu','decodium_qml','-j2'],cwd=root,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)
print('results',results)
