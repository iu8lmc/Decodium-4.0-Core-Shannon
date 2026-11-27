import json,os,secrets,shutil,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2];rig='AllTxtProbe'+secrets.token_hex(5);source=root/'src/bridge/DecodiumBridgeLegacyInit.cpp';qml=root/'build/qml/decodium/Main.qml';original_source=source.read_text();original_qml=qml.read_text();macro='#define embeddedLegacyRigControlEnabledForBackend decodiumBridgeEmbeddedLegacyRigEnabled';assert original_source.count(macro)==1
marker='GlassApplicationWindow {';assert original_qml.count(marker)==1
probe='''
 Timer { interval:9000; running:true; repeat:false; onTriggered: {
  var p=String(bridge.logAllTxtPath);var safe=p.indexOf("/.qttest/")>=0;
  var opened=safe && bridge.openAllTxtFolder();
  console.log("[ALLTXT-FOLDER-APP] "+JSON.stringify({safe:safe,opened:opened,tx:bridge.transmitting,path:p}));Qt.quit();
 } }
'''
try:
 source.write_text(original_source.replace(macro,'#define embeddedLegacyRigControlEnabledForBackend(...) false',1))
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=root,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
 original_qml=qml.read_text();assert original_qml.count(marker)==1;qml.write_text(original_qml.replace(marker,marker+probe,1))
 with tempfile.TemporaryDirectory(prefix='folder-probe-settings-') as temp:
  env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=temp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='cocoa',QT_QUICK_BACKEND='software')
  policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
  cmd=['sandbox-exec','-p',policy,str(root/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','22000'];p=subprocess.run(cmd,cwd=root,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=45)
  rows=[x.split('[ALLTXT-FOLDER-APP] ',1)[1] for x in p.stdout.splitlines() if '[ALLTXT-FOLDER-APP] ' in x]
  if len(rows)!=1: raise RuntimeError('Missing or duplicate probe result')
  value=json.loads(rows[0]);print('[ALLTXT-FOLDER-APP] '+rows[0],flush=True)
  if not value['safe'] or not value['opened'] or value['tx']: raise RuntimeError('Unsafe or unsuccessful probe: '+rows[0])
  print('rig='+rig,flush=True)
  if p.returncode or '[ALLTXT-FOLDER-APP]' not in p.stdout or '[LAB] CAT auto-connect disabled' not in p.stdout:raise RuntimeError(p.stdout[-4000:])
finally:
 source.write_text(original_source);qml.write_text(original_qml)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  profile=parent/('Decodium - '+rig+' - test')
  if profile.is_dir(): shutil.rmtree(profile)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=root,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
