#!/usr/bin/env python3
"""Measure DX waterfall filtering with synthetic 200-entry spot lists."""
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2];main=root/'build-baseline/qml/decodium/Main.qml';component=root/'build-baseline/qml/decodium/components/Waterfall.qml'
orig_main,orig_component=main.read_text(),component.read_text()
anchor='''    id: waterfallPanel
'''
spots='''        var spots = bridge.dxClusterSpots || []'''
dial='''        var dialHz = Number(bridge.frequency) || 0'''
startlog='''        console.log("[Waterfall][DxCluster] refresh: spots=" + spots.length
                    + " dialHz=" + dialHz + " fmin=" + fmin + " fmax=" + fmax
                    + " toggle=" + dxClusterCheck.checked)
'''
endlog='''        console.log("[Waterfall][DxCluster] inRange=" + totalRange + " sentToWaterfall=" + out.length)
'''
if any(orig_component.count(x)!=1 for x in (anchor,spots,dial,startlog,endlog)):raise RuntimeError('unexpected component layout')
probe='''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{var w=waterfallEmbeddedLoader.item;
  if(!w||!w.visible){console.log('[DX-PROFILE] '+JSON.stringify({ready:false}));Qt.quit();return}
  if(!bridge.dxCluster){console.log('[DX-PROFILE] '+JSON.stringify({ready:false,reason:'no-cluster'}));Qt.quit();return}
  for(var i=0;i<200;i++)bridge.dxCluster.injectSpot({dxCall:'K'+i+'ABC',band:'20m',mode:'FT8',frequency:i<20?14074.4+i*0.12:14100+i});
  w.labDx.checked=true;var spotCount=bridge.dxClusterSpots.length;var t=Date.now();for(var n=0;n<1000;n++)w.refreshDxClusterSpots();
  var elapsed=Date.now()-t;console.log('[DX-PROFILE] '+JSON.stringify({ready:true,elapsedMs:elapsed,iterations:1000,spots:spotCount,shown:w.labLast.length,visible:w.visible,tx:bridge.transmitting,cat:bridge.catConnected}));Qt.quit();}}
'''
try:
 main.write_text(orig_main.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1))
 component_text=orig_component.replace(anchor,anchor+'    property var labLast: []\n    property alias labDx: dxClusterCheck\n',1).replace(dial,'        var dialHz = 14074000',1)
 if os.environ.get('DECODIUM_LAB_DX_LOGS')!='1':component_text=component_text.replace(startlog,'',1).replace(endlog,'',1)
 else:component_text=component_text.replace(endlog,endlog+'        waterfallPanel.labLast = out\n',1)
 if os.environ.get('DECODIUM_LAB_DX_LOGS')!='1':component_text=component_text.replace('        waterfallDisplay.setDxClusterSpots(out)','        waterfallPanel.labLast = out\n        waterfallDisplay.setDxClusterSpots(out)',1)
 component.write_text(component_text)
 with tempfile.TemporaryDirectory(prefix='decodium-dx-profile-') as tmp:
  env=dict(os.environ,QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
  cmd=[str(root/'build-baseline/decodium'),'--test-mode','--rig-name','DXProfile'+secrets.token_hex(5),'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','40000']
  run=subprocess.run(cmd,cwd=root,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=50)
  rows=[json.loads(x.split('[DX-PROFILE] ',1)[1]) for x in run.stdout.splitlines() if '[DX-PROFILE] ' in x]
  if run.returncode or len(rows)!=1 or not rows[0].get('ready') or rows[0].get('tx') or rows[0].get('cat') or rows[0].get('spots')!=200 or rows[0].get('shown')!=20:raise RuntimeError(run.stdout[-6000:])
  result=rows[0];result['msPerRefresh']=result['elapsedMs']/result['iterations'];result['logs']=os.environ.get('DECODIUM_LAB_DX_LOGS')=='1';(root/('build-baseline/performance/results/waterfall-dx-refresh-profile-'+('logs' if result['logs'] else 'quiet')+'.json')).write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,sort_keys=True))
finally:main.write_text(orig_main);component.write_text(orig_component)
