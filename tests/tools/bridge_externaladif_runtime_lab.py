#!/usr/bin/env python3
"""Send an explicit synthetic ADIF upload to a private UDP loopback port."""
import argparse,json,os,secrets,shutil,socket,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--variant',choices=('a','b'),required=True);p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
wav=ROOT/('tests/bridge_refactor_fixtures/ft8-k1abc-1500.wav' if a.variant=='a' else 'tests/bridge_refactor_fixtures/ft8-9h1sr-1300.wav');wanted='CQ K1ABC FN42' if a.variant=='a' else 'CQ 9H1SR JM75';rig='UdpLab'+secrets.token_hex(5)
probe='''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{var bad=bridge.uploadExternalAdif("", "", "UDP");var good=bridge.uploadExternalAdif("k1abc", "<CALL:5>K1ABC <BAND:3>20M", "UDP");console.log("[UDP-LAB] "+JSON.stringify({decoded:!bad.ok && good.ok,tx:bridge.transmitting || bridge.txEnabled}));}}
'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-udp-lab-') as tmp, socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sink:
  sink.bind(('127.0.0.1',0));port=sink.getsockname()[1];sink.settimeout(0.2)
  ini=Path(tmp)/'Decodium'/'Decodium3.ini';ini.parent.mkdir();ini.write_text(f'[{rig}]\n_ProfileSettingsInitialized=true\nUDPServer=127.0.0.1\nUDPServerPort={port}\nUDPSecondaryEnabled=false\nUDPTertiaryEnabled=false\nAcceptUDPRequests=false\nUDPPrimarySendDecode=true\n')
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1',QT_QPA_PLATFORM='offscreen',QT_QPA_PLATFORM_PLUGIN_PATH='/opt/homebrew/share/qt/plugins/platforms',QT_QUICK_BACKEND='software')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','43000'];policy=f'(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:{port}"))'
  r=subprocess.run(['sandbox-exec','-p',policy]+cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
  packets=[]
  while True:
   try:packets.append(sink.recv(65535))
   except socket.timeout:break
  matches=[p for p in packets if b'<CALL:5>K1ABC' in p and b'<BAND:3>20m' in p and b'<eor>' in p.lower()]
  if r.returncode or '[UDP-LAB] {"decoded":true,"tx":false}' not in r.stdout or '[LAB] CAT auto-connect disabled' not in r.stdout or not matches:
   print(r.stdout[-12000:]);raise RuntimeError('no matching local UDP decode packet: '+str([len(p) for p in packets]))
  print(f'{a.variant}: synthetic ADIF upload received {len(matches)} matching UDP packet(s) on private loopback; TX disabled')
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  path=parent/('Decodium - '+rig+' - test')
  if path.is_dir():shutil.rmtree(path)
