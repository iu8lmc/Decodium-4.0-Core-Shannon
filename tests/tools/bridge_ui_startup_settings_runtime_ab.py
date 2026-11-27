#!/usr/bin/env python3
"""Compare startup policy logs in isolated real application sessions."""
from pathlib import Path
import os,secrets,shutil,subprocess,tempfile,json
root=Path(__file__).resolve().parents[2]
main=root/'src/bridge/DecodiumBridge.cpp';module=root/'src/bridge/DecodiumBridgeUiStartupSettings.cpp'
original_main,original_module=main.read_text(),module.read_text()
fixture=(root/'tests/bridge_refactor_fixtures/ui_startup_settings_before.inc').read_text()
assert original_module.replace('decodiumBridgeAudioLevelLog(', 'bridgeLog(').endswith('void DecodiumBridge::initialiseUiStartupSettings()\n{\n'+fixture+'}\n')
marker='    initialiseUiStartupSettings();\n\n';assert original_main.count(marker)==1
policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
results={}
qml=root/'build/qml/decodium/Main.qml'
original_qml=qml.read_text()
probe='\n Timer { interval:6000; running:true; repeat:false; onTriggered:{console.log("[UI-START-PROBE] "+JSON.stringify({quality:bridge.uiQuality,style:bridge.uiStyle,detach:bridge.autoDetachFullSpectrum,fps:bridge.spectrumFpsCap}));Qt.quit();} }\n'
try:
 for label in ('before','after'):
  if label=='before':main.write_text(original_main.replace(marker,fixture,1));module.write_text('#include "DecodiumBridge.h"\n')
  else:main.write_text(original_main);module.write_text(original_module)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=root,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  results[label]=[]
  for mode in ('default',):
   rig='GpuInit'+secrets.token_hex(4)
   try:
    with tempfile.TemporaryDirectory(prefix='decodium-gpu-start-') as tmp:
     env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1',QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software')
     for k in ('DECODIUM_DISABLE_GPU_PANADAPTER_FFT','DECODIUM_DISABLE_GPU_PANADAPTER_FORCE','DECODIUM_ENABLE_EXPERIMENTAL_OPENGL_GPU_PANADAPTER_FFT'):env.pop(k,None)
     if mode=='disabled':env['DECODIUM_DISABLE_GPU_PANADAPTER_FFT']='1'
     qml.write_text(original_qml.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1))
     cmd=['sandbox-exec','-p',policy,str(root/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','9000']
     result=subprocess.run(cmd,cwd=root,env=env,capture_output=True,text=True,timeout=45);log=result.stdout+result.stderr
     rows=[line.split('[UI-START-PROBE] ',1)[1] for line in log.splitlines() if '[UI-START-PROBE] ' in line]
     ok=result.returncode==0 and len(rows)==1 and '[LAB] CAT auto-connect disabled' in log
     if not ok:raise RuntimeError(log[-5000:])
     value=json.loads(rows[0]);results[label].append(value);print(label,value,flush=True)
   finally:
    for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
     path=parent/('Decodium - '+rig+' - test')
     if path.is_dir():shutil.rmtree(path)
 assert results['before']==results['after']
finally:
 qml.write_text(original_qml);main.write_text(original_main);module.write_text(original_module)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=root,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
