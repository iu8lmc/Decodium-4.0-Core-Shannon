#!/usr/bin/env python3
"""A/B satellite CAT/PTT guards and real Qt timer transitions with a fake rig."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/satellitehd_before.inc').read_text();new='bool DecodiumBridge::ft2LinkSatelliteHalfDuplexRequested() const'+(r/'src/bridge/DecodiumBridgeSatelliteHalfDuplex.cpp').read_text().split('bool DecodiumBridge::ft2LinkSatelliteHalfDuplexRequested() const',1)[1];new=new.replace('decodiumBridgeSatelliteHdLog(', 'bridgeLog(');assert old==new
src=r'''
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QVariant>
#include <QVariantMap>
#include <QVector>
#include <QString>
#include <cmath>
#include <iostream>
#include "src/bridge/Ft2LinkSatelliteHalfDuplex.h"
void bridgeLog(const QString&){}
struct Cat {bool isConnected=true,pttCapable=true,ptt=false,prepare=true,restore=true,splitFlag=false;QString mode="rig";double rx=0,tx=0;bool connected(){return isConnected;}QString splitMode(){return mode;}bool canPtt(){return pttCapable;}bool pttActive(){return ptt;}bool split(){return splitFlag;}double frequency(){return rx;}double txFrequency(){return tx;}bool prepareSatelliteHalfDuplex(double a,double b){if(!prepare)return false;rx=a;tx=b;splitFlag=true;return true;}bool restoreSatelliteHalfDuplexRx(double a){if(!restore)return false;rx=a;tx=0;splitFlag=false;return true;}};
class DecodiumBridge:public QObject{public:int m_ft2LinkSatelliteHalfDuplexState=0,m_ft2LinkSatelliteHalfDuplexSettleMs=0,changes=0,finished=0,starts=0;quint64 m_ft2LinkSatelliteHalfDuplexSerial=0;qint64 m_ft2LinkSatelliteHalfDuplexRxDialHz=0,m_ft2LinkSatelliteHalfDuplexTxDialHz=0;QString m_ft2LinkSatelliteHalfDuplexDetail,m_catBackend="hamlib",m_pendingFt2LinkText,lastError;QVector<float> m_pendingFt2LinkWave;QVariantMap m_pendingFt2LinkPlan,settings;bool m_ft2LinkTxActive=false,m_transmitting=false;Cat cat,*m_hamlibCat=&cat;QVariant getSetting(const QString& k,const QVariant& d)const{return settings.value(k,d);}void ft2LinkSatelliteHalfDuplexStatusChanged(){changes++;}void errorMessage(const QString& x){lastError=x;}void statusMessage(const QString&){}void ft2LinkTxFinished(){finished++;}void restoreTxAudioSchedulingBoost(const QString&){}void resumeRxAudioAfterTx(const QString&){}void resumeNonAudioTxWork(const QString&){}void startTx(){starts++;m_transmitting=true;}
 bool ft2LinkSatelliteHalfDuplexRequested()const;bool ft2LinkSatelliteHalfDuplexOperationActive()const;void setFt2LinkSatelliteHalfDuplexState(int,const QString&);QVariantMap ft2LinkSatelliteHalfDuplexStatus()const;bool beginFt2LinkSatelliteHalfDuplexTx();void finishFt2LinkSatelliteHalfDuplexTx(const QString&,bool);void restoreFt2LinkSatelliteHalfDuplexRx(const QString&,bool);void cancelFt2LinkSatelliteHalfDuplexTx(const QString&);};
#include "methods.inc"
void wait(){QEventLoop loop;QTimer::singleShot(300,&loop,&QEventLoop::quit);loop.exec();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);for(int n=0;n<9;n++){DecodiumBridge b;b.settings["Ft2LinkSatelliteRxDialHz"]=145900000;b.settings["Ft2LinkSatelliteTxDialHz"]=435100000;b.settings["Ft2LinkSatelliteCatSettleMs"]=250;if(n==0)b.settings["Ft2LinkSatelliteRxDialHz"]=0;if(n==1)b.cat.isConnected=false;if(n==2)b.cat.mode="none";if(n==3)b.cat.ptt=true;if(n==4)b.cat.prepare=false;if(n==5)b.cat.pttCapable=false;bool accepted=b.beginFt2LinkSatelliteHalfDuplexTx();if(n==6)b.cancelFt2LinkSatelliteHalfDuplexTx("cancel");if(n==7)b.cat.rx=0;wait();if(n==6||n==7)wait();if(n==8){b.restoreFt2LinkSatelliteHalfDuplexRx("done",false);wait();}std::cout<<accepted<<':'<<b.m_ft2LinkSatelliteHalfDuplexState<<':'<<b.starts<<':'<<b.finished<<':'<<(!b.lastError.isEmpty())<<':'<<b.cat.splitFlag<<'\n';}DecodiumBridge b;b.settings["Ft2LinkSatelliteHalfDuplexEnabled"]=true;b.settings["Ft2LinkSatelliteRxDialHz"]=145900000;b.settings["Ft2LinkSatelliteTxDialHz"]=435100000;b.settings["Ft2LinkSatelliteCatSettleMs"]=1;auto x=b.ft2LinkSatelliteHalfDuplexStatus();std::cout<<x["enabled"].toBool()<<':'<<x["settleMs"].toInt()<<':'<<x["supportedBackend"].toBool()<<':'<<x["catConnected"].toBool()<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core'],text=True));out=[]
with tempfile.TemporaryDirectory(prefix='decodium-satellitehd-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(src)
 for body in (old,new):
  (p/'methods.inc').write_text(body);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17','-I',str(r),str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1],out
lines=out[0].splitlines();assert len(lines)==10 and all(x.startswith('0:') for x in lines[:6]) and lines[6].startswith('1:0:0:1:0:0') and lines[7].startswith('1:0:0:1:1:0') and lines[8].startswith('1:0:1:1:0:0') and lines[9]=='1:250:1:1',lines
print('satellite half-duplex A/B: validation, CAT/PTT guards, successful TX preparation, cancellation, restore and timer verification passed')
