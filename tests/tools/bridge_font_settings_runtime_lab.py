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
    Timer {interval:12000;running:true;repeat:false;onTriggered:{
      var f=[],families=bridge.availableFontFamilies(false), fixed=bridge.availableFontFamilies(true);
      if(families.length===0 || fixed.length===0)f.push("families");
      var family=families.indexOf("Arial")>=0 ? "Arial" : families[0], key="BridgeSyntheticFont";
      bridge.setFontSetting(key,family,__SIZE__,__BOLD__,__ITALIC__,family,12);
      if(bridge.fontSettingFamily(key,family,12)!==family)f.push("family");
      if(bridge.fontSettingPointSize(key,family,12)!==__SIZE__)f.push("point");
      if(bridge.fontSettingBold(key,family,12)!==__BOLD__ || bridge.fontSettingItalic(key,family,12)!==__ITALIC__)f.push("traits");
      if(bridge.fontSettingPixelSize(key,family,12)<8 || bridge.fontSettingPixelSize(key,family,12)>36)f.push("pixels");
      if(bridge.fontSettingLabel(key,family,12).indexOf(family)<0)f.push("label");
      bridge.chooseFontSetting(key,family,12);bridge.resetFontSetting(key,family,12);
      if(bridge.fontSettingPointSize(key,family,12)!==12)f.push("reset");
      console.log("[ADV-DECODE] "+(f.length?"FAIL "+f.join(","):"PASS"));
    }}
'''.replace('__SIZE__','18' if a.variant=='a' else '9').replace('__BOLD__','true' if a.variant=='a' else 'false').replace('__ITALIC__','false' if a.variant=='a' else 'true')

try:
    with tempfile.TemporaryDirectory(prefix='decodium-adv-decode-') as tmp:
        qml.write_text(original.replace(marker, marker+probe, 1))
        env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1')
        cmd=[str(a.binary.resolve()),'--test-mode','--rig-name','BridgeFontPrefs'+secrets.token_hex(5),'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','20000']
        result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
        if result.returncode or '[ADV-DECODE] PASS' not in result.stdout or '[LAB] CAT auto-connect disabled' not in result.stdout:
            print(result.stdout[-16000:]);raise RuntimeError(f'advanced decoder test failed: {result.returncode}')
        print(f'{a.variant}: font family, sizes, traits, chooser and reset passed; CAT disabled')
finally:
    qml.write_text(original)
