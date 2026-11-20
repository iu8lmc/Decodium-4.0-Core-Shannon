#!/usr/bin/env python3
"""Compare FTx settings method placement in isolated real apps."""
from pathlib import Path
import os,subprocess,sys
r=Path(__file__).resolve().parents[2];main=r/'src/bridge/DecodiumBridge.cpp';mod=r/'src/bridge/DecodiumBridgeFtxSettings.cpp';m=main.read_text();n=mod.read_text();sig='void DecodiumBridge::setFt2SignoffRetryCap(';head,tail=n.split(sig,1);methods=sig+tail;point='// BRIDGE_LAB_INSERTION_POINT';assert m.count(point)==1
baseline=subprocess.check_output(['git','show','v1.0.737:src/bridge/DecodiumBridge.cpp'],cwd=r,text=True)
def sec(a,b):return baseline[baseline.index(a):baseline.index(b,baseline.index(a))]
old=sec(sig,'void DecodiumBridge::setAutoCqMaxCycles(')+sec('void DecodiumBridge::setCallerRetriesAlwaysHard(','void DecodiumBridge::setResumeQsoOnReply(')
assert methods.replace('decodiumBridgeFtxSettingsLog(', 'bridgeLog(')==old
policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))';env=os.environ.copy();env.update(QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software',DECODIUM_FORCE_STANDALONE_UDP='1')
try:
 for label in ('before','after'):
  main.write_text(m.replace(point,old+point,1) if label=='before' else m);mod.write_text(head if label=='before' else n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  p=subprocess.run(['sandbox-exec','-p',policy,sys.executable,'tests/tools/bridge_ftx_settings_runtime_lab.py'],cwd=r,env=env,capture_output=True,text=True,timeout=160)
  if p.returncode:raise RuntimeError(label+' '+p.stdout[-4000:]+p.stderr[-2000:])
  print(label,p.stdout.strip(),flush=True)
finally:
 main.write_text(m);mod.write_text(n)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
