#!/usr/bin/env python3
"""Exercise DecoSync's visual timer through real host-window visibility."""
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
main=root/'build-baseline/qml/decodium/Main.qml'
panel=root/'build-baseline/qml/decodium/components/DecoSyncPanel.qml'
original_main,original_panel=main.read_text(),panel.read_text()
assert original_main.count('        DecoSyncPanel {') == 1
probe='''
 Timer { interval:10000; running:true; repeat:false; onTriggered: {
   state.hidden=!labSyncPanel.panelActive && labSyncPanel.labTimer.running;
   mainWindow.openDecoSyncMonitor(); openWait.start();
 }}
 Timer { id:openWait; interval:1500; repeat:false; onTriggered: {
   state.open=decoSyncMonitorWindow.visible && labSyncPanel.panelActive && labSyncPanel.labTimer.running;
   state.sampled=labSyncPanel.offsetHistory.length>0;
   mainWindow.decoSyncMonitorVisible=false; state.stopped=labSyncPanel.labTimer.running && !labSyncPanel.panelActive;
   state.hiddenCount=labSyncPanel.offsetHistory.length; state.hiddenRequests=labSyncPanel.labRequests; hiddenWait.start();
 }}
 Timer {id:hiddenWait; interval:1500; repeat:false; onTriggered: {
   state.stayedHidden=labSyncPanel.offsetHistory.length>state.hiddenCount && labSyncPanel.labRequests===state.hiddenRequests;
   labSyncPanel.labStatus.text="sentinel";mainWindow.openDecoSyncMonitor();
   state.reopened=labSyncPanel.labTimer.running && labSyncPanel.panelActive;
   state.refreshed=labSyncPanel.labStatus.text!=="sentinel";
   state.tx=bridge.transmitting;state.cat=bridge.catConnected;
   console.log('[SYNC-VISIBILITY] '+JSON.stringify(state));Qt.quit();
 }}
 QtObject {id:state;property bool hidden:false;property bool open:false;property bool sampled:false;
  property bool stopped:false;property int hiddenCount:0;property int hiddenRequests:0;property bool stayedHidden:false;
  property bool reopened:false;property bool refreshed:false;property bool tx:false;property bool cat:false}
'''
# Serialize individual properties rather than QtObject identity.
probe=probe.replace("JSON.stringify(state)", "JSON.stringify({hidden:state.hidden,open:state.open,sampled:state.sampled,stopped:state.stopped,stayedHidden:state.stayedHidden,reopened:state.reopened,refreshed:state.refreshed,tx:state.tx,cat:state.cat})")
try:
 main.write_text(original_main.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1).replace('        DecoSyncPanel {','        DecoSyncPanel {\n            id: labSyncPanel',1))
 panel.write_text(original_panel.replace('    id: root\n','    id: root\n    property alias labTimer: refreshTimer\n    property alias labStatus: statusLabel\n    property int labRequests: 0\n',1).replace('offsetCanvas.requestPaint()','{ root.labRequests++; offsetCanvas.requestPaint() }'))
 with tempfile.TemporaryDirectory(prefix='decodium-sync-visibility-') as tmp:
  env=dict(os.environ,QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
  cmd=[str(root/'build-baseline/decodium'),'--test-mode','--rig-name','Sync'+secrets.token_hex(5),'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','40000']
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=55)
  lines=[line.split('[SYNC-VISIBILITY] ',1)[1] for line in result.stdout.splitlines() if '[SYNC-VISIBILITY] ' in line]
  if result.returncode or len(lines)!=1:raise RuntimeError(result.stdout[-7000:])
  state=json.loads(lines[0]); expected={'hidden':True,'open':True,'sampled':True,'stopped':True,'stayedHidden':True,'reopened':True,'refreshed':True,'tx':False,'cat':False}
  if state!=expected:raise RuntimeError(repr(state)+'\n'+result.stdout[-3000:])
  print(json.dumps(state,sort_keys=True))
finally:
 main.write_text(original_main);panel.write_text(original_panel)
