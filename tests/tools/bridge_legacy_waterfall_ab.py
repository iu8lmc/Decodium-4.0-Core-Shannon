#!/usr/bin/env python3
import os, shlex, subprocess, tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
a=(R/'tests/bridge_refactor_fixtures/legacy_waterfall_before.inc').read_text()
s=(R/'src/bridge/DecodiumBridgeLegacyWaterfall.cpp').read_text();sig='void DecodiumBridge::onLegacyWaterfallRow('
b=sig+s.split(sig,1)[1];b=b.replace('decodiumBridgeDecodeFlowLog(', 'bridgeLog(')
if a!=b: raise SystemExit('Waterfall source differs from pre-extraction snapshot')
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
h=r'''
#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QVector>
#include <iostream>
#define emit
void bridgeLog(const QString&){}
struct Remote { bool running=true, enabled=true; int calls=0; bool isRunning(){return running;} bool waterfallEnabled(){return enabled;} void publishWaterfallRow(QByteArray const&,int,int,int,int,QString const&){calls++;} };
class DecodiumBridge { public:
 qint64 m_lastLegacyWaterfallDiagLogMs=0,m_lastPanadapterFrameMs=0; QByteArray m_lastPanadapterData; Remote *m_remoteServer=nullptr; bool m_legacyPcmSpectrumFeed=false,legacy=true,modern=false; int count=0; QVector<float> last; float lo=0,hi=0,start=0,end=0;
 bool usingLegacyBackendForRx(){return legacy;} bool useModernSpectrumFeedWithLegacy(){return modern;}
 void panadapterDataReady(QVector<float> v,float l,float h,float s,float e){count++;last=v;lo=l;hi=h;start=s;end=e;}
 void onLegacyWaterfallRow(QByteArray const&,int,int,int,int,QString const&);
};
#include "method.inc"
void check(bool b){if(!b)abort();}
int main(){ DecodiumBridge b; Remote r;b.m_remoteServer=&r; QByteArray v;v.append(char(0));v.append(char(127));v.append(char(254));
 b.onLegacyWaterfallRow(v,100,300,120,130,"FT8");check(r.calls==1 && b.count==1 && b.last.size()==3 && b.last[0]==-130 && b.last[2]==-40 && b.start==100 && b.end==400);
 b.onLegacyWaterfallRow(QByteArray(),100,300,120,130,"FT8");check(r.calls==1 && b.count==1);
 b.onLegacyWaterfallRow(v,100,0,120,130,"FT8");check(r.calls==1 && b.count==1);
 b.legacy=false;b.onLegacyWaterfallRow(v,100,300,120,130,"FT8");check(r.calls==2 && b.count==1);
 b.legacy=true;b.m_lastPanadapterData="x";b.m_lastPanadapterFrameMs=QDateTime::currentMSecsSinceEpoch();b.modern=true;b.onLegacyWaterfallRow(v,100,300,120,130,"FT8");check(r.calls==2 && b.count==1);
 b.m_lastPanadapterFrameMs-=3000;b.onLegacyWaterfallRow(v,100,300,120,130,"FT8");check(r.calls==3 && b.count==1);
 b.modern=false;b.m_legacyPcmSpectrumFeed=true;b.onLegacyWaterfallRow(v,100,300,120,130,"FT8");check(r.calls==4 && b.count==1);
 b.m_legacyPcmSpectrumFeed=false;r.enabled=false;b.onLegacyWaterfallRow(v,100,300,120,130,"FT8");check(r.calls==4 && b.count==2);
 std::cout<<"waterfall A/B: scale, bounds, empty rows, source and remote guards pass\n"; }
'''
with tempfile.TemporaryDirectory(prefix='decodium-waterfall-ab-') as t:
 d=Path(t);(d/'test.cpp').write_text(h);outs=[]
 for code in (a,b):
  (d/'method.inc').write_text(code);exe=d/'test';subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(d/'test.cpp'),*flags,'-o',str(exe)],check=True);outs.append(subprocess.check_output([str(exe)],text=True))
 if outs[0]!=outs[1]:raise SystemExit('Waterfall A/B mismatch')
 print(outs[0],end='')
