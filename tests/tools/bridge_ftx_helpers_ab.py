#!/usr/bin/env python3
"""Compare the original FT message helpers with the production-linked extraction."""
import os, shlex, subprocess, tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*args,**kw): return subprocess.run(args,check=True,text=True,**kw)
def section(s,start):
 i=s.index(start); return s[i:s.index('\n}',i)+2]+'\n'
old=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
new=(R/'src/bridge/FtxMessageHelpers.cpp').read_text()
names=['static bool isFtx77MessageMode(', 'static QString normalizedTxCallToken(', 'static QString ftxHashedCallToken(', 'static QString standardFtxQsoMessage(', 'static QString standardFtxCqMessage(', 'static QString customCqPrefixFromMessage(', 'static QString ftxCqMessageWithCustomPrefix(']
oldcode='\n'.join(section(old,n) for n in names)
newcode='\n'.join(section(new,n.replace('static ','') if n.split('(')[0].split()[-1] in {'isFtx77MessageMode','standardFtxQsoMessage','standardFtxCqMessage','customCqPrefixFromMessage','ftxCqMessageWithCustomPrefix'} else n) for n in names)
for n in ('isFtx77MessageMode','standardFtxQsoMessage','standardFtxCqMessage','customCqPrefixFromMessage','ftxCqMessageWithCustomPrefix'):
 newcode=newcode.replace(('bool ' if n=='isFtx77MessageMode' else 'QString ')+n+'(',('static bool ' if n=='isFtx77MessageMode' else 'static QString ')+n+'(',1)
if oldcode.strip()!=newcode.strip(): raise RuntimeError('implementation differs from baseline')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
prefix='#include "Sequencer/MessageTokenRules.hpp"\n#include "DecodeSignalReport.h"\nusing decodium::seq::normalizeCallToken;\n'
with tempfile.TemporaryDirectory(prefix='decodium-ftx-ab-') as tmp:
 d=Path(tmp); (d/'test.cpp').write_text((R/'tests/bridge_refactor_ftx_helpers_harness.cpp').read_text()); outputs=[]
 for code in (prefix+oldcode,'#include "FtxMessageHelpers.h"\n'):
  (d/'bridge_refactor_ftx_helpers.inc').write_text(code)
  files=[str(d/'test.cpp'),str(R/'Sequencer/MessageTokenRules.cpp'),str(R/'src/radio/Radio.cpp'),str(R/'src/bridge/DecodeSignalReport.cpp')]
  if code.startswith('#include "FtxMessageHelpers.h"'):files.append(str(R/'src/bridge/FtxMessageHelpers.cpp'))
  run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(R),'-I'+str(R/'src/bridge'),'-I'+str(R/'src/radio'),'-I'+str(R/'src/core'),*files,*flags,'-o',str(d/'test'))
  outputs.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if outputs[0]!=outputs[1]: raise RuntimeError('A/B output differs')
 print(f'FTX helper A/B: {len(outputs[0].splitlines())} cases match; source and production-linked implementation agree')
