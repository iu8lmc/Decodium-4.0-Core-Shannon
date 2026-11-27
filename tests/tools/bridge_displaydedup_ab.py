#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def sec(s,a,b):i=s.index(a);return s[i:s.index(b,i)]
s=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout;t=(R/'src/bridge/DecodeChronology.cpp').read_text();a=sec(s,'static int displayDedupPeriodMsForMode(', 'static int utcTokenForSlotStart(');b=sec(t,'static int displayDedupPeriodMsForMode(', 'void decodiumBridgeNormalizeDecodeEntries(')
if a.strip()!=b.strip():raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-displaydedup-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_displaydedup_harness.cpp').read_text());out=[]
 for code in (a,b):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R/'src/bridge'),str(d/'test.cpp'),str(R/'src/bridge/DecodeUtcTime.cpp'),str(R/'src/bridge/DecodeChronology.cpp'),str(R/'src/bridge/DecodeMessageNormalization.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 actual=(R/'tests/bridge_refactor_displaydedup_harness.cpp').read_text();actual=actual.replace('#include "bridge_refactor_under_test.inc"','').replace('coalesceDecodeEntriesForDisplay(entries,"FT8")','decodiumBridgeNormalizeDecodeEntries(entries,1500,"FT8")');actual=actual[:actual.index('o<<"slots|"')]+'}\n';(d/'test.cpp').write_text(actual);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R/'src/bridge'),str(d/'test.cpp'),str(R/'src/bridge/DecodeUtcTime.cpp'),str(R/'src/bridge/DecodeChronology.cpp'),str(R/'src/bridge/DecodeMessageNormalization.cpp'),*flags,'-o',str(d/'test'));actualOut=run(str(d/'test'),stdout=subprocess.PIPE).stdout
 if actualOut.strip()!='coalesce|3|USA|1':raise RuntimeError('actual module: '+actualOut)
 if out[0]!=out[1] or out[0].splitlines()!=['coalesce|3|USA|1','slots|15000|7500|3750|0','frequency|1500|1504']:raise RuntimeError('unexpected '+repr(out))
 print(out[0],end='')
