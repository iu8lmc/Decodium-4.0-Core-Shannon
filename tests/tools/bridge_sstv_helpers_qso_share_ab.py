#!/usr/bin/env python3
"""Compare SSTV helper placement using QSO and encrypted-share app tests."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[2]
main=root/'src/bridge/DecodiumBridgeSstv.cpp';module=root/'src/bridge/DecodiumBridgeSstvHelpers.cpp'
m=main.read_text();n=module.read_text();fixture=(root/'tests/bridge_refactor_fixtures/sstv_helpers_before.inc').read_text()
assert n.endswith(fixture)
a=m.index('#if DECODIUM_HAS_SSTV\nusing decodium::sstv::SstvAudioSourceKind;');b=m.index('void DecodiumBridge::initialiseSstvRuntime()',a)
try:
 for label in ('before','after'):
  main.write_text(m[:a]+fixture+m[b:] if label=='before' else m)
  module.write_text('#include "DecodiumBridge.h"\n' if label=='before' else n)
  for name in ('bridge_sstv_qso_association_extraction_ab.py','bridge_sstv_storage_startup_share_ab.py'):
   result=subprocess.run([sys.executable,'tests/tools/'+name],cwd=root,capture_output=True,text=True,timeout=300)
   if result.returncode:raise RuntimeError(label+' '+name+'\n'+result.stdout[-5000:]+result.stderr[-3000:])
   rows=[line for line in result.stdout.splitlines() if line.startswith(('before ', 'after '))]
   if len(rows)<2:raise RuntimeError('missing A/B evidence: '+result.stdout[-5000:])
   print(label,name,*rows,sep='\n',flush=True)
finally:
 main.write_text(m);module.write_text(n)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=root,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
