#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
s=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(s,'QString DecodiumBridge::diagnosticLogPath() const','void DecodiumBridge::increaseFontScale()')+sec(s,'QVariantMap DecodiumBridge::primaryScreenAvailableGeometry() const','bool DecodiumBridge::recordRxEnabled() const')
new=sec((R/'src/bridge/DecodiumBridgeAppInfo.cpp').read_text(),'QString DecodiumBridge::diagnosticLogPath() const')
if old.strip()!=new.strip():raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core','Qt6Gui',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-appinfo-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_appinfo_harness.cpp').read_text());out=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));env=os.environ.copy();env['QT_QPA_PLATFORM']='offscreen';out.append(run(str(d/'test'),env=env,stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1] or out[0].splitlines()!=['log|1|1','link|1','screen|1|1|1','version|1.0.737']:raise RuntimeError('unexpected '+repr(out))
 print(out[0],end='')
