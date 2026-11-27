#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,n):i=s.index(n);return s[i:s.index('\n}',i)+2]+'\n'
old=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout;new=(R/'src/bridge/DecodeValidationRules.cpp').read_text();names=['bool decodeListModel_isTelemetryHexToken(', 'bool decodeListModel_isTelemetryOnlyMessage(', 'bool isValidDecode('];a='\n'.join(sec(old,('static ' if n.startswith('bool isValid') else '')+n).replace('static ','',1) if n.startswith('bool isValid') else sec(old,n) for n in names);b='\n'.join(sec(new,n) for n in names)
if a!=b:raise RuntimeError('decode telemetry source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-telemetry-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_telemetry_harness.cpp').read_text());out=[]
 for baseline in (True,False):
  (d/'bridge_refactor_telemetry.inc').write_text(a if baseline else '#include "DecodeValidationRules.h"\nusing namespace decodium::decode_validation;\n')
  files=[str(d/'test.cpp'),str(R/'Sequencer/MessageTokenRules.cpp'),str(R/'src/radio/Radio.cpp')]
  if not baseline:files.append(str(R/'src/bridge/DecodeValidationRules.cpp'))
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R),'-I'+str(R/'src/bridge'),'-I'+str(R/'src/radio'),'-I'+str(R/'src/core'),*files,*flags,'-o',str(d/'test'))
  out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1] or 'A1B2C3D4E5|-30|1|1|1' not in out[0] or 'CQ K1ABC FN42|-30|0|0|1' not in out[0] or '> <|-30|0|0|0' not in out[0]:raise RuntimeError('unexpected telemetry output '+repr(out[0]))
 print(f'Decode telemetry A/B: {len(out[0].splitlines())} classification and validation cases match using production rules')
