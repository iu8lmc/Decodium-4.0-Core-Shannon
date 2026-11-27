#!/usr/bin/env python3
"""Exercise advanced decoder controls in an isolated application session."""
import argparse, os, secrets, subprocess, tempfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--variant', choices=('a', 'b'), required=True)
p.add_argument('--binary', type=Path, default=ROOT/'build/decodium')
p.add_argument('--qml', type=Path, default=ROOT/'build/qml/decodium/Main.qml')
a = p.parse_args()
qml = a.qml.resolve()
original = qml.read_text()
marker = 'GlassApplicationWindow {'
if original.count(marker) != 1:
    raise RuntimeError('unexpected QML root')
probe = '''
    Timer { interval:12000; running:true; repeat:false; onTriggered: {
      var f=[];
      bridge.advAutoModeEnabled=false;
      bridge.neuralSyncEnabled=__MANUAL__; bridge.turboFeedbackEnabled=__MANUAL__; bridge.coherentAvgEnabled=__MANUAL__;
      if(bridge.effectiveNeuralSync()!==__MANUAL__ || bridge.effectiveTurboFeedback()!==__MANUAL__ || bridge.effectiveCoherentAvg(-22)!==__MANUAL__) f.push("manual");
      bridge.advAutoModeEnabled=true;
      if(bridge.effectiveNeuralSync() || bridge.effectiveTurboFeedback()) f.push("bootstrap");
      bridge.recordFt8DecodeCount(-1);bridge.recordFt8DecodeCount(1);
      if(!bridge.effectiveNeuralSync() || !bridge.effectiveTurboFeedback())f.push("sparse");
      bridge.advAutoModeEnabled=true;if(!bridge.effectiveNeuralSync())f.push("same-value-reset");
      bridge.advAutoModeEnabled=false;bridge.advAutoModeEnabled=true;bridge.recordFt8DecodeCount(1);bridge.recordFt8DecodeCount(3);if(bridge.effectiveNeuralSync() || bridge.effectiveTurboFeedback())f.push("average-boundary");
      bridge.advAutoModeEnabled=false;bridge.advAutoModeEnabled=true;bridge.recordFt8DecodeCount(0);bridge.recordFt8DecodeCount(1);
      if(!bridge.effectiveCoherentAvg(-22.1) || bridge.effectiveCoherentAvg(-22))f.push("threshold");
      bridge.recordFt8DecodeCount(5);bridge.recordFt8DecodeCount(5);bridge.recordFt8DecodeCount(5);bridge.recordFt8DecodeCount(5);
      if(bridge.effectiveNeuralSync() || bridge.effectiveTurboFeedback())f.push("rolling");
      bridge.advAutoModeEnabled=false;bridge.advAutoModeEnabled=true;
      if(bridge.effectiveNeuralSync())f.push("reset");
      console.log("[ADV-DECODE] "+(f.length ? "FAIL "+f.join(",") : "PASS"));
    } }
'''.replace('__MANUAL__', 'true' if a.variant == 'a' else 'false')
try:
    with tempfile.TemporaryDirectory(prefix='decodium-adv-decode-') as tmp:
        qml.write_text(original.replace(marker, marker+probe, 1))
        env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1')
        cmd=[str(a.binary.resolve()),'--test-mode','--rig-name','BridgeAdvanced'+secrets.token_hex(5),'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','20000']
        result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
        if result.returncode or '[ADV-DECODE] PASS' not in result.stdout or '[LAB] CAT auto-connect disabled' not in result.stdout:
            print(result.stdout[-16000:]);raise RuntimeError(f'advanced decoder test failed: {result.returncode}')
        print(f'{a.variant}: manual, bootstrap, unchanged setting, average boundary, rolling history, reset and SNR threshold passed; CAT disabled')
finally:
    qml.write_text(original)
