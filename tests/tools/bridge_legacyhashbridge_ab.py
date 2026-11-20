#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
def sec(s,a,b=None):i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
s=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
a=sec(s,'QString DecodiumBridge::resolveFt8HashPlaceholdersFromRecentAllTxt(', '\n}');a+='\n}\n';b=sec((R/'src/bridge/DecodiumBridgeLegacyHash.cpp').read_text(),'QString DecodiumBridge::resolveFt8HashPlaceholdersFromRecentAllTxt(')
for old,new in [('secondsFromUtcDisplayToken(', 'decodiumBridgeSecondsFromUtcDisplayToken('),('normalizeUtcDisplayToken(', 'decodiumBridgeNormalizeRemoteActivityTime('),('bridgeLog(', 'decodiumBridgeDecodeFlowLog(')]:a=a.replace(old,new)
if a.strip()!=b.strip():raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-legacyhashbridge-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_legacyhashbridge_harness.cpp').read_text());real=(R/'src/bridge/DecodeUtcTime.cpp').read_text();(d/'times.cpp').write_text('#include <QtCore>\n#include <utility>\n'+sec(real,'static QString normalizeUtcDisplayToken('));out=[]
 for code in (a,b):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R/'src/bridge'),'-I'+str(R),'-I'+str(R/'src/radio'),'-I'+str(R/'src/core'),str(d/'test.cpp'),str(d/'times.cpp'),str(R/'src/bridge/LegacyHashResolve.cpp'),str(R/'src/bridge/LegacyDecodeText.cpp'),str(R/'src/bridge/DecodeMessageNormalization.cpp'),str(R/'Sequencer/MessageTokenRules.cpp'),str(R/'src/radio/Radio.cpp'),*flags,'-o',str(d/'test'));out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 expected=['match|K1ABC K2XYZ RR73|ALL.TXT','wrong-mode|<...> K2XYZ RR73|1','no-match|<...> K2XYZ RR73|1','ambiguous|<...> K2XYZ RR73|1']
 if out[0]!=out[1] or out[0].splitlines()!=expected:raise RuntimeError('unexpected '+repr(out))
 print(out[0],end='')
