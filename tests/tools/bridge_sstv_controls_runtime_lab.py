#!/usr/bin/env python3
"""Exercise advanced decoder controls in an isolated application session."""
import argparse, os, secrets, subprocess, tempfile, shutil
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
    Timer {interval:12000;running:true;repeat:false;onTriggered:{
      var f=[], modes=bridge.sstvRxModeChoices;if(!bridge.sstvAvailable || modes.length<5)f.push("modes");
      if(!bridge.configureSstvInterferenceNotch(true,__FREQ__))f.push("notch-config");
      if(!bridge.configureSstvFax480Rx(true,__PROFILE__))f.push("fax-config");
      var c=bridge.sstvRxControls;if(!c.interferenceNotchEnabled || c.interferenceNotchHz!==__FREQ__ || !c.fax480Enabled || c.fax480Profile!==__PROFILE__)f.push("snapshot");
      if(!bridge.configureSstvInterferenceNotch(false,1700) || !bridge.configureSstvFax480Rx(false,__PROFILE__))f.push("disable");
      c=bridge.sstvRxControls;if(c.interferenceNotchEnabled || c.fax480Enabled)f.push("disabled-snapshot");
      if(!bridge.configureSstvRadiofaxRx(true,288,904,120))f.push("radiofax-basic");c=bridge.sstvRxControls;if(c.radiofaxIoc!==288 || c.radiofaxWidth!==904)f.push("radiofax-basic-snapshot");
      if(!bridge.configureSstvRadiofaxRxAligned(true,576,1809,140,true))f.push("radiofax-aligned");c=bridge.sstvRxControls;if(c.radiofaxIoc!==576 || !c.radiofaxCenteredPhasing)f.push("radiofax-aligned-snapshot");
      if(bridge.configureSstvRadiofaxRxProfile(true,576,1809,140,true,true))f.push("radiofax-incompatible");if(!bridge.configureSstvRadiofaxRxProfile(true,576,1809,140,false,true))f.push("radiofax-profile");c=bridge.sstvRxControls;if(!c.radiofaxWmoDeadSector)f.push("radiofax-profile-snapshot");
      if(!bridge.configureSstvRadiofaxRxAdvanced(true,576,1809,140,false,true,true))f.push("radiofax-advanced");c=bridge.sstvRxControls;if(!c.radiofaxWmoTimedPhasing)f.push("radiofax-advanced-snapshot");
      if(bridge.configureSstvRadiofaxRx(true,999,1809,140))f.push("radiofax-invalid");if(!bridge.configureSstvRadiofaxRx(false,576,1809,140))f.push("radiofax-disable");
      console.log("[ADV-DECODE] "+(f.length?"FAIL "+f.join(","):"PASS"));
    }}
'''.replace('__FREQ__','1700' if a.variant=='a' else '1900').replace('__PROFILE__','"dayton-480"' if a.variant=='a' else '"qsstv-500"')

rig='BridgeSstvControls'+secrets.token_hex(5)
try:
    with tempfile.TemporaryDirectory(prefix='decodium-adv-decode-') as tmp:
        qml.write_text(original.replace(marker, marker+probe, 1))
        env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1')
        cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','20000']
        result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
        if result.returncode or '[ADV-DECODE] PASS' not in result.stdout or '[LAB] CAT auto-connect disabled' not in result.stdout:
            print(result.stdout[-16000:]);raise RuntimeError(f'advanced decoder test failed: {result.returncode}')
        print(f'{a.variant}: SSTV mode choices, controls and Radiofax profiles passed; CAT disabled')
finally:
    qml.write_text(original)
    for base in ('Application Support','Caches'):
        profile=Path.home()/'.qttest/Library'/base/'IU8LMC'/('Decodium - '+rig+' - test')
        if profile.is_dir(): shutil.rmtree(profile)
