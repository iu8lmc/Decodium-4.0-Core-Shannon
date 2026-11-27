#!/usr/bin/env python3
"""Run FT8 virtual-audio A/B pair and clean only its unique profiles."""
import argparse,runpy,secrets,shutil,sys
from pathlib import Path
R=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=R/'build/decodium');p.add_argument('--qml',type=Path,default=R/'build/qml/decodium/Main.qml');a=p.parse_args()
suffix=secrets.token_hex(5);secrets.token_hex=lambda n:suffix
script=R/'tests/tools/bridge_legacydelta_pair_lab.py'
sys.argv=[str(script),'--binary',str(a.binary),'--qml',str(a.qml)]
try:runpy.run_path(str(script),run_name='__main__')
finally:
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  for role in ('LegacyRX','LegacyTX'):
   path=parent/('Decodium - '+role+suffix+' - test')
   if path.is_dir():shutil.rmtree(path)
