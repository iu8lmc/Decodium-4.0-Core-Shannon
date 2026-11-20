#!/usr/bin/env python3
"""Verify stable labels still follow zoom and visual settings in the app."""
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2];main=root/'build-baseline/qml/decodium/Main.qml';component=root/'build-baseline/qml/decodium/components/Waterfall.qml'
original_main,original_component=main.read_text(),component.read_text()
probe='''
 Connections { target:waterfallEmbeddedLoader.item;ignoreUnknownSignals:true;function onSpectrumDecodeLabelsChanged(){ labelView.changes++ } }
 Timer { id:labelView;interval:12000;running:true;repeat:false;property int changes:0;property var w;property var result:({});
  onTriggered:{w=waterfallEmbeddedLoader.item;if(!w||!w.visible){console.log('[LABEL-VIEW] '+JSON.stringify({ready:false}));Qt.quit();return}
    w.updateSpectrumDecodeLabels([{call:'K1ABC',freq:1400,snr:-17,isCQ:true,isMyCall:false,color:'#cc0033'}]);stage1.start()}}
 Timer { id:stage1;interval:600;repeat:false;onTriggered:{var m=labelView.w.labOverlay.cachedDecodeLabelModel;
   if(m.length!==1){console.log('[LABEL-VIEW] '+JSON.stringify({ready:false,reason:'initial-model',count:m.length}));Qt.quit();return}
   labelView.result.initialX=m[0].x;labelView.result.initialWidth=m[0].widthHint;labelView.result.initialHeight=m[0].rowHeight;
   labelView.changes=0;labelView.w.labDisplay.zoomFactor=2;stage2.start()}}
 Timer { id:stage2;interval:600;repeat:false;onTriggered:{var m=labelView.w.labOverlay.cachedDecodeLabelModel;
   labelView.result.zoom=m.length===1 && m[0].x!==labelView.result.initialX;
   labelView.w.labDisplay.labelFontSize=20;stage3.start()}}
 Timer { id:stage3;interval:600;repeat:false;onTriggered:{var m=labelView.w.labOverlay.cachedDecodeLabelModel;
   labelView.result.font=m.length===1 && m[0].widthHint>labelView.result.initialWidth && m[0].rowHeight>labelView.result.initialHeight;
   labelView.w.labDisplay.labelSpacing=9;labelView.result.spacingTimer=labelView.w.labLabelTimer.running;
   labelView.w.labDisplay.labelUseCustomColor=true;labelView.w.labDisplay.labelColor='#1122ee';stage4.start()}}
 Timer { id:stage4;interval:600;repeat:false;onTriggered:{var m=labelView.w.labOverlay.cachedDecodeLabelModel;
   labelView.result.color=m.length===1 && String(m[0].color).toLowerCase()==='#1122ee';
   labelView.result.beforePanX=m.length===1 ? m[0].x : -1;
   labelView.w.labDisplay.labelUseCustomColor=false;labelView.w.labDisplay.panHz=100;stage5.start()}}
 Timer { id:stage5;interval:600;repeat:false;onTriggered:{var m=labelView.w.labOverlay.cachedDecodeLabelModel;
   labelView.result.restoredColor=m.length===1 && String(m[0].color).toLowerCase()==='#cc0033';
   labelView.result.pan=m.length===1 && m[0].x!==labelView.result.beforePanX;
   labelView.result.labelsUnchanged=labelView.changes===0;labelView.result.ready=true;labelView.result.tx=bridge.transmitting;
   console.log('[LABEL-VIEW] '+JSON.stringify(labelView.result));Qt.quit()}}
'''
if original_main.count('GlassApplicationWindow {')!=1 or original_component.count('    id: waterfallPanel\n')!=1:raise RuntimeError('unexpected source layout')
try:
 main.write_text(original_main.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1))
 component.write_text(original_component.replace('    id: waterfallPanel\n','    id: waterfallPanel\n    property alias labDisplay: waterfallDisplay\n    property alias labOverlay: spectrumGpuOverlay\n    property alias labLabelTimer: decodeLabelRefreshTimer\n',1))
 with tempfile.TemporaryDirectory(prefix='decodium-label-view-') as tmp:
  env=dict(os.environ,QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
  cmd=[str(root/'build-baseline/decodium'),'--test-mode','--rig-name','LabelView'+secrets.token_hex(5),'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','40000']
  run=subprocess.run(cmd,cwd=root,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=50)
  lines=[json.loads(x.split('[LABEL-VIEW] ',1)[1]) for x in run.stdout.splitlines() if '[LABEL-VIEW] ' in x]
  if run.returncode or len(lines)!=1:raise RuntimeError(run.stdout[-6000:])
  v=lines[0]
  if not all(v.get(x) for x in ('ready','zoom','pan','font','color','restoredColor','spacingTimer','labelsUnchanged')) or v.get('tx'):raise RuntimeError(repr(v)+'\n'+run.stdout[-3000:])
  print(json.dumps(v,sort_keys=True))
finally:main.write_text(original_main);component.write_text(original_component)
