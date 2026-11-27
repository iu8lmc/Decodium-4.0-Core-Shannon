#!/usr/bin/env python3
"""Compare TX mode, WSPR power and waveform calculations."""
from pathlib import Path
import shlex,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
old=(root/'tests/bridge_refactor_fixtures/tx_wave_helpers_before.inc').read_text()
new=(root/'src/bridge/DecodiumBridgeTxWaveHelpers.cpp').read_text();assert new.endswith(old)
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
harness=r'''
#include <QString>
#include <QVector>
#include <QTextStream>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include "helpers.inc"
int main(){bool modes=bridgeGeneratedTxWaveMode(" ft8 ")&&bridgeGeneratedTxWaveMode("q65-30")&&bridgeGeneratedTxWaveMode("FST4W-120")&&!bridgeGeneratedTxWaveMode("RTTY")&&!bridgeGeneratedTxWaveMode("");bool power=normalizeWsprPowerDbm(-3)==0&&normalizeWsprPowerDbm(2)==3&&normalizeWsprPowerDbm(65)==60&&wsprPowerDbmLabel(37)=="37 dBm  5 W"&&std::abs(wsprPowerDbmToWatts(30)-1.)<1e-10; QVector<float> v{0.f,-0.5f,1.f,0.25f};bool metrics=txWavePeak(v)==1.0&&txWavePeak({})==0&&txWaveMetricsSummary(v)=="samples=4 duration_ms=0 peak=1.000000 avgAbs=0.437500 nonzero=3/4"&&txWaveMetricsSummary({}).contains("nonzero=0/0");QTextStream(stdout)<<"modes="<<modes<<" power="<<power<<" metrics="<<metrics<<Qt::endl;return modes&&power&&metrics?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-tx-wave-ab-') as tmp:
 d=Path(tmp);(d/'main.cpp').write_text(harness);outputs=[]
 for label,body in [('before',old),('after',new[-len(old):])]:
  (d/'helpers.inc').write_text(body);exe=d/label
  subprocess.run(['c++','-std=c++17','-I'+str(d),str(d/'main.cpp'),*flags,'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(label,result.stdout.strip(),flush=True);outputs.append(result.stdout)
 assert outputs[0]==outputs[1]
