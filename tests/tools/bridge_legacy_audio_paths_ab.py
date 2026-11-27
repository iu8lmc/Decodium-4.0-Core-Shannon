#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
a=(R/'tests/bridge_refactor_fixtures/legacy_audio_before.inc').read_text();sig='void DecodiumBridge::onLegacyAudioSamples('
s=(R/'src/bridge/DecodiumBridgeLegacyAudio.cpp').read_text();b=sig+s.split(sig,1)[1]
for x,y in [('decodiumBridgeDecodeFlowLog(', 'bridgeLog('),('decodiumBridgeIsFt2LinkApplicationMode(', 'isFt2LinkApplicationMode(')]:b=b.replace(x,y)
guard='    if (feedSpectrum)\n        decodium::normalizeWaterfallRingPosition(m_wfRingPos, WF_RING_SIZE);\n'
if b.count(guard)!=1 or a!=b.replace(guard,''):raise SystemExit('legacy audio callback differs')
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));outputs=[]
with tempfile.TemporaryDirectory(prefix='decodium-pcm-paths-ab-') as t:
 d=Path(t)
 for code in (a,b):
  (d/'method.inc').write_text(code);exe=d/'test';subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-include',str(R/'src/bridge/WaterfallRingPosition.hpp'),'-I'+str(d),'-I'+str(R/'src/bridge'),str(R/'tests/bridge_refactor_legacy_audio_harness.cpp'),*flags,'-o',str(exe)],check=True);outputs.append(subprocess.check_output([str(exe)],text=True))
if outputs[0]!=outputs[1]:raise SystemExit('legacy audio paths differ')
print('PCM paths A/B: '+outputs[0],end='')
