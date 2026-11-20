#!/usr/bin/env python3
"""Compare TX adapter placement via two isolated FT8 applications."""
from pathlib import Path
import json,os,re,shutil,subprocess,sys,tempfile
r=Path(__file__).resolve().parents[2]
main=r/'src/bridge/DecodiumBridge.cpp'; mod=r/'src/bridge/DecodiumBridgeRxMonitoring.cpp'
m=main.read_text();n=mod.read_text();fixture=(r/'tests/bridge_refactor_fixtures/rx_monitoring_before.inc').read_text();assert n.count(fixture)==1
point='// BRIDGE_RX_MONITORING_INSERTION_POINT';assert m.count(point)==1
policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
env=os.environ.copy();env.update(DECODIUM_FORCE_STANDALONE_UDP='1')
try:
 for label in ('before','after'):
  main.write_text(m.replace(point,fixture+point,1) if label=='before' else m)
  mod.write_text(n.replace(fixture, '', 1) if label=='before' else n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  proc=subprocess.run(['sandbox-exec','-p',policy,sys.executable,'tests/tools/bridge_ft8_matched_ab_lab.py'],cwd=r,env=env,text=True,capture_output=True,timeout=160)
  print(label,proc.stdout.strip(),proc.stderr.strip(),flush=True)
  if proc.returncode:raise RuntimeError('FT8 virtual audio test failed')
  report=json.loads(proc.stdout.strip().splitlines()[-1]);artifact=Path(report['artifacts'])
  if artifact.parent==Path(tempfile.gettempdir()) and artifact.name.startswith('decodium-bridge-ft8-'):
   for log in artifact.glob('*.log'):
    match=re.search(r'Application identity: (Decodium - BridgePairimmediate(?:TX|RX)[0-9a-f]{8} - test)',log.read_text(errors='replace'))
    if match:
     for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
      target=parent/match.group(1)
      if target.is_dir():shutil.rmtree(target)
   shutil.rmtree(artifact)
finally:
 main.write_text(m);mod.write_text(n)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
