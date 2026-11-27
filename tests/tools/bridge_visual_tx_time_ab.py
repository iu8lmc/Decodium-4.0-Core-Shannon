#!/usr/bin/env python3
"""Compare visual TX time selection at deterministic period boundaries."""
from pathlib import Path
import shlex,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
old=(root/'tests/bridge_refactor_fixtures/visual_tx_time_before.inc').read_text()
new=(root/'src/bridge/DecodiumBridgeVisualTxTime.cpp').read_text();assert new.endswith(old)
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
harness=r'''
#include <QString>
#include <QDateTime>
#include <QTextStream>
static qint64 nowMs=0;
static qint64 ntpCorrectedCurrentMSecsSinceEpoch(){return nowMs;}
static QString utcDisplayTokenForSlotStart(qint64 slot,int period){return QString::number(slot)+":"+QString::number(period);}
#include "helpers.inc"
int main(){nowMs=29999;bool ft8=currentTxVisualTimeToken(" ft8 ")=="1:15000";bool ft4=currentTxVisualTimeToken("FT4")=="3:7500";bool ft2=currentTxVisualTimeToken("FT2")=="7:3750";nowMs=30000;bool boundary=currentTxVisualTimeToken("FT8")=="2:15000";bool fallback=currentTxVisualTimeToken("other").size()==6;QTextStream(stdout)<<"ft8="<<ft8<<" ft4="<<ft4<<" ft2="<<ft2<<" boundary="<<boundary<<" fallback="<<fallback<<Qt::endl;return ft8&&ft4&&ft2&&boundary&&fallback?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-visual-time-ab-') as tmp:
 d=Path(tmp);(d/'main.cpp').write_text(harness);out=[]
 for label,body in [('before',old),('after',new[-len(old):])]:
  (d/'helpers.inc').write_text(body);exe=d/label;subprocess.run(['c++','-std=c++17','-I'+str(d),str(d/'main.cpp'),*flags,'-o',str(exe)],check=True);v=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(label,v.stdout.strip(),flush=True);out.append(v.stdout)
 assert out[0]==out[1]
