#!/usr/bin/env python3
"""Verify real startup reads a uniquely scoped synthetic ALL.TXT."""
import argparse,datetime,os,secrets,shutil,subprocess,tempfile,sys
from pathlib import Path
R=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=R/'build/decodium');a=p.parse_args()
rig='KnownCq'+secrets.token_hex(5);name='Decodium - '+rig+' - test'
if sys.platform == 'darwin':
 profile=Path.home()/'.qttest/Library/Application Support/IU8LMC'/name
 cache=Path.home()/'.qttest/Library/Caches/IU8LMC'/name
elif sys.platform.startswith('linux'):
 profile=Path.home()/'.qttest/share/IU8LMC'/name
 cache=Path.home()/'.qttest/cache/IU8LMC'/name
else:
 raise RuntimeError('unsupported QStandardPaths test location')
if profile.exists() or cache.exists():raise RuntimeError('unexpected existing test profile')
try:
 profile.mkdir(parents=True);dt=datetime.datetime.now(datetime.timezone.utc).strftime('%y%m%d_%H%M%S')
 (profile/'ALL.TXT').write_text(dt+' 14.074 Rx FT8 -10 0.2 1500 CQ K1ABC FN42\n')
 with tempfile.TemporaryDirectory(prefix='decodium-knowncq-lab-') as tmp:
  env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_FORCE_STANDALONE_UDP='1');env.pop('DECODIUM_FT8_SEED_KNOWN_CQ_REPLAY',None)
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','14000']
  res=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=60)
  msg='FT8 known-CQ cache seeded: entries=0 hashCalls=1 source='+str(profile/'ALL.TXT')
  if res.returncode or '[LAB] CAT auto-connect disabled' not in res.stdout or msg not in res.stdout:
   print(res.stdout[-12000:]);raise RuntimeError('startup seed not observed')
  print('startup synthetic ALL.TXT: hashCalls=1, CQ replay disabled, CAT disabled: OK')
finally:
 for path in (profile,cache):
  if path.is_dir():shutil.rmtree(path)
