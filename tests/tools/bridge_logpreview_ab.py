#!/usr/bin/env python3
"""Compare QSO log preview and comment selection with substituted dependencies."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'QVariantMap DecodiumBridge::pendingLogQsoPreview() const','bool DecodiumBridge::promptToLogEnabled() const')+sec(b,'QString DecodiumBridge::defaultLogCommentForQso(','static QString satelliteCodeFromDisplayText(')
new=sec((R/'src/bridge/DecodiumBridgeLogPreview.cpp').read_text(),'QVariantMap DecodiumBridge::pendingLogQsoPreview() const').replace('decodiumBridgeLogPromptDateTimeText(', 'logPromptDateTimeText(')
if old!=new:raise RuntimeError('QSO preview source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-logpreview-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_logpreview_harness.cpp').read_text());out=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1]:raise RuntimeError('QSO preview results differ')
 print(out[0],end='')
