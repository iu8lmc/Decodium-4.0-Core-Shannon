#!/usr/bin/env python3
"""Execute release and extracted MAM slot builders against the real FTX helper."""
import argparse, os, shlex, subprocess, tempfile
from pathlib import Path
R = Path(__file__).resolve().parents[2]
def section(source):
    marker = 'QString DecodiumBridge::mamBuildSlotMessage(MamQsoSlot& s)'
    start = source.index(marker)
    return source[start:source.index('\n}', start) + 2]
parser = argparse.ArgumentParser()
parser.add_argument('--base', default='v1.0.737')
args = parser.parse_args()
baseline = subprocess.check_output(['git', 'show', f'{args.base}:src/bridge/DecodiumBridge.cpp'], cwd=R, text=True)
current = (R / 'src/bridge/DecodiumBridgeMamState.cpp').read_text()
flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Core'], text=True))
harness = r'''#include <QtCore>
#include <cstdio>
#include "FtxMessageHelpers.h"
namespace decodium::txmsg {
bool isStandardFtxCall(QString const& c) { return c == "K1ABC" || c == "9H1SR"; }
QString bracketHashCall(QString const& c) { return "<" + c + ">"; }
}
struct MamQsoSlot { int currentTx; QString callFull; QString reportSent; };
struct DecodiumBridge { QString m_callsign, m_grid, m_mode; bool m_sendRR73, m_quickQsoEnabled; QString mamBuildSlotMessage(MamQsoSlot&); };
#include "slot.inc"
int main(int argc,char** argv) {
 QCoreApplication app(argc,argv);
 for (auto mode : QStringList{"FT8", "FT4", "FT2", "JT65"})
 for (auto call : QStringList{"K1ABC", "II9MESC"})
 for (int tx=1;tx<=6;++tx) for(bool rr:{false,true}) for(bool quick:{false,true}) {
  DecodiumBridge bridge{"9H1SR","JM75AA",mode,rr,quick}; MamQsoSlot slot{tx,call,"-08"};
  QString actual=bridge.mamBuildSlotMessage(slot);
  QString expected=standardFtxQsoMessage(tx,call,"9H1SR","JM75","-08",rr,quick,mode);
  if(actual!=expected) return 1;
  std::printf("%s\n",actual.toUtf8().constData());
 }
}'''
with tempfile.TemporaryDirectory(prefix='decodium-mam-slot-ab-') as tmp:
    d=Path(tmp); (d/'test.cpp').write_text(harness); output=[]
    for variant in (baseline,current):
        (d/'slot.inc').write_text(section(variant))
        files=[d/'test.cpp',R/'src/bridge/FtxMessageHelpers.cpp',R/'src/bridge/DecodeSignalReport.cpp',R/'Sequencer/MessageTokenRules.cpp',R/'src/radio/Radio.cpp']
        subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I'+str(d),'-I'+str(R),'-I'+str(R/'src/bridge'),'-I'+str(R/'src/radio'),'-I'+str(R/'src/core'),*map(str,files),*flags,'-o',str(d/'test')],check=True)
        output.append(subprocess.check_output([str(d/'test')],text=True))
    if output[0]!=output[1]: raise RuntimeError('slot message output differs')
    print('MAM slot-message A/B: '+str(len(output[0].splitlines()))+' cases match using production FTX helper')
