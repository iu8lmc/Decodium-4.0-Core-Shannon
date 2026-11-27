#!/usr/bin/env python3
"""Compare MAM clicked-station insertion with the original implementation."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/mamenqueue_before.inc').read_text();new='void DecodiumBridge::mamEnqueueClickedStation('+(r/'src/bridge/DecodiumBridgeMamQueue.cpp').read_text().split('void DecodiumBridge::mamEnqueueClickedStation(',1)[1];new=new.replace('decodiumBridgeMamQueueLog(', 'bridgeLog(')
if old!=new:raise SystemExit('MAM enqueue differs')
src=r'''
#include <QString>
#include <QStringList>
#include <QVector>
#include <QSet>
#include <QDateTime>
#include <iostream>
#define emit
void bridgeLog(QString const&){}
QString normalizedBaseCall(QString s){return s.trimmed().toUpper();}QString normalizeCallToken(QString s){return s.trimmed();}
struct Radio{static QString base_callsign(QString s){return s.trimmed().toUpper();}};
struct MamQsoSlot{enum class State{Active};QString call,callFull,grid;int audioFreqHz=0,progress=0,currentTx=0;qint64 startedOnMs=0,lastHeardMs=0;State state;};
class DecodiumBridge{public:
 bool active=true;QString m_callsign="9H1SR";int m_mamMaxStreams=2,m_rxFrequency=1000,changed=0,ranked=0;QVector<MamQsoSlot> m_mamSlots;QSet<QString>m_callerQueuePinned;QStringList queue;
 bool mamMultiStreamSequencerActive()const{return active;}int mamSlotIndexForCall(QString const&s)const{for(int i=0;i<m_mamSlots.size();++i)if(m_mamSlots[i].call==s)return i;return -1;}int mamMinStreamGapHz()const{return 50;}int mamStreamStepHz()const{return 100;}
 void enqueueCallerInternal(QString const&s,int f,int,bool){queue<<s+':'+QString::number(f);}void rankCallerQueue(QString const&){++ranked;}void mamActiveSlotsChanged(){++changed;}
 void mamEnqueueClickedStation(QString const&,int);
};
#include "method.inc"
int main(){DecodiumBridge b;b.active=false;b.mamEnqueueClickedStation("K1ABC",1000);b.active=true;b.mamEnqueueClickedStation("9H1SR",1000);b.mamEnqueueClickedStation("K1ABC",1000);b.mamEnqueueClickedStation("K1ABC",1100);b.mamEnqueueClickedStation("W1AW",1020);b.mamEnqueueClickedStation("K2AAA",5000);for(auto const&s:b.m_mamSlots)std::cout<<s.call.toStdString()<<':'<<s.audioFreqHz<<':'<<s.currentTx<<'\n';std::cout<<b.queue.join('|').toStdString()<<','<<b.m_callerQueuePinned.contains("K2AAA")<<','<<b.changed<<','<<b.ranked<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));outputs=[]
with tempfile.TemporaryDirectory(prefix='decodium-mamenqueue-ab-') as tmp:
 p=Path(tmp);(p/'main.cpp').write_text(src)
 for code in (old,new):
  (p/'method.inc').write_text(code);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);outputs.append(subprocess.check_output([str(p/'test')],text=True))
expected='K1ABC:1000:1\nW1AW:1120:1\nK2AAA:4000,1,2,1\n'
if outputs!=[expected,expected]:raise SystemExit('MAM enqueue A/B mismatch '+repr(outputs))
print('MAM enqueue A/B OK\n'+expected,end='')
