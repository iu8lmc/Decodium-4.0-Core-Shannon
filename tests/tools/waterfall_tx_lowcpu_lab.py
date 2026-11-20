#!/usr/bin/env python3
"""Check TX-guide animations with a simulated visual flag, never real TX."""
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
main=root/'build-baseline/qml/decodium/Main.qml'
waterfall=root/'build-baseline/qml/decodium/components/Waterfall.qml'
original_main,original_waterfall=main.read_text(),waterfall.read_text()
root_anchor='Item {\n    id: waterfallPanel'
tx_anchor='visible: bridge && (bridge.transmitting || bridge.tuning)'
overlay_anchor='visible: waterfallDisplay.spectrumGpuOverlayAvailable && height > 0'
if original_main.count('GlassApplicationWindow {')!=1 or original_waterfall.count(root_anchor)!=1 or original_waterfall.count(tx_anchor)!=2 or original_waterfall.count(overlay_anchor)!=1:
    raise RuntimeError('unexpected QML structure')
main_probe='''
 Timer {interval:14000;running:true;repeat:false;onTriggered:{bridge.lowCpuModeEnabled=false;}}
 Timer {interval:17000;running:true;repeat:false;onTriggered:{bridge.lowCpuModeEnabled=true;}}
 Timer {interval:20000;running:true;repeat:false;onTriggered:{Qt.quit();}}
'''
waterfall_probe='''
    property bool labFakeTx: true
    Timer {interval:500;running:true;repeat:true;onTriggered:{
      console.log('[TX-VISUAL-LAB] '+JSON.stringify({low:bridge.lowCpuModeEnabled,
          carrier:txCarrierActiveBar.visible,opacity:txCarrierActiveBar.opacity,
          pulse:txCarrierPulse.running,wave:txCascadeWave.visible,
          guide:txSignalWaterfallGuide.visible,marker:txSignalWaterfallGuide.markerX,
          width:txSignalWaterfallGuide.width,waveRunning:txCascadeWaveAnimation.running,tx:bridge.transmitting,tune:bridge.tuning}));
    }}
'''
try:
    with tempfile.TemporaryDirectory(prefix='decodium-tx-visual-') as tmp:
        settings=Path(tmp)/'Decodium'/'Decodium3.ini';settings.parent.mkdir(parents=True)
        settings.write_text('[General]\nLowCpuMode=true\nlowCpuModeEnabled=true\n')
        main.write_text(original_main.replace('GlassApplicationWindow {','GlassApplicationWindow {'+main_probe,1))
        waterfall.write_text(original_waterfall.replace(root_anchor,root_anchor+waterfall_probe,1).replace(
            tx_anchor,'visible: bridge && (bridge.transmitting || bridge.tuning || waterfallPanel.labFakeTx)').replace(
            overlay_anchor,'visible: height > 0',1))
        env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',
                                        DECODIUM_FORCE_STANDALONE_UDP='1')
        cmd=[str(root/'build-baseline/decodium'),'--test-mode','--rig-name','TxVisual'+secrets.token_hex(5),
             '--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','35000']
        result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=50)
        states=[json.loads(line.split('[TX-VISUAL-LAB] ',1)[1]) for line in result.stdout.splitlines() if '[TX-VISUAL-LAB] ' in line]
        if result.returncode or '[LAB] CAT auto-connect disabled' not in result.stdout or not states:
            raise RuntimeError(result.stdout[-8000:])
        low=[s for s in states if s['low']];normal=[s for s in states if not s['low']]
        if not low or not normal or any(s['tx'] or s['tune'] for s in states):
            raise RuntimeError('missing profiles or unexpected TX: '+repr(states))
        if not all(s['carrier'] and s['opacity']>0.99 and not s['pulse'] and not s['wave'] and not s['waveRunning'] for s in low):
            raise RuntimeError('bad LOW CPU state: '+repr(low))
        if not all(s['carrier'] and s['pulse'] and s['wave'] and s['waveRunning'] for s in normal):
            raise RuntimeError('bad normal state: '+repr(normal))
        print(json.dumps({'lowSamples':len(low),'normalSamples':len(normal),'tx':False,'tune':False}))
finally:
    main.write_text(original_main)
    waterfall.write_text(original_waterfall)
