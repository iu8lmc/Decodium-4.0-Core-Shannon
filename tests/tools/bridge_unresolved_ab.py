#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**k):return subprocess.run(a,check=True,text=True,**k)
def sec(s,n):i=s.index(n);return s[i:s.index('\n}',i)+2]+'\n'
old=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout;old=old[old.index('// === Shannon FDR'):];new=(R/'src/bridge/DecodeUnresolved.cpp').read_text();names=['bool messageIsUnresolvedGridOnlyDecode(', 'QString displayMessageWithCallsignsActorFirst(', 'bool messageHasUnresolvedPeerPlaceholder(', 'bool isFst4wUnresolvedHashDecode('];a='\n'.join(sec(old,'static '+n).replace('static ','',1) for n in names);b='\n'.join(sec(new,n).replace('QString* gridOut)', 'QString* gridOut = nullptr)',1) if n.startswith('bool messageIs') else sec(new,n) for n in names)
if a!=b:raise RuntimeError('unresolved message source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-unresolved-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_unresolved_harness.cpp').read_text());out=[]
 for baseline in (True,False):
  (d/'bridge_refactor_unresolved.inc').write_text(a if baseline else '#include "DecodeUnresolved.h"\nusing namespace decodium::decode_unresolved;\n')
  files=[str(d/'test.cpp'),str(R/'src/bridge/DecodeTokenClassifier.cpp'),str(R/'Sequencer/MessageTokenRules.cpp'),str(R/'src/radio/Radio.cpp')]
  if not baseline:files.append(str(R/'src/bridge/DecodeUnresolved.cpp'))
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R),'-I'+str(R/'src/bridge'),'-I'+str(R/'src/radio'),'-I'+str(R/'src/core'),*files,*flags,'-o',str(d/'test'))
  out.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if out[0]!=out[1] or 'placeholder_modifier=1 placeholder_recognized=1' not in out[0] or 'CQ K1ABC FN42|FT8|0|0|FN42|0|0|' not in out[0] or '<...> FN42|FST4W|0|0|FN42|0|1|' not in out[0]:raise RuntimeError('unexpected unresolved output '+repr(out[0][:1500])+' identical='+str(out[0]==out[1]))
 print(f'Unresolved message A/B: {len(out[0].splitlines())-1} cases match using production token classifier; placeholder_modifier=1 placeholder_recognized=1')
