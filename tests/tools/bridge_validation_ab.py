#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,n):i=s.index(n);return s[i:s.index('\n}',i)+2]+'\n'
old=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout;new=(R/'src/bridge/DecodeValidationRules.cpp').read_text();names=['isPlainSignalReportToken','isRogerSignalReportToken','isQsoSignoffToken','isDirectedDecodeTerminalToken','looksLikeMalformedRGrid','hasPortableOperatingDesignator','isPlausibleDecodedCallsign','isSyntheticLabCallsign','digitRunCount','isPlausibleFt2CqCallsign','isCqRoutingModifierToken','isValidFt2CqTrailingPayload','hasHighConfidenceGhostPrefix','directedPeerLooksStructurallyGhost','isDirectedDecodePayloadValid','isResolvablePlaceholderDirectedPayload'];sigs=[('int ' if n=='digitRunCount' else 'bool ')+n+'(' for n in names];a='\n'.join(sec(old,'static '+n).replace('static ','',1) for n in sigs);b='\n'.join(sec(new,n) for n in sigs)
if a!=b:raise RuntimeError('decode validation source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-validation-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_validation_harness.cpp').read_text());out=[]
 for baseline in (True,False):
  (d/'bridge_refactor_validation.inc').write_text(a if baseline else '#include "DecodeValidationRules.h"\nusing namespace decodium::decode_validation;\n')
  files=[str(d/'test.cpp'),str(R/'Sequencer/MessageTokenRules.cpp'),str(R/'src/radio/Radio.cpp')]
  if not baseline:files.append(str(R/'src/bridge/DecodeValidationRules.cpp'))
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R),'-I'+str(R/'src/bridge'),'-I'+str(R/'src/radio'),'-I'+str(R/'src/core'),*files,*flags,'-o',str(d/'test'))
  out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1] or 'payload=R FN42|0|0|0|' not in out[0] or 'payload=R FN42|1|1|1|' not in out[0] or 'peer=1,0,0' not in out[0]:raise RuntimeError('unexpected validation output '+repr(out[0]))
 print(f'Decode validation A/B: {len(out[0].splitlines())} call, token and payload cases match using production rules')
