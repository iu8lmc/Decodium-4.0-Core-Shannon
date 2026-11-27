#!/usr/bin/env python3
from pathlib import Path
import os,shlex,subprocess,tempfile
R=Path(__file__).resolve().parents[2]
a=(R/'tests/bridge_refactor_fixtures/legacy_jt_callbacks_before.inc').read_text();sig='void DecodiumBridge::onFt4DecodeReady('
s=(R/'src/bridge/DecodiumBridgeLegacyJtCallbacks.cpp').read_text();b=sig+s.split(sig,1)[1]
for new,old in [('decodiumBridgeDecodeFlowLog(', 'bridgeLog('),('decodiumBridgeDedupKey(', 'decodeDedupKey(')]:b=b.replace(new,old)
if a!=b:raise SystemExit('Legacy callbacks differ from pre-extraction source')
start='void DecodiumBridge::onFst4DecodeReady('; first=a[:a.index('void DecodiumBridge::onLegacyJtDecodeReady(')];last=a[a.index(start):]
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
h=r'''
#include <QStringList>
#include <iostream>
void bridgeLog(QString const&){}
class DecodiumBridge {public: bool ignore=false;QString m_mode="FT8"; int calls=0;quint64 lastSerial=0;QStringList lastRows;bool shouldIgnoreDecodeCallbacks(){return ignore;}void onFt8DecodeReady(quint64 s,QStringList r){calls++;lastSerial=s;lastRows=r;}void onFt4DecodeReady(quint64,QStringList);void onQ65DecodeReady(quint64,QStringList);void onMsk144DecodeReady(quint64,QStringList);void onFst4DecodeReady(quint64,QStringList);};
#include "method.inc"
int main(){ DecodiumBridge b;QStringList rows{"test"}; b.onFt4DecodeReady(1,rows);b.onQ65DecodeReady(2,rows);b.onMsk144DecodeReady(3,rows);b.onFst4DecodeReady(4,rows);if(b.calls!=4||b.lastSerial!=4||b.lastRows!=rows)abort();b.ignore=true;b.onFst4DecodeReady(5,rows);if(b.calls!=4)abort();std::cout<<"FT4/Q65/MSK144/FST4 dispatch and shutdown guard passed\n";}
'''
with tempfile.TemporaryDirectory(prefix='decodium-jt-ab-') as t:
 d=Path(t);(d/'test.cpp').write_text(h);outs=[]
 for source in (a,b):
  code=source[:source.index('void DecodiumBridge::onLegacyJtDecodeReady(')]+source[source.index(start):];(d/'method.inc').write_text(code);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(d/'test')],check=True);outs.append(subprocess.check_output([str(d/'test')],text=True))
 if outs[0]!=outs[1]:raise SystemExit('dispatch A/B differs')
 print(outs[0],end='')
