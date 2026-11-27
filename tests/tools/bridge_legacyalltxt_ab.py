#!/usr/bin/env python3
"""Compare ALL.TXT routing with temporary filesystem and actual text normalizer."""
import os,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=ROOT,stdout=subprocess.PIPE).stdout
old=sec(b,'QString DecodiumBridge::legacyAllTxtPath() const','QString DecodiumBridge::resolveFt8HashPlaceholdersFromRecentAllTxt(')
new=sec((ROOT/'src/bridge/DecodiumBridgeLegacyAllTxt.cpp').read_text(),'QString DecodiumBridge::legacyAllTxtPath() const','QString DecodiumBridge::logAllTxtPath() const').replace('decodiumBridgeApproxUtcDateTimeForDisplayToken(', 'approxUtcDateTimeForDisplayToken(')
if old!=new:raise RuntimeError('ALL.TXT source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-alltxt-ab-') as t:
 d=Path(t);h=(ROOT/'tests/bridge_refactor_alltxt_harness.cpp').read_text();out=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);(d/'test.cpp').write_text(h)
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(ROOT/'src/bridge'),str(d/'test.cpp'),str(ROOT/'src/bridge/DecodeMessageNormalization.cpp'),*flags,'-o',str(d/'test'))
  out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1]:raise RuntimeError('ALL.TXT outputs differ')
 print(out[1],end='')
