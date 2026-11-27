import os,secrets,shutil,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];rig='WindowProbe'+secrets.token_hex(5);qml=r/'build/qml/decodium/Main.qml';original=qml.read_text();marker='GlassApplicationWindow {';assert original.count(marker)==1
probe='''
 Timer { interval:9000; running:true; repeat:false; onTriggered: {
   bridge.saveWindowState("SyntheticLab",-40,60,800,400,true,false);
   var s=bridge.loadWindowState("SyntheticLab");
   var ok=s.x===-40 && s.y===60 && s.width===800 && s.height===400 && s.detached===true && s.minimized===false;
   bridge.saveWindowStatesAsync({"SyntheticLab": {"x":100,"maximized":true}}, {"LabLayout": "synthetic"});
   console.log("[WINDOW-APP] sync="+ok); afterAsync.start();
 } }
 Timer { id:afterAsync; interval:1500; repeat:false; onTriggered: {
   var s=bridge.loadWindowState("SyntheticLab");
   var ok=s.x===100 && s.width===800 && s.maximized===true;
   console.log("[WINDOW-APP] async="+ok+" tx="+bridge.transmitting); Qt.quit();
 } }
'''
try:
 with tempfile.TemporaryDirectory(prefix='decodium-window-probe-') as temp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=temp,DECODIUM_DISABLE_QML_CACHE='1',QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software')
  policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:2237"))(allow network-outbound (remote ip "localhost:2239"))'
  cmd=['sandbox-exec','-p',policy,str(r/'build/decodium'),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','22000'];p=subprocess.run(cmd,cwd=r,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=40)
  lines=[x for x in p.stdout.splitlines() if '[WINDOW-APP]' in x];print('\n'.join(lines));assert p.returncode==0 and len(lines)==2 and 'sync=true' in lines[0] and 'async=true tx=false' in lines[1] and '[LAB] CAT auto-connect disabled' in p.stdout
finally:
 qml.write_text(original)
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  target=parent/('Decodium - '+rig+' - test')
  if target.is_dir():shutil.rmtree(target)

