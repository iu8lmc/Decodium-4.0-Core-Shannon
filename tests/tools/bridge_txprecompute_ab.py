#!/usr/bin/env python3
"""Compare TX precompute and device resolution source; exercise guarded paths."""
import os,shlex,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[2];old=(r/'tests/bridge_refactor_fixtures/txprecompute_before.inc').read_text();new='QAudioDevice DecodiumBridge::resolveTxOutputDevice('+(r/'src/bridge/DecodiumBridgeTxPrecompute.cpp').read_text().split('QAudioDevice DecodiumBridge::resolveTxOutputDevice(',1)[1]
for a,b in [('decodiumBridgeFindTxOutputDevice(', 'findOutputDevice('),('decodiumBridgeAudioDeviceIdSettingForLog(', 'audioDeviceIdSettingForLog('),('decodiumBridgeAudioDeviceIdForSettings(', 'audioDeviceIdForSettings('),('decodiumBridgeTxPrecomputeLog(', 'bridgeLog(')]:new=new.replace(a,b)
assert old==new,'TX precompute source differs'
program=r'''
#include <QCoreApplication>
#include <QTimer>
#include <QEventLoop>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QHash>
#include <iostream>
int slotDelay=0,logCount=0;int txAudioPrecomputeRetryDelayMs(const QString&,bool){return slotDelay;}void bridgeLog(const QString&){++logCount;}
class DecodiumBridge:public QObject{public:bool m_txAudioPrecomputeScheduled=false,m_shuttingDown=false,m_transmitting=false,m_tuning=false,m_decoding=false,m_asyncDecodePending=false,m_monitoring=true,m_txEnabled=true,m_autoCqRepeat=false,m_deferredManualSyncTx=false,m_decoPortUseRemote=false;QHash<int,int> m_decodeStartMsBySerial;QString m_mode="FT2",message="CQ TEST";int calls=0;bool lastPcm=false,fail=false;QString buildCurrentTxMessage(){return message;}bool usingTciAudioInput(){return false;}bool shouldUseBridgeAudioForLegacyDigitalTx(){return true;}int effectiveTxAudioFrequencyHz(){return 1500;}bool ensureTxAudioPrepared(const QString&,int,bool pcm,QVector<float>*w,QByteArray*p,QAudioFormat*,QAudioDevice*,QString*err){calls++;lastPcm=pcm;w->append(0.5f);p->append("xx");if(fail){*err="failed";return false;}return true;}void scheduleTxAudioPrecompute(int);void precomputeTxAudioForCurrentMessage(const QString&);};
#include "methods.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);for(int k=0;k<11;k++){DecodiumBridge b;slotDelay=0;logCount=0;if(k==0)b.m_shuttingDown=true;if(k==1)b.m_transmitting=true;if(k==2)b.m_tuning=true;if(k==3)b.m_decoding=true;if(k==4)b.m_asyncDecodePending=true;if(k==5)slotDelay=500;if(k==6)b.m_txEnabled=false;if(k==7)b.message="  ";if(k==8)b.m_decoPortUseRemote=true;if(k==9)b.fail=true;b.precomputeTxAudioForCurrentMessage("test");std::cout<<b.calls<<':'<<b.lastPcm<<':'<<b.m_txAudioPrecomputeScheduled<<':'<<logCount<<'\n';}DecodiumBridge b;b.scheduleTxAudioPrecompute(0);b.scheduleTxAudioPrecompute(0);QEventLoop loop;QTimer::singleShot(10,&loop,&QEventLoop::quit);loop.exec();std::cout<<"timer:"<<b.calls<<':'<<b.m_txAudioPrecomputeScheduled<<'\n';}
'''
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','Qt6Core','Qt6Multimedia'],text=True));out=[]
with tempfile.TemporaryDirectory(prefix='decodium-precompute-ab-') as t:
 p=Path(t);(p/'main.cpp').write_text(program)
 for body in (old,new):
  (p/'methods.inc').write_text('void DecodiumBridge::scheduleTxAudioPrecompute('+body.split('void DecodiumBridge::scheduleTxAudioPrecompute(',1)[1]);subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++17',str(p/'main.cpp'),*flags,'-o',str(p/'test')],check=True);out.append(subprocess.check_output([str(p/'test')],text=True))
assert out[0]==out[1],out
lines=out[0].splitlines();assert len(lines)==12 and lines[3]=='0:0:1:0' and lines[8]=='1:0:0:1' and lines[-1]=='timer:1:0',lines
print('TX precompute A/B: guards, delayed retry, remote PCM, failure and timer deduplication passed')
