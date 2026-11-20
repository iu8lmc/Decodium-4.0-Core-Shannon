#!/usr/bin/env python3
"""Compare FT2 audio gate and adaptive thresholds against the original."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/ft2audiogates_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeFt2AudioGates.cpp').read_text();mark='void DecodiumBridge::onAudioLevelForFt2Gate()';new=mark+s.split(mark,1)[1];assert old==new
source=r'''
#include <QString>
#include <iostream>
class DecodiumBridge{public:QString m_mode="FT2";bool m_asyncTxEnabled=true,m_ft2AdaptiveTxGates=false;double m_audioLevel=0;int m_ft2AsyncAudioQuietRuns=0;qint64 m_ft2AsyncLastDecodeMs=0;void onAudioLevelForFt2Gate();void ft2AsyncGateRequirements(qint64,int&,int&,int&,int&)const;};
#include "methods.inc"
void print(DecodiumBridge&b){int a,b1,c,d;b.ft2AsyncGateRequirements(10000,a,b1,c,d);std::cout<<a<<":"<<b1<<":"<<c<<":"<<d<<"\n";}
int main(){DecodiumBridge b;for(int i=0;i<105;i++)b.onAudioLevelForFt2Gate();std::cout<<"quiet:"<<b.m_ft2AsyncAudioQuietRuns<<"\n";b.m_audioLevel=.005;b.onAudioLevelForFt2Gate();std::cout<<"threshold:"<<b.m_ft2AsyncAudioQuietRuns<<"\n";b.m_audioLevel=.004;b.m_mode="FT8";b.onAudioLevelForFt2Gate();std::cout<<"other:"<<b.m_ft2AsyncAudioQuietRuns<<"\n";b.m_mode="FT2";b.m_asyncTxEnabled=false;b.onAudioLevelForFt2Gate();std::cout<<"asyncOff:"<<b.m_ft2AsyncAudioQuietRuns<<"\n";print(b);b.m_asyncTxEnabled=true;b.m_ft2AdaptiveTxGates=true;b.m_ft2AsyncAudioQuietRuns=6;print(b);b.m_ft2AsyncLastDecodeMs=9700;print(b);b.m_ft2AsyncAudioQuietRuns=3;b.m_ft2AsyncLastDecodeMs=9000;print(b);b.m_ft2AsyncAudioQuietRuns=1;print(b);b.m_mode="FT8";print(b);}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-ft2audiogates-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(source);outputs=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
assert outputs[0]==outputs[1]=='quiet:100\nthreshold:0\nother:0\nasyncOff:0\n3:400:50:200\n2:250:20:80\n5:600:80:300\n3:400:50:200\n5:600:80:300\n3:400:50:200\n',outputs
print('FT2 audio gates A/B: threshold, saturation, mode, async and adaptive channel profiles passed')
