#include <QtCore>
#include <limits>
#include <cstdio>
#include "Sequencer/AutoCqCallPolicy.hpp"
static void bridgeLog(const QString&){}
struct Legacy {int calls=0,a=0,b=0,c=0,d=0;void setAutoCqBurstCadence(int x,int y,int z,int w){++calls;a=x;b=y;c=z;d=w;}};
class DecodiumBridge:public QObject {public:
 bool m_autoCqRepeat=true,m_tuning=false,m_activeTxWasPureAutoCqCq=false,m_shuttingDown=false,m_manualTxHold=false,m_transmitting=false,txEnabled=true,disabled=false;
 int m_autoCqBurstCalls=2,m_autoCqListenCycles=1,m_autoCqBurstCompletedCalls=0,m_autoCqMaxCycles=0,m_autoCqCycleCount=0,m_autoCqPauseSec=0,m_pendingAutoSeqTxAfterActiveTx=0,m_qsoProgress=0,wakes=0;
 qint64 m_autoCqBurstListenUntilMs=0,m_autoCqGenericListenUntilMs=0,now=100,period=10;
 quint64 m_autoCqBurstSerial=0,m_txRequestGeneration=1;QString m_tx6="CQ TEST",m_dxCall,m_autoCqLockedCall,m_autoCqBurstListenMode,m_mode="FT8";Legacy legacy,*m_legacyBackend=&legacy;
 bool isTxDisabled(int)const{return disabled;}qint64 correctedUtcEpochMs()const{return now;}qint64 effectivePeriodMsForMode(const QString&)const{return period;}
 void setAutoCqRepeat(bool x){m_autoCqRepeat=x;}void setTxEnabled(bool x){txEnabled=x;}void checkAndStartPeriodicTx(){++wakes;}
 bool autoCqCanRestartTx6()const;bool autoCqBurstCadenceEnabled()const;bool isAutoCqBurstPureCq(int,const QString&)const;
 void resetAutoCqBurstCadence(const QString&);bool autoCqBurstListening();void applyAutoCqBurstCadenceToLegacyBackend();void noteAutoCqBurstCqCompleted(bool,const QString&,bool);
};
#include "bridge_refactor_autocq_cadence.inc"
static void check(bool ok,const char* label){if(!ok){fprintf(stderr,"FAIL %s\n",label);exit(1);}puts(label);}
static void waitMs(int m){QEventLoop loop;QTimer::singleShot(m,&loop,&QEventLoop::quit);loop.exec();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;
 check(b.autoCqCanRestartTx6()&&b.autoCqBurstCadenceEnabled()&&b.isAutoCqBurstPureCq(6,"CQ TEST"),"eligible");b.m_dxCall="AB1CD";check(!b.isAutoCqBurstPureCq(6,"CQ TEST"),"partner-blocks");b.m_dxCall.clear();b.disabled=true;check(!b.autoCqCanRestartTx6(),"tx6-disabled");b.disabled=false;
 b.applyAutoCqBurstCadenceToLegacyBackend();check(b.legacy.calls==1&&b.legacy.a==2&&b.legacy.b==1,"legacy-sync");
 b.noteAutoCqBurstCqCompleted(false,"ignored",false);b.noteAutoCqBurstCqCompleted(true,"error",true);check(b.m_autoCqCycleCount==0,"ignore-failed");
 b.noteAutoCqBurstCqCompleted(true,"first",false);check(b.m_autoCqCycleCount==1&&b.m_autoCqBurstCompletedCalls==1&&!b.autoCqBurstListening(),"first-call");
 b.noteAutoCqBurstCqCompleted(true,"second",false);check(b.m_autoCqCycleCount==2&&b.m_autoCqBurstListenUntilMs==110&&b.autoCqBurstListening(),"burst-pause");b.now=111;waitMs(70);check(b.wakes==1&&!b.autoCqBurstListening(),"pause-wakes");
 b.m_autoCqGenericListenUntilMs=200;check(b.autoCqBurstListening(),"generic-pause");b.m_autoCqGenericListenUntilMs=0;b.m_autoCqBurstListenMode="FT4";b.m_autoCqBurstListenUntilMs=250;check(!b.autoCqBurstListening()&&b.m_autoCqBurstListenUntilMs==0,"mode-reset");
 b.m_autoCqMaxCycles=3;b.noteAutoCqBurstCqCompleted(true,"limit",false);check(!b.m_autoCqRepeat&&!b.txEnabled&&b.m_autoCqCycleCount==3,"limit-stops");
}
