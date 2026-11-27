#!/usr/bin/env python3
"""Compare UI preference persistence with synthetic, isolated profiles."""
import argparse, os, secrets, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args()
qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
try:
 with tempfile.TemporaryDirectory(prefix='decodium-uiprefs-restart-') as root:
  for variant in ('a','b'):
   profile=Path(root)/variant;profile.mkdir();rig='BridgeUiRestart'+variant+secrets.token_hex(4)
   for phase in ('write','restart'):
    value='true' if variant=='a' else 'false';style='Fusion' if variant=='a' else 'Material';fps=15 if variant=='a' else 30
    action=('bridge.uiQuality="High";bridge.uiStyle="'+style+'";bridge.autoDetachFullSpectrum='+value+';bridge.uiFramelessPopouts='+value+';bridge.spectrumFpsCap='+str(fps)+';') if phase=='write' else ''
    probe='''\n    Timer { interval:12000; running:true; repeat:false; onTriggered: {var f=[]; __ACTION__
      bridge.themeManager.currentTheme="Ocean Blue"; if(bridge.themeManager.isFlatTheme)f.push("flat-theme");
      if(bridge.uiQuality!=="High")f.push("quality="+bridge.uiQuality);
      if(bridge.uiStyle!=="__STYLE__")f.push("style="+bridge.uiStyle);
      if(bridge.autoDetachFullSpectrum!==__BOOL__)f.push("detach="+bridge.autoDetachFullSpectrum);
      if(bridge.uiFramelessPopouts!==__BOOL__)f.push("frameless="+bridge.uiFramelessPopouts);
      if(bridge.spectrumFpsCap!==__FPS__)f.push("fps="+bridge.spectrumFpsCap);
      console.log("[UI-RESTART] "+(f.length?"FAIL "+f.join(","):"PASS")); } }\n'''.replace('__ACTION__',action).replace('__STYLE__',style).replace('__BOOL__',value).replace('__FPS__',str(fps))
    qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=str(profile),DECODIUM_DISABLE_QML_CACHE='1')
    cmd=[str(a.binary.resolve()),'--test-mode','--rig-name',rig,'--lab-no-cat','--lab-no-monitor','--lab-quit-ms','20000'];result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
    if result.returncode or '[UI-RESTART] PASS' not in result.stdout or '[LAB] CAT auto-connect disabled' not in result.stdout:
     print(result.stdout[-16000:]);raise RuntimeError(f'{variant} {phase} failed: {result.returncode}')
    print(f'{variant} {phase}: isolated UI properties and quality passed with CAT disabled',flush=True)
finally:qml.write_text(original)
