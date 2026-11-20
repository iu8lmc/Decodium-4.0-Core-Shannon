#!/usr/bin/env python3
"""A/B dedicated SSTV RX capture with two isolated virtual-audio apps."""
import os, subprocess, sys, tempfile, shutil
from pathlib import Path
r=Path(__file__).resolve().parents[2]
main=r/'src/bridge/DecodiumBridge.cpp'; module=r/'src/bridge/DecodiumBridgeAudioSink.cpp'
m=main.read_text(); n=module.read_text(); fixture=(r/'tests/bridge_refactor_fixtures/audiosink_before.inc').read_text()
mark='void DecodiumBridge::ensureAudioSink()'; head=n.split(mark,1)[0]
assert n==head+fixture
# Identical test-only instrumentation confirms the moved function is invoked.
instrumented=fixture.replace(mark+'\n{',mark+'\n{\n    qWarning("[CAPTURE-AB] startAudioCapture invoked");',1)
assert instrumented != fixture
parents=[Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC']
existing={p:{d for pattern in ('Decodium - AVT* - test','Decodium - Capture* - test') for d in p.glob(pattern)} if p.exists() else set() for p in parents}
policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
try:
 with tempfile.TemporaryDirectory(prefix='decodium-capture-sstv-ab-') as tmp:
  for label in ('before','after'):
   if label=='before':
    point='// BRIDGE_UTC_TIMER_INSERTION_POINT';assert m.count(point)==1
    main.write_text(m.replace(point,instrumented+point));module.write_text(head)
   else:
    main.write_text(m);module.write_text(head+instrumented)
   subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
   proc=subprocess.run([sys.executable,'tests/tools/bridge_audiocapture_sstv_lab.py'],cwd=r,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=60)
   print(label,proc.stdout,flush=True)
   if proc.returncode:raise RuntimeError('dedicated SSTV capture test failed')
   import json
   result=json.loads(proc.stdout.strip().splitlines()[-1])
   if not all(result.get(k) for k in ('method_invoked','worker','chunks_positive','active','signal_present')) or result.get('source')!='BlackHole 2ch':raise RuntimeError('unexpected capture result '+str(result))

finally:
 main.write_text(m);module.write_text(n)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
 for p in parents:
  if p.exists():
   for generated in {d for pattern in ('Decodium - AVT* - test','Decodium - Capture* - test') for d in p.glob(pattern)}-existing[p]:
    if generated.is_dir():shutil.rmtree(generated)
