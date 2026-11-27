#!/usr/bin/env python3
"""Compare canonical application and radio mode classification."""
from pathlib import Path
import shlex,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
fixture=(root/'tests/bridge_refactor_fixtures/application_mode_helpers_before.inc').read_text()
module=(root/'src/bridge/DecodiumBridgeApplicationModes.cpp').read_text()
assert module.endswith(fixture)
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
harness=r'''
#include <QCoreApplication>
#include <QString>
#include <QTextStream>
#include "modes.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);bool ftx=canonicalApplicationDecodeMode(" ft8 ")=="FT8"&&canonicalApplicationDecodeMode("q65-60a")=="Q65-60A";bool link=canonicalApplicationDecodeMode("d4 link")=="FT2-Link"&&isFt2LinkApplicationMode(" FT2LINK ");bool keyboard=isStreamingKeyboardMode("rtty")&&isStreamingKeyboardMode("jtty")&&isNativePcmKeyboardMode("jtty")&&!isNativePcmKeyboardMode("rtty");bool radio=isRadioOnlyModeLabel("USB")&&isRadioOnlyModeLabel("RTTY")&&!isRadioOnlyModeLabel("FT8");bool names=canonicalApplicationDecodeMode("echo")=="Echo"&&canonicalApplicationDecodeMode("cw")=="CW"&&canonicalApplicationDecodeMode("unknown").isEmpty();bool ok=ftx&&link&&keyboard&&radio&&names;QTextStream(stdout)<<"ftx="<<ftx<<" link="<<link<<" keyboard="<<keyboard<<" radio="<<radio<<" names="<<names<<Qt::endl;return ok?0:1;}
'''
with tempfile.TemporaryDirectory(prefix='decodium-app-modes-ab-') as tmp:
 d=Path(tmp);(d/'main.cpp').write_text(harness);outs=[]
 for label,body in [('before',fixture),('after',module[-len(fixture):])]:
  (d/'modes.inc').write_text(body);exe=d/label
  subprocess.run(['c++','-std=c++17','-I'+str(d),str(d/'main.cpp'),*flags,'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True,text=True,check=True);print(label,result.stdout.strip(),flush=True);outs.append(result.stdout)
 assert outs[0]==outs[1]
