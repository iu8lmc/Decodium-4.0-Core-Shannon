#!/usr/bin/env python3
"""Compare corrected UTC offset and day wrapping."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/correctedtime_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeCorrectedTime.cpp').read_text();mark='qint64 DecodiumBridge::correctedUtcEpochMs() const';new=mark+s.split(mark,1)[1];assert old==new
source=r'''
#include <QtGlobal>
#include <cmath>
#include <limits>
#include <iostream>
qint64 clockMs=0;qint64 preciseCurrentMSecsSinceEpoch(){return clockMs;}
class DecodiumBridge{public:bool m_ntpEnabled=false,m_ntpSynced=false;double m_ntpOffsetMs=0;qint64 correctedUtcEpochMs()const;qint64 correctedUtcMsecsSinceStartOfDay()const;};
#include "methods.inc"
void show(DecodiumBridge&b){std::cout<<b.correctedUtcEpochMs()<<':'<<b.correctedUtcMsecsSinceStartOfDay()<<'\n';}
int main(){DecodiumBridge b;clockMs=86399990;b.m_ntpOffsetMs=15.6;show(b);b.m_ntpEnabled=true;show(b);b.m_ntpSynced=true;show(b);clockMs=5;b.m_ntpOffsetMs=-10;show(b);b.m_ntpOffsetMs=std::numeric_limits<double>::quiet_NaN();show(b);b.m_ntpOffsetMs=std::numeric_limits<double>::infinity();show(b);}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-correctedtime-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1]=='86399990:86399990\n86399990:86399990\n86400006:6\n-5:86399995\n5:5\n5:5\n',outputs
print('Corrected UTC A/B: NTP gating, rounding, invalid offsets and midnight wrapping passed')
