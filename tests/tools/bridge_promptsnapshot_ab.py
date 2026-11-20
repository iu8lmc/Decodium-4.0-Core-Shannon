#!/usr/bin/env python3
"""Compile and compare baseline and extracted prompt snapshot logic."""
import os, shlex, subprocess, tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw): return subprocess.run(a,check=True,text=True,**kw)
def sec(s,a,b=None):
 i=s.index(a);return s[i:s.index(b,i) if b else len(s)]
b=run('git','show','v1.0.737:src/bridge/DecodiumBridge.cpp',cwd=R,stdout=subprocess.PIPE).stdout
old=sec(b,'void DecodiumBridge::capturePromptLogSnapshot(', 'QString DecodiumBridge::defaultLogCommentForQso(')
new=sec((R/'src/bridge/DecodiumBridgePromptSnapshot.cpp').read_text(),'void DecodiumBridge::capturePromptLogSnapshot(')
if old!=new:raise RuntimeError('source differs')
flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-promptsnapshot-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_refactor_prompt_snapshot_harness.cpp').read_text());outputs=[]
 for code in (old,new):
  (d/'bridge_refactor_under_test.inc').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));outputs.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout)
 if outputs[0]!=outputs[1]:raise RuntimeError('results differ')
 print(outputs[0],end='')
