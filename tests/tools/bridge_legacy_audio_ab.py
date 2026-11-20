#!/usr/bin/env python3
from pathlib import Path
import os,shlex,subprocess,tempfile
R=Path(__file__).resolve().parents[2]
a=(R/'tests/bridge_refactor_fixtures/legacy_audio_before.inc').read_text();sig='void DecodiumBridge::onLegacyAudioSamples('
s=(R/'src/bridge/DecodiumBridgeLegacyAudio.cpp').read_text();b=sig+s.split(sig,1)[1]
for x,y in [('decodiumBridgeDecodeFlowLog(', 'bridgeLog('),('decodiumBridgeIsFt2LinkApplicationMode(', 'isFt2LinkApplicationMode(')]:b=b.replace(x,y)
guard='    if (feedSpectrum)\n        decodium::normalizeWaterfallRingPosition(m_wfRingPos, WF_RING_SIZE);\n'
if b.count(guard)!=1:raise SystemExit('missing ring normalization')
b=b.replace(guard,'')
if a!=b:raise SystemExit('legacy audio callback differs')
a=(R/'tests/bridge_refactor_fixtures/ft2_pcm_stats_before.inc').read_text();s=(R/'src/bridge/Ft2LinkPcmStats.h').read_text();b='struct Ft2LinkPcmStats\n'+s.split('struct Ft2LinkPcmStats\n',1)[1];b=b.replace('inline Ft2LinkPcmStats ft2LinkPcmStats(', 'Ft2LinkPcmStats ft2LinkPcmStats(')
if a!=b:raise SystemExit('PCM stats differs')
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
h=r'''
#include <QVector>
#include <algorithm>
#include <cmath>
#include <iostream>
#include "method.inc"
int main(){for(auto v:{QVector<short>{},QVector<short>{0,32,-33,32767,-32768},QVector<short>{100,-100}}){auto s=ft2LinkPcmStats(v);std::cout<<s.rms<<'|'<<s.peak<<'|'<<s.nonZero<<'|'<<s.aboveNoise<<'\n';} }
'''
with tempfile.TemporaryDirectory(prefix='decodium-pcm-ab-') as t:
 d=Path(t);(d/'test.cpp').write_text(h);outputs=[]
 for code in (a,b):
  (d/'method.inc').write_text(code);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test')],check=True);outputs.append(subprocess.check_output([str(d/'test')],text=True))
 if outputs[0]!=outputs[1] or '0|0|0|0' not in outputs[0]:raise SystemExit('PCM statistics mismatch')
 print('PCM statistics A/B match for silence, threshold, full-scale and balanced samples')
