#!/usr/bin/env python3
"""Compare new-profile creation with isolated QSettings in baseline and current."""
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def run(*a,**kw):return subprocess.run(a,check=True,text=True,**kw)
old=run('git','show','v1.0.737:src/radio/DecodiumProfileSettings.h',cwd=R,stdout=subprocess.PIPE).stdout;new=(R/'src/radio/DecodiumProfileSettings.h').read_text();flags=shlex.split(run('pkg-config','--cflags','--libs','Qt6Core',stdout=subprocess.PIPE).stdout)
with tempfile.TemporaryDirectory(prefix='decodium-profile-growth-ab-') as tmp:
 d=Path(tmp);(d/'test.cpp').write_text((R/'tests/bridge_profile_growth_ab_harness.cpp').read_text());results=[]
 for code in (old,new):
  (d/'profile_under_test.h').write_text(code);run(*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test'));results.append(run(str(d/'test'),stdout=subprocess.PIPE).stdout.splitlines())
 if results!=[['global|17|35','copied-other-profile|1','first-intact|99'],['global|17|35','copied-other-profile|0','first-intact|99']]:raise RuntimeError(str(results))
 print('baseline:',', '.join(results[0]));print('current:',', '.join(results[1]))
