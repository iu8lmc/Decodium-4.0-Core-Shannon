#!/usr/bin/env python3
"""Compare audio level conversions, limits, and round trips."""
from pathlib import Path
import shlex,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
fixture=(root/'tests/bridge_refactor_fixtures/audio_level_math_before.inc').read_text()
module=(root/'src/bridge/DecodiumBridgeAudioLevelMath.cpp').read_text();assert module.endswith(fixture)
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
harness=r'''
#include <QtGlobal>
#include <QTextStream>
#include <cmath>
#include "math.inc"
int main(){bool bounds=rxInputGainFromLevel(-10)==0&&rxInputGainFromLevel(50)==1&&rxInputGainFromLevel(100)==4&&rxInputGainFromLevel(200)==4;bool inverse=true;for(double v:{0.,10.,50.,75.,100.})inverse &= std::abs(rxInputLevelFromGain(rxInputGainFromLevel(v))-v)<0.0001;bool tx=std::abs(txGainFromSlider(0)-0.9)<1e-8&&txGainFromSlider(450)<0.006&&txAttenuationFromSlider(600)==45;bool legacy=legacyTxAttenuationFromLevel(-1)==0&&legacyTxAttenuationFromLevel(500)==450&&txLevelFromLegacyAttenuation(500)==450;bool ok=bounds&&inverse&&tx&&legacy;QTextStream(stdout)<<"bounds="<<bounds<<" inverse="<<inverse<<" tx="<<tx<<" legacy="<<legacy<<Qt::endl;return ok?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-level-math-ab-') as tmp:
 d=Path(tmp);(d/'main.cpp').write_text(harness);outs=[]
 for label,body in [('before',fixture),('after',module[-len(fixture):])]:
  (d/'math.inc').write_text(body);exe=d/label
  subprocess.run(['c++','-std=c++17','-I'+str(d),str(d/'main.cpp'),*flags,'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(label,result.stdout.strip(),flush=True);outs.append(result.stdout)
 assert outs[0]==outs[1]
