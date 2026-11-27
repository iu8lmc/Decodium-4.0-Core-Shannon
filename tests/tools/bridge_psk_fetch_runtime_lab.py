#!/usr/bin/env python3
"""Verify local-call-aware directed decoding in an isolated legacy session."""
import argparse,json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--variant',choices=('a','b'),default='a');p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
rig='AllTxt'+secrets.token_hex(5);wav=ROOT/'tests/bridge_refactor_fixtures/ft8-k1abc-1500.wav'
probe = r'''
 Timer { interval:12000; running:true; repeat:false; onTriggered:{
  bridge.pskReporterEnabled=false;bridge.sendPskReporterNow();__FETCH__;checkSearch.start();
 }}
 Timer {id:checkSearch;interval:11000;repeat:false;onTriggered:{
  console.log("[ALLTXT-PROBE] "+JSON.stringify({heardFetching:bridge.pskHeardByFetching,mapFetching:bridge.pskMapSpotFetching,count:bridge.pskHeardByCount,tx:bridge.transmitting}));
 }}
'''.replace('__FETCH__','bridge.fetchPskHeardBy()' if a.variant=='a' else 'bridge.fetchPskMapSpots();bridge.refreshPskMapSpots()')

try:
 with tempfile.TemporaryDirectory(prefix='decodium-alltxt-lab-') as tmp:
  sinks=[]
  for port in (2237,2239):
   sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',port));sinks.append(sock)
  guard=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);guard.bind(('127.0.0.1',2238));guard.close()
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','27000']
  result=subprocess.run(['sandbox-exec','-p','(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))']+cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=100)
  lines=[x.split('[ALLTXT-PROBE] ',1)[1] for x in result.stdout.splitlines() if '[ALLTXT-PROBE] ' in x]
  if result.returncode or len(lines)!=1 or lines[0] in ('unsafe-path-or-legacy','write-error','timeout') or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-10000:]);raise RuntimeError('ALL.TXT probe failed')
  v=json.loads(lines[0]);expected={'heardFetching':False,'mapFetching':False,'count':0,'tx':False}
  if v!=expected:raise RuntimeError('unexpected PSK fetch state '+str(v))
  print(json.dumps(v,sort_keys=True))

finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
