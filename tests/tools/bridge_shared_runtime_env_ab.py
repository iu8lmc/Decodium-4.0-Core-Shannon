#!/usr/bin/env python3
"""Check shared runtime values for independent PSK environment settings."""
from pathlib import Path
import os,shlex,subprocess,tempfile
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/shared_runtime_before.inc').read_text();new=(r/'src/bridge/DecodiumBridgeSharedRuntime.cpp').read_text();assert old in new
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
head='#include "FtDecodeThreadBudget.hpp"\n#include <QByteArray>\n#include <QTimeZone>\n#include <QTextStream>\n'
tail='\nint main(){QTextStream(stdout)<<decodiumBridgePskDebugEnabled()<<" "<<decodiumBridgeMaxFtDecodeThreads()<<" "<<decodiumBridgeUtcTimeZone().id()<<Qt::endl;}\n'
with tempfile.TemporaryDirectory(prefix='decodium-shared-runtime-') as d:
 d=Path(d);exes=[]
 for label,body in [('before',old),('after',new[new.index(old):new.index(old)+len(old)])]:
  source=d/(label+'.cpp');source.write_text(head+body+tail);exe=d/label;subprocess.run(['c++','-std=c++17','-I'+str(r/'src/core'),str(source),*flags,'-o',str(exe)],check=True);exes.append(exe)
 for value,expected in [('',False),('0',False),('false',False),('other',False),('1',True),('true',True),(' YES ',True),('ON',True)]:
  env=os.environ.copy();env['DECODIUM_PSK_DEBUG']=value;outputs=[subprocess.check_output([str(exe)],env=env,text=True).strip() for exe in exes];assert outputs[0]==outputs[1] and outputs[0].startswith(('1' if expected else '0')+' '),outputs;print(repr(value),outputs[0])
