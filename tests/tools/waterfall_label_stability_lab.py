#!/usr/bin/env python3
"""Exercise waterfall label change notifications in the real application."""
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2];qml=root/'build-baseline/qml/decodium/Main.qml';original=qml.read_text()
probe='''
 Connections { target: waterfallEmbeddedLoader.item; ignoreUnknownSignals:true;
   function onSpectrumDecodeLabelsChanged() { labelLab.changes++ } }
 Timer { id:labelLab; interval:12000; repeat:false; running:true; property int changes:0; onTriggered: {
   var w=waterfallEmbeddedLoader.item;
   if(!w || !w.visible) {console.log('[LABEL-LAB] '+JSON.stringify({ready:false}));Qt.quit();return}
   w.updateSpectrumDecodeLabels([]); changes=0;
   function make() { return [{call:'K1ABC',freq:1500,snr:-17,isCQ:true,isMyCall:false,color:'#cc0033'},
                              {call:'W2XYZ',freq:1600,snr:-10,isCQ:false,isMyCall:true,color:'#00cc33'}] }
   w.updateSpectrumDecodeLabels(make()); var first=changes;
   for(var i=0;i<20;++i) w.updateSpectrumDecodeLabels(make()); var unchanged=changes;
   var fields=['call','freq','snr','isCQ','isMyCall','color'];var values=['K2ABC',1501,-18,false,true,'#ffee00'];
   var each=[];
   for(var j=0;j<fields.length;++j) {
     var next=make();next[0][fields[j]]=values[j];var before=changes;w.updateSpectrumDecodeLabels(next);
     each.push(changes===before+1 && w.spectrumDecodeLabels[0][fields[j]]===values[j]);w.updateSpectrumDecodeLabels(make());
   }
   var reversed=make().reverse();var previous=changes;w.updateSpectrumDecodeLabels(reversed);
   var order=changes===previous+1 && w.spectrumDecodeLabels[0].call==='W2XYZ';
   previous=changes;w.clearDecodeLabels();var cleared=changes===previous+1 && w.spectrumDecodeLabels.length===0;
   previous=changes;w.clearDecodeLabels();var emptyStable=changes===previous;
   console.log('[LABEL-LAB] '+JSON.stringify({ready:true,first:first,unchanged:unchanged,each:each,order:order,cleared:cleared,emptyStable:emptyStable,tx:bridge.transmitting}));Qt.quit();
 }}
'''
if original.count('GlassApplicationWindow {')!=1:raise RuntimeError('unexpected main QML')
try:
 qml.write_text(original.replace('GlassApplicationWindow {','GlassApplicationWindow {'+probe,1))
 with tempfile.TemporaryDirectory(prefix='decodium-label-stability-') as tmp:
  env=dict(os.environ,QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
  cmd=[str(root/'build-baseline/decodium'),'--test-mode','--rig-name','Label'+secrets.token_hex(5),'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','40000']
  run=subprocess.run(cmd,cwd=root,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=50)
  lines=[json.loads(x.split('[LABEL-LAB] ',1)[1]) for x in run.stdout.splitlines() if '[LABEL-LAB] ' in x]
  if run.returncode or len(lines)!=1:raise RuntimeError(run.stdout[-6000:])
  a=lines[0]
  if not a['ready'] or a['first']!=1 or a['unchanged']!=1 or not all(a['each']) or not all(a[k] for k in ('order','cleared','emptyStable')) or a['tx']:
   raise RuntimeError(repr(a)+'\n'+run.stdout[-3000:])
  print(json.dumps(a,sort_keys=True))
finally:qml.write_text(original)
