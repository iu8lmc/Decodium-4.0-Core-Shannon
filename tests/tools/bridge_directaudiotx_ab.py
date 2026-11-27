#!/usr/bin/env python3
"""Compare direct CW and FT2-Link audio request handling."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/directaudiotx_before.inc').read_text();s=(r/'src/bridge/DecodiumBridgeDirectAudioTx.cpp').read_text();mark='void DecodiumBridge::sendCwAudio(';new=mark+s.split(mark,1)[1].split('#undef bridgeLog',1)[0];assert old==new
source=r'''
#include <QObject>
#include <QCoreApplication>
#include <QMetaObject>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QVariantMap>
#include <QVector>
#include <QStringList>
#include <algorithm>
#include <cmath>
#include <iostream>
#define emit
QStringList events;void bridgeLog(const QString&s){events<<"log:"+s;}
class DecodiumBridge:public QObject{public:QString m_pendingCwText,m_pendingFt2LinkText,m_mode="FT8";QVector<float>m_pendingFt2LinkWave;QVariantMap m_pendingFt2LinkPlan;QVector<short>m_audioBuffer;QMutex m_audioBufferMutex;bool m_transmitting=false,m_tuning=false,m_cwTxActive=false,m_ft2LinkTxActive=false,m_monitoring=false,m_rxAudioSuspendedForTx=false,startOk=false,sat=false,satOk=false;void*m_soundInput=nullptr;int m_cwWpm=0,m_cwSidetoneHz=700,m_currentTx=1;double frequency=0;
 void statusMessage(const QString&s){events<<"status:"+s;}void errorMessage(const QString&s){events<<"error:"+s;}void ft2LinkTxFinished(){events<<"finished";}void armExplicitManualTxRequest(int,const QString&s){events<<"arm:"+s;}void setFrequency(double f){frequency=f;events<<QString("freq:%1").arg(f,0,'f',0);}void startTx(){events<<"start";m_transmitting=startOk;}bool ft2LinkSatelliteHalfDuplexRequested(){return sat;}bool beginFt2LinkSatelliteHalfDuplexTx(){events<<"sat-start";return satOk;}
 void sendCwAudio(const QString&,qint64,int);bool transmitFt2LinkAudio(const QString&,const QVector<float>&,const QVariantMap&);};
#include "methods.inc"
void show(const char*n,DecodiumBridge&b){std::cout<<n<<':'<<b.m_cwTxActive<<':'<<b.m_ft2LinkTxActive<<':'<<b.m_cwWpm<<':'<<b.m_pendingCwText.toStdString()<<':'<<b.m_pendingFt2LinkWave.size()<<'|'<<events.join(';').toStdString()<<'\n';events.clear();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;b.sendCwAudio(" ",0,20);show("cw-empty",b);b.m_tuning=true;b.sendCwAudio("HI",0,20);show("cw-busy",b);b.m_tuning=false;b.sendCwAudio(" HI ",14000000,100);show("cw-fail",b);b.startOk=true;b.sendCwAudio("HI",14000000,1);show("cw-ok",b);b.m_transmitting=false;b.m_cwTxActive=false;QVariantMap p;QVector<float>empty,quiet{0,0},wave{.1f,-.2f};std::cout<<"empty-return:"<<b.transmitFt2LinkAudio("",empty,p)<<'\n';show("ft-empty",b);std::cout<<"quiet-return:"<<b.transmitFt2LinkAudio("",quiet,p)<<'\n';show("ft-quiet",b);b.m_tuning=true;std::cout<<"busy-return:"<<b.transmitFt2LinkAudio("",wave,p)<<'\n';show("ft-busy",b);b.m_tuning=false;b.sat=true;std::cout<<"sat-fail-return:"<<b.transmitFt2LinkAudio("",wave,p)<<'\n';show("sat-fail",b);b.satOk=true;std::cout<<"sat-ok-return:"<<b.transmitFt2LinkAudio("",wave,p)<<'\n';show("sat-ok",b);b.sat=false;b.startOk=false;std::cout<<"ft-fail-return:"<<b.transmitFt2LinkAudio("test",wave,p)<<'\n';show("ft-fail",b);b.startOk=true;std::cout<<"ft-ok-return:"<<b.transmitFt2LinkAudio("test",wave,p)<<'\n';show("ft-ok",b);}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True))
with tempfile.TemporaryDirectory(prefix='decodium-directaudiotx-ab-') as d:
 p=Path(d);(p/'main.cpp').write_text(source);out=[]
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1];v=out[0]
for item in ('CW: testo vuoto','CW: TX o Tune','cw-fail:0:0:60:', 'freq:13999300','cw-ok:1:0:5:HI','empty-return:0','piano TX radio vuoto','quiet-return:0','piano TX radio silenzioso','busy-return:0','sat-fail-return:0','sat-fail:0:0:', 'sat-ok-return:1','ft-fail-return:0','ft-ok-return:1'): assert item in v,item
print('Direct audio TX A/B: CW validation/clamps and FT2-Link guards, satellite and start outcomes passed')
