#!/usr/bin/env python3
"""Run identical virtual FT8 loopback with the TX trigger aligned in QML."""
import argparse, json, os, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args()
qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
probe='''\n    Timer { interval:9000; running:true; repeat:false; onTriggered: {\n      if(bridge.callsign!=="9H1SR")return;\n      var delay=15000-(Date.now()%15000)+150;matchedTx.interval=delay;matchedTx.start();\n      console.log("[MATCHED-FT8] awaiting slot "+delay);\n    } }\n    Timer { id: matchedTx; repeat:false; onTriggered: {\n      bridge.resetStandardTxMessages();bridge.logSstvQso({});console.log("[MATCHED-FT8] tx requested");\n    } }\n'''
try:
 qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_BINARY=str(a.binary.resolve()),DECODIUM_LAB_QPA_PLATFORM='cocoa',DECODIUM_LAB_SENDTX_MS='300000')
 result=subprocess.run([sys.executable,str(ROOT/'tests/tools/bridge_ft8_two_app_lab.py'),'immediate'],env=env,cwd=ROOT,timeout=130,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 print(result.stdout,end='')
 if result.returncode:raise RuntimeError(f'loopback failed: {result.returncode}')
 report=json.loads(result.stdout.strip().splitlines()[-1])
 if report['first_active_mod15'] is None or report['first_active_mod15'] > 1.5: raise RuntimeError('FT8 audio was not aligned to the slot')
finally:qml.write_text(original)
