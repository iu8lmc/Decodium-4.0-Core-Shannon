#!/usr/bin/env python3
"""Compare TX slot and reply timing."""
from pathlib import Path
import shlex,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
old=(root/'tests/bridge_refactor_fixtures/tx_slot_helpers_before.inc').read_text()
new=(root/'src/bridge/DecodiumBridgeTxSlotHelpers.cpp').read_text();assert new.endswith(old)
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
harness=r'''
#include <QString>
#include <QTextStream>
#include "DecodeUtcTime.h"
#include "helpers.inc"
int main(){bool p=bridgeTxPeriodIsEven(1)&&!bridgeTxPeriodIsEven(0);bool ft8=replyTxPeriodForClickedDecode("FT8","12:00:00",1)==0&&replyTxPeriodForClickedDecode("FT8","12:00:15",0)==1;bool ft4=replyTxPeriodForClickedDecode("FT4","12:00:07",0)==1&&replyTxPeriodForClickedDecode("FT4","12:00:15",1)==0;bool ft2=replyTxPeriodForClickedDecode("FT2","12:00:18",0)==1;bool fallback=replyTxPeriodForClickedDecode("UNKNOWN","12:00:00",1)==1&&replyTxPeriodForClickedDecode("FT8","bad",0)==0;QTextStream(stdout)<<"period="<<p<<" ft8="<<ft8<<" ft4="<<ft4<<" ft2="<<ft2<<" fallback="<<fallback<<Qt::endl;return p&&ft8&&ft4&&ft2&&fallback?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-tx-wave-ab-') as tmp:
 d=Path(tmp);(d/'main.cpp').write_text(harness);outputs=[]
 for label,body in [('before',old),('after',new[-len(old):])]:
  (d/'helpers.inc').write_text(body);exe=d/label
  subprocess.run(['c++','-std=c++17','-I'+str(d),'-I'+str(root/'src/bridge'),str(d/'main.cpp'),str(root/'src/bridge/DecodeUtcTime.cpp'),*flags,'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(label,result.stdout.strip(),flush=True);outputs.append(result.stdout)
 assert outputs[0]==outputs[1]
