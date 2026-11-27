#!/usr/bin/env python3
"""Run an FT2 virtual-audio pair and clean only its unique profiles."""
import argparse,secrets,shutil,sys
from pathlib import Path
R=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--safe-slot',action='store_true');p.add_argument('--binary',type=Path,default=R/'build/decodium');p.add_argument('--qml',type=Path,default=R/'build/qml/decodium/Main.qml');a=p.parse_args()
suffix=secrets.token_hex(5);secrets.token_hex=lambda n:suffix
script=R/'tests/tools/bridge_legacydelta_pair_lab.py';source=script.read_text();needle="'--lab-mode','FT8'";assert source.count(needle)==1;source=source.replace(needle,"'--lab-mode','FT2'")
sender_probe = """
 Timer {interval:200;running:true;repeat:true;onTriggered:{if(bridge.callsign===\"9H1SR\" && bridge.transmitting){console.log(\"[FT2-SENDER] active\");stop();}}}
"""
if a.safe_slot:
 sender_probe += """
 Timer {interval:25;running:true;repeat:true;property int ticks:0;onTriggered:{if(++ticks>=480 && bridge.callsign===\"9H1SR\" && Date.now()%3750<150){bridge.resetStandardTxMessages();console.log(\"[FT2-SAFE] request\");bridge.sendTx(6);stop();}}}
 """
 source=source.replace("'--lab-sendtx-ms','10000',",'')
source=source.replace("probe=\'\'\'", "probe=\'\'\'"+sender_probe, 1)
source=source.replace("  if rxcode or txcode or len(lines)!=1", "  print('FT2 evidence sender_active=', '[FT2-SENDER] active' in t, 'send_requested=', ('[LAB] auto SENDTX requested' in t or '[FT2-SAFE] request' in t), 'receiver=', lines, flush=True)\n  if '[FT2-SENDER] active' not in t or rxcode or txcode or len(lines)!=1", 1)
sys.argv=[str(script),'--binary',str(a.binary),'--qml',str(a.qml)]
try:exec(compile(source,str(script),'exec'),{'__name__':'__main__','__file__':str(script)})
finally:
 for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
  for role in ('LegacyRX','LegacyTX'):
   path=parent/('Decodium - '+role+suffix+' - test')
   if path.is_dir():shutil.rmtree(path)
