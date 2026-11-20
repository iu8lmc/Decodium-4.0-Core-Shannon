#!/usr/bin/env python3
"""Compare manual MAM queue promotion and slot lookup against the original."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/mamqueue_before.inc').read_text();new=(r/'src/bridge/DecodiumBridgeMamQueue.cpp').read_text().split('void DecodiumBridge::mamPromoteQueuedCaller(',1)[1];new=('void DecodiumBridge::mamPromoteQueuedCaller('+new).split('\nvoid DecodiumBridge::mamEnqueueClickedStation(',1)[0]
if old!=new:raise SystemExit('MAM queue extraction differs')
src=r'''
#include <QString>
#include <QStringList>
#include <QVector>
#include <iostream>
#define emit
QString normalizedBaseCall(QString s){return s.trimmed().toUpper();}
struct Slot{QString call;};
class DecodiumBridge {public:
 bool active=true,m_callerQueueManualOrder=false;int m_mamMaxStreams=3,promotes=0,changed=0;
 QVector<Slot> m_mamSlots;QStringList m_callerQueue;
 bool mamMultiStreamSequencerActive()const{return active;}void callerQueueChanged(){++changed;}void mamPromoteFromQueue(){++promotes;}
 void mamPromoteQueuedCaller(QString const&);int mamSlotIndexForCall(QString const&)const;
};
#include "method.inc"
int main(){DecodiumBridge b;b.m_mamSlots={{"K1ABC"},{"W1AW"}};b.m_callerQueue={"N0CALL FN42","K2AAA FN31","W3XYZ FN20"};std::cout<<b.mamSlotIndexForCall(" k1abc ")<<','<<b.mamSlotIndexForCall("unknown")<<','<<b.mamSlotIndexForCall("")<<'\n';b.active=false;b.mamPromoteQueuedCaller("K2AAA");b.active=true;b.mamPromoteQueuedCaller("missing");b.mamPromoteQueuedCaller("   ");std::cout<<b.promotes<<','<<b.changed<<'\n';b.mamPromoteQueuedCaller("k2aaa");std::cout<<b.m_callerQueue.join('|').toStdString()<<','<<b.promotes<<','<<b.changed<<','<<b.m_callerQueueManualOrder<<'\n';b.mamPromoteQueuedCaller("k2aaa");std::cout<<b.promotes<<','<<b.changed<<'\n';b.m_mamMaxStreams=2;b.mamPromoteQueuedCaller("W3XYZ");std::cout<<b.promotes<<','<<b.changed<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));outputs=[]
with tempfile.TemporaryDirectory(prefix='decodium-mamqueue-ab-') as tmp:
 p=Path(tmp);(p/'main.cpp').write_text(src)
 for code in (old,new):
  (p/'method.inc').write_text(code);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
expected='0,-1,-1\n0,0\nK2AAA FN31|N0CALL FN42|W3XYZ FN20,1,1,1\n2,1\n2,1\n'
if outputs!=[expected,expected]:raise SystemExit('MAM queue A/B mismatch '+repr(outputs))
print('MAM queue A/B OK\n'+expected,end='')
