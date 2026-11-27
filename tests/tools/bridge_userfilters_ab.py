#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
old=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout;new=(R/'src/bridge/DecodeUserFilters.cpp').read_text();start='static void addDecodeFilterSubject(';end='bool DecodiumBridge::contestDecodeExchangesEnabled()';a=old[old.index(start):old.index(end,old.index(start))].strip().replace('static bool shouldAcceptDecodeEntryByUserFilters(', 'bool shouldAcceptDecodeEntryByUserFilters(');b=new[new.index(start):].strip().replace('decodiumBridgeLooksLikeCallsignToken(', 'looksLikeCallsignToken(')
if a!=b:raise RuntimeError('decode user filter source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-userfilters-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_userfilters_harness.cpp').read_text());out=[]
 for baseline in (True,False):
  (d/'bridge_refactor_userfilters.inc').write_text(a if baseline else '')
  files=[str(d/'test.cpp'),str(R/'src/bridge/DecodeMessageNormalization.cpp'),str(R/'Sequencer/MessageTokenRules.cpp'),str(R/'src/radio/Radio.cpp'),str(R/'src/bridge/DecodeTokenClassifier.cpp')]
  if not baseline:files.append(str(R/'src/bridge/DecodeUserFilters.cpp'))
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R),'-I'+str(R/'src/bridge'),'-I'+str(R/'src/radio'),'-I'+str(R/'src/core'),*files,*flags,'-o',str(d/'test'))
  out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1] or '|0|blacklist' not in out[0] or '|0|territory' not in out[0] or '|0|not whitelisted' not in out[0]:raise RuntimeError('unexpected user filter output')
 print(f'Decode user filters A/B: {len(out[0].splitlines())} cases match; real normalization and token rules, production callsign classifier')
