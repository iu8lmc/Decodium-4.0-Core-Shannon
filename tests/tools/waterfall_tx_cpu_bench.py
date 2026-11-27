#!/usr/bin/env python3
"""Measure simulated TX-guide animations in LOW CPU without transmitting."""
import argparse
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile
import time

root=Path(__file__).resolve().parents[2]
main=root/'build-baseline/qml/decodium/Main.qml'
waterfall=root/'build-baseline/qml/decodium/components/Waterfall.qml'
p=argparse.ArgumentParser();p.add_argument('--animate',action='store_true');p.add_argument('--output',type=Path,required=True);a=p.parse_args()
original_main,original_waterfall=main.read_text(),waterfall.read_text()
root_anchor='Item {\n    id: waterfallPanel'
tx_anchor='visible: bridge && (bridge.transmitting || bridge.tuning)'
overlay_anchor='visible: waterfallDisplay.spectrumGpuOverlayAvailable && height > 0'
pulse_anchor='running: txCarrierActiveBar.visible && !bridge.lowCpuModeEnabled'
wave_anchor='visible: !(bridge && bridge.lowCpuModeEnabled)'
if (original_main.count('GlassApplicationWindow {')!=1 or original_waterfall.count(root_anchor)!=1
    or original_waterfall.count(tx_anchor)!=2 or original_waterfall.count(overlay_anchor)!=1
    or original_waterfall.count(pulse_anchor)!=1 or original_waterfall.count(wave_anchor)!=1):
    raise RuntimeError('unexpected QML structure')
main_probe='''
 Timer {interval:15000;running:true;repeat:false;onTriggered:{console.log('[TX-BENCH-START]');}}
 Timer {interval:26000;running:true;repeat:false;onTriggered:{Qt.quit();}}
'''
waterfall_probe='''
    property bool labFakeTx: true
    Timer {interval:1000;running:true;repeat:true;onTriggered:{
      console.log('[TX-BENCH-STATE] '+JSON.stringify({low:bridge.lowCpuModeEnabled,
        carrier:txCarrierActiveBar.visible,opacity:txCarrierActiveBar.opacity,
        pulse:txCarrierPulse.running,guide:txSignalWaterfallGuide.visible,
        wave:txCascadeWave.visible,waveRunning:txCascadeWaveAnimation.running,
        tx:bridge.transmitting,tune:bridge.tuning}));
    }}
'''
app=None
try:
    with tempfile.TemporaryDirectory(prefix='decodium-tx-bench-') as tmp:
        settings=Path(tmp)/'Decodium'/'Decodium3.ini';settings.parent.mkdir(parents=True)
        settings.write_text('[General]\nLowCpuMode=true\nlowCpuModeEnabled=true\n')
        main.write_text(original_main.replace('GlassApplicationWindow {','GlassApplicationWindow {'+main_probe,1))
        instrumented=original_waterfall.replace(root_anchor,root_anchor+waterfall_probe,1).replace(
            tx_anchor,'visible: bridge && (bridge.transmitting || bridge.tuning || waterfallPanel.labFakeTx)').replace(
            overlay_anchor,'visible: height > 0',1)
        if a.animate:
            instrumented=instrumented.replace(pulse_anchor,'running: txCarrierActiveBar.visible',1).replace(wave_anchor,'visible: true',1)
        waterfall.write_text(instrumented)
        env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',
                                        DECODIUM_FORCE_STANDALONE_UDP='1')
        cmd=[str(root/'build-baseline/decodium'),'--test-mode','--rig-name','TxBench'+secrets.token_hex(5),
             '--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','35000']
        log_path=Path(tmp)/'app.log'
        with log_path.open('w') as log:
            app=subprocess.Popen(cmd,env=env,stdout=log,stderr=subprocess.STDOUT)
            deadline=time.monotonic()+32
            while time.monotonic()<deadline:
                text=log_path.read_text(errors='replace')
                if '[TX-BENCH-START]' in text:break
                if app.poll() is not None:raise RuntimeError('app exited before measurement: '+text[-3000:])
                time.sleep(.1)
            else:raise RuntimeError('measurement did not start')
            def ticks():
                fields=Path(f'/proc/{app.pid}/stat').read_text().rsplit(')',1)[1].split()
                return int(fields[11])+int(fields[12])
            start=ticks();offset=log_path.stat().st_size;start_wall=time.monotonic();time.sleep(9)
            cpu=(ticks()-start)/os.sysconf('SC_CLK_TCK');wall=time.monotonic()-start_wall
            data=log_path.read_text(errors='replace')[offset:]
            states=[json.loads(line.split('[TX-BENCH-STATE] ',1)[1]) for line in data.splitlines() if '[TX-BENCH-STATE] ' in line]
            if not states or not all(s['low'] and s['carrier'] and s['guide'] and not s['tx'] and not s['tune']
                                     and s['pulse']==a.animate and s['wave']==a.animate and s['waveRunning']==a.animate for s in states):
                raise RuntimeError('unexpected visual state: '+repr(states))
            result={'animated':a.animate,'cpuSeconds':cpu,'wallSeconds':wall,'samples':len(states),'tx':False,'tune':False}
            a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,sort_keys=True))
finally:
    if app is not None and app.poll() is None:
        app.terminate()
        try: app.wait(timeout=5)
        except subprocess.TimeoutExpired: app.kill();app.wait()
    main.write_text(original_main);waterfall.write_text(original_waterfall)
