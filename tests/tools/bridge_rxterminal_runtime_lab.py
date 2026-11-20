#!/usr/bin/env python3
"""Check real app RX terminal availability against its installed executable."""
import argparse,json,os,secrets,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();binary=a.binary.resolve();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
rig='Rxterm'+secrets.token_hex(5)
probe='''
 Timer { interval: 12000; running: true; repeat: false; onTriggered: {console.log("[RXTERM-PROBE] "+JSON.stringify({available:bridge.decodiumRxTerminalAvailable(),tx:bridge.transmitting}));} }
'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-rxterminal-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
  cmd=[str(binary),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','17000']
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=60)
  lines=[x.split('[RXTERM-PROBE] ',1)[1] for x in result.stdout.splitlines() if '[RXTERM-PROBE] ' in x]
  if result.returncode or len(lines)!=1 or '[LAB] CAT auto-connect disabled' not in result.stdout:raise RuntimeError('probe failed '+result.stdout[-4000:])
  v=json.loads(lines[0]);expected=(binary.parent/'decodium-rx').exists()
  if v!={'available':expected,'tx':False}:raise RuntimeError('unexpected '+str(v))
  print(json.dumps({'availability_matches_installation':True,'tx':False},sort_keys=True))
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
