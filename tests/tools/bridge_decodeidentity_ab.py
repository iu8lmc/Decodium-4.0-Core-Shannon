#!/usr/bin/env python3
"""A/B compare extraction of calls and grids, using production token rules."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw): return subprocess.run(a,check=True,text=True,**kw)
def sec(s,n):
 i=s.index(n);return s[i:s.index('\n}',i)+2]+'\n'
old=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
new=(R/'src/bridge/DecodiumBridgeDecodeIdentity.cpp').read_text()
methods=['QString DecodiumBridge::extractDecodedCallsign(', 'QString DecodiumBridge::extractDecodedGrid(']
oldmethods='\n'.join(sec(old,n) for n in methods)
newmethods='\n'.join(sec(new,n) for n in methods).replace('decodiumBridgeIsGridToken(', 'isGridToken(').replace('decodiumBridgeLooksLikeCallsignToken(', 'looksLikeCallsignToken(')
if oldmethods.strip()!=newmethods.strip():raise RuntimeError('identity implementation differs from baseline')
current=(R/'src/bridge/DecodeTokenClassifier.cpp').read_text()
helpers=['bool isGridToken(', 'bool isTelemetryHexToken(', 'bool looksLikeCallsignToken(']
for n in helpers:
 if sec(old,n)!=sec(current,n):raise RuntimeError('token helper differs: '+n)
helpercode='\n'.join(sec(current,n) for n in helpers)
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-identity-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_identity_harness.cpp').read_text()); outputs=[]
 for methods in (oldmethods,newmethods):
  (d/'bridge_refactor_identity.inc').write_text(helpercode+'\n'+methods)
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R),'-I'+str(R/'src/radio'),'-I'+str(R/'src/core'),str(d/'test.cpp'),str(R/'Sequencer/MessageTokenRules.cpp'),str(R/'src/radio/Radio.cpp'),*flags,'-o',str(d/'test'))
  outputs.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if outputs[0]!=outputs[1]:raise RuntimeError('identity A/B differs')
 print(f'Decoded call and grid A/B: {len(outputs[0].splitlines())} cases match, using production token rules')
